#!/bin/bash

# Example: `sudo ./run.sh /dev/ttyACM1`

declare DEVICE="$1"

# Example acquisition parameters:
# - 50 Hz
# - 4-bit oversample
# - Enabling source #1 only
# - Photodiode gains of 16, 1, 1, and 1
./tools/bl start "$DEVICE" 50 4 0x01 16 1 1 1
