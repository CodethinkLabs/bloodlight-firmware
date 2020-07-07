#!/bin/bash

# Example: `sudo ./run.sh /dev/ttyACM1`

declare DEVICE="$1"

# Example acquisition:

# Turn on:
# - the green LED    (1 << 14)
# - an infra-red LED (1 <<  2)
./tools/bl led     "$DEVICE" 0x4004

# chancfg <channel> <gain> [offset] [shift] [saturate]
./tools/bl chancfg "$DEVICE" 0 16 1250000 2 1 # Photodiode 1
./tools/bl chancfg "$DEVICE" 1  1       0 2 1 # Photodiode 2
./tools/bl chancfg "$DEVICE" 2  1  500000 2 1 # Photodiode 3
./tools/bl chancfg "$DEVICE" 3  1       0 2 1 # Photodiode 4
./tools/bl chancfg "$DEVICE" 4  1  900000 2 1 # 3.3V
./tools/bl chancfg "$DEVICE" 5  1  900000 2 1 # 5.0V
./tools/bl chancfg "$DEVICE" 6  1  840000 2 1 # Temperature

# Start an acquisition at 50 Hz with 512 samples accumulated and
# source mask 0x7f (enabling all 7 channels).
./tools/bl start  "$DEVICE" 50 512 0x77
