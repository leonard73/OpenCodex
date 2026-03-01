#include "thread_pool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct thread_task {
  thread_task_fn fn;
  void *arg;
  struct thread_task *next;
} thread_task_t;

struct thread_pool {
  pthread_t *threads;
  size_t thread_count;
  thread_task_t *head;
  thread_task_t *tail;
  pthread_mutex_t lock;
  pthread_cond_t has_work;
  int stop;
};

static void *worker_loop(void *arg) {
  thread_pool_t *pool = (thread_pool_t *)arg;

  while (1) {
    thread_task_t *task = NULL;

    pthread_mutex_lock(&pool->lock);
    while (!pool->stop && pool->head == NULL) {
      pthread_cond_wait(&pool->has_work, &pool->lock);
    }

    if (pool->stop && pool->head == NULL) {
      pthread_mutex_unlock(&pool->lock);
      break;
    }

    task = pool->head;
    pool->head = task->next;
    if (pool->head == NULL) {
      pool->tail = NULL;
    }
    pthread_mutex_unlock(&pool->lock);

    task->fn(task->arg);
    free(task);
  }

  return NULL;
}

thread_pool_t *thread_pool_create(size_t thread_count) {
  thread_pool_t *pool;
  size_t i;

  if (thread_count == 0) {
    fprintf(stderr, "thread_pool_create: thread_count must be > 0\n");
    return NULL;
  }

  pool = calloc(1, sizeof(*pool));
  if (pool == NULL) {
    return NULL;
  }

  pool->threads = calloc(thread_count, sizeof(*pool->threads));
  if (pool->threads == NULL) {
    free(pool);
    return NULL;
  }

  pool->thread_count = thread_count;
  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->has_work, NULL);

  for (i = 0; i < thread_count; ++i) {
    if (pthread_create(&pool->threads[i], NULL, worker_loop, pool) != 0) {
      pool->stop = 1;
      pthread_cond_broadcast(&pool->has_work);
      while (i > 0) {
        --i;
        pthread_join(pool->threads[i], NULL);
      }
      pthread_cond_destroy(&pool->has_work);
      pthread_mutex_destroy(&pool->lock);
      free(pool->threads);
      free(pool);
      return NULL;
    }
  }

  return pool;
}

int thread_pool_enqueue(thread_pool_t *pool, thread_task_fn fn, void *arg) {
  thread_task_t *task;

  if (pool == NULL || fn == NULL) {
    return -1;
  }

  task = calloc(1, sizeof(*task));
  if (task == NULL) {
    return -1;
  }

  task->fn = fn;
  task->arg = arg;

  pthread_mutex_lock(&pool->lock);
  if (pool->tail == NULL) {
    pool->head = task;
    pool->tail = task;
  } else {
    pool->tail->next = task;
    pool->tail = task;
  }
  pthread_cond_signal(&pool->has_work);
  pthread_mutex_unlock(&pool->lock);

  return 0;
}

void thread_pool_destroy(thread_pool_t *pool) {
  size_t i;
  thread_task_t *task;

  if (pool == NULL) {
    return;
  }

  pthread_mutex_lock(&pool->lock);
  pool->stop = 1;
  pthread_cond_broadcast(&pool->has_work);
  pthread_mutex_unlock(&pool->lock);

  for (i = 0; i < pool->thread_count; ++i) {
    pthread_join(pool->threads[i], NULL);
  }

  task = pool->head;
  while (task != NULL) {
    thread_task_t *next = task->next;
    free(task);
    task = next;
  }

  pthread_cond_destroy(&pool->has_work);
  pthread_mutex_destroy(&pool->lock);
  free(pool->threads);
  free(pool);
}
