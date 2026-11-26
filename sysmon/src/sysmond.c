
#include <stdio.h>
#include <unistd.h>
#include <sysmon_ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>

extern char **environ;
int pipefd[2];
int cpu_fd, proc_fd;
pid_t mem_pid, cpu_pid, proc_pid;


volatile sig_atomic_t stop_requested = 0;

void stop_handler(int sig)
{
    stop_requested = 1;
}

static pid_t spawn_fifo_sensor(char *path)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork FIFO sensor");
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);

        char *args[] = {path, NULL};
        execve(path, args, environ);

        perror(path);
        _exit(1);
    }

    return pid;
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


    cpu_pid = spawn_fifo_sensor("./bin/cpu_sensor");
    if (cpu_pid == -1)
        return 1;

    proc_pid = spawn_fifo_sensor("./bin/proc_sensor");
    if (proc_pid == -1)
        return 1;


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

static int read_sample(const struct pollfd *channel, void *buffer,
                       size_t size, size_t *received)
{
    if (channel->revents & (POLLERR | POLLNVAL))
        return -1;

    if (!(channel->revents & (POLLIN | POLLHUP)))
        return 0;

    ssize_t n = read(channel->fd, (char *)buffer + *received,
                     size - *received);

    if (n == 0)
        return -1;

    if (n < 0)
        return errno == EINTR ? 0 : -1;

    *received += (size_t)n;

    if (*received < size)
        return 0;

    *received = 0;
    return 1;
}

void processing(void)
{
    struct pollfd channels[] = {
        { .fd = pipefd[0], .events = POLLIN },  // Memory
        { .fd = cpu_fd,    .events = POLLIN },  // CPU
        { .fd = proc_fd,   .events = POLLIN }   // Processes
    };

    MemData mem_data;
    size_t mem_received = 0;
    CpuData cpu_data;
    size_t cpu_received = 0;

    ProcData proc_data;
    size_t proc_received = 0;

    while (!stop_requested) {
        int ready = poll(channels, sizeof(channels) / sizeof(channels[0]), 1000);

        if (stop_requested)
            break;

        if (ready == -1) {
            if (errno == EINTR)
                continue;

            perror("poll");
            break;
        }

        if (ready == 0)
            continue;
        
        int mem_result = read_sample(&channels[0], &mem_data,
                                    sizeof(mem_data), &mem_received);

        if (mem_result < 0) {
            fprintf(stderr, "Memory channel closed or read failed\n");
            break;
        }

        if (mem_result == 1) {
            printf("=== Memory ===\n");
            printf("MemTotal: %ld kB\n", mem_data.mem_total);
            printf("MemFree: %ld kB\n", mem_data.mem_free);
            printf("MemAvailable: %ld kB\n", mem_data.mem_avail);
            printf("SwapTotal: %ld kB\n", mem_data.swap_total);
            printf("SwapFree: %ld kB\n", mem_data.swap_free);
        }

        int cpu_result = read_sample(&channels[1], &cpu_data,
                                    sizeof(cpu_data), &cpu_received);

        if (cpu_result < 0) {
            fprintf(stderr, "CPU channel closed or read failed\n");
            break;
        }

        if (cpu_result == 1) {
            printf("\n=== CPU ===\n");
            printf("1 min load: %.2f\n", cpu_data.load1);
            printf("5 min load: %.2f\n", cpu_data.load5);
            printf("15 min load: %.2f\n", cpu_data.load15);
        }

        int proc_result = read_sample(&channels[2], &proc_data,
                                     sizeof(proc_data), &proc_received);

        if (proc_result < 0) {
            fprintf(stderr, "Processes channel closed or read failed\n");
            break;
        }

        if (proc_result == 1) {
            printf("\n=== Processes ===\n");
            printf("Active processes: %d\n", proc_data.total_procs);
        }

    }
}

void teardown(void)
{

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
