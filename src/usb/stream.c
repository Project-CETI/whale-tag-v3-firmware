/*****************************************************************************
 *   @file      usb/stream.c
 *   @brief     USB sensor data streaming manager
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "stream.h"
#include "tusb.h"

#include "gps/gps.h"
#include "audio/acq_audio.h"
#include "pressure/acq_pressure.h"
#include "battery/acq_battery.h"

#include <string.h>

// Ring buffer size — 64 KB to handle audio bursts
#define STREAM_RINGBUF_SIZE (64 * 1024)

#define STREAM_HEADER_SIZE sizeof(StreamPacketHeader)
#define STREAM_PACKET_OVERHEAD sizeof(StreamPacketHeader) + 1

static uint8_t s_ringbuf[STREAM_RINGBUF_SIZE];
static volatile uint32_t s_ring_head; // write position (producer: ISR/callbacks)
static volatile uint32_t s_ring_tail; // read position (consumer: stream_task)

static volatile uint32_t s_subscriptions; // bitmask of subscribed sensors

/* sensor specific callbacks */
#define AUDIO_STREAM_BUFFER_SIZE_BLOCKS (2)
#define AUDIO_STREAM_BLOCK_SIZE_MAX (STREAM_RINGBUF_SIZE/2)
#define AUDIO_STREAM_BLOCK_SIZE ((AUDIO_STREAM_BLOCK_SIZE_MAX) - ((AUDIO_STREAM_BLOCK_SIZE_MAX) % AUDIO_SAMPLE_SIZE_LSM))
uint8_t audio_stream_buffer[AUDIO_STREAM_BUFFER_SIZE_BLOCKS][AUDIO_STREAM_BLOCK_SIZE];

static void __stream_audio_block_complete_callback(uint16_t block) {
    stream_push_packet(STREAM_SENSOR_AUDIO, audio_stream_buffer[block], AUDIO_STREAM_BLOCK_SIZE);
}

static void __stream_pressure_push_sample(const CetiPressureSample *p_sample) {
    stream_push_packet(STREAM_SENSOR_PRESSURE, p_sample, sizeof(CetiPressureSample));
}

static void __stream_battery_push_sample(const CetiBatterySample *p_sample) {
    stream_push_packet(STREAM_SENSOR_BATTERY, p_sample, sizeof(CetiBatterySample));
}

static void __stream_gps_push_sample(const uint8_t *p_msg, uint16_t len) {
    stream_push_packet(STREAM_SENSOR_GPS, p_msg, len);
}

static uint32_t ring_used(void) {
    uint32_t h = s_ring_head;
    uint32_t t = s_ring_tail;
    return (h >= t) ? (h - t) : (STREAM_RINGBUF_SIZE - t + h);
}

static uint32_t ring_free(void) {
    // Reserve 1 byte to distinguish full from empty
    return STREAM_RINGBUF_SIZE - 1 - ring_used();
}

// Write bytes into ring buffer. Caller must ensure enough space.
static void ring_write(const void *data, uint32_t len) {
    const  uint8_t *data_bytes = (const uint8_t *)data;
    uint32_t h = s_ring_head;
    for (uint32_t i = 0; i < len; i++) {
        s_ringbuf[h] = data_bytes[i];
        h = (h + 1) % STREAM_RINGBUF_SIZE;
    }
    s_ring_head = h;
}

// Read bytes from ring buffer into dst. Caller must ensure enough data.
static void ring_read(uint8_t *dst, uint32_t len) {
    uint32_t t = s_ring_tail;
    for (uint32_t i = 0; i < len; i++) {
        dst[i] = s_ringbuf[t];
        t = (t + 1) % STREAM_RINGBUF_SIZE;
    }
    s_ring_tail = t;
}

void stream_init(void) {
    s_ring_head = 0;
    s_ring_tail = 0;
    s_subscriptions = 0;
}

