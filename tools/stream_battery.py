#!/usr/bin/env python3
"""
Stream battery data from a CETI WhaleTag over USB vendor bulk interface.

Subscribes to the battery sensor stream, decodes CetiBatterySample packets,
and prints CSV lines matching the format of log_battery.c's __sample_to_csv().

Press Ctrl+C to unsubscribe and exit.

Requirements:
    pip install pyusb

On Windows the device uses WinUSB (via MS OS 2.0 descriptors).
On Linux you may need a udev rule or run as root.
"""

import struct
import signal
import sys
import time

import usb.core
import usb.util

# --- USB identifiers --------------------------------------------------------
VID = 0xCE71
PID = 0x4013  # (0x4000 | CDC=1 | MSC=2 | VENDOR=16)

# --- Stream protocol constants (from src/usb/stream.h) ----------------------
STREAM_SYNC_BYTE       = 0xCE
STREAM_CMD_SUBSCRIBE   = 0x01
STREAM_CMD_UNSUBSCRIBE = 0x02
STREAM_SENSOR_BATTERY  = 0x02

STREAM_HEADER_SIZE = 4  # sync(1) + id(1) + len(2)

# --- CetiBatterySample struct layout (ARM Cortex-M33, GCC, natural alignment)
# Assuming 64-bit time_t (matches %lld format in firmware).
# If your toolchain uses 32-bit time_t, change 'q' to 'i' and adjust padding.
#
#   int64_t  time_us                 (8)
#   uint32_t error                   (4)
#   4 bytes padding
#   double   cell_voltage_v[2]      (16)
#   double   cell_temperature_c[2]  (16)
#   double   current_mA              (8)
#   double   state_of_charge_pct     (8)
#   uint16_t status                  (2)
#   uint16_t protection_alert        (2)
#   4 bytes padding
#   Total: 72 bytes
BATTERY_STRUCT_FMT = "<qI4x2d2ddd2H4x"
BATTERY_STRUCT_SIZE = struct.calcsize(BATTERY_STRUCT_FMT)

# --- Status / protection-alert flag decoders (mirrors log_battery.c) ---------
_STATUS_FLAGS = [
    (15, "PA"),
    (1,  "POR"),
    (2,  "Imn"),
    (6,  "Imx"),
    (8,  "Vmn"),
    (12, "Vmx"),
    (9,  "Tmn"),
    (13, "Tmx"),
    (10, "Smn"),
    (14, "Smx"),
]

_PROT_ALERT_FLAGS = [
    (15, "ChgWDT"),
    (14, "TooHotC"),
    (13, "Full"),
    (12, "TooColdC"),
    (11, "OVP"),
    (10, "OCCP"),
    (9,  "Qovflw"),
    (8,  "PrepF"),
    (7,  "Imbalance"),
    (6,  "PermFail"),
    (5,  "DieHot"),
    (4,  "TooHotD"),
    (3,  "UVP"),
    (2,  "ODCP"),
    (1,  "ResDFault"),
    (0,  "LDet"),
]


def _flags_to_str(raw, flag_table):
    parts = [name for bit, name in flag_table if raw & (1 << bit)]
    return " | ".join(parts)


def _status_to_str(raw):
    return _flags_to_str(raw & 0xF7C6, _STATUS_FLAGS)


def _prot_alert_to_str(raw):
    return _flags_to_str(raw, _PROT_ALERT_FLAGS)


# --- CSV output (mirrors __sample_to_csv in log_battery.c) -------------------
CSV_HEADER = (
    "Timestamp [us]"
    ", Notes"
    ", Battery V1 [V]"
    ", Battery V2 [V]"
    ", Battery I [mA]"
    ", Battery T1 [C]"
    ", Battery T2 [C]"
    ", State of Charge [%]"
    ", Status"
    ", Protection Alerts"
)


