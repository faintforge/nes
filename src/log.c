#include "nes/log.h"

#include <assert.h>
#include <stdio.h>

#define MAX_LOGGER_CALLBACK_COUNT 16

typedef struct _LoggerCallback _LoggerCallback;
struct _LoggerCallback {
    LoggerCallback func;
    void* userdata;
};

static _LoggerCallback g_logger_callbacks[MAX_LOGGER_CALLBACK_COUNT] = {0};
static uint32_t g_logger_callback_count = 0;

void logger_register_callback(LoggerCallback func, void* userdata) {
    assert(g_logger_callback_count < MAX_LOGGER_CALLBACK_COUNT &&
            "Maximum amount of logger callbacks has been reached!");
    g_logger_callbacks[g_logger_callback_count] = (_LoggerCallback) {
        .func = func,
        .userdata = userdata,
    };
    g_logger_callback_count++;
}

void _log_log(LogLevel level, const char* file, int32_t line, const char* message, ...) {
    for (uint32_t i = 0; i < g_logger_callback_count; i++) {
        LogEvent event = {
            .level = level,
            .file = file,
            .line = line,
            .message = message,
        };
        va_start(event.args, message);
        _LoggerCallback callback = g_logger_callbacks[i];
        callback.func(event, callback.userdata);
        va_end(event.args);
    }
}

void log_stdout_callback(LogEvent event, void* user_data) {
    (void) user_data;
    const char* const level_color[_LOG_LEVEL_COUNT + 1] = {
        "\033[101;30m",
        "\033[0;91m",
        "\033[0;93m",
        "\033[0;92m",
        "\033[0;94m",
        "\033[0;95m",
    };

    const char* const level_str[_LOG_LEVEL_COUNT + 1] = {
        "FATAL",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG",
        "TRACE",
    };

    printf("%s%s\033[0;90m %s:%u: \033[0m",
            level_color[event.level],
            level_str[event.level],
            event.file,
            event.line);
    vprintf(event.message, event.args);
    printf("\n");
}
