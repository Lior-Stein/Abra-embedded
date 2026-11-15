
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    while (1) {
        FILE *file = fopen("/proc/loadavg", "r");

        if (file == NULL) {
            perror("fopen");
            return 1;
        }

        FILE *output = fopen("/tmp/sysmon/cpu.dat", "w");

        if (output == NULL) {
            perror("fopen output");
            fclose(file);
            return 1;
        }

        double load1, load5, load15;

        if (fscanf(file, "%lf %lf %lf", &load1, &load5, &load15) == 3) {
            fprintf(output, "1 min load: %.2f\n", load1);
            fprintf(output, "5 min load: %.2f\n", load5);
            fprintf(output, "15 min load: %.2f\n", load15);
        }

        fclose(output);
        fclose(file);
        sleep(5);
    }   
    return 0;
}
