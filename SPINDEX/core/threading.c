#include "threading.h"
#include "utils.h" // For LOG_INFO, LOG_ERROR macros
#include <stdio.h> // For NULL (used by some LOG_X macros indirectly if not careful)
#include <stdlib.h> // For malloc, free
#include <pthread.h>

// Forward declaration for the worker function
static void* worker_thread_main(void* arg);

// Task Queue utility function implementations
// REVIEW: Correctness of pthread synchronization: Mutex `lock` protects queue data (head, tail, count).
// REVIEW: `not_empty` condition variable is used to signal waiting threads when tasks are added.
int task_queue_init(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_init: Queue pointer is NULL.");
        // REVIEW: No return value to indicate failure here. Consider changing signature to `int task_queue_init(...)`.
        return -1;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    // REVIEW: Error handling for pthread_mutex_init: It can fail (e.g., ENOMEM).
    // The function currently logs but doesn't propagate the error. This could lead to crashes later.
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize mutex.");
        // Consider how to handle this error, maybe return status or exit
        return -1;
    }
    // REVIEW: Error handling for pthread_cond_init: Similar to mutex, can fail.
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        LOG_ERROR("task_queue_init: Failed to initialize condition variable.");
        pthread_mutex_destroy(&queue->lock); // Clean up successfully initialized mutex.
        // Consider how to handle this error
        return -1;
    }
    LOG_INFO("Task queue initialized.");
    return 0;
}

// REVIEW: `task_queue_push` correctly locks the mutex before modifying shared queue data and signals.
void task_queue_push(TaskQueue* queue, Task* task) {
    if (!queue || !task) {
        LOG_ERROR("task_queue_push: Queue or Task pointer is NULL.");
        if(task) free(task); // Prevent memory leak if task was allocated and queue is NULL.
        // REVIEW: Consider if task should be freed if queue is valid but task is NULL (though current check prevents this).
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
    // REVIEW: `pthread_cond_signal` is correct to wake up one thread if any are waiting.
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    LOG_INFO("Task pushed to queue."); // REVIEW: Logging inside queue push can be verbose. Consider level or context.
}

// REVIEW: `task_queue_pop` uses `pthread_cond_wait` in a loop, which is standard practice to handle spurious wakeups.
// REVIEW: However, the pop logic itself is directly inside `worker_thread_main` in the latest version of the code.
// This `task_queue_pop` function seems to be an older or alternative version.
// For the purpose of this review, I'll assume this `task_queue_pop` is the one intended for use by `worker_thread_main`
// if it were to be refactored into this function.
Task* task_queue_pop(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_pop: Queue pointer is NULL.");
        return NULL;
    }

    pthread_mutex_lock(&queue->lock);
    // The shutdown check for the thread pool needs to happen in the worker_thread_main,
    // not in the generic task_queue_pop. This is a good separation of concerns.
    // This loop just waits for items.
    // REVIEW: The condition `queue->count == 0 && queue->head == NULL` is redundant. `queue->count == 0` implies `head == NULL`.
    while (queue->count == 0) {
        // REVIEW: Correct usage of pthread_cond_wait. It atomically releases and re-acquires the lock.
        pthread_cond_wait(&queue->not_empty, &queue->lock);
        // REVIEW: Upon waking, thread should re-check condition due to potential spurious wakeups or if shutdown is signaled.
        // The worker_thread_main function handles the shutdown check after this pop.
        // A shutdown check could also be added here if task_queue_pop needs to be aware of it,
        // for example, by accepting the ThreadPool pointer or a shutdown flag.
        // For now, worker_thread_main handles this.
    }

    Task* task = NULL;
    if (queue->count > 0 && queue->head == NULL) {
        // This state (count > 0 but head is NULL) should not occur if queue logic is correct.
        LOG_ERROR("task_queue_pop: Inconsistent queue state (count > 0 but head is NULL).");
        // task remains NULL
    } else if (queue->head != NULL) { // Normal case: queue has items and head is valid
        task = queue->head;
        queue->head = queue->head->next;
        if (queue->head == NULL) { // Queue became empty
            queue->tail = NULL;
        }
        queue->count--;
    }
    // If queue->count == 0 (implying queue->head is also NULL), it means we were likely signaled
    // for shutdown or a spurious wakeup happened and the condition is still false.
    // In this case, task remains NULL, and the caller (worker_thread_main) will handle it.
    pthread_mutex_unlock(&queue->lock);
    if(task) {
        LOG_INFO("Task popped from queue."); // REVIEW: Logging can be verbose.
    }
    return task;
}