void stream_subscribe(StreamSensorId sensor) {
    // special case of all
    if (sensor == STREAM_SENSOR_ALL) {
        for (int i = 0; i < STREAM_SENSOR_COUNT; i++) {
            stream_subscribe(i + 1);
        }
        return;
    } 
    
    // check input range
    if ((sensor < 1) || (sensor > STREAM_SENSOR_COUNT)) {
        return;
    }

    // check sensor isn't already streaming
    if (s_subscriptions & (1 << sensor)) {
        return;
    }

    s_subscriptions |= (1 << sensor);
    switch(sensor) {
        case STREAM_SENSOR_AUDIO:
            acq_audio_register_block_complete_callback(__stream_audio_block_complete_callback);
            acq_audio_start(&audio_stream_buffer[0][0], AUDIO_STREAM_BUFFER_SIZE_BLOCKS, AUDIO_STREAM_BLOCK_SIZE);
            break;

        case STREAM_SENSOR_PRESSURE:
            acq_pressure_register_sample_callback(__stream_pressure_push_sample);
            acq_pressure_start();
            break;

        case STREAM_SENSOR_BATTERY:
            acq_battery_register_callback(__stream_battery_push_sample);
            acq_battery_start();
            break;

        case STREAM_SENSOR_GPS:
            gps_register_bytes_received_callback(__stream_gps_push_sample);
            gps_wake();
            gps_high_data_rate();
            break;
            
        case STREAM_SENSOR_IMU_ACCEL:
            break;

        case STREAM_SENSOR_IMU_MAG:
            break;
        
        case STREAM_SENSOR_IMU_GYRO:
            break;

        case STREAM_SENSOR_IMU_QUAT:
            break;

        case STREAM_SENSOR_ECG:
            break;
        default:
            break;
    }
}

void stream_unsubscribe(StreamSensorId sensor) {
    // special case of all
    if (sensor == STREAM_SENSOR_ALL) {
        for (int i = 0; i < STREAM_SENSOR_COUNT; i++) {
            stream_unsubscribe(i + 1);
        }
        return;
    } 
    
    // check input range
    if ((sensor < 1) || (sensor > STREAM_SENSOR_COUNT)) {
        return;
    }


    s_subscriptions &= ~(1u << sensor);
    switch(sensor) {
        case STREAM_SENSOR_AUDIO:
            acq_audio_stop();
            acq_audio_register_block_complete_callback(NULL);
            break;
            
        case STREAM_SENSOR_PRESSURE:
            acq_pressure_stop();
            acq_pressure_register_sample_callback(NULL);
            break;

        case STREAM_SENSOR_BATTERY:
            acq_battery_stop();
            acq_battery_register_callback(NULL);
            break;

        case STREAM_SENSOR_GPS:
            gps_standby();
            gps_register_bytes_received_callback(NULL);
            break;

        case STREAM_SENSOR_IMU_ACCEL:
            break;

        case STREAM_SENSOR_IMU_MAG:
            break;
        
        case STREAM_SENSOR_IMU_GYRO:
            break;

        case STREAM_SENSOR_IMU_QUAT:
            break;

        case STREAM_SENSOR_ECG:
            break;

        default:
            break;
    }
}

uint32_t stream_get_subscriptions(void) {
    return s_subscriptions;
}

int stream_push_packet(StreamSensorId sensor_id, const void *payload, uint16_t payload_len) {
    // Check subscription
    if (!(s_subscriptions & (1u << sensor_id))) {
        return -1;
    }

    uint32_t total_len = STREAM_PACKET_OVERHEAD + payload_len;

    // Brief critical section to protect ring buffer from concurrent callbacks
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (ring_free() < total_len) {
        __set_PRIMASK(primask);
        return -1; // drop packet (lossy, like UDP)
    }

    // Build header
    StreamPacketHeader header = {
        .sync_symbol = STREAM_SYNC_BYTE,
        .packet_id = sensor_id,
        .packet_len = payload_len,
    };

    // Compute checksum over header + payload
    uint8_t checksum = 0;
    for (int i = 0; i < sizeof(StreamPacketHeader); i++) {
        checksum ^= ((uint8_t *)&header)[i];
    }
    const uint8_t *p = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++) {
        checksum ^= p[i];
    }

    // Write header + payload + checksum to ring buffer
    ring_write(&header, sizeof(StreamPacketHeader));
    ring_write((const uint8_t *)payload, payload_len);
    ring_write(&checksum, 1);

    __set_PRIMASK(primask);
    return 0;
}

void stream_task(void) {
    // Drain ring buffer into vendor bulk IN endpoint
    uint32_t avail = ring_used();
    if (avail == 0) {
        return;
    }

    // Write up to what the vendor endpoint can accept
    uint32_t writable = tud_vendor_write_available();
    if (writable == 0) {
        return;
    }

    uint32_t to_send = (avail < writable) ? avail : writable;

    // Transfer in chunks to avoid large stack buffer
    uint8_t tmp[512];
    while (to_send > 0) {
        uint32_t chunk = (to_send < sizeof(tmp)) ? to_send : sizeof(tmp);
        ring_read(tmp, chunk);
        tud_vendor_write(tmp, chunk);
        to_send -= chunk;
    }

    tud_vendor_write_flush();
}
