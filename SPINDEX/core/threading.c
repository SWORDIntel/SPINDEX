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
int task_queue_init(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_init: Queue pointer is NULL.");
        return -1;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize mutex.");
        return -1;
    }
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize condition variable.");
        pthread_mutex_destroy(&queue->lock); // Clean up successfully initialized mutex.
        return -1;
    }
    LOG_DEBUG("Task queue initialized.");
    return 0;
}

void task_queue_push(TaskQueue* queue, Task* task) {
    if (!queue || !task) {
        LOG_ERROR("task_queue_push: Queue or Task pointer is NULL.");
        if(task) free(task); // Prevent memory leak if task was allocated and queue is NULL.
        return;
    }

    task->next = NULL; // Ensure the new task's next pointer is NULL

    pthread_mutex_lock(&queue->lock);
    if (queue->tail == NULL) { // Queue is empty
        queue->head = task;
        queue->tail = task;
    } else {
        queue->tail->next = task;
        queue->tail = task;
    }
    queue->count++;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    LOG_DEBUG("Task pushed to queue.");
}

// This function is a general utility for blocking pop.
// Worker threads use actual_pop helper internally with their own condition variable logic.
Task* task_queue_pop(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_pop: Queue pointer is NULL.");
        return NULL;
    }

    pthread_mutex_lock(&queue->lock);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
        // Upon waking, re-check condition due to potential spurious wakeups.
        // If a shutdown mechanism were part of this generic pop, it would be checked here.
    }

    Task* task = NULL;
    // This state check is a safeguard; count == 0 should imply head is NULL.
    if (queue->count > 0 && queue->head == NULL) {
        LOG_ERROR("task_queue_pop: Inconsistent queue state (count > 0 but head is NULL).");
    } else if (queue->head != NULL) {
        task = queue->head;
        queue->head = queue->head->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        queue->count--;
    }
    pthread_mutex_unlock(&queue->lock);
    // LOG_DEBUG("Task popped from queue via public pop."); // Optional: if verbose logging for this specific function is needed
    return task;
}

// Task queue destroy function - free any remaining tasks
void task_queue_destroy(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_destroy: Queue pointer is NULL.");
        return;
    }

    // No lock is taken here to iterate and free tasks, as this function
    // is expected to be called only after all worker threads have been joined
    // and no other operations are pending on the queue.
    Task* current = queue->head;
    Task* next;
    while (current != NULL) {
        next = current->next;
        LOG_DEBUG("Freeing task during queue destruction.");
        // This frees the Task struct itself. `task->arg` is not freed here;
        // its lifecycle management is the responsibility of the submitter or the task function.
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    if (pthread_mutex_destroy(&queue->lock) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy task queue mutex. It might be locked or still in use.");
    }
    if (pthread_cond_destroy(&queue->not_empty) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy task queue condition variable. A thread might still be waiting.");
    }
    LOG_DEBUG("Task queue destroyed successfully.");
}


// Thread pool function implementations
int thread_pool_init(ThreadPool* pool, size_t num_threads) {
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

    if (task_queue_init(&pool->task_queue) != 0) {
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
        // If this function fails, the caller must free 'arg' if it was dynamically allocated.
        return -1;
    }

    pthread_mutex_lock(&pool->task_queue.lock);
     if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_ERROR("thread_pool_submit: Pool is shutting down. Cannot submit new tasks.");
        return -1;
    }

    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_ERROR("thread_pool_submit: Failed to allocate memory for new task.");
        return -1;
    }

    new_task->function = function;
    new_task->arg = arg;
    new_task->next = NULL;

    if (pool->task_queue.tail == NULL) { // Queue is empty
        pool->task_queue.head = new_task;
        pool->task_queue.tail = new_task;
    } else {
        pool->task_queue.tail->next = new_task;
        pool->task_queue.tail = new_task;
    }
    pool->task_queue.count++;
    pthread_cond_signal(&pool->task_queue.not_empty);
    pthread_mutex_unlock(&pool->task_queue.lock);

    LOG_DEBUG("Task submitted to thread pool.");
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
    pthread_cond_broadcast(&pool->task_queue.not_empty);
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

        task = NULL;
        if (pool->task_queue.head != NULL) {
            task = actual_pop(&pool->task_queue);
        }

        pthread_mutex_unlock(&pool->task_queue.lock);

        if (task) {
            if (task->function) {
                snprintf(msg, sizeof(msg), "Worker thread %lu executing task.", (unsigned long)pthread_self());
                LOG_DEBUG(msg);
                task->function(task->arg);
            } else {
                LOG_ERROR("Worker thread: Popped task has NULL function pointer.");
            }
            // Task struct is freed by the worker thread after execution.
            free(task);
            LOG_DEBUG("Task execution processed and task freed.");
        }
    }

    snprintf(msg, sizeof(msg), "Worker thread %lu shutting down.", (unsigned long)pthread_self());
    LOG_DEBUG(msg);
    pthread_exit(NULL);
}

// Helper function to actually pop the task, assumes lock is held and queue is not empty.
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
    return task;
}