// Task queue destroy function - free any remaining tasks
// REVIEW: Resource management: `task_queue_destroy` frees Task structs remaining in the queue.
// This is important for cleanup, especially if the pool is destroyed prematurely or if tasks were added but not processed.
void task_queue_destroy(TaskQueue* queue) {
    if (!queue) {
        LOG_ERROR("task_queue_destroy: Queue pointer is NULL.");
        return;
    }

    // REVIEW: No lock is taken here. If other threads could still be accessing the queue (e.g. faulty shutdown), this is unsafe.
    // However, this is typically called after threads are joined, so it should be safe.
    Task* current = queue->head;
    Task* next; // Renamed to avoid conflict with Task member
    while (current != NULL) {
        next = current->next;
        LOG_INFO("Freeing task during queue destruction."); // REVIEW: Logging here is informative for debugging leaks.
        // REVIEW: This frees the Task struct itself. `task->arg` is not freed here, which is typical,
        // as its lifecycle management is usually the responsibility of the submitter or the task function.
        free(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    // REVIEW: Error handling for pthread_mutex_destroy/pthread_cond_destroy: Important to check return values.
    // Failures here might indicate misuse (e.g., destroying a locked mutex).
    if (pthread_mutex_destroy(&queue->lock) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy task queue mutex. It might be locked or still in use.");
    }
    if (pthread_cond_destroy(&queue->not_empty) != 0) {
        LOG_ERROR("task_queue_destroy: Failed to destroy task queue condition variable. A thread might still be waiting.");
    }
    LOG_INFO("Task queue destroyed.");
}


// Thread pool function implementations
// REVIEW: `thread_pool_init` sets up the thread pool structure, task queue, and creates worker threads.
int thread_pool_init(ThreadPool* pool, size_t num_threads) {
    if (!pool) {
        LOG_ERROR("thread_pool_init: Pool pointer is NULL.");
        return -1; // REVIEW: Clear error indication.
    }
    if (num_threads == 0) {
        LOG_ERROR("thread_pool_init: Number of threads cannot be zero.");
        return -1; // REVIEW: Handles invalid argument.
    }

    LOG_INFO("Initializing thread pool...");
    pool->num_threads = num_threads;
    pool->shutdown = 0; // REVIEW: Shutdown flag correctly initialized.
    // pool->destroying_started = 0; // This field is not in the struct definition yet. Skipping for now.

    // REVIEW: task_queue_init error handling: if task_queue_init could fail and return an error, it should be checked here.
    // Current task_queue_init logs but doesn't propagate.
    if (task_queue_init(&pool->task_queue) != 0) {
        LOG_ERROR("thread_pool_init: Failed to initialize task queue.");
        return -1;
    }

    // REVIEW: Resource management: Allocates memory for pthread_t array.
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        LOG_ERROR("thread_pool_init: Failed to allocate memory for threads.");
        task_queue_destroy(&pool->task_queue); // REVIEW: Good practice to clean up partially initialized resources.
        return -1;
    }

    // REVIEW: Completeness of error handling: Loop to create threads.
    for (size_t i = 0; i < num_threads; ++i) {
        // REVIEW: Error handling for pthread_create: This is critical.
        if (pthread_create(&pool->threads[i], NULL, worker_thread_main, pool) != 0) {
            LOG_ERROR("thread_pool_init: Failed to create worker thread.");
            // REVIEW: Graceful shutdown sequence on partial failure:
            // 1. Set shutdown flag.
            // 2. Broadcast to wake any created threads (so they can see shutdown flag).
            // 3. Join already created threads.
            // 4. Free allocated memory.
            // This sequence is mostly correct.
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->task_queue.not_empty); // Wake up all threads.
            for (size_t j = 0; j < i; ++j) { // Join successfully created threads.
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads); // Free the threads array.
            task_queue_destroy(&pool->task_queue); // Destroy the task queue.
            return -1; // Indicate failure.
        }
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "Thread pool initialized with %zu threads.", num_threads);
    LOG_INFO(msg); // REVIEW: Using sprintf for logging is okay, but consider snprintf for safety.
    return 0; // REVIEW: Success.
}

