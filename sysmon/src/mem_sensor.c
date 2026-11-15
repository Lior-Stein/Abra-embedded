
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
  while (1) {
    FILE *file = fopen("/proc/meminfo", "r");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    FILE *output = fopen("/tmp/sysmon/mem.dat", "w");

    if (output == NULL) {
        perror("fopen output");
        fclose(file);
        return 1;
    }

    char line[256];
    char key[64];
    unsigned long value;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%63[^:]: %lu", key, &value) == 2) {
            if (strcmp(key, "MemTotal") == 0 ||
                strcmp(key, "MemFree") == 0 ||
                strcmp(key, "MemAvailable") == 0 ||
                strcmp(key, "SwapTotal") == 0 ||
                strcmp(key, "SwapFree") == 0) {

                fprintf(output, "%s: %lu kB\n", key, value);
            }
        }
    }

    fclose(output);
    fclose(file);
    sleep(5);
 } 
    return 0;
}
