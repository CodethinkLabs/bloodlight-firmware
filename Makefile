PROJECT = bloodlight-firmware
BUILD_DIR = bin

CFILES = \
	src/tick.c \
	src/delay.c \
	src/led.c \
	src/bl.c \
	src/msg.c \
	src/usb.c \
	src/mq.c \
	src/acq.c

REVISION ?= 1
CFLAGS += -DBL_REVISION=$(REVISION)

ifeq ($(REVISION),1)
	DEVICE         = stm32f303CCT6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32f3x
else
	DEVICE         = stm32g474CET6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32g4x
endif

# You shouldn't have to edit anything below here.
OPENCM3_DIR=libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
