#!/usr/bin/env python3
"""Convert a tag_errors.bin file into a human-readable CSV.

Usage:
    python errors_to_csv.py <bin_file> [-e elf_file] [-o output.csv]

Each binary record is a packed ErrorQueueElement (ARM32, 24 bytes):
    uint8_t  frame_header[4]   "CETI"
    uint32_t error             CetiStatus bitfield
    uint32_t func              function pointer
    uint32_t _pad
    uint64_t timestamp         epoch microseconds

If an ELF file is provided, function pointers are resolved to symbol names
via arm-none-eabi-nm.
"""

import argparse
import csv
import datetime
import struct
import subprocess
import sys
from pathlib import Path

FRAME_HEADER = b"CETI"
# ARM32 struct with uint64_t 8-byte alignment: 4 + 4 + 4 + 4(pad) + 8 = 24
RECORD_FMT = "<4sIII Q"
RECORD_SIZE = struct.calcsize(RECORD_FMT)

SUBSYSTEM_NAMES = {
    0:  "NONE",
    1:  "ERROR_QUEUE",
    2:  "LOG_MISSION",
    3:  "SYSLOG",
    4:  "MISSION",
    5:  "METADATA",
    6:  "AUDIO",
    7:  "LOG_AUDIO",
    8:  "GPS",
    9:  "LOG_GPS",
    10: "ARGOS",
    11: "BMS",
    12: "LOG_BMS",
    13: "PRESSURE",
    14: "LOG_PRESSURE",
    15: "IMU",
    16: "LOG_IMU",
    17: "ECG",
    18: "LOG_ECG",
    19: "LED",
    20: "FLASH",
}

DEFAULT_ERROR_NAMES = {
    0: "NONE",
    1: "BUFFER_OVERFLOW",
    2: "OUTDATED_AOP_TABLE",
    3: "SLOW_SD_CARD_ACCESS",
}


def decode_status(status):
    """Decode a CetiStatus uint32 into subsystem, error type, and error code."""
    subsystem = (status >> 24) & 0xFF
    err_type = (status >> 16) & 0xFF
    err_code = status & 0xFFFF
    return subsystem, err_type, err_code


def subsystem_name(code):
    return SUBSYSTEM_NAMES.get(code, f"UNKNOWN({code})")


def error_type_name(code):
    if code == 0:
        return "DEFAULT"
    elif code == 1:
        return "FILEX"
    return f"UNKNOWN({code})"


def error_code_name(err_type, code):
    if err_type == 0:
        return DEFAULT_ERROR_NAMES.get(code, f"0x{code:04X}")
    # FileX error codes are numeric; just show the value
    return f"0x{code:04X}"


def load_symbols(elf_path):
    """Run arm-none-eabi-nm to build an address -> name lookup table."""
    try:
        result = subprocess.run(
            ["arm-none-eabi-nm", "-nC", str(elf_path)],
            capture_output=True, text=True, check=True,
        )
    except FileNotFoundError:
        print("Warning: arm-none-eabi-nm not found. Function addresses will not be resolved.",
              file=sys.stderr)
        return None

    symbols = []
    for line in result.stdout.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3:
            addr, _kind, name = parts
            symbols.append((int(addr, 16), name))
    symbols.sort()
    return symbols


def resolve_address(symbols, addr):
    """Find the closest symbol at or before addr."""
    if symbols is None or addr == 0:
        return f"0x{addr:08X}"
    lo, hi = 0, len(symbols) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if symbols[mid][0] <= addr:
            lo = mid + 1
        else:
            hi = mid - 1
    if hi < 0:
        return f"0x{addr:08X}"
    base_addr, name = symbols[hi]
    offset = addr - base_addr
    if offset == 0:
        return name
    return f"{name}+0x{offset:X}"


def timestamp_to_str(us):
    """Convert epoch microseconds to an ISO-ish datetime string."""
    if us == 0:
        return ""
    try:
        dt = datetime.datetime.fromtimestamp(us / 1_000_000, tz=datetime.timezone.utc)
        return dt.strftime("%Y-%m-%d %H:%M:%S.") + f"{us % 1_000_000:06d}"
    except (OSError, OverflowError, ValueError):
        return str(us)


def main():
    parser = argparse.ArgumentParser(description="Convert tag_errors.bin to CSV")
    parser.add_argument("bin_file", type=Path, help="Path to tag_errors.bin")
    parser.add_argument("-e", "--elf", type=Path, default=None,
                        help="ELF file for resolving function addresses")
    parser.add_argument("-o", "--output", type=Path, default=None,
                        help="Output CSV path (default: stdout)")
    args = parser.parse_args()

    data = args.bin_file.read_bytes()
    if len(data) % RECORD_SIZE != 0:
        print(f"Warning: file size ({len(data)}) is not a multiple of record size ({RECORD_SIZE}). "
              f"Trailing {len(data) % RECORD_SIZE} bytes will be ignored.", file=sys.stderr)

    symbols = load_symbols(args.elf) if args.elf else None

    out_file = open(args.output, "w", newline="") if args.output else sys.stdout
    writer = csv.writer(out_file)
    writer.writerow(["timestamp_utc", "timestamp_us", "subsystem", "error_type", "error_code",
                      "status_raw", "caller"])

    n_records = len(data) // RECORD_SIZE
    for i in range(n_records):
        record = data[i * RECORD_SIZE:(i + 1) * RECORD_SIZE]
        header, status, func, _pad, timestamp = struct.unpack(RECORD_FMT, record)

        if header != FRAME_HEADER:
            print(f"Warning: record {i} has bad header {header!r}, skipping.", file=sys.stderr)
            continue

        subsys, err_type, err_code = decode_status(status)
        writer.writerow([
            timestamp_to_str(timestamp),
            timestamp,
            subsystem_name(subsys),
            error_type_name(err_type),
            error_code_name(err_type, err_code),
            f"0x{status:08X}",
            resolve_address(symbols, func),
        ])

    if args.output:
        out_file.close()
        print(f"Wrote {n_records} records to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
