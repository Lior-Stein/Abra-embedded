
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <stdlib.h>

int main(void)
{


  while (1) {
    FILE *file = fopen("/proc/meminfo", "r");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    MemData data = {0};

    char line[256];
    char key[64];
    unsigned long value;

    
    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%63[^:]: %lu", key, &value) == 2) {
            if (strcmp(key, "MemTotal") == 0)
                data.mem_total = value;
            else if (strcmp(key, "MemFree") == 0)
                data.mem_free = value;
            else if (strcmp(key, "MemAvailable") == 0)
                data.mem_avail = value;
            else if (strcmp(key, "SwapTotal") == 0)
                data.swap_total = value;
            else if (strcmp(key, "SwapFree") == 0)
                data.swap_free = value;
        }
    }

    fwrite(&data, sizeof(data), 1, stdout);
    fflush(stdout);


    fclose(file);
    sleep(1);
 } 
    return 0;
}
