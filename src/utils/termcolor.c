#include "termcolor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>

    #include <io.h>
#else
    #include <unistd.h>
#endif

#define TERMCOLOR_RESET "\x1b[0m"
#define TERMCOLOR_TIMESTAMP "\x1b[90m"
#define TERMCOLOR_ERROR "\x1b[1;31m"
#define TERMCOLOR_WARNING "\x1b[1;33m"
#define TERMCOLOR_INFO "\x1b[1;34m"
#define TERMCOLOR_DEBUG "\x1b[1;36m"
#define TERMCOLOR_TRACE "\x1b[1;35m"
#define TERMCOLOR_SUCCESS "\x1b[1;32m"

const char* termcolor_for_log_tag(const char* tag) {
    if (strcmp(tag, "[ERROR]") == 0) {
        return TERMCOLOR_ERROR;
    }
    if (strcmp(tag, "[WARNING]") == 0) {
        return TERMCOLOR_WARNING;
    }
    if (strcmp(tag, "[INFO]") == 0) {
        return TERMCOLOR_INFO;
    }
    if (strcmp(tag, "[DEBUG]") == 0) {
        return TERMCOLOR_DEBUG;
    }
    if (strcmp(tag, "[TRACE]") == 0) {
        return TERMCOLOR_TRACE;
    }
    if (strcmp(tag, "[SUCCESS]") == 0) {
        return TERMCOLOR_SUCCESS;
    }
    return "";
}

const char* termcolor_reset(void) {
    return TERMCOLOR_RESET;
}

const char* termcolor_timestamp(void) {
    return TERMCOLOR_TIMESTAMP;
}

static int env_var_equals(const char* name, const char* value) {
#if defined(_WIN32)
    char* env_value = NULL;
    size_t length = 0;
    int result = _dupenv_s(&env_value, &length, name) == 0 && env_value != NULL &&
                 strcmp(env_value, value) == 0;
    free(env_value);
    return result;
#else
    const char* env_value = getenv(name);
    return env_value != NULL && strcmp(env_value, value) == 0;
#endif
}

static int env_var_set(const char* name) {
#if defined(_WIN32)
    char* env_value = NULL;
    size_t length = 0;
    int result = _dupenv_s(&env_value, &length, name) == 0 && env_value != NULL &&
                 env_value[0] != '\0';
    free(env_value);
    return result;
#else
    const char* env_value = getenv(name);
    return env_value != NULL && env_value[0] != '\0';
#endif
}

static int color_disabled_by_env(void) {
    return env_var_set("NO_COLOR") || env_var_equals("CLICOLOR", "0");
}

int termcolor_stderr_enabled(void) {
    if (color_disabled_by_env()) {
        return 0;
    }

#if defined(_WIN32)
    if (!_isatty(_fileno(stderr))) {
        return 0;
    }

    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
        return 0;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return 0;
    }

    DWORD vt_mode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(handle, vt_mode) != 0;
#else
    return isatty(fileno(stderr));
#endif
}
