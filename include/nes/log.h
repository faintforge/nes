#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdarg.h>

typedef enum LogLevel {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,

    _LOG_LEVEL_COUNT,
} LogLevel;

typedef struct LogEvent LogEvent;
struct LogEvent {
    LogLevel level;
    const char* file;
    int32_t line;
    const char* message;
    va_list args;
};

typedef void (*LoggerCallback)(LogEvent event, void* userdata);

extern void logger_register_callback(LoggerCallback func, void* userdata);
extern void log_stdout_callback(LogEvent event, void* user_data);

#define log_fatal(...) _log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) _log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) _log_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) _log_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) _log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_trace(...) _log_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

extern void _log_log(LogLevel level, const char* file, int32_t line, const char* message, ...) __attribute__((format(printf, 4, 5)));

#endif // LOG_H
