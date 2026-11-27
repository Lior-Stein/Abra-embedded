
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
typedef struct {
    pid_t pid;
    int read_fd;
    unsigned int restart_count;
} SensorState;

static SensorState mem_state  = { .pid = -1, .read_fd = -1 };
static SensorState cpu_state  = { .pid = -1, .read_fd = -1 };
static SensorState proc_state = { .pid = -1, .read_fd = -1 };
static int cpu_fifo_created = 0;
static int proc_fifo_created = 0;

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
    (void)sig;
    stop_requested = 1;
}

static void prepare_sensor_child(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    if (stop_requested)
        _exit(0);

    if (mem_state.read_fd >= 0)
        close(mem_state.read_fd);

    if (cpu_state.read_fd >= 0)
        close(cpu_state.read_fd);

    if (proc_state.read_fd >= 0)
        close(proc_state.read_fd);
}

static pid_t spawn_fifo_sensor(char *path)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        prepare_sensor_child();

        char *args[] = { path, NULL };
        execve(path, args, environ);

        perror(path);
        _exit(1);
    }

    return pid;
}

static int spawn_mem_sensor(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("mem_sensor: pipe");
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("mem_sensor: fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        prepare_sensor_child();
        close(pipefd[0]);

        if (pipefd[1] != STDOUT_FILENO) {
            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("mem_sensor: dup2");
                _exit(1);
            }

            close(pipefd[1]);
        }

        char *args[] = { "./bin/mem_sensor", NULL };
        execve(args[0], args, environ);

        perror("mem_sensor: execve");
        _exit(1);
    }

    close(pipefd[1]);
    mem_state.pid = pid;
    mem_state.read_fd = pipefd[0];
    return 0;
}


int init(void)
{

    struct sigaction sa = {0};
    sa.sa_handler = stop_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);


    if (spawn_mem_sensor() == -1)
        return 1;

    if (mkfifo(CPU_FIFO_PATH, 0666) == 0) {
        cpu_fifo_created = 1;
    } else if (errno != EEXIST) {
        perror("mkfifo CPU");
        return 1;
    }

    if (mkfifo(PROC_FIFO_PATH, 0666) == 0) {
        proc_fifo_created = 1;
    } else if (errno != EEXIST) {
        perror("mkfifo PROC");
        return 1;
    }

    cpu_state.pid = spawn_fifo_sensor("./bin/cpu_sensor");
    if (cpu_state.pid == -1)
        return 1;

    proc_state.pid = spawn_fifo_sensor("./bin/proc_sensor");
    if (proc_state.pid == -1)
        return 1;
    

    cpu_state.read_fd = open(CPU_FIFO_PATH, O_RDONLY);
   
    if (cpu_state.read_fd == -1) {
        perror("open CPU FIFO");
        return 1;
    }

    proc_state.read_fd = open(PROC_FIFO_PATH, O_RDONLY);

    if (proc_state.read_fd == -1) {
        perror("open PROC FIFO");
        return 1;
    }
    return 0;
}

static int reap_sensor(SensorState *sensor, const char *name)
{
    int status;

    if (sensor->pid <= 0)
        return 0;

    pid_t finished = waitpid(sensor->pid, &status, WNOHANG);

    if (finished == 0)
        return 0;

    if (finished == -1) {
        if (!stop_requested)
            perror("waitpid");
        return -1;
    }

    sensor->pid = -1;

    if (WIFSIGNALED(status))
        fprintf(stderr, "%s terminated by signal %d\n", name, WTERMSIG(status));
    else if (WIFEXITED(status))
        fprintf(stderr, "%s exited with status %d\n", name, WEXITSTATUS(status));

    return 1;
}

typedef enum {
    SAMPLE_STOP,
    SAMPLE_SKIP,
    SAMPLE_OK
} SampleResult;

