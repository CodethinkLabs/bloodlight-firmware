#!/bin/bash

declare DEVICE="$1"

./tools/bl acq-abort "$DEVICE"
sleep 1
./tools/bl acq-setup "$DEVICE" 2048 4 0x07 16 1 1 1
sleep 1
./tools/bl acq-start "$DEVICE"
