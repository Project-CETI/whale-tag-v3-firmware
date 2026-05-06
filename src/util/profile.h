#ifndef CETI_UTIL_PROFILE_H
#define CETI_UTIL_PROFILE_H
#include <stdint.h>

void profile_init(void);
void profile_flush(void);

void profile_pause(void);
void profile_continue(void);
uint32_t profile_get_dropped_count(void);
#endif // CETI_UTIL_PROFILE_H
