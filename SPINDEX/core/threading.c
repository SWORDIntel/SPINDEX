#include "threading.h" // Should include stdbool.h, pthread.h, stddef.h
#include "utils.h"     // For LOG_INFO, LOG_ERROR macros etc.
#include <stdlib.h>    // For malloc, free
#include <stdio.h>     // For snprintf if not in utils.h logging
#include <string.h>    // For memset (optional, calloc preferred)
#include <stdbool.h>   // For bool, true, false


// --- Internal Structures ---
// Note: Task, TaskQueue, and ThreadPool are defined in threading.h


// --- Forward Declarations for Static Functions ---
static void* worker_thread_main(void* arg);
static int _task_queue_init(TaskQueue* queue, size_t max_size);
static void _task_queue_destroy(TaskQueue* queue);
static int _task_queue_push(TaskQueue* queue, Task* task, ThreadPool* pool); // Pass pool for shutdown check
static Task* _task_queue_pop_internal(TaskQueue* queue); // Internal pop, assumes lock is held

// --- Task Queue Internal Functions ---
static int _task_queue_init(TaskQueue* queue, size_t max_size) {
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

// _task_queue_push assumes queue->lock is held by the caller.
// Returns 0 on success, -1 if pool is shutting down (caller should free task).
static int _task_queue_push(TaskQueue* queue, Task* task, ThreadPool* pool) {
    if (!queue || !task || !pool) {
        LOG_ERROR("_task_queue_push: Queue, Task, or Pool pointer is NULL.");
        return -1;
    }

    while (queue->max_size > 0 && queue->count >= queue->max_size) {
        if (pool->shutdown) {
            LOG_WARN("_task_queue_push: Pool is shutting down while queue is full. Task not added.");
            return -1;
        }
        LOG_DEBUG("Task queue is full (count: %d, max: %zu). Waiting for space...", queue->count, queue->max_size);
        pthread_cond_wait(&queue->not_full, &queue->lock);
        if (pool->shutdown) {
            LOG_WARN("_task_queue_push: Pool is shutting down after wait. Task not added.");
            return -1;
        }
    }

    task->next = NULL;
    if (queue->tail == NULL) {
        queue->head = task;
        queue->tail = task;
    } else {
        queue->tail->next = task;
        queue->tail = task;
    }
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    LOG_DEBUG("Task pushed to queue. Count: %d.", queue->count);
    return 0;
}

// _task_queue_pop_internal assumes lock is held and queue is not empty (or pool is shutting down).
// Signals not_full if appropriate.
static Task* _task_queue_pop_internal(TaskQueue* queue) {
    if (queue == NULL || queue->head == NULL) {
        LOG_ERROR("_task_queue_pop_internal: Queue is NULL or head is NULL, though it shouldn't be if called correctly.");
        return NULL;
    }
    Task* task = queue->head;
    queue->head = task->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->count--;

    if (queue->max_size > 0 && queue->count == (int)(queue->max_size - 1)) {
        LOG_DEBUG("_task_queue_pop_internal: Queue was full, now has space. Signaling not_full. Count: %d", queue->count);
        pthread_cond_signal(&queue->not_full);
    }
    return task;
}

static void _task_queue_destroy(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("_task_queue_destroy: Queue pointer is NULL.");
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
        LOG_ERROR("_task_queue_destroy: Failed to destroy task queue mutex.");
    }
    if (pthread_cond_destroy(&queue->not_empty) != 0) {
        LOG_ERROR("_task_queue_destroy: Failed to destroy not_empty task queue condition variable.");
    }
    if (pthread_cond_destroy(&queue->not_full) != 0) {
        LOG_ERROR("_task_queue_destroy: Failed to destroy not_full task queue condition variable.");
    }
    LOG_DEBUG("Task queue destroyed successfully.");
}


// --- Thread Pool Public API Implementations ---

// Internal initialization function
static int _thread_pool_initialize_internal(ThreadPool* pool, size_t num_threads, size_t max_queue_size) {
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


ThreadPool* thread_pool_create(size_t num_threads, size_t max_queue_size) {
    if (num_threads == 0) {
        LOG_ERROR("thread_pool_create: Number of threads cannot be zero.");
        return NULL;
    }
    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool)); // Use calloc for zero-initialization
    if (!pool) {
        LOG_ERROR("thread_pool_create: Failed to allocate memory for ThreadPool struct.");
        return NULL;
    }

    if (_thread_pool_initialize_internal(pool, num_threads, max_queue_size) != 0) {
        free(pool);
        return NULL;
    }
    return pool;
}

bool thread_pool_submit_task(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) {
        LOG_ERROR("thread_pool_submit_task: Pool or function pointer is NULL.");
        return false;
    }

    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) {
        LOG_ERROR("thread_pool_submit_task: Failed to allocate memory for new task.");
        return false;
    }

    new_task->function = function;
    new_task->arg = arg;
    new_task->next = NULL;

    pthread_mutex_lock(&pool->task_queue.lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_WARN("thread_pool_submit_task: Pool is shutting down. Cannot submit new tasks.");
        free(new_task);
        return false;
    }

    if (_task_queue_push(&pool->task_queue, new_task, pool) != 0) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_WARN("thread_pool_submit_task: Failed to push task, likely due to shutdown during wait.");
        free(new_task);
        return false;
    }

    pthread_mutex_unlock(&pool->task_queue.lock);
    LOG_DEBUG("Task successfully submitted to thread pool.");
    return true;
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

        // Use the internal pop function that assumes lock is held
        task = _task_queue_pop_internal(&pool->task_queue);
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