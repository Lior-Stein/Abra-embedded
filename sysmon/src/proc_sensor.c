
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <fcntl.h>
#include <errno.h>
#include <mqueue.h>
#include <signal.h>

static volatile sig_atomic_t stop_requested = 0;

static void stop_handler(int sig)
{
    (void)sig;
    stop_requested = 1;
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

    mqd_t queue = mq_open(PROC_QUEUE_NAME, O_WRONLY);
    if (queue == (mqd_t)-1) {
        perror("mq_open " PROC_QUEUE_NAME);
        return 1;
    }

    int status = 0;


    while (!stop_requested){
        DIR *dir = opendir("/proc");

        if (dir == NULL) {
            perror("opendir");
            status = 1;
            break;
        }

        struct dirent *entry;
        int count = 0;

            
        while ((entry = readdir(dir)) != NULL) {
            int i = 0;

            while (entry->d_name[i] != '\0' &&
                isdigit((unsigned char)entry->d_name[i])) {
                i++;
            }

            if (i > 0 && entry->d_name[i] == '\0') {
                count++;
            }
        }

                closedir(dir);

        if (stop_requested)
            break;

        ProcData data = {count};

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



