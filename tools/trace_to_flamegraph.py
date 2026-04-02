#!/usr/bin/env python3
"""Convert a profile.bin trace from the STM32 profiler into a speedscope JSON
file that can be visualized at https://www.speedscope.app.

Usage:
    python trace_to_flamegraph.py <elf_file> <trace_file> [-o output.json]

The trace file contains packed 9-byte records:
    uint32_t timestamp   (DWT cycle count)
    uint32_t func_addr   (function address)
    uint8_t  event       (0 = enter, 1 = exit)

Function addresses are resolved to symbol names via arm-none-eabi-nm.
"""

import argparse
import json
import struct
import subprocess
import sys
from pathlib import Path

RECORD_FMT = "<IIB"  # little-endian: uint32, uint32, uint8
RECORD_SIZE = struct.calcsize(RECORD_FMT)

EVENT_ENTER = 0
EVENT_EXIT = 1


def load_symbols(elf_path):
    """Run arm-none-eabi-nm to build an address -> name lookup table."""
    try:
        result = subprocess.run(
            ["arm-none-eabi-nm", "-nC", str(elf_path)],
            capture_output=True, text=True, check=True,
        )
    except FileNotFoundError:
        print("Error: arm-none-eabi-nm not found. Is the ARM toolchain on your PATH?",
              file=sys.stderr)
        sys.exit(1)

    symbols = []
    for line in result.stdout.splitlines():
        parts = line.split(None, 2)
        if len(parts) < 3:
            continue
        addr_str, _sym_type, name = parts
        try:
            addr = int(addr_str, 16)
        except ValueError:
            continue
        symbols.append((addr, name))

    symbols.sort(key=lambda s: s[0])
    return symbols


def resolve_address(symbols, addr):
    """Binary search for the symbol whose start address is <= addr."""
    lo, hi = 0, len(symbols) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if symbols[mid][0] <= addr:
            lo = mid + 1
        else:
            hi = mid - 1
    if hi < 0:
        return f"0x{addr:08x}"
    return symbols[hi][1]


def read_trace(trace_path):
    """Read binary trace file into a list of (timestamp, func_addr, event)."""
    data = Path(trace_path).read_bytes()
    if len(data) % RECORD_SIZE != 0:
        trailing = len(data) % RECORD_SIZE
        print(f"Warning: {trailing} trailing bytes in trace file (truncated)",
              file=sys.stderr)
        data = data[:len(data) - trailing]

    # Unwrap 32-bit DWT cycle counter into 64-bit monotonic timestamps.
    # A real wrap crosses the full 32-bit range (~26.8s at 160MHz), so the
    # backward jump is large (> 2^31).  Small backward jumps are just ISR
    # reordering — keep the previous timestamp for those to stay monotonic.
    WRAP_THRESHOLD = 1 << 31  # half the 32-bit range
    records = []
    wrap_offset = 0
    prev_raw = 0
    prev_ts = 0
    for i in range(0, len(data), RECORD_SIZE):
        raw_ts, func_addr, event = struct.unpack_from(RECORD_FMT, data, i)
        if raw_ts < prev_raw:
            backward = prev_raw - raw_ts
            if backward >= WRAP_THRESHOLD:
                # genuine 32-bit counter wrap
                wrap_offset += 1 << 32
            else:
                # small backward jump from ISR reordering — clamp to previous
                raw_ts = prev_raw
        prev_raw = raw_ts
        ts = raw_ts + wrap_offset
        if ts < prev_ts:
            ts = prev_ts  # guarantee monotonic output
        prev_ts = ts
        records.append((ts, func_addr, event))
    return records


