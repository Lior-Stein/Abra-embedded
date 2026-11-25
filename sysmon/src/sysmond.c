
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

extern char **environ;
int pipefd[2];
int cpu_fd, proc_fd;
pid_t mem_pid, cpu_pid, proc_pid;

int read_full(int fd, void *buffer, size_t size)
{
    size_t total = 0;

    while (total < size) {
        ssize_t n = read(fd, (char *)buffer + total, size - total);

        if (n == 0)
            return 0;

        if (n < 0)
            return -1;

        total += n;
    }

    return 1;
}

volatile sig_atomic_t stop_requested = 0;

void stop_handler(int sig)
{
    stop_requested = 1;
}


int init(void)
{

    struct sigaction sa = {0};
    sa.sa_handler = stop_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }


    mem_pid = fork();

    if (mem_pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (mem_pid == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(1);
        }

        close(pipefd[1]);

        char *args[] = {"./bin/mem_sensor", NULL};
        execve("./bin/mem_sensor", args, environ);

        perror("execve");
        _exit(1);
    }

    close(pipefd[1]);

    if (mkfifo(CPU_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo CPU");
    return 1;
    }

    if (mkfifo(PROC_FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo PROC");
        return 1;
    }


    cpu_pid = fork();

    if (cpu_pid == -1) {
        perror("fork cpu");
        return 1;
    }

    if (cpu_pid == 0) {
        close(pipefd[0]);

        char *args[] = {"./bin/cpu_sensor", NULL};
        execve("./bin/cpu_sensor", args, environ);

        perror("execve cpu_sensor");
        _exit(1);
    }

    proc_pid = fork();

    if (proc_pid == -1) {
        perror("fork proc");
        return 1;
    }

    if (proc_pid == 0) {
        close(pipefd[0]);

        char *args[] = {"./bin/proc_sensor", NULL};
        execve("./bin/proc_sensor", args, environ);

        perror("execve proc_sensor");
        _exit(1);
    }


    cpu_fd = open(CPU_FIFO_PATH, O_RDONLY);

    if (cpu_fd == -1) {
        perror("open CPU FIFO");
        return 1;
    }

    proc_fd = open(PROC_FIFO_PATH, O_RDONLY);

    if (proc_fd == -1) {
        perror("open PROC FIFO");
        return 1;
    }
    return 0;
}

void processing(void)
{

    while(!stop_requested){
      
        MemData mem_data;

        if (read_full(pipefd[0], &mem_data, sizeof(mem_data)) != 1) {
            fprintf(stderr, "Failed to read complete MemData\n");
            break;
        }

        printf("=== Memory ===\n");
        printf("MemTotal: %ld kB\n", mem_data.mem_total);
        printf("MemFree: %ld kB\n", mem_data.mem_free);
        printf("MemAvailable: %ld kB\n", mem_data.mem_avail);
        printf("SwapTotal: %ld kB\n", mem_data.swap_total);
        printf("SwapFree: %ld kB\n", mem_data.swap_free);



        CpuData cpu_data;

        if (read_full(cpu_fd, &cpu_data, sizeof(cpu_data)) != 1) {
            fprintf(stderr, "Failed to read complete CpuData\n");
            break;
        }

        printf("\n=== CPU ===\n");
        printf("1 min load: %.2f\n", cpu_data.load1);
        printf("5 min load: %.2f\n", cpu_data.load5);
        printf("15 min load: %.2f\n", cpu_data.load15);



        ProcData proc_data;

        if (read_full(proc_fd, &proc_data, sizeof(proc_data)) != 1) {
            fprintf(stderr, "Failed to read complete ProcData\n");
            break;
        }

        printf("\n=== Processes ===\n");
        printf("Active processes: %d\n", proc_data.total_procs);



    }
}

void teardown(void){

    kill(mem_pid, SIGTERM);
    kill(cpu_pid, SIGTERM);
    kill(proc_pid, SIGTERM);

    waitpid(mem_pid, NULL, 0);
    waitpid(cpu_pid, NULL, 0);
    waitpid(proc_pid, NULL, 0);

    close(pipefd[0]);
    close(cpu_fd);
    close(proc_fd);

    unlink(CPU_FIFO_PATH);
    unlink(PROC_FIFO_PATH);
}


int main(void)
{
    if (init() != 0)
        return 1;

    processing();
    teardown();



    return 0;
}
