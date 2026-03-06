#!/bin/bash
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <serial-device>"
    echo "  e.g. $0 /dev/ttyACM0"
    echo "  e.g. $0 COM11"
    exit 1
fi

DEV="$1"

EPOCH=$(date +%s)
echo "Setting tag time to epoch: $EPOCH"
echo "datetime $EPOCH" > "$DEV"
echo "Done."
