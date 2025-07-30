#ifndef CETI_SYSLOG_H
#define CETI_SYSLOG_H
#include "app_filex.h"
#include "util/str.h"

// this automatically grabs calling function's name

#define syslog_write(FMT_STR, ...) __syslog_write(&str_from_string(__FUNCTION__), FMT_STR __VA_OPT__(, ) __VA_ARGS__);
#define CETI_LOG(FMT_STR, ...) __syslog_write(&str_from_string(__FUNCTION__), FMT_STR __VA_OPT__(, ) __VA_ARGS__);
#define CETI_WARN(FMT_STR, ...) CETI_LOG("[WARN]: " FMT_STR __VA_OPT__(, ) __VA_ARGS__)
#define CETI_ERR(FMT_STR, ...) CETI_LOG("[ERROR]: " FMT_STR __VA_OPT__(, ) __VA_ARGS__)


void syslog_init(void);
UINT __syslog_write(const str *identifier, const char *fmt, ...);
#endif