#ifndef SPINDEX_CORE_THREADING_H
#define SPINDEX_CORE_THREADING_H

#include <pthread.h>
#include <stdlib.h> // For size_t

// Note: Basic logging macros LOG_INFO and LOG_ERROR were removed from here
// to avoid conflict with the more advanced logger in utils.h.
// It's assumed that files including threading.h will also have access to utils.h
// (e.g., through a higher-level include like ingestion_pipeline.h or directly).
// If a file only includes threading.h and needs logging, it should include utils.h.

// REVIEW: Consider if a maximum queue size should be implemented for TaskQueue to prevent excessive memory usage if tasks are produced much faster than consumed.
// REVIEW: API clarity - thread_pool_submit takes a function pointer and arg. This is suitable for general tasks.
// For file processing in the ingestion pipeline, 'arg' could be a struct containing file path, metadata, processing parameters, etc.
// The submitted 'function' would then be responsible for casting 'arg' to the appropriate type and processing the file.

// Task structure
// REVIEW: Simple task structure. 'arg' being void* provides flexibility.
// REVIEW: Ensure 'function' handles its 'arg' safely (e.g., correct type casting, NULL checks if applicable).
typedef struct Task {
    void (*function)(void* arg);
    void* arg;
    struct Task* next;
} Task;

// Task queue structure
// REVIEW: Standard FIFO queue implementation.
typedef struct TaskQueue {
    Task* head;
    Task* tail;
    pthread_mutex_t lock; // REVIEW: Mutex to protect access to head, tail, and count. This is correct.
    pthread_cond_t not_empty; // REVIEW: Condition variable to signal worker threads when a new task is available. Correctly used with 'lock'.
    int count; // REVIEW: Keeping track of task count is useful for metrics or potentially for implementing a max queue size.
} TaskQueue;

// Thread pool structure
// REVIEW: Contains essential components for a thread pool.
typedef struct ThreadPool {
    pthread_t* threads; // REVIEW: Array of worker threads.
    size_t num_threads; // REVIEW: Number of threads in the pool.
    TaskQueue task_queue; // REVIEW: Embedded task queue.
    volatile int shutdown; // Flag to signal threads to shutdown. REVIEW: 'volatile' is appropriate here as it's accessed by multiple threads and can change unexpectedly.
} ThreadPool;

// Function declarations
// REVIEW: The API (init, submit, destroy) is clear and provides the necessary functionalities.
// REVIEW: Consistency in return types (int for success/error) is good. 0 for success, -1 for error is a common pattern.

/**
 * @brief Initializes the thread pool.
 *
 * @param pool Pointer to the ThreadPool structure.
 * @param num_threads Number of worker threads to create.
 * @return 0 on success, -1 on error.
 */
// REVIEW: Consider what happens if some threads are created successfully but others fail during init. The current error code doesn't distinguish.
// REVIEW: Ensure robust error handling for memory allocation (for `threads` array) and `pthread_create`.
int thread_pool_init(ThreadPool* pool, size_t num_threads);

/**
 * @brief Submits a task to the thread pool.
 *
 * @param pool Pointer to the ThreadPool structure.
 * @param function Pointer to the function to be executed.
 * @param arg Argument to be passed to the function.
 * @return 0 on success, -1 on error.
 */
// REVIEW: Error handling for task allocation (if dynamic) and queue operations should be present.
// REVIEW: What happens if task submission fails? Does the caller need to free 'arg'? (Assumed task function or submitter handles 'arg' lifecycle).
int thread_pool_submit(ThreadPool* pool, void (*function)(void*), void* arg);

/**
 * @brief Destroys the thread pool.
 *
 * Waits for all tasks to complete and then joins all worker threads.
 *
 * @param pool Pointer to the ThreadPool structure.
 * @return 0 on success, -1 on error.
 */
// REVIEW: Graceful shutdown sequence is critical.
// 1. Signal shutdown.
// 2. Wake up all worker threads (possibly blocked on condition variable).
// 3. Worker threads should finish their current task and then exit.
// 4. Join all threads.
// 5. Clean up resources (task queue, mutexes, condition variables, threads array).
// REVIEW: Ensure all dynamically allocated memory for tasks is freed.
// REVIEW: Potential race condition if tasks are being submitted while the pool is shutting down. The submit function should probably check the shutdown flag.
int thread_pool_destroy(ThreadPool* pool);

// Task Queue utility functions (optional, can be static in .c)
// REVIEW: These are good helper functions. If they are only used by threading.c, making them static is appropriate for encapsulation.
int task_queue_init(TaskQueue* queue); // REVIEW: Ensure mutex and cond_var are initialized.
void task_queue_push(TaskQueue* queue, Task* task);
Task* task_queue_pop(TaskQueue* queue);
void task_queue_destroy(TaskQueue* queue); // REVIEW: Ensure mutex and cond_var are destroyed. Any remaining tasks in the queue should be handled (e.g., freed or processed, though during pool destroy, they are typically processed before this).

#endif // SPINDEX_CORE_THREADING_H
