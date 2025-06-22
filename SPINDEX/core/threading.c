#include "threading.h"
#include "utils.h" // For LOG_INFO, LOG_ERROR macros
#include <stdio.h> // For NULL (used by some LOG_X macros indirectly if not careful)
#include <stdlib.h> // For malloc, free
#include <pthread.h>

// Forward declaration for the worker function
static void* worker_thread_main(void* arg);
// Forward declaration for internal helper
static Task* actual_pop(TaskQueue* q);

// Task Queue utility function implementations
int task_queue_init(TaskQueue* queue, size_t max_size) {
    if (!queue) {
        LOG_ERROR("task_queue_init: Queue pointer is NULL.");
        return -1;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_size = max_size; // 0 means unbounded

    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize mutex.");
        return -1;
    }
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize not_empty condition variable.");
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize not_full condition variable.");
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    LOG_DEBUG("Task queue initialized with max_size: %zu.", max_size);
    return 0;
}

// task_queue_push assumes queue->lock is held by the caller.
// Returns 0 on success, -1 if pool is shutting down (caller should free task).
int task_queue_push(TaskQueue* queue, Task* task, ThreadPool* pool) {
    if (!queue || !task || !pool) {
        LOG_ERROR("task_queue_push: Queue, Task, or Pool pointer is NULL.");
        // Task should be freed by caller if this pathway is hit due to bad queue/pool ptr
        return -1;
    }

    // Wait if the queue is full and max_size is set (greater than 0)
    // Must also check for shutdown condition while waiting.
    while (queue->max_size > 0 && queue->count >= queue->max_size) {
        if (pool->shutdown) {
            LOG_WARN("task_queue_push: Pool is shutting down while queue is full. Task not added.");
            return -1; // Indicate failure due to shutdown
        }
        LOG_DEBUG("Task queue is full (count: %d, max: %zu). Waiting for space...", queue->count, queue->max_size);
        pthread_cond_wait(&queue->not_full, &queue->lock);
        // After waking up, re-check shutdown status before proceeding
        if (pool->shutdown) {
            LOG_WARN("task_queue_push: Pool is shutting down after wait. Task not added.");
            return -1; // Indicate failure due to shutdown
        }
    }

    task->next = NULL; // Ensure the new task's next pointer is NULL
    if (queue->tail == NULL) { // Queue is empty
        queue->head = task;
        queue->tail = task;
    } else {
        queue->tail->next = task;
        queue->tail = task;
    }
    queue->count++;
    pthread_cond_signal(&queue->not_empty); // Signal that the queue is no longer empty
    LOG_DEBUG("Task pushed to queue. Count: %d.", queue->count);
    return 0; // Success
}

// This function is a general utility for blocking pop.
// Worker threads use actual_pop helper internally.
Task* task_queue_pop(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_pop: Queue pointer is NULL.");
        return NULL;
    }

    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0 /* && !pool->shutdown - This generic pop doesn't know about pool->shutdown */) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    Task* task = NULL;
    if (queue->head != NULL) {
        task = actual_pop(q); // Use helper to ensure consistent signaling
    } else {
         LOG_ERROR("task_queue_pop: Inconsistent queue state (count > 0 but head is NULL after wait).");
    }
    pthread_mutex_unlock(&queue->lock);
    if (task) LOG_DEBUG("Task popped via public pop. Count: %d.", queue->count);
    return task;
}

void task_queue_destroy(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_destroy: Queue pointer is NULL.");
        return;
    }

    Task* current = queue->head;
    Task* next;
    while (current != NULL) {
        next = current->next;
        LOG_DEBUG("Freeing task during queue destruction.");
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    if (pthread_mutex_destroy(&queue->lock) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy task queue mutex.");
    }
    if (pthread_cond_destroy(&queue->not_empty) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy not_empty task queue condition variable. A thread might still be waiting.");
    }
    if (pthread_cond_destroy(&queue->not_full) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy not_full task queue condition variable.");
    }
    LOG_DEBUG("Task queue destroyed successfully.");
}


// Thread pool function implementations
int thread_pool_init(ThreadPool* pool, size_t num_threads, size_t max_queue_size) {
    if (!pool) {
        LOG_ERROR("thread_pool_init: Pool pointer is NULL.");
        return -1;
    }
    if (num_threads == 0) {
        LOG_ERROR("thread_pool_init: Number of threads cannot be zero.");
        return -1;
    }

    LOG_INFO("Initializing thread pool...");
    pool->num_threads = num_threads;
    pool->shutdown = 0;

    if (task_queue_init(&pool->task_queue, max_queue_size) != 0) { // Pass max_queue_size
        LOG_ERROR("thread_pool_init: Failed to initialize task queue.");
        return -1;
    }

    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        LOG_ERROR("thread_pool_init: Failed to allocate memory for threads.");
        task_queue_destroy(&pool->task_queue);
        return -1;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread_main, pool) != 0) {
            LOG_ERROR("thread_pool_init: Failed to create worker thread.");
            // Cleanup for partially created pool
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->task_queue.not_empty);
            for (size_t j = 0; j < i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            task_queue_destroy(&pool->task_queue);
            return -1;
        }
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "Thread pool initialized with %zu threads.", num_threads);
    LOG_INFO(msg);
    return 0;
}

