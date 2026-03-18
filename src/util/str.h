#ifndef CETI_STR_H
#define CETI_STR_H

typedef struct {
    const char *ptr;
    size_t length;
} str;

#define str_from_string(string) ((str){ \
    .ptr = string,                      \
    .length = strlen(string),           \
})

#endif // CETI_STR_H