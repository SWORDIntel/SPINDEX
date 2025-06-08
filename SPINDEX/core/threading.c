#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h> // Required for bool type
#include <errno.h>   // For error checking pthread functions

#include "../include/threading.h" // Path relative to core directory
// #include "../include/utils.h" // For LOG_ERROR, LOG_DEBUG if available and desired

// Define a default capacity for the task queue if not dynamically sized
#define DEFAULT_QUEUE_CAPACITY 256

// Internal definition of the ThreadPool structure
struct ThreadPool {
    pthread_t* threads;
    size_t num_threads;

    ThreadPoolTask* task_queue;
    size_t queue_capacity;
    size_t queue_size;
    size_t queue_front; // Index of the next task to execute
    size_t queue_rear;  // Index where the next task will be added

    pthread_mutex_t lock;
    pthread_cond_t notify_task;  // Condition variable for worker threads to wait for tasks
    pthread_cond_t notify_empty; // Condition variable to notify when queue is empty AND no active tasks during shutdown

    bool shutdown;
    volatile int active_tasks; // Number of tasks currently being processed or in queue. Volatile for visibility.
};

// Static forward declaration for the worker thread function
static void* worker_thread_function(void* arg);

ThreadPool* thread_pool_create(size_t num_threads) {
    if (num_threads == 0) {
        // LOG_ERROR("Thread pool creation failed: num_threads cannot be 0.");
        fprintf(stderr, "Thread pool creation failed: num_threads cannot be 0.\n");
        return NULL;
    }

    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("Failed to allocate memory for ThreadPool");
        return NULL;
    }

    pool->num_threads = num_threads;
    pool->queue_capacity = DEFAULT_QUEUE_CAPACITY; // Or make this configurable
    pool->task_queue = (ThreadPoolTask*)malloc(sizeof(ThreadPoolTask) * pool->queue_capacity);
    if (!pool->task_queue) {
        perror("Failed to allocate memory for task queue");
        free(pool);
        return NULL;
    }

    pool->queue_size = 0;
    pool->queue_front = 0;
    pool->queue_rear = 0;
    pool->shutdown = false;
    pool->active_tasks = 0;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(pool->task_queue);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->notify_task, NULL) != 0) {
        perror("Failed to initialize notify_task condition variable");
        pthread_mutex_destroy(&pool->lock);
        free(pool->task_queue);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->notify_empty, NULL) != 0) {
        perror("Failed to initialize notify_empty condition variable");
        pthread_cond_destroy(&pool->notify_task);
        pthread_mutex_destroy(&pool->lock);
        free(pool->task_queue);
        free(pool);
        return NULL;
    }

    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        perror("Failed to allocate memory for threads array");
        pthread_cond_destroy(&pool->notify_empty);
        pthread_cond_destroy(&pool->notify_task);
        pthread_mutex_destroy(&pool->lock);
        free(pool->task_queue);
        free(pool);
        return NULL;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread_function, pool) != 0) {
            perror("Failed to create worker thread");
            // Cleanup previously created threads and other resources
            pool->shutdown = true; // Signal other threads to exit
            pthread_cond_broadcast(&pool->notify_task); // Wake them up

            for (size_t j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pthread_cond_destroy(&pool->notify_empty);
            pthread_cond_destroy(&pool->notify_task);
            pthread_mutex_destroy(&pool->lock);
            free(pool->task_queue);
            free(pool);
            return NULL;
        }
    }
    // LOG_INFO("Thread pool created successfully with %zu threads.", num_threads);
    return pool;
}

bool thread_pool_submit_task(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) {
        // LOG_ERROR("Submit task failed: pool or function is NULL.");
        return false;
    }

    pthread_mutex_lock(&pool->lock);

    if (pool->shutdown) {
        // LOG_WARN("Submit task failed: pool is shutting down.");
        pthread_mutex_unlock(&pool->lock);
        return false;
    }

    if (pool->queue_size >= pool->queue_capacity) {
        // LOG_WARN("Submit task failed: task queue is full.");
        // Optionally, implement dynamic resizing or a blocking submit here
        pthread_mutex_unlock(&pool->lock);
        return false; // Queue is full
    }

    pool->task_queue[pool->queue_rear].function = function;
    pool->task_queue[pool->queue_rear].argument = arg;
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_capacity;
    pool->queue_size++;
    pool->active_tasks++; // Increment active tasks when a task is added

    pthread_cond_signal(&pool->notify_task); // Notify one waiting worker thread
    pthread_mutex_unlock(&pool->lock);

    return true;
}

void thread_pool_destroy(ThreadPool* pool) {
    if (!pool) return;

    // LOG_INFO("Thread pool destroy initiated.");
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    // Wake up all worker threads so they can recognize the shutdown flag
    pthread_cond_broadcast(&pool->notify_task);

    // Wait until all tasks are processed (active_tasks becomes 0)
    while (pool->active_tasks > 0) {
        // LOG_DEBUG("Destroy: Waiting for %d active tasks to complete.", pool->active_tasks);
        pthread_cond_wait(&pool->notify_empty, &pool->lock);
    }
    // LOG_INFO("All tasks completed. Proceeding with thread join.");
    pthread_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < pool->num_threads; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            perror("Failed to join thread");
            // Continue cleanup as much as possible
        }
    }
    // LOG_INFO("All worker threads joined.");

    // Destroy mutex and condition variables
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify_task);
    pthread_cond_destroy(&pool->notify_empty);

    // Free allocated memory
    free(pool->threads);
    free(pool->task_queue);
    free(pool);
    // LOG_INFO("Thread pool destroyed successfully.");
}

static void* worker_thread_function(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    ThreadPoolTask task;

    while (true) {
        pthread_mutex_lock(&pool->lock);

        // Wait for a task or shutdown signal
        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->notify_task, &pool->lock);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            // Pool is shutting down and no tasks left to process
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        if (pool->queue_size == 0 && pool->shutdown) {
             // This case should ideally be covered by the above,
             // but as a safeguard if active_tasks logic needs adjustment.
             pthread_mutex_unlock(&pool->lock);
             pthread_exit(NULL);
        }


        // Dequeue task
        task.function = pool->task_queue[pool->queue_front].function;
        task.argument = pool->task_queue[pool->queue_front].argument;
        pool->queue_front = (pool->queue_front + 1) % pool->queue_capacity;
        pool->queue_size--;

        // Note: active_tasks is decremented *after* task execution.
        // The task is now considered "dequeued" but still "active" until completion.
        pthread_mutex_unlock(&pool->lock);

        // Execute the task
        // LOG_DEBUG("Worker thread %p executing task.", pthread_self());
        task.function(task.argument);
        // LOG_DEBUG("Worker thread %p finished task.", pthread_self());

        // Task finished, decrement active_tasks count
        pthread_mutex_lock(&pool->lock);
        pool->active_tasks--;
        if (pool->shutdown && pool->active_tasks == 0 && pool->queue_size == 0) {
            // If shutting down and this was the last active task,
            // and queue is also confirmed empty under lock.
            pthread_cond_signal(&pool->notify_empty);
        }
        pthread_mutex_unlock(&pool->lock);
    }
    return NULL; // Should not be reached
}
