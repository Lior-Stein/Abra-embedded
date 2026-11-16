
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysmon.h>

void *mem_sensor_thread(void *arg)
{
  while (running) {
    FILE *file = fopen("/proc/meminfo", "r");

    if (file == NULL) {
        perror("fopen");
        return NULL;
    }



    char line[256];
    char key[64];
    unsigned long value;


    pthread_mutex_lock(&state.lock);

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%63[^:]: %lu", key, &value) == 2) {
            if (strcmp(key, "MemTotal") == 0)
                state.mem_total = value;
            else if (strcmp(key, "MemFree") == 0)
                state.mem_free = value;
            else if (strcmp(key, "MemAvailable") == 0)
                state.mem_avail = value;
            else if (strcmp(key, "SwapTotal") == 0)
                state.swap_total = value;
            else if (strcmp(key, "SwapFree") == 0)
                state.swap_free = value;
        }
    }

    pthread_mutex_unlock(&state.lock);


    fclose(file);
    sleep(5);
 } 
    return NULL;
}
