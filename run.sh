#!/bin/bash

# Try to find the device automatically.  Override with `DEVICE=`.
declare DEFAULT_DEVICE="--auto"

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

	# srccfg <source> <gain> <offset> <sw oversample> [hw oversample] [hw shift]
	./tools/bl srccfg "$device" 0 16 0 "$oversample" 0 0 # Photodiode 1
	./tools/bl srccfg "$device" 1  1 0 "$oversample" 0 0 # Photodiode 2
	./tools/bl srccfg "$device" 2  1 0 "$oversample" 0 0 # Photodiode 3
	./tools/bl srccfg "$device" 3  1 0 "$oversample" 0 0 # Photodiode 4
	./tools/bl srccfg "$device" 4  1 0 "$oversample" 0 0 # 3.3V
	./tools/bl srccfg "$device" 5  1 0 "$oversample" 0 0 # 5.0V
	./tools/bl srccfg "$device" 6  1 0 "$oversample" 0 0 # Temperature

	# chancfg <channel> <source> [offset] [shift] [sample32]
	./tools/bl chancfg "$device" 0 0  0  0  1 # Photodiode 1
	./tools/bl chancfg "$device" 1 1  0  0  1 # Photodiode 2
	./tools/bl chancfg "$device" 2 2  0  0  1 # Photodiode 3
	./tools/bl chancfg "$device" 3 3  0  0  1 # Photodiode 4
	./tools/bl chancfg "$device" 4 4  0  0  1 # 3.3V
	./tools/bl chancfg "$device" 5 5  0  0  1 # 5.0V
	./tools/bl chancfg "$device" 6 6  0  0  1 # Temperature

	# Start the calibration acquisition.
	./tools/bl start   "$device" "$frequency" "$src_mask"
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

	# srccfg <source> <gain> <offset> <sw oversample> [hw oversample] [hw shift]
	./tools/bl srccfg "$device" 0 16 0 "$oversample" 0 0 # Photodiode 1
	./tools/bl srccfg "$device" 1  1 0 "$oversample" 0 0 # Photodiode 2
	./tools/bl srccfg "$device" 2  1 0 "$oversample" 0 0 # Photodiode 3
	./tools/bl srccfg "$device" 3  1 0 "$oversample" 0 0 # Photodiode 4
	./tools/bl srccfg "$device" 4  1 0 "$oversample" 0 0 # 3.3V
	./tools/bl srccfg "$device" 5  1 0 "$oversample" 0 0 # 5.0V
	./tools/bl srccfg "$device" 6  1 0 "$oversample" 0 0 # Temperature

	# chancfg <channel> <source> [offset] [shift] [sample32]
	./tools/bl chancfg "$device" 0 0 1264480 0 # Photodiode 1
	./tools/bl chancfg "$device" 1 1   54879 0 # Photodiode 2
	./tools/bl chancfg "$device" 2 2  567447 0 # Photodiode 3
	./tools/bl chancfg "$device" 3 3       0 0 # Photodiode 4
	./tools/bl chancfg "$device" 4 4 1038856 0 # 3.3V
	./tools/bl chancfg "$device" 5 5 1031704 0 # 5.0V
	./tools/bl chancfg "$device" 6 6  860701 0 # Temperature

	# Start the calibration acquisition.
	./tools/bl start   "$device" "$frequency" "$src_mask"
}

# Turn off the lights and stop any acquisition.
run_off()
{
	declare device="${DEVICE:-$DEFAULT_DEVICE}"

	./tools/bl led     "$device" 0x0000
	./tools/bl abort   "$device"
}

# Sequential exexute calibration and acquisition
run_cal_acq()
{
	TMP_CFG=tmp_cfg

	declare device="${DEVICE:-$DEFAULT_DEVICE}"
	declare led_mask="${LED_MASK:-$DEFAULT_LED_MASK}"
	declare src_mask="${SRC_MASK:-$DEFAULT_SRC_MASK}"
	declare frequency="${FREQUENCY:-$DEFAULT_FREQUENCY}"
	declare oversample="${OVERSAMPLE:-$DEFAULT_OVERSAMPLE}"

	# Run calibration to get the config
	run_cal | ./tools/calibrate | grep chancfg > "$TMP_CFG" &
	#echo "Wait 10s for default configuration"
	sleep 10

	# Save pid of acquisition to later use
	BID=$(pgrep -f "bl start")
	#echo "Run acquisition for 30 seconds to get calibration data."
	sleep 30

	# Terminate acquisition
	kill -s SIGINT "$BID"
	sleep 1

	# Run saved configurate command from calibration
	while read -r LINE
	do
		eval "$LINE"
	done < "$TMP_CFG"
	rm "$TMP_CFG"

	# Start the calibration acquisition.
	./tools/bl start   "$device" "$frequency" "$src_mask"
}

if [ $# -lt 1 ]; then
	usage
	exit 1
fi
cmd="$1"
shift 1
"run_$cmd" "$@"