static SampleResult read_sensor_sample(SensorState *sensor, const char *name,
                                       char *fifo_program,
                                       void *buffer, size_t size)
{
    if (stop_requested)
        return SAMPLE_STOP;

    int result = read_full(sensor->read_fd, buffer, size);

    if (stop_requested)
        return SAMPLE_STOP;

    if (result == 1)
        return SAMPLE_OK;

    if (result == -1) {
        perror(name);
        return SAMPLE_STOP;
    }

    int reaped = reap_sensor(sensor, name);

    if (stop_requested || reaped < 0)
        return SAMPLE_STOP;

    if (reaped == 0)
        return SAMPLE_SKIP;

    if (fifo_program == NULL) {
        close(sensor->read_fd);
        sensor->read_fd = -1;

        if (spawn_mem_sensor() == -1)
            return SAMPLE_STOP;
    } else {
        pid_t new_pid = spawn_fifo_sensor(fifo_program);

        if (new_pid == -1)
            return SAMPLE_STOP;

        sensor->pid = new_pid;
    }

    sensor->restart_count++;

    fprintf(stderr, "%s restarted (PID %ld) | Restart Count: %u\n",
            name, (long)sensor->pid, sensor->restart_count);

    return SAMPLE_SKIP;
}

void processing(void)
{
    while (!stop_requested) {
        MemData mem_data;
        SampleResult mem_result =
            read_sensor_sample(&mem_state, "mem_sensor", NULL,
                            &mem_data, sizeof(mem_data));

        if (mem_result == SAMPLE_STOP)
            break;

        if (mem_result == SAMPLE_OK) {
            printf("=== Memory ===\n");
            printf("MemTotal: %ld kB\n", mem_data.mem_total);
            printf("MemFree: %ld kB\n", mem_data.mem_free);
            printf("MemAvailable: %ld kB\n", mem_data.mem_avail);
            printf("SwapTotal: %ld kB\n", mem_data.swap_total);
            printf("SwapFree: %ld kB\n", mem_data.swap_free);
        }

        CpuData cpu_data;
        SampleResult cpu_result =
            read_sensor_sample(&cpu_state, "cpu_sensor", "./bin/cpu_sensor",
                               &cpu_data, sizeof(cpu_data));

        if (cpu_result == SAMPLE_STOP)
            break;

        if (cpu_result == SAMPLE_OK) {
            printf("\n=== CPU ===\n");
            printf("1 min load: %.2f\n", cpu_data.load1);
            printf("5 min load: %.2f\n", cpu_data.load5);
            printf("15 min load: %.2f\n", cpu_data.load15);
        }

        ProcData proc_data;
        SampleResult proc_result =
            read_sensor_sample(&proc_state, "proc_sensor", "./bin/proc_sensor",
                               &proc_data, sizeof(proc_data));

        if (proc_result == SAMPLE_STOP)
            break;

        if (proc_result == SAMPLE_OK) {
            printf("\n=== Processes ===\n");
            printf("Active processes: %d\n", proc_data.total_procs);
        }
    }
}

void teardown(void)
{
    SensorState *sensors[] = { &mem_state, &cpu_state, &proc_state };
    size_t count = sizeof(sensors) / sizeof(sensors[0]);

    for (size_t i = 0; i < count; i++) {
        if (sensors[i]->pid > 0)
            kill(sensors[i]->pid, SIGTERM);
    }

    for (size_t i = 0; i < count; i++) {
        if (sensors[i]->pid > 0) {
            while (waitpid(sensors[i]->pid, NULL, 0) == -1 &&
                   errno == EINTR) {
                /* Retry if a signal interrupted the wait. */
            }
        }

        if (sensors[i]->read_fd >= 0)
            close(sensors[i]->read_fd);
    }

    if (cpu_fifo_created)
        unlink(CPU_FIFO_PATH);

    if (proc_fifo_created)
        unlink(PROC_FIFO_PATH);
}

int main(void)
{
    int status = init();

    if (status == 0)
        processing();

    teardown();
    return status;
}