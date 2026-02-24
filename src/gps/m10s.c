#include "m10s.h"

#include <string.h>

/* uBX communication protocol */
typedef struct {
    uint8_t a;
    uint8_t b;
} UbxChecksum;

typedef enum {
    UbxClassCfg = 0x06,
} UbxClass;

typedef enum {
    UbxCfgIdRst = 0x04,
    UbxCfgIdOtp = 0x41,
    UbxCfgIdValSet = 0x8a,
    UbxCfgIdValDel = 0x8c,
    UbxCfgIdValGet = 0x8d,
} UbxCfgId;

typedef struct {
    uint8_t class;
    uint8_t id;
    uint16_t length;
    struct{
        uint8_t *ptr;
        uint16_t length;
    } payload;
    UbxChecksum checksum;
}UbxMessage;

UbxChecksum ubx_calculate_checksum(const uint8_t *message, uint16_t msg_length) {
    UbxChecksum cs = {.a = 0, .b = 0};
    for(int i = 0; i < msg_length; i++) {
        cs.a += message[i];
        cs.b += cs.a;
    }
    return cs;
}

size_t m10s_create_ubx_frame(uint8_t *buffer, size_t buffer_length,  uint8_t class, uint8_t id, const uint8_t * payload, uint16_t payload_length) {
    if (NULL == buffer) {
        return 0;
    }
    if (buffer_length < payload_length + 8) {
        return 0;
    }  

    buffer[0] = 0xb5;
    buffer[1] = 0x62;
    buffer[2] = class;
    buffer[3] = id;
    buffer[4] = (uint8_t)(payload_length & 0xff);
    buffer[5] = (uint8_t)((payload_length >> 8) & 0xff);
    memcpy(&buffer[6], payload, payload_length);
    UbxChecksum checksum = ubx_calculate_checksum(&buffer[2], payload_length + 4);
    buffer[payload_length + 6] = checksum.a;
    buffer[payload_length + 6 + 1] = checksum.b;
    return 8 + payload_length;
}

#define M10S_LAYER_RAM  (1 << 0)
#define M10S_LAYER_BBR  (1 << 1)
#define M10S_LAYER_FLASH  (1 << 2)

size_t m10s_ubx_cfg_set_msg_rate(uint8_t *buffer, size_t buffer_length, uint8_t data_rate_s) {
    uint8_t payload[] = {
        0x00, // version
        M10S_LAYER_RAM | M10S_LAYER_BBR, //layers
        0x00, 0x00, // reserved0
        //GGA
        //GGL
        //GSA
        //GSV
        //VTG
        0xbb, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_GGA_UART1
        0xca, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_GLL_UART1
        0xc0, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_GSA_UART1
        0xc5, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_GSV_UART1
        0xac, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_RMC_UART1
        0xb1, 0x00, 0x91, 0x20, (uint8_t)(data_rate_s & 0xff), // NMEA_ID_VTG_UART1
        // 0x02, 0x00, 0x21, 0x30, (uint8_t)(data_rate_s & 0xff), (uint8_t)((data_rate_s >> 8) & 0xff), // CFG-RATE-NAV
        // 0x02, 0x00, 0xd0, 0x40, (uint8_t)(data_rate_s & 0xff), (uint8_t)((data_rate_s >> 8) & 0xff), (uint8_t)((data_rate_s >> 16) & 0xff), (uint8_t)((data_rate_s >> 24) & 0xff), // CFG-PM-POSUPDATEPERIOD
        // 0x40, 0xd0, 0x00, 0x02, (uint8_t)(data_rate_s & 0xff), (uint8_t)((data_rate_s >> 8) & 0xff), (uint8_t)((data_rate_s >> 16) & 0xff), (uint8_t)((data_rate_s >> 24) & 0xff), // CFG-PM-POSUPDATEPERIOD
        // 0x01, 0xd0, 0xd0, 0x20, operate_mode, // CFG-PM-OPERATEMODE: PSMOO
        //0x20, 0xd0, 0x00, 0x01, operate_mode, // CFG-PM-OPERATEMODE: PSMOO
    };
    return m10s_create_ubx_frame(buffer, buffer_length, UbxClassCfg, UbxCfgIdValSet, payload, sizeof(payload));
}

