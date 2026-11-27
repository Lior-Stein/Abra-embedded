#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sysmon_ipc.h>

static volatile sig_atomic_t stop_requested = 0;

static void stop_handler(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static int read_loadavg(CpuData *data)
{
    FILE *file = fopen("/proc/loadavg", "r");
    if (file == NULL) {
        perror("fopen /proc/loadavg");
        return -1;
    }

    int fields = fscanf(file, "%lf %lf %lf",
                        &data->load1, &data->load5, &data->load15);
    fclose(file);

    return fields == 3;
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = stop_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        return 1;
    }

    mqd_t queue = mq_open(CPU_QUEUE_NAME, O_WRONLY);
    if (queue == (mqd_t)-1) {
        perror("mq_open " CPU_QUEUE_NAME);
        return 1;
    }

    int status = 0;

    while (!stop_requested) {
        CpuData data = {0};

        int result = read_loadavg(&data);

        if (result == -1) {
            status = 1;
            break;
        }

        if (stop_requested)
            break;

        if (result == 0) {
            sleep(1);
            continue;
        }

        unsigned int priority = (data.load1 > 1.50) ? 10U : 1U;

        if (mq_send(queue, (const char *)&data, sizeof(data), priority) == -1) {
            if (errno == EINTR)
                continue;

            perror("mq_send");
            status = 1;
            break;
        }

        sleep(1);
    }

    mq_close(queue);
    return status;
}