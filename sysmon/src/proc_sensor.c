
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sysmon.h>

void *proc_sensor_thread(void *arg)
{
    while(running){
        DIR *dir = opendir("/proc");

        if (dir == NULL) {
            perror("opendir");
            return NULL;
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
        
        pthread_mutex_lock(&state.lock);

        state.total_procs = count;

        pthread_mutex_unlock(&state.lock);

        sleep(5);
    }
    return NULL;
}
