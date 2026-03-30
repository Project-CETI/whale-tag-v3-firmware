#include "cdc.h"
#include "main.h"

#include "timing.h"
#include "tusb.h"

#include "battery/bms_ctl.h"
#include "burnwire.h"
#include "led/led.h"
#include "version_hw.h"
#include "satellite/satellite.h"

#include <ctype.h>
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
static void __cmd_audio_negative(int argc, const char *const *argv);
static void __cmd_audio_positive(int argc, const char *const *argv);
static void __cmd_argos_message(int argc, const char *const *argv);
static void __cmd_bms_program(int argc, const char *const *argv);
static void __cmd_burnwire(int argc, const char *const *argv);
static void __cmd_datetime(int argc, const char *const *argv);
static void __cmd_flasher(int argc, const char *const *argv);
static void __cmd_help(int argc, const char *const *argv);
static void __cmd_i2cdetect(int argc, const char *const *argv);
static void __cmd_shutdown(int argc, const char *const *argv);
static void __cmd_restart(int argc, const char *const *argv);
static void __cmd_update(int argc, const char *const *argv);
static void __cmd_vhf_pinger(int argc, const char *const *argv);


static char s_cmdline[CMDLINE_MAX];
static int s_cmdlen;

static const CdcCommand cdc_options[] = {
    {.key = "audio_negative", .description = "enable/disable audio -5V input", .action = __cmd_audio_negative},
    {.key = "audio_positive", .description = "enable/disable audio 5V input", .action = __cmd_audio_positive},
    {.key = "argos_message", .description = "transmit argos message", .action = __cmd_argos_message},
    {.key = "burnwire", .description = "turn burnwire on/off", .action = __cmd_burnwire},
    {.key = "bms_program", .description = "set BMS NV memory", .action = __cmd_bms_program},
    {.key = "datetime", .description = "get/set RTC epoch", .action = __cmd_datetime},
    {.key = "flasher", .description = "enable/disanble antenna LED flasher", .action = __cmd_flasher},
    {.key = "help", .description = "list available commands", .action = __cmd_help},
    {.key = "i2cdetect", .description = "list devices on specified i2c bus", .action = __cmd_i2cdetect},
    {.key = "restart", .description = "restart tag", .action = __cmd_restart},
    {.key = "shutdown", .description = "powerdown tag", .action = __cmd_shutdown},
    {.key = "update", .description = "reboot into DFU system bootloader", .action = __cmd_update},
    {.key = "vhf_pinger", .description = "enable/disabled VHF pinger", .action = __cmd_vhf_pinger},

    // i2c passthrough
    // 
};

#define NUM_COMMANDS (sizeof(cdc_options) / sizeof(cdc_options[0]))

static void __cmd_audio_negative(int argc, const char *const *argv) {
    if (argc < 2) {
        return;
    }    

    switch(argv[1][0]) {
        case '0':
            HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port, Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_SET);
            break;

        case '1':
            HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port, Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_RESET);
            break;
        
        default:
            break;
    }
}

static void __cmd_audio_positive(int argc, const char *const *argv) {
    if (argc < 2) {
        return;
    }    

    switch(argv[1][0]) {
        case '0':
            HAL_GPIO_WritePin(AUDIO_VP_EN_GPIO_Output_GPIO_Port, AUDIO_VP_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
            break;
        
        case '1':
            HAL_GPIO_WritePin(AUDIO_VP_EN_GPIO_Output_GPIO_Port, AUDIO_VP_EN_GPIO_Output_Pin, GPIO_PIN_SET);
            break;

        default:
            break;
    }
}

static void __cmd_argos_message(int argc, const char *const *argv) {
    uint16_t msg_length = 0;
    uint16_t max_length = 24; // ToDo:change based on args protocol
    char tx_message_ascii[(2 * 24) + 1];    

    // copy message over to output as long as valid 
    if (argc >= 2) {
        const char *msg = argv[1];
        for  (int i = 0; i < 2*max_length; i++) {
            if (!isxdigit((int)msg[i])) {
                break;
            } 
            tx_message_ascii[i] = toupper(msg[i]);
            msg_length++;
        }
    }

    // pad output
    while (msg_length < 2*max_length) {
        tx_message_ascii[msg_length] = '0';
        msg_length++;
    } 

    // transmit via argos
    satellite_transmit(tx_message_ascii, 2 * max_length);

}

static void __cmd_bms_program(int argc, const char *const *argv) {
    bms_ctl_program_nonvolatile_memory();
}

static void __cmd_burnwire(int argc, const char *const *argv) {
    if ((argc >= 2) && (strcmp(argv[1], "0") == 0)) {
        burnwire_off();
        led_usb();
        return;
    } else if ((argc >= 2) && (strcmp(argv[1], "1") == 0)) {
        burnwire_on();
        led_burn();
        return;
    } else {
        cdc_print("usage: burnwire  0 | burnwire 1" ENDL);
        return;
    }
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

static void __cmd_flasher(int argc, const char *const *argv) {
    if (argc < 2) {
        return;
    }    

    switch(argv[1][0]) {
        case '0':
            HAL_GPIO_WritePin(FLASHER_LED_EN_GPIO_Output_GPIO_Port, FLASHER_LED_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
            break;
        
        case '1':
            HAL_GPIO_WritePin(FLASHER_LED_EN_GPIO_Output_GPIO_Port, FLASHER_LED_EN_GPIO_Output_Pin, GPIO_PIN_SET);
            break;

        default:
            break;
    }
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


extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;
static void __cmd_i2cdetect(int argc, const char *const *argv) {
    if (argc < 2) {
        return;
    }

    I2C_HandleTypeDef *hi2c = NULL;
    switch (argv[0][1]) {
        case '1': 
            hi2c = &hi2c1;
            break;
        case '2': 
            hi2c = &hi2c2;
            break;
        case '3': 
            hi2c = &hi2c3;
            break;
            
        default:
            break;
    }

    if (NULL != hi2c) {
        for (uint8_t device_address = 0; device_address < 0x80; device_address++) {
            // print out device address if it exists on the bus
            if (HAL_OK == HAL_I2C_IsDeviceReady(hi2c, (device_address << 1), 3, 5)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%02x ", device_address);
                cdc_print(buf);
            }
        }
    }
    cdc_print(ENDL);
}

static void __cmd_restart(int argc, const char *const *argv) {
    NVIC_SystemReset();
}

static void __cmd_shutdown(int argc, const char *const *argv) {
    bms_disable_FETs();
    HAL_PWREx_EnterSHUTDOWNMode(); // tag shutdown if powered by
}

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

static void __cmd_vhf_pinger(int argc, const char *const *argv) {
#ifdef VHF_EN_GPIO_Output_Pin
    if (argc < 2) {
        return;
    }    

    switch(argv[1][0]) {
        case '0':
            HAL_GPIO_WritePin(VHF_EN_GPIO_Output_GPIO_Port, VHF_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
            break;
        
        case '1':
            HAL_GPIO_WritePin(VHF_EN_GPIO_Output_GPIO_Port, VHF_EN_GPIO_Output_Pin, GPIO_PIN_SET);
            break;

        default:
            break;
    }
#endif 

}


[[maybe_unused]] 
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
