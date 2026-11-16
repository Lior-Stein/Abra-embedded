
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysmon.h>
#include <signal.h>

SystemState state;

volatile sig_atomic_t running = 1;

void handle_sigint(int sig)
{
    running = 0;
}


void *alert_thread(void *arg)
{
    pthread_mutex_lock(&state.lock);

    while (running) {
        pthread_cond_wait(&state.alert_cond, &state.lock);

        if (running && state.load1 > 1.50) {
            fprintf(stderr, "ALERT: High CPU load: %.2f\n", state.load1);
        }
    }

    pthread_mutex_unlock(&state.lock);
    return NULL;
}



int main(void)
{
    signal(SIGINT, handle_sigint);

    pthread_mutex_init(&state.lock, NULL);
    pthread_cond_init(&state.alert_cond, NULL);

    pthread_t threads[4];
    pthread_create(&threads[0], NULL, mem_sensor_thread, NULL);
    pthread_create(&threads[1], NULL, cpu_sensor_thread, NULL);
    pthread_create(&threads[2], NULL, proc_sensor_thread, NULL);
    pthread_create(&threads[3], NULL, alert_thread, NULL);

    while (running) {
        pthread_mutex_lock(&state.lock);

        printf("\033[2J\033[H");
        printf("===== SYSTEM MONITOR =====\n");

        printf("\n=== Memory ===\n");
        printf("MemTotal: %ld kB\n", state.mem_total);
        printf("MemFree: %ld kB\n", state.mem_free);
        printf("MemAvailable: %ld kB\n", state.mem_avail);
        printf("SwapTotal: %ld kB\n", state.swap_total);
        printf("SwapFree: %ld kB\n", state.swap_free);

        printf("\n=== CPU ===\n");
        printf("1 min load: %.2f\n", state.load1);
        printf("5 min load: %.2f\n", state.load5);
        printf("15 min load: %.2f\n", state.load15);

        printf("\n=== Processes ===\n");
        printf("Active processes: %d\n", state.total_procs);

        pthread_mutex_unlock(&state.lock);

        sleep(2);
    }

    pthread_mutex_lock(&state.lock);
    pthread_cond_signal(&state.alert_cond);
    pthread_mutex_unlock(&state.lock);

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

