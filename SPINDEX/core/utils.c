#include "utils.h"
#include <time.h>
#include <string.h> // For strerror, strlen
#include <errno.h>  // For errno
#include <stdlib.h> // For free if needed later

// Global state for the logger
static FILE *g_log_fp = NULL;
static LogLevel g_min_log_level = LOG_LEVEL_INFO; // Default log level

// Helper to convert LogLevel enum to string
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

void spindex_log_init(const char *log_filepath, LogLevel min_level) {
    g_min_log_level = min_level;

    if (g_log_fp && g_log_fp != stdout && g_log_fp != stderr) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }

    if (log_filepath && strlen(log_filepath) > 0) {
        g_log_fp = fopen(log_filepath, "a"); // Open in append mode
        if (!g_log_fp) {
            // If file opening fails, log error to stderr and continue logging to stderr
            fprintf(stderr, "[%s] CRITICAL: Failed to open log file '%s': %s. Logging to stderr instead.\n",
                    level_to_string(LOG_LEVEL_ERROR), log_filepath, strerror(errno));
            g_log_fp = stderr;
        }
    } else {
        g_log_fp = stdout; // Default to stdout if no path provided
    }
    LOG_INFO("Logging system initialized. Log level: %s.", level_to_string(g_min_log_level));
}

void spindex_log_shutdown() {
    LOG_INFO("Logging system shutting down.");
    if (g_log_fp && g_log_fp != stdout && g_log_fp != stderr) {
        fclose(g_log_fp);
    }
    g_log_fp = NULL; // Reset to prevent use after shutdown
}

void spindex_set_log_level(LogLevel min_level) {
    g_min_log_level = min_level;
}

void spindex_log_message(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (!g_log_fp) { // If not initialized, default to stderr and info level
        g_log_fp = stderr;
        // Do not print an error here, as this is a fallback for uninitialized logging
    }

    if (level > g_min_log_level) {
        return; // Message level is lower than the configured minimum, so skip
    }

    // Get current time
    time_t now = time(NULL);
    char time_buf[20]; // Buffer for "YYYY-MM-DD HH:MM:SS"
    if (now != (time_t)(-1)) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    } else {
        strcpy(time_buf, "TIME_ERROR");
    }


    // Print log prefix: timestamp, level, file:line
    // Use a temporary buffer for the whole message to ensure atomicity for typical cases
    char log_buffer[2048];
    int prefix_len = snprintf(log_buffer, sizeof(log_buffer), "[%s] [%s] [%s:%d] ",
                              time_buf, level_to_string(level), file, line);

    if (prefix_len < 0) { // snprintf error
        fprintf(stderr, "Error formatting log prefix.\n");
        return;
    }

    // Append user message
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(log_buffer + prefix_len, sizeof(log_buffer) - prefix_len, fmt, args);
    va_end(args);

    if (msg_len < 0) { // vsnprintf error
        fprintf(stderr, "Error formatting log message.\n");
        return;
    }

    // Ensure there's a newline
    int total_len = prefix_len + msg_len;
    if (total_len > 0 && log_buffer[total_len - 1] != '\n' && total_len < (int)sizeof(log_buffer) -1) {
        log_buffer[total_len] = '\n';
        log_buffer[total_len + 1] = '\0';
    } else if (total_len >= (int)sizeof(log_buffer) -1 ) {
        // Message was truncated, ensure newline at the very end if possible
        log_buffer[sizeof(log_buffer) - 2] = '\n';
        log_buffer[sizeof(log_buffer) - 1] = '\0';
    }


    // Actually write to the log output
    fputs(log_buffer, g_log_fp);

    // Flush if error or warn to ensure it's written immediately
    if (level <= LOG_LEVEL_WARN) {
        fflush(g_log_fp);
    }
}
