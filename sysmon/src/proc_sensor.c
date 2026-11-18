
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <fcntl.h>

int main(void)
{
    int fifo_fd = open(PROC_FIFO_PATH, O_WRONLY);

    if (fifo_fd == -1) {
        perror("open PROC FIFO");
        return 1;
    }

    FILE *fifo = fdopen(fifo_fd, "w");

    if (fifo == NULL) {
        perror("fdopen");
        close(fifo_fd);
        return 1;
    }

    while(1){
        DIR *dir = opendir("/proc");

        if (dir == NULL) {
            perror("opendir");
            return 1;
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

        ProcData data = {count};

        fwrite(&data, sizeof(data), 1, fifo);
        fflush(fifo);

      
        sleep(1);
    }
    return 0;
}