def build_speedscope(records, symbols, cpu_freq_hz):
    """Convert trace records into speedscope's evented profile format."""
    # Collect unique frames
    frame_map = {}  # func_addr -> index
    frames = []

    for _, func_addr, _ in records:
        if func_addr not in frame_map:
            frame_map[func_addr] = len(frames)
            frames.append({"name": resolve_address(symbols, func_addr)})

    # Build events list, tracking the open stack so we can balance it
    events = []
    stack = []  # stack of func_addrs currently open

    # Strip leading exit events that have no matching enter (trace started mid-call)
    first_enter = 0
    for i, (_, _, event) in enumerate(records):
        if event == EVENT_ENTER:
            first_enter = i
            break
    if first_enter > 0:
        print(f"  Skipping {first_enter} leading exit events (trace started mid-call)",
              file=sys.stderr)
        records = records[first_enter:]

    for timestamp, func_addr, event in records:
        if event == EVENT_ENTER:
            # Close any orphaned frames from the current stack level and above
            # that never got an exit event — attribute their time up to now
            if stack and func_addr not in stack:
                # New call at same or higher level: close frames that should
                # have exited already (their exit was lost)
                pass  # normal case, stack is consistent
            elif func_addr in stack:
                # Re-entering a function that's already on the stack means
                # everything above (and including) it missed their exits
                while stack and stack[-1] != func_addr:
                    orphan = stack.pop()
                    events.append({
                        "type": "C",
                        "frame": frame_map[orphan],
                        "at": timestamp,
                    })
                # Close the stale entry for this function too
                stack.pop()
                events.append({
                    "type": "C",
                    "frame": frame_map[func_addr],
                    "at": timestamp,
                })

            stack.append(func_addr)
            events.append({
                "type": "O",
                "frame": frame_map[func_addr],
                "at": timestamp,
            })
        else:
            # Only emit close if this function is actually on the stack
            if func_addr in stack:
                # Close any mismatched frames above it (e.g. missed exits)
                while stack and stack[-1] != func_addr:
                    orphan = stack.pop()
                    events.append({
                        "type": "C",
                        "frame": frame_map[orphan],
                        "at": timestamp,
                    })
                stack.pop()
                events.append({
                    "type": "C",
                    "frame": frame_map[func_addr],
                    "at": timestamp,
                })

    # Close any frames still open at end of trace
    end_time = records[-1][0] if records else 0
    while stack:
        func_addr = stack.pop()
        events.append({
            "type": "C",
            "frame": frame_map[func_addr],
            "at": end_time,
        })

    # Determine time range
    start_value = records[0][0] if records else 0
    end_value = end_time

    profile = {
        "type": "evented",
        "name": "STM32 Trace",
        "unit": "none",
        "startValue": start_value,
        "endValue": end_value,
        "events": events,
    }

    return {
        "$schema": "https://www.speedscope.app/file-format-schema.json",
        "shared": {"frames": frames},
        "profiles": [profile],
        "name": "STM32 Profile",
        "exporter": "trace_to_flamegraph.py",
    }


def main():
    parser = argparse.ArgumentParser(
        description="Convert STM32 profile.bin trace to speedscope JSON",
    )
    parser.add_argument("elf", help="Path to the firmware .elf file")
    parser.add_argument("trace", help="Path to the profile.bin trace file")
    parser.add_argument("-o", "--output", default="profile.speedscope.json",
                        help="Output JSON file (default: profile.speedscope.json)")
    parser.add_argument("--cpu-freq", type=int, default=160_000_000,
                        help="CPU frequency in Hz (default: 160000000)")
    args = parser.parse_args()

    print(f"Loading symbols from {args.elf}...")
    symbols = load_symbols(args.elf)
    print(f"  {len(symbols)} symbols loaded")

    print(f"Reading trace from {args.trace}...")
    records = read_trace(args.trace)
    print(f"  {len(records)} events read")

    if not records:
        print("Error: no trace records found", file=sys.stderr)
        sys.exit(1)

    speedscope = build_speedscope(records, symbols, args.cpu_freq)

    Path(args.output).write_text(json.dumps(speedscope))
    print(f"Written {args.output} — open at https://www.speedscope.app")


if __name__ == "__main__":
    main()
