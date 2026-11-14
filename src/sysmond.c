
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    while(1){
        if (system("mkdir -p /tmp/sysmon") != 0) {
            printf("Failed to create directory\n");
            return 1;
        }

        FILE *mem = fopen("/tmp/sysmon/mem.dat", "r");

        if (mem == NULL) {
            perror("fopen mem.dat");
            return 1;
        }

        char line[256];

        printf("=== Memory ===\n");

        while (fgets(line, sizeof(line), mem) != NULL) {
            printf("%s", line);
        }

        fclose(mem);


        FILE *cpu = fopen("/tmp/sysmon/cpu.dat", "r");

        if (cpu == NULL) {
            perror("fopen cpu.dat");
            return 1;
        }

        printf("\n=== CPU ===\n");

        while (fgets(line, sizeof(line), cpu) != NULL) {
            printf("%s", line);
        }

        fclose(cpu);

        FILE *proc = fopen("/tmp/sysmon/proc.dat", "r");

        if (proc == NULL) {
            perror("fopen proc.dat");
            return 1;
        }

        printf("\n=== Processes ===\n");

        while (fgets(line, sizeof(line), proc) != NULL) {
            printf("%s", line);
        }

        fclose(proc);
        sleep(5);

    }
    return 0;
}
