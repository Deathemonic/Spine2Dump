#include "log.h"

#include <stdio.h>
#include <time.h>

#include <zf_log/zf_log.h>

static const char* log_level_tag(const int level) {
    switch (level) {
        case ZF_LOG_VERBOSE: return "[TRACE]";
        case ZF_LOG_DEBUG:   return "[DEBUG]";
        case ZF_LOG_INFO:    return "[INFO]";
        case ZF_LOG_WARN:    return "[WARNING]";
        case ZF_LOG_ERROR:   return "[ERROR]";
        case ZF_LOG_FATAL:   return "[ERROR]";
        default:             return "[INFO]";
    }
}

static void log_output_callback(const zf_log_message* msg, void* arg) {
    (void)arg;

    char timestamp[16] = "00:00:00";
    time_t now = time(NULL);
    struct tm local;
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &local);

    *msg->p = '\0';
    fprintf(stderr, "%s %9s: %s\n", timestamp, log_level_tag(msg->lvl), msg->msg_b);
    fflush(stderr);
}

void log_setup(void) {
    zf_log_set_output_v(ZF_LOG_PUT_STD, NULL, log_output_callback);
}
