#ifndef SPINE2DUMP_LOG_H
#define SPINE2DUMP_LOG_H

#include <zf_log/zf_log.h>

#define ZF_LOG_SUCCESS(...) ZF_LOG_WRITE(ZF_LOG_INFO, "SUCCESS", __VA_ARGS__)

void log_setup(int verbose);

#endif