// REVIEW: `thread_pool_submit` creates a task and adds it to the queue.
// REVIEW: Suitability for ingestion pipeline: This function is how file processing tasks would be submitted.
// `function` would be a wrapper that calls the specific file processing logic.
// `arg` would be a struct containing file path, processing options, etc.
int thread_pool_submit(ThreadPool* pool, void (*function)(void*), void* arg) {
    if (!pool || !function) {
        LOG_ERROR("thread_pool_submit: Pool or function pointer is NULL.");
        // REVIEW: If `arg` was dynamically allocated by the caller, this error path could leak `arg`.
        // The contract for `arg` lifecycle should be documented.
        return -1;
    }

    pthread_mutex_lock(&pool->task_queue.lock);
    // REVIEW: Race condition/Deadlock: Check for shutdown status.
    // This check is outside the queue lock. If pool shuts down right after this check but before task_queue_push locks,
    // a task might be added to a shutting-down pool.
    // Consider locking earlier or having task_queue_push check shutdown status (but that mixes concerns).
    // A common pattern is to lock, check shutdown, then push, then unlock.
    // Now checking shutdown status *after* acquiring the lock.
     if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_ERROR("thread_pool_submit: Pool is shutting down. Cannot submit new tasks.");
        // REVIEW: Again, potential `arg` leak if caller doesn't handle it on error.
        // Document that 'arg' must be freed by caller if submit fails.
        return -1;
    }

    // REVIEW: Resource management: Allocates memory for Task struct.
    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_ERROR("thread_pool_submit: Failed to allocate memory for new task.");
        // REVIEW: `arg` leak.
        return -1;
    }

    new_task->function = function;
    new_task->arg = arg;
    new_task->next = NULL; // task_queue_push also sets this, but good for clarity.

    // Performing the push operations directly since lock is already held.
    if (pool->task_queue.tail == NULL) { // Queue is empty
        pool->task_queue.head = new_task;
        pool->task_queue.tail = new_task;
    } else {
        pool->task_queue.tail->next = new_task;
        pool->task_queue.tail = new_task;
    }
    pool->task_queue.count++;
    pthread_cond_signal(&pool->task_queue.not_empty); // Signal that queue is not empty
    pthread_mutex_unlock(&pool->task_queue.lock);

    LOG_INFO("Task submitted to thread pool via direct push logic."); // Updated log message
    return 0; // REVIEW: Success.
}

// REVIEW: `thread_pool_destroy` implements the graceful shutdown sequence.
int thread_pool_destroy(ThreadPool* pool) {
    if (!pool) {
        LOG_ERROR("thread_pool_destroy: Pool pointer is NULL.");
        return -1;
    }

    LOG_INFO("Destroying thread pool...");

    // REVIEW: Correctly locks the queue mutex to safely modify `shutdown` and broadcast.
    pthread_mutex_lock(&pool->task_queue.lock);
    // REVIEW: Idempotency: Check if already shutting down.
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->task_queue.lock);
        LOG_WARN("Thread pool destroy called multiple times or already in shutdown.");
        // If threads are still there, means previous destroy might not have completed join.
        // Attempt to join them to be safe, but this indicates a potential issue if called concurrently.
        if (pool->threads != NULL) {
            LOG_INFO("Attempting to join threads from a repeated destroy call...");
            for (size_t i = 0; i < pool->num_threads; ++i) {
                pthread_join(pool->threads[i], NULL); // This might block if another destroy is also joining.
            }
            free(pool->threads);
            pool->threads = NULL;
            // task_queue_destroy should ideally be safe from double-destroy or handled by the first call.
            // If the first call didn't complete task_queue_destroy, it's complex.
            // For now, assume first successful call to destroy will handle queue.
        }
        return 0; // Or an error code indicating it was already shutting down.
    }
    pool->shutdown = 1; // REVIEW: Signal threads to shut down.
    // REVIEW: `pthread_cond_broadcast` is essential to wake up all threads waiting on `not_empty`.
    pthread_cond_broadcast(&pool->task_queue.not_empty);
    pthread_mutex_unlock(&pool->task_queue.lock);

    char msg[100];
    // REVIEW: Resource management: Joining all threads. This ensures they complete execution.
    for (size_t i = 0; i < pool->num_threads; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            snprintf(msg, sizeof(msg), "thread_pool_destroy: Failed to join thread %zu.", i);
            LOG_ERROR(msg); // REVIEW: Error on join is serious. Thread resources might not be fully reclaimed by OS.
        } else {
            snprintf(msg, sizeof(msg), "Successfully joined thread %zu.", i);
            LOG_INFO(msg);
        }
    }

    // REVIEW: Resource management: Freeing the `threads` array.
    if (pool->threads) {
        free(pool->threads);
        pool->threads = NULL; // REVIEW: Good practice to NULL freed pointers.
    }

    // REVIEW: Resource management: Destroying the task queue.
    // This should happen after threads are joined, so no thread is using the queue.
    task_queue_destroy(&pool->task_queue);

    LOG_INFO("Thread pool destroyed.");
    return 0;
}

