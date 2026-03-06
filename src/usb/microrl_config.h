/* microrl configuration override — included via -include before library's config.h */
#ifndef _MICRORL_CONFIG_H_
#define _MICRORL_CONFIG_H_

#define MICRORL_LIB_VER "1.5.1"

#define _COMMAND_LINE_LEN (1+100)
#define _COMMAND_TOKEN_NMB 8

#define _PROMPT_DEFAULT "\033[32mceti >\033[0m "
#define _PROMPT_LEN       7

#define _USE_COMPLETE
#define _USE_HISTORY
#define _RING_HISTORY_LEN 64
#define _USE_ESC_SEQ
#define _USE_LIBC_STDIO
#define _USE_CTLR_C

#undef _ENABLE_INIT_PROMPT

#define _ENDL_CRLF

#if defined(_ENDL_CR)
#define ENDL "\r"
#elif defined(_ENDL_CRLF)
#define ENDL "\r\n"
#elif defined(_ENDL_LF)
#define ENDL "\n"
#elif defined(_ENDL_LFCR)
#define ENDL "\n\r"
#else
#error "You must define new line symbol."
#endif

#if _RING_HISTORY_LEN > 256
#error "This history implementation (ring buffer with 1 byte iterator) allow 256 byte buffer size maximum"
#endif

#endif