int thread_pool_submit(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) {
        LOG_ERROR("thread_pool_submit: Pool or function pointer is NULL.");
        // The lifecycle of 'arg' is the caller's responsibility.
        return -1;
    }

    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) {
        LOG_ERROR("thread_pool_submit: Failed to allocate memory for new task.");
        return -1; // Caller handles arg
    }

    new_task->function = function;
    new_task->arg = arg;
    new_task->next = NULL;

    pthread_mutex_lock(&pool->task_queue.lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_WARN("thread_pool_submit: Pool is shutting down. Cannot submit new tasks.");
        free(new_task); // Free the allocated task as it won't be queued
        return -1;
    }

    // task_queue_push assumes the lock is held and will handle waiting if queue is full.
    // It returns 0 on success, -1 if shutdown occurred during wait.
    if (task_queue_push(&pool->task_queue, new_task, pool) != 0) {
        // Shutdown occurred while task_queue_push was waiting, or other push error.
        pthread_mutex_unlock(&pool->task_queue.lock); // Ensure lock is released
        LOG_WARN("thread_pool_submit: Failed to push task, likely due to shutdown during wait.");
        free(new_task); // Free the task as it was not queued
        return -1;
    }

    pthread_mutex_unlock(&pool->task_queue.lock);
    LOG_DEBUG("Task successfully submitted to thread pool.");
    return 0;
}

int thread_pool_destroy(ThreadPool* pool) {
    if (!pool) {
        LOG_ERROR("thread_pool_destroy: Pool pointer is NULL.");
        return -1;
    }

    LOG_INFO("Destroying thread pool...");

    pthread_mutex_lock(&pool->task_queue.lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_WARN("Thread pool destroy called multiple times or already in shutdown.");
        // Attempt to join threads if not NULL, assuming previous destroy might not have completed.
        if (pool->threads != NULL) {
            LOG_INFO("Attempting to join threads from a repeated destroy call...");
            for (size_t i = 0; i < pool->num_threads; ++i) {
                pthread_join(pool->threads[i], NULL);
            }
            free(pool->threads);
            pool->threads = NULL;
        }
        return 0;
    }
    pool->shutdown = 1;
    // Broadcast to both conditions to wake up all threads (workers and producers).
    pthread_cond_broadcast(&pool->task_queue.not_empty);
    pthread_cond_broadcast(&pool->task_queue.not_full);
    pthread_mutex_unlock(&pool->task_queue.lock);

    char msg[100];
    for (size_t i = 0; i < pool->num_threads; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            snprintf(msg, sizeof(msg), "thread_pool_destroy: Failed to join thread %zu.", i);
            LOG_ERROR(msg);
        } else {
            snprintf(msg, sizeof(msg), "Successfully joined thread %zu.", i);
            LOG_DEBUG(msg);
        }
    }

    if (pool->threads) {
        free(pool->threads);
        pool->threads = NULL;
    }

    task_queue_destroy(&pool->task_queue);

    LOG_INFO("Thread pool destroyed.");
    return 0;
}

// Worker thread main function
static void* worker_thread_main(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Task* task;
    char msg[128];
    snprintf(msg, sizeof(msg), "Worker thread %lu started.", (unsigned long)pthread_self());
    LOG_DEBUG(msg);

    while (1) {
        pthread_mutex_lock(&pool->task_queue.lock);
        while (pool->task_queue.count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->task_queue.not_empty, &pool->task_queue.lock);
        }

        if (pool->shutdown && pool->task_queue.count == 0) {
            pthread_mutex_unlock(&pool->task_queue.lock);
            break;
        }

        task = actual_pop(&pool->task_queue);
        pthread_mutex_unlock(&pool->task_queue.lock);

        if (task) {
            if (task->function) {
                snprintf(msg, sizeof(msg), "Worker thread %lu executing task.", (unsigned long)pthread_self());
                LOG_DEBUG(msg);
                task->function(task->arg);
            } else {
                LOG_ERROR("Worker thread: Popped task has NULL function pointer.");
            }
            free(task); // Task struct is freed by the worker thread after execution.
            LOG_DEBUG("Task execution processed and task freed by worker %lu.", (unsigned long)pthread_self());
        }
    }

    snprintf(msg, sizeof(msg), "Worker thread %lu shutting down.", (unsigned long)pthread_self());
    LOG_DEBUG(msg);
    pthread_exit(NULL);
}

// Helper function to actually pop the task, assumes lock is held and queue is not empty.
// Signals not_full if appropriate.
static Task* actual_pop(TaskQueue* q) {
    if (q == NULL || q->head == NULL) {
        LOG_ERROR("actual_pop: Queue is NULL or head is NULL, though it shouldn't be.");
        return NULL;
    }
    Task* task = q->head;
    q->head = task->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->count--;

    // If the queue was full and now has space, signal any waiting producers.
    // This check is (q->count == q->max_size - 1) because we just decremented count.
    if (q->max_size > 0 && q->count == (int)(q->max_size - 1)) {
        LOG_DEBUG("actual_pop: Queue was full, now has space. Signaling not_full. Count: %d", q->count);
        pthread_cond_signal(&q->not_full);
    }
    return task;
}