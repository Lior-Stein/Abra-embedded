
#include <stdio.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <fcntl.h>

int main(void)
{
    int fifo_fd = open(CPU_FIFO_PATH, O_WRONLY);

    if (fifo_fd == -1) {
        perror("open CPU FIFO");
        return 1;
    }

    FILE *fifo = fdopen(fifo_fd, "w");

    if (fifo == NULL) {
        perror("fdopen");
        close(fifo_fd);
        return 1;
    }

    while (1) {
        FILE *file = fopen("/proc/loadavg", "r");

        if (file == NULL) {
            perror("fopen");
            return 1;
        }


        double load1, load5, load15;

        if (fscanf(file, "%lf %lf %lf", &load1, &load5, &load15) == 3) {
            CpuData data = {load1, load5, load15};
            
            fwrite(&data, sizeof(data), 1, fifo);
            fflush(fifo);
        }

      
        fclose(file);
        sleep(20);
    }   
    return 0;
}
