#include "cdc.h"
#include "main.h"

#include "timing.h"
#include "tusb.h"

#include "battery/bms_ctl.h"
#include "burnwire.h"
#include "led/led.h"
#include "version_hw.h"
#include "satellite/satellite.h"

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENDL "\r\n"
#define CMDLINE_MAX 101
#define BOOTLOADER_REQUEST_MAGIC 0xB00710ADUL


extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;

typedef struct cdc_cmd_t {
    const char *key;
    const char *description;
    void (*action)(int argc, const char *const *argv);
} CdcCommand;

static void cdc_print(const char *str);
static void priv__not_implemented(int argc, const char *const *argv);
static void priv__cmd_audio_negative(int argc, const char *const *argv);
static void priv__cmd_audio_positive(int argc, const char *const *argv);
static void priv__cmd_argos_message(int argc, const char *const *argv);
static void priv__cmd_bms_program(int argc, const char *const *argv);
static void priv__cmd_burnwire(int argc, const char *const *argv);
static void priv__cmd_datetime(int argc, const char *const *argv);
static void priv__cmd_flasher(int argc, const char *const *argv);
static void priv__cmd_help(int argc, const char *const *argv);
static void priv__cmd_shutdown(int argc, const char *const *argv);
static void priv__cmd_program_argos(int argc, const char *const *argv);
static void priv__cmd_restart(int argc, const char *const *argv);
static void priv__cmd_update(int argc, const char *const *argv);
static void priv__cmd_vhf_pinger(int argc, const char *const *argv);

static void priv__cmd_i2cdetect(int argc, const char *const *argv);
static void priv__cmd_i2cset(int argc, const char *const *argv);
static void priv__cmd_i2cget(int argc, const char *const *argv);

static char s_cmdline[CMDLINE_MAX];
static int s_cmdlen;

static const CdcCommand cdc_options[] = {
    {.key = "audio_negative", .description = "enable/disable audio -5V input", .action = priv__cmd_audio_negative},
    {.key = "audio_positive", .description = "enable/disable audio 5V input", .action = priv__cmd_audio_positive},
    {.key = "argos_message", .description = "transmit argos message", .action = priv__cmd_argos_message},
    {.key = "burnwire", .description = "turn burnwire on/off", .action = priv__cmd_burnwire},
    {.key = "bms_program", .description = "set BMS NV memory", .action = priv__cmd_bms_program},
    {.key = "datetime", .description = "get/set RTC epoch", .action = priv__cmd_datetime},
    {.key = "flasher", .description = "enable/disanble antenna LED flasher", .action = priv__cmd_flasher},
    {.key = "help", .description = "list available commands", .action = priv__cmd_help},
    {.key = "program_argos", .description = "Set argos module into a programming state", .action = priv__cmd_program_argos},
    {.key = "restart", .description = "restart tag", .action = priv__cmd_restart},
    {.key = "shutdown", .description = "powerdown tag", .action = priv__cmd_shutdown},
    {.key = "update", .description = "reboot into DFU system bootloader", .action = priv__cmd_update},
    {.key = "vhf_pinger", .description = "enable/disabled VHF pinger", .action = priv__cmd_vhf_pinger},
    
    // i2c passthrough
    {.key = "i2cdetect", .description = "list devices on specified i2c bus", .action = priv__cmd_i2cdetect},
    {.key = "i2cget", .description = "i2cget [b|w] <bus> <device> [register] [count]", .action = priv__cmd_i2cget},
    {.key = "i2cset", .description = "i2cset [b|w] <bus> <device> <register> <value> [value...]", .action = priv__cmd_i2cset},
    // 
};

#define NUM_COMMANDS (sizeof(cdc_options) / sizeof(cdc_options[0]))

