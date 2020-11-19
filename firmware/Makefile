PROJECT = bloodlight-firmware
BUILD_DIR = bin

CFLAGS += -I..

CFILES = \
	src/tick.c \
	src/delay.c \
	src/led.c \
	src/bl.c \
	src/msg.c \
	src/usb.c \
	src/spi.c \
	src/mq.c \
	src/acq.c \
	src/acq/timer.c \
	src/acq/dma.c \
	src/acq/adc.c \
	src/acq/opamp.c \
	src/acq/source.c \
	src/acq/channel.c

REVISION ?= 1
CFLAGS += -DBL_REVISION=$(REVISION)
CFLAGS += -DBL_COMMIT_SHA=\"$(shell git rev-parse --verify HEAD)\"

ifeq ($(REVISION),1)
	DEVICE         = stm32f303CCT6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32f3x
else
	DEVICE         = stm32g474CET6
	OOCD_INTERFACE = stlink-v2-1
	OOCD_TARGET    = stm32g4x

	CFILES += src/acq/dac.c
endif

ifeq ($(SPI),TEST)
	CFILES += src/spi-test.c
	CFLAGS += -DBL_SPI_TEST
else ifeq ($(SPI),DMA-TEST)
	CFILES += src/spi-test.c
	CFLAGS += -DBL_SPI_TEST
	CFLAGS += -DBL_SPI_DMA_TEST
endif

# You shouldn't have to edit anything below here.
OPENCM3_DIR=libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
