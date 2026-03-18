#include "cdc.h"

#include "timing.h"
#include "tusb.h"

#include "battery/bms_ctl.h"
#include "led/led.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENDL "\r\n"
#define CMDLINE_MAX 101
#define BOOTLOADER_REQUEST_MAGIC 0xB00710ADUL

typedef struct cdc_cmd_t {
    const char *key;
    const char *description;
    void (*action)(int argc, const char *const *argv);
} CdcCommand;

static void cdc_print(const char *str);
static void __not_implemented(int argc, const char *const *argv);
static void __cmd_datetime(int argc, const char *const *argv);
static void __cmd_help(int argc, const char *const *argv);
static void __cmd_shutdown(int argc, const char *const *argv);
static void __cmd_update(int argc, const char *const *argv);
static void __cmd_pressure(int argc, const char *const *argv);


static char s_cmdline[CMDLINE_MAX];
static int s_cmdlen;

static const CdcCommand cdc_options[] = {
    {.key = "help", .description = "list available commands", .action = __cmd_help},
    {.key = "datetime", .description = "get/set RTC epoch", .action = __cmd_datetime},
    {.key = "update", .description = "reboot into DFU system bootloader", .action = __cmd_update},
    {.key = "shutdown", .description = "powerdown tag", .action = __cmd_shutdown},
    // i2c passthrough
    // 
};

#define NUM_COMMANDS (sizeof(cdc_options) / sizeof(cdc_options[0]))

static void __cmd_shutdown(int argc, const char *const *argv) {
    bms_disable_FETs();
    HAL_PWREx_EnterSHUTDOWNMode(); // tag shutdown if powered by
}

/// @brief 
/// @param argc 
/// @param argv 
static void __cmd_update(int argc, const char *const *argv) {
    cdc_print("Rebooting into DFU system bootloader..." ENDL);
    tud_cdc_write_flush();
    HAL_Delay(100); // allow USB to flush

    led_bootloader();

    // Set magic value in TAMP backup register to signal bootloader entry
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    TAMP->BKP0R = BOOTLOADER_REQUEST_MAGIC;

    NVIC_SystemReset();
}

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
    cdc_print(argv[0]);
    cdc_print(" is not implemented yet" ENDL);
}

/* ---- I/O ---- */

static void cdc_print(const char *str) {
    tud_cdc_write_str(str);
    tud_cdc_write_flush();
}

/* ---- command dispatch ---- */

static void cdc_execute(void) {
    /* tokenise: split on spaces, max 8 tokens */
    const char *argv[8];
    int argc = 0;
    char *p = s_cmdline;

    while (*p && argc < 8) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }

    if (argc == 0) return;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], cdc_options[i].key) == 0) {
            cdc_options[i].action(argc, argv);
            return;
        }
    }

    cdc_print("Unknown command: ");
    cdc_print(argv[0]);
    cdc_print(ENDL);
}

/* ---- public API ---- */

void usb_cdc_init(void) {
    s_cmdlen = 0;
}

void usb_cdc_task(void) {
#if CFG_TUD_CDC
    if (!tud_cdc_connected()) return;

    while (tud_cdc_available()) {
        char buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        for (uint32_t i = 0; i < count; i++) {
            char ch = buf[i];
            if (ch == '\r' || ch == '\n') {
                if (s_cmdlen > 0) {
                    s_cmdline[s_cmdlen] = '\0';
                    cdc_execute();
                    s_cmdlen = 0;
                }
            } else if (s_cmdlen < CMDLINE_MAX - 1) {
                s_cmdline[s_cmdlen++] = ch;
            }
        }
    }
    tud_cdc_write_flush();
#endif
}
