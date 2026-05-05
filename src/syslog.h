#ifndef CETI_SYSLOG_H
#define CETI_SYSLOG_H
#include "app_filex.h"
#include "util/str.h"

// this automatically grabs calling function's name

#define syslog_write(FMT_STR, ...) priv__syslog_write(&str_from_string(__FUNCTION__), FMT_STR __VA_OPT__(, ) __VA_ARGS__);
#define CETI_LOG(FMT_STR, ...) priv__syslog_write(&str_from_string(__FUNCTION__), FMT_STR __VA_OPT__(, ) __VA_ARGS__);
#define CETI_WARN(FMT_STR, ...) CETI_LOG("[WARN]: " FMT_STR __VA_OPT__(, ) __VA_ARGS__)
#define CETI_ERR(FMT_STR, ...)                                    \
    do {                                                          \
        CETI_LOG("[ERROR]: " FMT_STR __VA_OPT__(, ) __VA_ARGS__); \
        syslog_flush();                                           \
    } while (0)

#ifdef DEBUG
#define CETI_DEBUG(FMT_STR, ...) itm_printf("[%s] " FMT_STR "\n", __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
void itm_printf(const char *fmt, ...);
#else
#define CETI_DEBUG(FMT_STR, ...) ((void)0)
#endif

void syslog_init(void);
void syslog_deinit(void);
void syslog_flush(void);
UINT priv__syslog_write(const str identifier[static 1], const char fmt[static 1], ...);
#endif