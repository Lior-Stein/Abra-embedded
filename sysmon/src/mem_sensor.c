#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysmon_ipc.h>

static volatile sig_atomic_t stop_requested = 0;

static void stop_handler(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static int read_meminfo(MemData *data)
{
    FILE *file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("fopen /proc/meminfo");
        return -1;
    }

    char line[256];
    char key[64];
    unsigned long value;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%63[^:]: %lu", key, &value) == 2) {
            if (strcmp(key, "MemTotal") == 0)
                data->mem_total = value;
            else if (strcmp(key, "MemFree") == 0)
                data->mem_free = value;
            else if (strcmp(key, "MemAvailable") == 0)
                data->mem_avail = value;
            else if (strcmp(key, "SwapTotal") == 0)
                data->swap_total = value;
            else if (strcmp(key, "SwapFree") == 0)
                data->swap_free = value;
        }
    }

    fclose(file);
    return 0;
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

    mqd_t queue = mq_open(MEM_QUEUE_NAME, O_WRONLY);
    if (queue == (mqd_t)-1) {
        perror("mq_open " MEM_QUEUE_NAME);
        return 1;
    }

    int status = 0;

    while (!stop_requested) {
        MemData data = {0};

        if (read_meminfo(&data) == -1) {
            status = 1;
            break;
        }

        if (stop_requested)
            break;

        if (mq_send(queue, (const char *)&data, sizeof(data), 1) == -1) {
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