// REVIEW: `worker_thread_main` is the core logic for each worker thread.
static void* worker_thread_main(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    Task* task;
    char msg[128];
    snprintf(msg, sizeof(msg), "Worker thread %lu started.", (unsigned long)pthread_self());
    LOG_INFO(msg);

    // REVIEW: Main worker loop.
    while (1) {
        pthread_mutex_lock(&pool->task_queue.lock); // REVIEW: Lock acquired to check queue state.
        // REVIEW: Wait condition: Loop while queue is empty AND not shutting down.
        // `pthread_cond_wait` atomically releases lock and waits; re-acquires on wakeup.
        while (pool->task_queue.count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->task_queue.not_empty, &pool->task_queue.lock);
        }

        // REVIEW: Shutdown condition: If shutdown is signaled AND queue is empty, thread exits.
        // This ensures all tasks submitted before shutdown signal are processed.
        if (pool->shutdown && pool->task_queue.count == 0) {
            pthread_mutex_unlock(&pool->task_queue.lock);
            break; // Exit the main while loop.
        }

        // Pop task if available
        // REVIEW: Direct manipulation of queue internals here instead of calling `task_queue_pop`.
        // This is okay if `task_queue_pop` is not meant to be a standalone utility or if this is for optimization.
        // However, it duplicates logic and could diverge from a separate `task_queue_pop` function.
        // Replacing inline pop logic with a call to actual_pop helper.
        task = NULL; // Initialize task to NULL
        if (pool->task_queue.head != NULL) { // Check if there's a task to pop
            task = actual_pop(&pool->task_queue);
        }
        // else task remains NULL, this case can happen if woken by shutdown signal and queue became empty.

        pthread_mutex_unlock(&pool->task_queue.lock); // REVIEW: Lock released after task is retrieved or decision not to pop.

        // REVIEW: Task execution.
        if (task) { // Check if a task was actually popped.
            if (task->function) { // REVIEW: Check function pointer.
                snprintf(msg, sizeof(msg), "Worker thread %lu executing task.", (unsigned long)pthread_self());
                LOG_INFO(msg);
                task->function(task->arg); // Execute the task.
            } else {
                LOG_ERROR("Worker thread: Popped task has NULL function pointer.");
            }
            // REVIEW: Resource management: Task struct is freed by the worker thread after execution.
            // This implies `thread_pool_submit` allocates it, and this is the designated point of freeing.
            free(task); // Free task regardless of function pointer, if task itself is not NULL.
            LOG_INFO("Task execution processed and task freed.");
        }
        // If task is NULL (e.g. woken by shutdown, queue empty), loop continues to re-evaluate shutdown condition.
    }

    snprintf(msg, sizeof(msg), "Worker thread %lu shutting down.", (unsigned long)pthread_self());
    LOG_INFO(msg);
    pthread_exit(NULL); // REVIEW: Graceful thread exit.
}

// Helper function to actually pop the task, assumes lock is held and queue is not empty.
static Task* actual_pop(TaskQueue* q) {
    if (q == NULL || q->head == NULL) {
        // This should not happen if called correctly by worker_thread_main
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
