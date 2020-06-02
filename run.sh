#!/bin/bash

# Example: `sudo ./run.sh /dev/ttyACM1`

declare DEVICE="$1"

./tools/bl acq-abort "$DEVICE"
sleep 1

# 50 Hz acquisition of one channel (the first), with gains of 16, 1, 1 and 1:
#
# Sample every 1250us
# With 4 bit oversample (2^4=16 actual samples per recorded sample)
# (us per sec ) / ((oversamples) * period)
# (1000 * 1000) / ((2^4        ) * 1250  ) = 50
./tools/bl acq-setup "$DEVICE" 1250 4 0x01 16 1 1 1
sleep 2

./tools/bl acq-start "$DEVICE"
