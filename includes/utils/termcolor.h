#ifndef SPINE2DUMP_TERMCOLOR_H
#define SPINE2DUMP_TERMCOLOR_H

const char* termcolor_for_log_tag(const char* tag);
const char* termcolor_reset(void);
const char* termcolor_timestamp(void);
int termcolor_stderr_enabled(void);

#endif
