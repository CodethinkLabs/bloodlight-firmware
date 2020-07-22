#!/bin/bash

# Try to find the device automatically.  Override with `DEVICE=`.
declare DEFAULT_DEVICE="auto"

# By default, turn on:
# - the green LED    (1 << 14)
# - an infra-red LED (1 <<  2)
# Override with `LED_MASK=`.
declare DEFAULT_LED_MASK=0x4004

# Default to enabling all channels. Override with `SRC_MASK=`.
declare DEFAULT_SRC_MASK=0x7f

# Default to 512 sample oversample.  Override with `OVERSAMPLE=`.
declare DEFAULT_OVERSAMPLE=512

# Default to 50Hz. Override with `FREQUENCY=`.
declare DEFAULT_FREQUENCY=50

usage()
{
	echo "$0 command"
}

# Collect 32-bit samples to be processed for calibrating the device.
run_cal()
{
	declare device="${DEVICE:-$DEFAULT_DEVICE}"
	declare led_mask="${LED_MASK:-$DEFAULT_LED_MASK}"
	declare src_mask="${SRC_MASK:-$DEFAULT_SRC_MASK}"
	declare frequency="${FREQUENCY:-$DEFAULT_FREQUENCY}"
	declare oversample="${OVERSAMPLE:-$DEFAULT_OVERSAMPLE}"

	# Shine the lights
	./tools/bl led     "$device" "$led_mask"

	# chancfg <channel> <gain> [offset] [shift] [sample32]
	./tools/bl chancfg "$device" 0 16  0  0  1 # Photodiode 1
	./tools/bl chancfg "$device" 1  1  0  0  1 # Photodiode 2
	./tools/bl chancfg "$device" 2  1  0  0  1 # Photodiode 3
	./tools/bl chancfg "$device" 3  1  0  0  1 # Photodiode 4
	./tools/bl chancfg "$device" 4  1  0  0  1 # 3.3V
	./tools/bl chancfg "$device" 5  1  0  0  1 # 5.0V
	./tools/bl chancfg "$device" 6  1  0  0  1 # Temperature

	# Start the calibration acquisition.
	./tools/bl start   "$device" "$frequency" "$oversample" "$src_mask"
}

# Run an acquisition.
run_acq()
{
	declare device="${DEVICE:-$DEFAULT_DEVICE}"
	declare led_mask="${LED_MASK:-$DEFAULT_LED_MASK}"
	declare src_mask="${SRC_MASK:-$DEFAULT_SRC_MASK}"
	declare frequency="${FREQUENCY:-$DEFAULT_FREQUENCY}"
	declare oversample="${OVERSAMPLE:-$DEFAULT_OVERSAMPLE}"

	# Shine the lights
	./tools/bl led     "$device" "$led_mask"

	# chancfg <channel> <gain> [offset] [shift] [sample32]
	./tools/bl chancfg "$device" 0 16 1264480 0 # Photodiode 1
	./tools/bl chancfg "$device" 1  1   54879 0 # Photodiode 2
	./tools/bl chancfg "$device" 2  1  567447 0 # Photodiode 3
	./tools/bl chancfg "$device" 3  1       0 0 # Photodiode 4
	./tools/bl chancfg "$device" 4  1 1038856 0 # 3.3V
	./tools/bl chancfg "$device" 5  1 1031704 0 # 5.0V
	./tools/bl chancfg "$device" 6  1  860701 0 # Temperature

	# Start the calibration acquisition.
	./tools/bl start   "$device" "$frequency" "$oversample" "$src_mask"
}

# Turn off the lights.
run_off()
{
	declare device="${DEVICE:-$DEFAULT_DEVICE}"

	./tools/bl led     "$device" 0x0000
}

if [ $# -lt 1 ]; then
	usage
	exit 1
fi
cmd="$1"
shift 1
"run_$cmd" "$@"
