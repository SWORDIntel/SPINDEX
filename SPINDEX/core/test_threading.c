#include "threading.h" // The module to test
#include "utils.h"     // For LOG_INFO, LOG_ERROR (if used directly or by threading.c)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For sleep/usleep
#include <pthread.h> // For pthread_self in test tasks if needed for logging

// --- Test Task Payloads and Functions ---

typedef struct {
    int task_id;
    int *execution_counter; // Pointer to a shared counter
    pthread_mutex_t *counter_mutex; // Mutex for the shared counter
    long execution_duration_us; // Duration to sleep in microseconds
} TestTaskArgs;

void simple_test_task_func(void *arg) {
    TestTaskArgs *task_args = (TestTaskArgs *)arg;
    if (!task_args) return;

    if (task_args->execution_duration_us > 0) {
        usleep(task_args->execution_duration_us);
    }

    if (task_args->counter_mutex && task_args->execution_counter) {
        pthread_mutex_lock(task_args->counter_mutex);
        (*task_args->execution_counter)++;
        // printf("Task %d executed by thread %lu. Counter: %d\n",
        //        task_args->task_id, (unsigned long)pthread_self(), *task_args->execution_counter);
        pthread_mutex_unlock(task_args->counter_mutex);
    } else {
         //printf("Task %d executed by thread %lu (no counter).\n",
         //      task_args->task_id, (unsigned long)pthread_self());
    }
    // Argument `task_args` (if heap allocated) would be freed by submitter or task itself.
    // In these tests, they are stack allocated or part of an array.
}

// --- Test Cases ---

void test_pool_creation_destruction() {
    printf("\n--- Test: Pool Creation and Destruction ---\n");
    ThreadPool *pool = thread_pool_create(4, 10); // 4 threads, queue size 10
    if (pool) {
        printf("PASS: Thread pool created successfully.\n");
    } else {
        printf("FAIL: Thread pool creation failed.\n");
        return;
    }

    if (thread_pool_destroy(pool) == 0) {
        printf("PASS: Thread pool destroyed successfully.\n");
    } else {
        printf("FAIL: Thread pool destruction failed.\n");
    }
}

void test_submit_single_task() {
    printf("\n--- Test: Submit Single Task ---\n");
    ThreadPool *pool = thread_pool_create(1, 5);
    if (!pool) { printf("FAIL: Pool creation failed.\n"); return; }

    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    TestTaskArgs task_arg = {1, &counter, &mutex, 10000}; // 10ms sleep

    printf("Submitting single task...\n");
    if (thread_pool_submit_task(pool, simple_test_task_func, &task_arg)) {
        printf("Task submitted.\n");
    } else {
        printf("FAIL: Failed to submit task.\n");
        thread_pool_destroy(pool); // Attempt cleanup
        pthread_mutex_destroy(&mutex);
        return;
    }

    // Give time for task to execute before destroying pool (which waits)
    // For a single task and single thread, destroy will wait.
    // Adding a small sleep here can make test output sequence more predictable for simple cases.
    // usleep(50000);

    thread_pool_destroy(pool); // This will wait for tasks to complete

    if (counter == 1) {
        printf("PASS: Single task executed successfully. Counter: %d\n", counter);
    } else {
        printf("FAIL: Single task did not execute as expected. Counter: %d\n", counter);
    }
    pthread_mutex_destroy(&mutex);
}

void test_submit_multiple_tasks_multiple_threads() {
    printf("\n--- Test: Submit Multiple Tasks to Multiple Threads ---\n");
    const int num_threads = 4;
    const int num_tasks = 20;
    const size_t queue_size = 10; // Queue smaller than num_tasks to test queuing

    ThreadPool *pool = thread_pool_create(num_threads, queue_size);
    if (!pool) { printf("FAIL: Pool creation failed.\n"); return; }

    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    TestTaskArgs tasks_args[num_tasks]; // Array of task args

    printf("Submitting %d tasks...\n", num_tasks);
    for (int i = 0; i < num_tasks; ++i) {
        tasks_args[i] = (TestTaskArgs){i + 1, &counter, &mutex, (rand() % 5 + 1) * 1000}; // 1-5 ms sleep
        if (!thread_pool_submit_task(pool, simple_test_task_func, &tasks_args[i])) {
            printf("FAIL: Failed to submit task %d.\n", i + 1);
            // Optionally break or handle error, for now continue submitting
        }
    }
    printf("All tasks submitted (or attempted).\n");

    thread_pool_destroy(pool); // Waits for all tasks to complete

    if (counter == num_tasks) {
        printf("PASS: All %d tasks executed successfully. Counter: %d\n", num_tasks, counter);
    } else {
        printf("FAIL: Not all tasks executed. Expected: %d, Got: %d\n", num_tasks, counter);
    }
    pthread_mutex_destroy(&mutex);
}

void test_queue_boundedness_indicative() {
    printf("\n--- Test: Queue Boundedness (Indicative) (max_queue_size=1) ---\n");
    const int num_threads = 1;
    const size_t queue_size = 1;
    ThreadPool *pool = thread_pool_create(num_threads, queue_size);
    if (!pool) { printf("FAIL: Pool creation failed.\n"); return; }

    int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    TestTaskArgs task_args[3];
    task_args[0] = (TestTaskArgs){1, &counter, &mutex, 200000}; // Task 1: 200ms
    task_args[1] = (TestTaskArgs){2, &counter, &mutex, 10000};  // Task 2: 10ms
    task_args[2] = (TestTaskArgs){3, &counter, &mutex, 10000};  // Task 3: 10ms

    printf("Submitting task 1 (long running)...\n");
    thread_pool_submit_task(pool, simple_test_task_func, &task_args[0]);

    // Brief sleep to allow worker to pick up task 1 and potentially fill queue slot if submit is fast
    usleep(1000);

    printf("Submitting task 2 (should ideally wait if queue was full)...\n");
    bool t2_submitted = thread_pool_submit_task(pool, simple_test_task_func, &task_args[1]);
    if(t2_submitted) printf("Task 2 submitted.\n"); else printf("Task 2 submission failed (indicates queue might have been full or shutdown).\n");

    printf("Submitting task 3 (should also potentially wait)...\n");
    bool t3_submitted = thread_pool_submit_task(pool, simple_test_task_func, &task_args[2]);
    if(t3_submitted) printf("Task 3 submitted.\n"); else printf("Task 3 submission failed.\n");

    printf("Waiting for tasks to complete by destroying pool...\n");
    thread_pool_destroy(pool);

    int expected_executions = (1 + (t2_submitted ? 1:0) + (t3_submitted ? 1:0));
    if (counter == expected_executions) {
        printf("PASS: Bounded queue test completed. Executed tasks: %d\n", counter);
    } else {
        printf("FAIL: Bounded queue test. Expected tasks based on submission success: %d, Actual counter: %d\n",
               expected_executions, counter);
    }
    pthread_mutex_destroy(&mutex);
}


int main() {
    // Initialize logger from utils if it has an init function and is used by threading.c
    // For example: if (spindex_logger_init("test_threading.log", LOG_LEVEL_DEBUG) != 0) return 1;
    // For now, threading.c uses LOG_x macros which might just be printf.

    printf("--- Starting Threading Test Suite ---\n");

    test_pool_creation_destruction();
    test_submit_single_task();
    test_submit_multiple_tasks_multiple_threads();
    test_queue_boundedness_indicative();

    printf("--- Threading Test Suite Complete ---\n");

    // if (spindex_logger_shutdown) spindex_logger_shutdown();
    return 0;
}