size_t m10s_disable_i2c_output(uint8_t *buffer, size_t buffer_length) {
        uint8_t payload[] = {
        0x00, // version
        M10S_LAYER_RAM | M10S_LAYER_BBR, //layers
        0x00, 0x00, // reserved0
        0xba, 0x00, 0x91, 0x20, 0, // NMEA_ID_GGA_I2C
        0xc9, 0x00, 0x91, 0x20, 0, // NMEA_ID_GLL_I2C
        0xbf, 0x00, 0x91, 0x20, 0, // NMEA_ID_GSA_I2C
        0xc4, 0x00, 0x91, 0x20, 0, // NMEA_ID_GSV_I2C
        0xab, 0x00, 0x91, 0x20, 0, // NMEA_ID_RMC_I2C
        0xb0, 0x00, 0x91, 0x20, 0, // NMEA_ID_VTG_I2C
    };
    return m10s_create_ubx_frame(buffer, buffer_length, UbxClassCfg, UbxCfgIdValSet, payload, sizeof(payload));
}

size_t m10s_disable_spi_output(uint8_t *buffer, size_t buffer_length) {
        uint8_t payload[] = {
        0x00, // version
        M10S_LAYER_RAM | M10S_LAYER_BBR, //layers
        0x00, 0x00, // reserved0
        0xbe, 0x00, 0x91, 0x20, 0, // NMEA_ID_GGA_SPI
        0xcd, 0x00, 0x91, 0x20, 0, // NMEA_ID_GLL_SPI
        0xc3, 0x00, 0x91, 0x20, 0, // NMEA_ID_GSA_SPI
        0xc8, 0x00, 0x91, 0x20, 0, // NMEA_ID_GSV_SPI
        0xaf, 0x00, 0x91, 0x20, 0, // NMEA_ID_RMC_SPI
        0xb4, 0x00, 0x91, 0x20, 0, // NMEA_ID_VTG_SPI
    };
    return m10s_create_ubx_frame(buffer, buffer_length, UbxClassCfg, UbxCfgIdValSet, payload, sizeof(payload));
}

#define PMREQ_FLAGS_BACKUP (1 << 1)
#define PMREQ_FLAGS_FORCE  (1 << 2)

#define PMREQ_WAKEUPSOURCE_UART_RX   (1 << 3)
#define PMREQ_WAKEUPSOURCE_EXT_INT_0 (1 << 5)
#define PMREQ_WAKEUPSOURCE_EXT_INT_1 (1 << 6)
#define PMREQ_WAKEUPSOURCE_SPI_CS    (1 << 7)

size_t m10s_enter_software_standby_mode(uint8_t buffer[static 24], size_t buffer_length) {
    uint8_t payload[] = {
        0x00, // version,
        0x00, 0x00, 0x00, //reserved0
        0x00, 0x00, 0x00, 0x00, // duration ms //wait for signal
        PMREQ_FLAGS_BACKUP | PMREQ_FLAGS_FORCE, 0x00, 0x00, 0x00, // flags | u1:b1 (backup), u1:b2 (force)
        PMREQ_WAKEUPSOURCE_UART_RX | PMREQ_WAKEUPSOURCE_EXT_INT_0 | PMREQ_WAKEUPSOURCE_EXT_INT_1, 0x00, 0x00, 0x00, // wakeupSources | u1:b3 (uart_rx) | u1:b5 (ext_int_0) | u1:b6 (ext_int_1) | u1:b7 (spi_cs)
    };
    return m10s_create_ubx_frame(buffer, buffer_length, 0x2, 0x41, payload, sizeof(payload));
}

