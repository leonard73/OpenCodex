#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>

typedef struct thread_pool thread_pool_t;
typedef void (*thread_task_fn)(void *arg);

thread_pool_t *thread_pool_create(size_t thread_count);
int thread_pool_enqueue(thread_pool_t *pool, thread_task_fn fn, void *arg);
void thread_pool_destroy(thread_pool_t *pool);

#endif
