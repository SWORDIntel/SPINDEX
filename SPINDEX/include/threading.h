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
 * @brief Creates a thread pool with a specified number of worker threads.
 *
 * @param num_threads The number of worker threads to create in the pool.
 * @return ThreadPool* A pointer to the created thread pool, or NULL if an error occurred.
 */
ThreadPool* thread_pool_create(size_t num_threads);

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
void thread_pool_destroy(ThreadPool* pool);

#endif // SPINDEX_INCLUDE_THREADING_H
