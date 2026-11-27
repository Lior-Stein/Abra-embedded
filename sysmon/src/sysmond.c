
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
#include <mqueue.h>

extern char **environ;
static mqd_t mem_queue = (mqd_t)-1;
static mqd_t cpu_queue = (mqd_t)-1;
static mqd_t proc_queue = (mqd_t)-1;

static pid_t mem_pid = -1;
static pid_t cpu_pid = -1;
static pid_t proc_pid = -1;


volatile sig_atomic_t stop_requested = 0;

void stop_handler(int sig)
{
    (void)sig;
    stop_requested = 1;
}


static mqd_t create_queue(const char *name, size_t message_size)
{
    if (mq_unlink(name) == -1 && errno != ENOENT) {
        perror("mq_unlink");
        return (mqd_t)-1;
    }

    struct mq_attr attr = {
        .mq_maxmsg = MAX_MESSAGES,
        .mq_msgsize = (long)message_size
    };

    mqd_t queue = mq_open(name, O_CREAT | O_RDONLY, 0666, &attr);
    if (queue == (mqd_t)-1)
        perror(name);

    return queue;
}

static pid_t spawn_sensor(char *path)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        mq_close(mem_queue);
        mq_close(cpu_queue);
        mq_close(proc_queue);

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

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    mem_queue = create_queue(MEM_QUEUE_NAME, sizeof(MemData));
    if (mem_queue == (mqd_t)-1)
        return 1;

    cpu_queue = create_queue(CPU_QUEUE_NAME, sizeof(CpuData));
    if (cpu_queue == (mqd_t)-1)
        return 1;

    proc_queue = create_queue(PROC_QUEUE_NAME, sizeof(ProcData));
    if (proc_queue == (mqd_t)-1)
        return 1;

    mem_pid = spawn_sensor("./bin/mem_sensor");
    if (mem_pid == -1)
        return 1;

    cpu_pid = spawn_sensor("./bin/cpu_sensor");
    if (cpu_pid == -1)
        return 1;

    proc_pid = spawn_sensor("./bin/proc_sensor");
    if (proc_pid == -1)
        return 1;

    return 0;
}

static int receive_sample(mqd_t queue, void *data, size_t size,
                          unsigned int *priority)
{
    if (stop_requested)
        return 0;

    ssize_t received = mq_receive(queue, data, size, priority);

    if (stop_requested)
        return 0;

    if (received == -1) {
        perror("mq_receive");
        return 0;
    }

    if (received != (ssize_t)size) {
        fprintf(stderr, "Unexpected message size\n");
        return 0;
    }

    return 1;
}

void processing(void)
{

    while(!stop_requested){
      
        MemData mem_data;
        unsigned int priority;

       if (!receive_sample(mem_queue, &mem_data, sizeof(mem_data), &priority))
            break;

        printf("=== Memory (PID %ld, Priority %u) ===\n",(long)mem_pid, priority);
        printf("MemTotal: %ld kB\n", mem_data.mem_total);
        printf("MemFree: %ld kB\n", mem_data.mem_free);
        printf("MemAvailable: %ld kB\n", mem_data.mem_avail);
        printf("SwapTotal: %ld kB\n", mem_data.swap_total);
        printf("SwapFree: %ld kB\n", mem_data.swap_free);



        CpuData cpu_data;

        if (!receive_sample(cpu_queue, &cpu_data, sizeof(cpu_data), &priority))
            break;

        printf("\n=== CPU (PID %ld, Priority %u) ===\n",(long)cpu_pid, priority);
        printf("1 min load: %.2f\n", cpu_data.load1);
        printf("5 min load: %.2f\n", cpu_data.load5);
        printf("15 min load: %.2f\n", cpu_data.load15);



        ProcData proc_data;

        if (!receive_sample(proc_queue, &proc_data, sizeof(proc_data), &priority))
            break;

        printf("\n=== Processes (PID %ld, Priority %u) ===\n",(long)proc_pid, priority);
        printf("Active processes: %d\n", proc_data.total_procs);



    }
}

static void remove_queue(mqd_t queue, const char *name)
{
    if (queue == (mqd_t)-1)
        return;

    mq_close(queue);
    mq_unlink(name);
}

void teardown(void)
{
    pid_t pids[] = {mem_pid, cpu_pid, proc_pid};
    size_t count = sizeof(pids) / sizeof(pids[0]);

    for (size_t i = 0; i < count; i++) {
        if (pids[i] > 0)
            kill(pids[i], SIGTERM);
    }

    for (size_t i = 0; i < count; i++) {
        if (pids[i] > 0) {
            while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR) {
                /* Retry if the wait was interrupted by a signal. */
            }
        }
    }

    remove_queue(mem_queue, MEM_QUEUE_NAME);
    remove_queue(cpu_queue, CPU_QUEUE_NAME);
    remove_queue(proc_queue, PROC_QUEUE_NAME);
}

int main(void)
{
    int status = init();

    if (status == 0)
        processing();

    teardown();
    return status;
}
