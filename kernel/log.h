#ifndef LOG_H
#define LOG_H

#include <stdint.h>

// Log severity levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

/**
 * @brief Initialize the serial port for logging.
 */
void log_init(void);

/**
 * @brief Write a formatted log message to the serial port.
 * * @param level The severity level of the log.
 * @param format Printf-style format string.
 * @param ... Additional arguments for the format string.
 */
void log_write(log_level_t level, const char* format, ...);

// Convenience macros for cleaner usage in your OS
#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) log_write(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)

#endif // LOG_H