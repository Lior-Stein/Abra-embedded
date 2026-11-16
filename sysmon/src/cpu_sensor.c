
#include <stdio.h>
#include <unistd.h>
#include <sysmon.h>

void *cpu_sensor_thread(void *arg)
{
    while (running) {
        FILE *file = fopen("/proc/loadavg", "r");

        if (file == NULL) {
            perror("fopen");
            return NULL;
        }

     

        double load1, load5, load15;

        
        if (fscanf(file, "%lf %lf %lf", &load1, &load5, &load15) == 3) {
            pthread_mutex_lock(&state.lock);

            state.load1 = load1;
            state.load5 = load5;
            state.load15 = load15;

            if (load1 > 1.50) {
                pthread_cond_signal(&state.alert_cond);
            }

            pthread_mutex_unlock(&state.lock);
        }


        
        fclose(file);
        sleep(5);
    }   
    return NULL;
}
