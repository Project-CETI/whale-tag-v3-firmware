/*****************************************************************************
 *   @file      usb/vendor.c
 *   @brief     USB vendor bulk interface for sensor data streaming
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "vendor.h"
#include "stream.h"
#include "tusb.h"

#include <string.h>

void usb_vendor_init(void) {
    stream_init();
}

// Process incoming commands from the host on the vendor OUT endpoint.
// Protocol: [command_byte, sensor_id]
//   command: 0x01=SUBSCRIBE, 0x02=UNSUBSCRIBE, 0x03=LIST
//   sensor_id: 1-9 for specific sensor, 0xFF for all
static void vendor_process_rx(void) {
    uint8_t buf[64];
    uint32_t count = tud_vendor_read(buf, sizeof(buf));

    uint32_t i = 0;
    while (i + 1 < count) {
        uint8_t cmd = buf[i];
        uint8_t sensor = buf[i + 1];
        i += 2;

        switch (cmd) {
            case STREAM_CMD_SUBSCRIBE:
                stream_subscribe((StreamSensorId)sensor);
                break;

            case STREAM_CMD_UNSUBSCRIBE:
                stream_unsubscribe((StreamSensorId)sensor);
                break;

            case STREAM_CMD_LIST: {
                // Respond with subscription bitmask
                uint8_t resp[sizeof(StreamPacketHeader) + sizeof(uint32_t) + 1];
                uint32_t mask = stream_get_subscriptions();

                // Build a stream packet with sensor_id=0 (control)
                resp[0] = STREAM_SYNC_BYTE;
                resp[1] = 0x00; // control response
                resp[2] = sizeof(uint32_t); // payload length low
                resp[3] = 0;                // payload length high
                memcpy(&resp[sizeof(StreamPacketHeader)], &mask, sizeof(uint32_t));

                // Checksum
                uint8_t checksum = 0;
                for (size_t j = 0; j < sizeof(StreamPacketHeader) + sizeof(uint32_t); j++) {
                    checksum ^= resp[j];
                }
                resp[sizeof(StreamPacketHeader) + sizeof(uint32_t)] = checksum;

                tud_vendor_write(resp, sizeof(resp));
                tud_vendor_write_flush();
                break;
            }

            default:
                break;
        }
    }
}

// TinyUSB callback when data is received on vendor OUT endpoint
void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint16_t bufsize) {
    (void)itf;
    (void)buffer;
    (void)bufsize;
    // Data is buffered by TinyUSB; we process it in usb_vendor_task()
}

void usb_vendor_task(void) {
#if CFG_TUD_VENDOR
    if (tud_vendor_available()) {
        vendor_process_rx();
    }
    stream_task();
#endif
}
