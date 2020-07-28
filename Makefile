PROJECT = bloodlight-firmware
BUILD_DIR = bin

CFILES = \
	src/tick.c \
	src/delay.c \
	src/acq.c \
	src/led.c \
	src/bl.c \
	src/msg.c \
	src/usb.c \
	src/mq.c

DEVICE=stm32f303CCT6
OOCD_INTERFACE = stlink-v2-1
OOCD_TARGET = stm32f3x

# You shouldn't have to edit anything below here.
OPENCM3_DIR=libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
