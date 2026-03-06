#include "cdc.h"

#include "tusb.h"
#include "timing.h"

#include "microrl_config.h"
#include "microrl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct cdc_cmd_t {
    const char *key;
    const char *description;
    void (*action)(int argc, const char *const *argv);
} CdcCommand;

static void cdc_print(const char *str);
static void __not_implemented(int argc, const char *const *argv);
static void __cmd_datetime(int argc, const char *const *argv);
static void __cmd_help(int argc, const char *const *argv);

static microrl_t s_rl;

static const CdcCommand cdc_options[] = {
    {.key = "help",     .description = "list available commands", .action = __cmd_help},
    {.key = "datetime", .description = "get/set RTC epoch",      .action = __cmd_datetime},
};

#define NUM_COMMANDS (sizeof(cdc_options) / sizeof(cdc_options[0]))

static void __cmd_datetime(int argc, const char *const *argv) {
    if (argc < 2) {
        cdc_print("usage: datetime <epoch> | datetime ?" ENDL);
        return;
    }

    if (strcmp(argv[1], "?") == 0) {
        time_t epoch = rtc_get_epoch_s();
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld" ENDL, (long)epoch);
        cdc_print(buf);
        return;
    }

    char *endptr;
    long epoch = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        cdc_print("usage: datetime <epoch> | datetime ?" ENDL);
        return;
    }

    rtc_set_epoch_s((time_t)epoch);
    cdc_print("OK" ENDL);
}

static void __cmd_help(int argc, const char *const *argv) {
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        cdc_print("  ");
        cdc_print(cdc_options[i].key);
        cdc_print(" - ");
        cdc_print(cdc_options[i].description);
        cdc_print(ENDL);
    }
}

static void __not_implemented(int argc, const char *const *argv) {
    cdc_print("\a");
    cdc_print(argv[0]);
    cdc_print(" is not implemented yet!!!" ENDL);
}

/* microrl callbacks */
static void cdc_print(const char *str) {
    tud_cdc_write_str(str);
    tud_cdc_write_flush();
}

static int cdc_execute(int argc, const char *const *argv) {
    if (argc == 0) {
        __cmd_help(0, NULL);
        return 0;
    }

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], cdc_options[i].key) == 0) {
            cdc_options[i].action(argc, argv);
            return 0;
        }
    }

    cdc_print("Unknown command: ");
    cdc_print(argv[0]);
    cdc_print(ENDL);
    __cmd_help(0, NULL);
    return 1;
}

static char *compl_words[NUM_COMMANDS + 1];

static char **cdc_complete(int argc, const char *const *argv) {
    int j = 0;
    compl_words[0] = NULL;
    if (argc == 0) {
        compl_words[j++] = " help";
    } else if (argc == 1) {
        const char *bit = argv[0];
        size_t bit_len = strlen(bit);
        for (size_t i = 0; i < NUM_COMMANDS; i++) {
            if (strncmp(cdc_options[i].key, bit, bit_len) == 0) {
                compl_words[j++] = (char *)cdc_options[i].key;
            }
        }
    }

    compl_words[j] = NULL;
    return compl_words;
}


void usb_cdc_init(void) {
    microrl_init(&s_rl, cdc_print);
    microrl_set_execute_callback(&s_rl, cdc_execute);
    microrl_set_complete_callback(&s_rl, cdc_complete);
}

void usb_cdc_task(void) {
    static uint8_t connected = 0;
#if CFG_TUD_CDC
    if (!tud_cdc_connected()) {
        connected = 0;
        return;
    }

    if (!connected) {
        connected = 1;
        cdc_print(_PROMPT_DEFAULT);
    }

    while (tud_cdc_available()) {
        char buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        for (int i = 0; i < count; i++) {
            microrl_insert_char(&s_rl, buf[i]);
            if (buf[i] == '\r')
                microrl_insert_char(&s_rl, '\n');
        }
    }
    tud_cdc_write_flush();
#endif
}

