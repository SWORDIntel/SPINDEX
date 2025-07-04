#ifndef SPINDEX_INCLUDE_THREADING_H
#define SPINDEX_INCLUDE_THREADING_H

#include <pthread.h>
#include <stddef.h>
#include <stdbool.h>

// Structure for a task that will be executed by a thread in the pool
typedef struct {
    void (*function)(void *arg); // Pointer to the function to execute
    void *argument;              // Argument to the function
} ThreadPoolTask;

// Opaque structure for the thread pool. Definition is in threading.c
typedef struct ThreadPool ThreadPool;

/**
 * @brief Creates a thread pool with a specified number of worker threads and task queue size.
 *
 * @param num_threads The number of worker threads to create in the pool. Must be > 0.
 * @param max_queue_size The maximum number of tasks that can be pending in the queue.
 *                       If 0, the queue is unbounded (within memory limits).
 * @return ThreadPool* A pointer to the created thread pool, or NULL if an error occurred.
 */
#ifdef __cplusplus
extern "C" {
#endif

ThreadPool* thread_pool_create(size_t num_threads, size_t max_queue_size);

/**
 * @brief Submits a task to the thread pool for execution.
 *
 * The task will be added to a queue and picked up by an available worker thread.
 *
 * @param pool The thread pool to submit the task to.
 * @param function The function to be executed.
 * @param arg The argument to be passed to the function.
 * @return bool true if the task was successfully submitted, false otherwise
 *              (e.g., if the pool is shutting down or the task queue is full).
 */
bool thread_pool_submit_task(ThreadPool* pool, void (*function)(void*), void* arg);

/**
 * @brief Shuts down the thread pool and cleans up its resources.
 *
 * This function will wait for all currently queued and executing tasks to complete
 * before destroying the pool. No new tasks can be submitted after this function is called.
 *
 * @param pool The thread pool to destroy.
 */
int thread_pool_destroy(ThreadPool* pool);

#ifdef __cplusplus
}
#endif

#endif // SPINDEX_INCLUDE_THREADING_H