/// @brief enable/disable audio-+5 voltage
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_audio_negative(int argc, const char *const *argv) {
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

/// @brief enable/disable audio +5 voltage
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_audio_positive(int argc, const char *const *argv) {
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

/// @brief send a message via argos
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_argos_message(int argc, const char *const *argv) {
    static uint8_t argos_started = 0;
    uint16_t msg_length = 0;
    uint16_t max_length = 24; // ToDo:change based on args protocol
    switch (tag_config.argos.modulation_protocol) {
        case ARGOS_MOD_LDA2:
            max_length = 24;
            break;

        case ARGOS_MOD_VLDA4:
            max_length = 3;
            break;

        case ARGOS_MOD_LDK:
            max_length = 16;
            break;

        case ARGOS_MOD_LDA2L:
            max_length = 24;
            break;
    }
    char tx_message_ascii[(2 * 24) + 1];    

    if (!argos_started) {
        satellite_start(&tag_config.argos);
        argos_started = 1;
    }

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

/// @brief write bms settings to nonvolatile memory
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_bms_program(int argc, const char *const *argv) {
    bms_ctl_program_nonvolatile_memory();
}

/// @brief disable or enable burnwire
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_burnwire(int argc, const char *const *argv) {
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

/// @brief get or set rtc date time
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_datetime(int argc, const char *const *argv) {
    if (argc < 2) {
        cdc_print("usage: datetime <epoch> | datetime ?" ENDL);
        return;
    }

    if (strcmp(argv[1], "?") == 0) {
        uint64_t epoch = rtc_get_epoch_s();
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

    rtc_set_epoch_s((uint64_t)epoch);
    cdc_print("OK" ENDL);
}

/// @brief enable of disable antenna flasher
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_flasher(int argc, const char *const *argv) {
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

/// @brief print help message
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_help(int argc, const char *const *argv) {
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

static I2C_HandleTypeDef *priv__i2c_bus_from_arg(const char *arg) {
    switch (arg[0]) {
        case '1': return &hi2c1;
        case '2': return &hi2c2;
        case '3': return &hi2c3;
        default:  return NULL;
    }
}

static void priv__cmd_i2cdetect(int argc, const char *const *argv) {
    if (argc < 2) {
        cdc_print("usage: i2cdetect <bus>" ENDL);
        return;
    }

    I2C_HandleTypeDef *hi2c = priv__i2c_bus_from_arg(argv[1]);
    if (NULL == hi2c) {
        cdc_print("invalid bus (1-3)" ENDL);
        return;
    }

    for (uint8_t device_address = 0; device_address < 0x80; device_address++) {
        if (HAL_OK == HAL_I2C_IsDeviceReady(hi2c, (device_address << 1), 3, 5)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%02x ", device_address);
            cdc_print(buf);
        }
    }
    cdc_print(ENDL);
}

// i2cset [b|w] <bus> <device> <register> <value> [value...]
static void priv__cmd_i2cset(int argc, const char *const *argv) {
    if (argc < 5) {
        cdc_print("usage: i2cset [b|w] <bus> <device> <register> <value> [value...]" ENDL);
        return;
    }

    // optional width flag as first arg (default: byte)
    int arg = 1;
    int width = 1;
    if (argv[arg][0] == 'b' && argv[arg][1] == '\0') {
        arg++;
    } else if (argv[arg][0] == 'w' && argv[arg][1] == '\0') {
        width = 2;
        arg++;
    }

    if (argc - arg < 4) {
        cdc_print("usage: i2cset [b|w] <bus> <device> <register> <value> [value...]" ENDL);
        return;
    }

    I2C_HandleTypeDef *hi2c = priv__i2c_bus_from_arg(argv[arg]);
    if (NULL == hi2c) {
        cdc_print("invalid bus (1-3)" ENDL);
        return;
    }
    arg++;

    uint8_t dev_addr = (uint8_t)strtol(argv[arg++], NULL, 0);
    uint8_t reg_addr = (uint8_t)strtol(argv[arg++], NULL, 0);

    int n_values = argc - arg;
    uint8_t data[16];
    int size = 0;
    for (int i = 0; i < n_values && size < (int)sizeof(data); i++) {
        uint16_t val = (uint16_t)strtol(argv[arg + i], NULL, 0);
        data[size++] = (uint8_t)(val & 0xFF);
        if (width == 2) {
            data[size++] = (uint8_t)((val >> 8) & 0xFF);
        }
    }

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(
        hi2c, (dev_addr << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
        data, (uint16_t)size, 10
    );

    cdc_print((status == HAL_OK) ? "OK" ENDL : "ERROR" ENDL);
}

// i2cget [b|w] <bus> <device> [register] [count]
static void priv__cmd_i2cget(int argc, const char *const *argv) {
    if (argc < 3) {
        cdc_print("usage: i2cget [b|w] <bus> <device> [register] [count]" ENDL);
        return;
    }

    // optional width flag as first arg (default: byte)
    int arg = 1;
    int width = 1;
    if (argv[arg][0] == 'b' && argv[arg][1] == '\0') {
        arg++;
    } else if (argv[arg][0] == 'w' && argv[arg][1] == '\0') {
        width = 2;
        arg++;
    }

    if (argc - arg < 2) {
        cdc_print("usage: i2cget [b|w] <bus> <device> [register] [count]" ENDL);
        return;
    }

    I2C_HandleTypeDef *hi2c = priv__i2c_bus_from_arg(argv[arg]);
    if (NULL == hi2c) {
        cdc_print("invalid bus (1-3)" ENDL);
        return;
    }
    arg++;

    uint8_t dev_addr = (uint8_t)strtol(argv[arg++], NULL, 0);

    // no register specified: read one unit directly from device
    if (arg >= argc) {
        uint8_t data[2];
        HAL_StatusTypeDef status = HAL_I2C_Master_Receive(
            hi2c, (dev_addr << 1), data, (uint16_t)width, 10
        );
        if (status == HAL_OK) {
            char buf[16];
            if (width == 2) {
                uint16_t word = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
                snprintf(buf, sizeof(buf), "0x%04x" ENDL, word);
            } else {
                snprintf(buf, sizeof(buf), "0x%02x" ENDL, data[0]);
            }
            cdc_print(buf);
        } else {
            cdc_print("ERROR" ENDL);
        }
        return;
    }

    uint8_t reg_addr = (uint8_t)strtol(argv[arg++], NULL, 0);
    int count = 1;
    if (arg < argc) {
        count = (int)strtol(argv[arg], NULL, 0);
        if (count < 1) count = 1;
        if (count > 16) count = 16;
    }

    uint8_t data[32];
    uint16_t size = (uint16_t)(count * width);
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        hi2c, (dev_addr << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
        data, size, 10
    );

    if (status == HAL_OK) {
        for (int i = 0; i < count; i++) {
            char buf[16];
            if (width == 2) {
                uint16_t word = (uint16_t)data[i * 2] | ((uint16_t)data[i * 2 + 1] << 8);
                snprintf(buf, sizeof(buf), "0x%04x ", word);
            } else {
                snprintf(buf, sizeof(buf), "0x%02x ", data[i]);
            }
            cdc_print(buf);
        }
        cdc_print(ENDL);
    } else {
        cdc_print("ERROR" ENDL);
    }
}

/// @brief relinquish control of arribada module's nrest line so stlink can program it.
/// @param argc 
/// @param argv 
static void priv__cmd_program_argos(int argc, const char *const *argv) {
    GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = SAT_NRST_GPIO_Output_Pin,
        .Mode = GPIO_MODE_ANALOG,
        .Pull = GPIO_PULLUP,
    };
    HAL_GPIO_Init(SAT_NRST_GPIO_Output_GPIO_Port, &GPIO_InitStruct);
}


static void priv__cmd_restart(int argc, const char *const *argv) {
    NVIC_SystemReset();
}

/// @brief shutdown tag
/// @param argc argument_count
/// @param argv argument_values
/// @note formerly known as "Sleep" in v2.x
static void priv__cmd_shutdown(int argc, const char *const *argv) {
    bms_disable_FETs();
    HAL_PWREx_EnterSHUTDOWNMode(); // tag shutdown if powered by
}

/// @brief restart the tag in STM32 bootloader
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_update(int argc, const char *const *argv) {
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

/// @brief enable/disable vhf pinger
/// @param argc argument_count
/// @param argv argument_values
static void priv__cmd_vhf_pinger(int argc, const char *const *argv) {
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
}


[[maybe_unused]] 
static void priv__not_implemented(int argc, const char *const *argv) {
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
    
    // needed for cdc command
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
