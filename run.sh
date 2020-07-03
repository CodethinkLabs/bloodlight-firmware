#!/bin/bash

# Example: `sudo ./run.sh /dev/ttyACM1`

declare DEVICE="$1"

# Example acquisition:

# Turn on:
# - the green LED    (1 << 14)
# - an infra-red LED (1 <<  2)
./tools/bl led        "$DEVICE" 0x4004

# Accumulate 512 readings per sample
./tools/bl oversample "$DEVICE" 512

# 16x gain for photodiode 1, and the rest without gains
./tools/bl gains      "$DEVICE" 16 1 1 1

# Channel offsets:
# - Photodiode 1:  280000
# - Photodiode 2:  0
# - Photodiode 3:  100000
# - Photodiode 4:  0
# - 3.3 volt line: 240000
# - 5.0 volt line: 240000
# - Temperature:   180000
./tools/bl offset     "$DEVICE" 1250000 0 500000 0 900000 900000 840000

# Start an acquisition at 50 Hz with source mask 0x77 (enabling channels
# 1, 2, 3, 5, 6, and 7).
./tools/bl start      "$DEVICE" 50 0x77
