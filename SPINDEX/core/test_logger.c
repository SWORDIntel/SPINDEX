#include "utils.h"  // To test the logging utility
#include "config.h" // For LogLevel enum values
#include <stdio.h>   // For printf

int main() {
    printf("--- Starting Logger Test ---\n");

    // Test 1: Initialize logging to a file with DEBUG level
    printf("Initializing log to 'test_spindex.log' with level DEBUG.\n");
    spindex_log_init("test_spindex.log", LOG_LEVEL_DEBUG);

    LOG_TRACE("This is a TRACE message. (Should NOT be visible with DEBUG level)");
    LOG_DEBUG("This is a DEBUG message. (Value: %d)", 123);
    LOG_INFO("This is an INFO message.");
    LOG_WARN("This is a WARNING message.");
    LOG_ERROR("This is an ERROR message. errno: %d (Just an example)", 42);

    // Test 2: Change log level to TRACE
    printf("\nChanging log level to TRACE.\n");
    spindex_set_log_level(LOG_LEVEL_TRACE);
    LOG_TRACE("This is another TRACE message. (Should BE visible NOW)");
    LOG_DEBUG("Another DEBUG message. (String: %s)", "test");

    // Test 3: Logging without file init (should default to stdout/stderr)
    // For this, we need to shutdown first, then log.
    // In a real app, init is usually once. This is for testing the fallback.
    printf("\nShutting down logger to test default stdout/stderr logging.\n");
    spindex_log_shutdown();
    // Note: After shutdown, g_log_fp is NULL. The logger will switch to stderr.
    // The default log level LOG_LEVEL_INFO will be used implicitly if not set after a full reset (which we don't have here).
    // Let's explicitly set a level for the uninitialized state for predictability in this test.
    // If spindex_log_init is not called, g_min_log_level remains at its static default (LOG_LEVEL_INFO).
    // We can simulate this by setting it directly for testing purposes after shutdown.
    // A better test would be a separate program run.
    // For now, let's see what happens with implicit fallback.
    // The g_min_log_level will retain its last set value (LOG_LEVEL_TRACE).

    printf("Logging after shutdown (should go to stderr/stdout with last log level TRACE):\n");
    LOG_ERROR("Error message after shutdown. Should go to stderr.");
    LOG_INFO("Info message after shutdown. Should go to stdout/stderr.");


    // Test 4: Re-initialize to stdout with INFO level
    printf("\nRe-initializing log to STDOUT with level INFO.\n");
    spindex_log_init(NULL, LOG_LEVEL_INFO); // NULL filepath means stdout
    LOG_TRACE("This TRACE message should NOT be visible with INFO level.");
    LOG_DEBUG("This DEBUG message should NOT be visible with INFO level.");
    LOG_INFO("This INFO message should be visible (stdout).");
    LOG_ERROR("This ERROR message should be visible (stdout, but formatted).");


    spindex_log_shutdown();
    printf("\n--- Logger Test Complete ---\n");
    printf("Please check 'test_spindex.log' and console output.\n");

    return 0;
}
