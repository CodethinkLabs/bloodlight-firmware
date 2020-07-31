PROJECT = bloodlight-firmware
BUILD_DIR = bin

CFILES = \
	src/tick.c \
	src/delay.c \
	src/led.c \
	src/bl.c \
	src/msg.c \
	src/usb.c \
	src/mq.c

REVISION ?= 1
CFLAGS += -DBL_REVISION=$(REVISION)

ifeq ($(REVISION),1)
	DEVICE         = stm32f303CCT6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32f3x
	CFILES        += src/acq.c
else
	DEVICE         = stm32g474CET6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32g4x
	CFILES        += src/acq2.c
endif

# You shouldn't have to edit anything below here.
OPENCM3_DIR=libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