def sample_to_csv(payload):
    fields = struct.unpack(BATTERY_STRUCT_FMT, payload)
    time_us        = fields[0]
    # error        = fields[1]  # reserved for notes
    v1             = fields[2]
    v2             = fields[3]
    t1             = fields[4]
    t2             = fields[5]
    current        = fields[6]
    soc            = fields[7]
    status         = fields[8]
    prot_alert     = fields[9]

    return (
        f"{time_us}"
        f", "  # Notes placeholder
        f", {v1:.3f}"
        f", {v2:.3f}"
        f", {current:.3f}"
        f", {t1:.3f}"
        f", {t2:.3f}"
        f", {soc:.3f}"
        f", {_status_to_str(status)}"
        f", {_prot_alert_to_str(prot_alert)}"
    )


# --- Packet parser -----------------------------------------------------------
class StreamParser:
    """Incrementally parse stream packets from a byte stream."""

    def __init__(self):
        self._buf = bytearray()

    def feed(self, data):
        """Feed raw bytes from the USB bulk endpoint. Yields (sensor_id, payload) tuples."""
        self._buf.extend(data)
        while True:
            # Find sync byte
            try:
                idx = self._buf.index(STREAM_SYNC_BYTE)
            except ValueError:
                self._buf.clear()
                return
            if idx > 0:
                del self._buf[:idx]

            # Need at least header
            if len(self._buf) < STREAM_HEADER_SIZE:
                return

            sensor_id = self._buf[1]
            payload_len = self._buf[2] | (self._buf[3] << 8)
            total = STREAM_HEADER_SIZE + payload_len + 1  # +1 for checksum

            if len(self._buf) < total:
                return

            packet = bytes(self._buf[:total])
            del self._buf[:total]

            # Verify checksum (XOR of header + payload)
            checksum = 0
            for b in packet[:-1]:
                checksum ^= b
            if checksum != packet[-1]:
                continue  # bad checksum, skip

            payload = packet[STREAM_HEADER_SIZE:STREAM_HEADER_SIZE + payload_len]
            yield (sensor_id, payload)


# --- Main --------------------------------------------------------------------
def main():
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        print(f"Error: CETI WhaleTag not found (VID=0x{VID:04X} PID=0x{PID:04X})", file=sys.stderr)
        sys.exit(1)

    dev.set_configuration()

    # Find vendor interface endpoints
    cfg = dev.get_active_configuration()
    intf = None
    for i in cfg:
        if i.bInterfaceClass == 0xFF:  # vendor class
            intf = i
            break
    if intf is None:
        print("Error: vendor interface not found", file=sys.stderr)
        sys.exit(1)

    ep_out = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
    ep_in  = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)

    if ep_out is None or ep_in is None:
        print("Error: could not find vendor bulk endpoints", file=sys.stderr)
        sys.exit(1)

    def cleanup(*_):
        """Unsubscribe and exit."""
        try:
            ep_out.write(bytes([STREAM_CMD_UNSUBSCRIBE, STREAM_SENSOR_BATTERY]))
        except Exception:
            pass
        usb.util.dispose_resources(dev)
        print("\nUnsubscribed.", file=sys.stderr)
        sys.exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    # Subscribe to battery stream
    ep_out.write(bytes([STREAM_CMD_SUBSCRIBE, STREAM_SENSOR_BATTERY]))
    print(CSV_HEADER)

    parser = StreamParser()
    while True:
        try:
            data = ep_in.read(ep_in.wMaxPacketSize, timeout=1000)
        except usb.core.USBTimeoutError:
            continue
        except usb.core.USBError as e:
            print(f"USB error: {e}", file=sys.stderr)
            break

        for sensor_id, payload in parser.feed(data):
            if sensor_id != STREAM_SENSOR_BATTERY:
                continue
            if len(payload) != BATTERY_STRUCT_SIZE:
                print(f"Warning: unexpected battery payload size {len(payload)} (expected {BATTERY_STRUCT_SIZE})", file=sys.stderr)
                continue
            print(sample_to_csv(payload))


if __name__ == "__main__":
    main()
