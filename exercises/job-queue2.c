#include <malloc.h>
#include <pthread.h>
#include <stdio.h>

struct job {
  /* Link field for linked list.  */
  struct job* next;
  int id; 

  /* Other fields describing work to be done... */
};

/* A linked list of pending jobs.  */
struct job* job_queue;

extern void process_job (struct job*);

void process_job(struct job* job)
{
    printf("Processing job %d\n", job->id);
}

/* A mutex protecting job_queue.  */
pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Process queued jobs until the queue is empty.  */

void* thread_function (void* arg)
{
  while (1) {
    struct job* next_job;

    /* Lock the mutex on the job queue.  */
    pthread_mutex_lock (&job_queue_mutex);
    /* Now it's safe to check if the queue is empty.  */
    if (job_queue == NULL)
      next_job = NULL;
    else {
      /* Get the next available job.  */
      next_job = job_queue;
      /* Remove this job from the list.  */
      job_queue = job_queue->next;
    }
    /* Unlock the mutex on the job queue, since we're done with the
       queue for now.  */
    pthread_mutex_unlock (&job_queue_mutex);

    /* Was the queue empty?  If so, end the thread.  */
    if (next_job == NULL)
      break;

    /* Carry out the work.  */
    process_job (next_job);
    /* Clean up.  */
    free (next_job);
  }
  return NULL;
}

int main(void)
{
    struct job* last = NULL;

    for (int i = 1; i <= 5; i++) {
        struct job* new_job = malloc(sizeof(struct job));

        if (new_job == NULL) {
            perror("malloc");
            return 1;
        }

        new_job->id = i;
        new_job->next = NULL;

        if (job_queue == NULL)
            job_queue = new_job;
        else
            last->next = new_job;

        last = new_job;
    }

    pthread_t threads[3];

    for (int i = 0; i < 3; i++) {
    	pthread_create(&threads[i], NULL, thread_function, NULL);
    }

    for (int i = 0; i < 3; i++) {
    	pthread_join(threads[i], NULL);
    }

    return 0;
}
