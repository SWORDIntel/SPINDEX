#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdarg.h> // For va_list
#include "config.h" // For LogLevel enum

/**
 * @brief Initializes the logging system.
 *
 * Must be called before any LOG_X macros are used if logging to a file
 * or a specific log level is desired. Defaults to LOG_LEVEL_INFO to stdout
 * if not called.
 *
 * @param log_filepath Path to the log file. If NULL or empty, logs to stdout.
 *                     If the file cannot be opened, logs to stderr.
 * @param min_level The minimum log level to record. Messages below this level will be ignored.
 */
void spindex_log_init(const char *log_filepath, LogLevel min_level);

/**
 * @brief Shuts down the logging system.
 *
 * Closes the log file if it was opened by spindex_log_init.
 */
void spindex_log_shutdown();

/**
 * @brief Sets the minimum log level for messages to be recorded.
 * @param min_level The minimum log level.
 */
void spindex_set_log_level(LogLevel min_level);

/**
 * @brief Core logging function. Generally not called directly; use LOG_X macros instead.
 *
 * @param level The log level of the message.
 * @param file The source file name where the log message originated.
 * @param line The line number in the source file.
 * @param fmt The printf-style format string.
 * @param ... Additional arguments for the format string.
 */
void spindex_log_message(LogLevel level, const char *file, int line, const char *fmt, ...);

// Logging macros to simplify usage
#define LOG_ERROR(fmt, ...) spindex_log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  spindex_log_message(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  spindex_log_message(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) spindex_log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) spindex_log_message(LOG_LEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // UTILS_H
