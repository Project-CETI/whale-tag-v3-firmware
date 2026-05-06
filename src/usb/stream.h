/*****************************************************************************
 *   @file      usb/stream.h
 *   @brief     USB sensor data streaming manager
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_USB_STREAM_H
#define CETI_USB_STREAM_H

#include <stdint.h>
#include <stddef.h>

#include "audio/acq_audio.h"

typedef struct {
    uint8_t sync_symbol;
    uint8_t packet_id;
    uint16_t packet_len;
} StreamPacketHeader;

// Sensor IDs used in the streaming protocol
typedef enum {
    STREAM_SENSOR_PRESSURE = 1,
    STREAM_SENSOR_BATTERY = 2,
    STREAM_SENSOR_IMU_ACCEL = 3,
    STREAM_SENSOR_IMU_GYRO = 4,
    STREAM_SENSOR_IMU_MAG = 5,
    STREAM_SENSOR_IMU_QUAT = 6,
    STREAM_SENSOR_ECG = 7,
    STREAM_SENSOR_GPS = 8,
    STREAM_SENSOR_AUDIO = 9,
} StreamSensorId;

#define STREAM_SENSOR_COUNT 9

// Packet framing constants
#define STREAM_SYNC_BYTE 0xCE

// Subscription commands (host -> device via vendor OUT)
#define STREAM_CMD_SUBSCRIBE 0x01
#define STREAM_CMD_UNSUBSCRIBE 0x02
#define STREAM_CMD_LIST 0x03
#define STREAM_SENSOR_ALL 0xFF

void stream_init(void);
void stream_task(void);
void stream_subscribe(StreamSensorId sensor);
void stream_unsubscribe(StreamSensorId sensor);
void stream_unsubscribe_all(void);
uint32_t stream_get_subscriptions(void);

// Push a framed packet into the stream ring buffer.
// Called from sensor callbacks (may be ISR context).
// Returns 0 on success, -1 if ring buffer is full (packet dropped).
int stream_push_packet(StreamSensorId sensor_id,
                       const void *payload, uint16_t payload_len);

#endif // CETI_USB_STREAM_H
