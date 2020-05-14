PROJECT = mpd-firmware
BUILD_DIR = bin

CFILES = \
	src/bl.c

# TODO: Change for the actual board
DEVICE=stm32f303ZET6
OOCD_INTERFACE = stlink-v2-1
OOCD_TARGET = stm32f3x

# You shouldn't have to edit anything below here.
OPENCM3_DIR=libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
