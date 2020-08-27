#ifndef BL_ACQ_DAC_H
#define BL_ACQ_DAC_H

#include <stdint.h>

#include "../error.h"

typedef struct bl_acq_dac_s bl_acq_dac_t;

extern bl_acq_dac_t *bl_acq_dac3;
extern bl_acq_dac_t *bl_acq_dac4;

void bl_acq_dac_init(uint32_t bus_freq);

void bl_acq_dac_calibrate(bl_acq_dac_t *dac);

enum bl_error bl_acq_dac_channel_configure(bl_acq_dac_t *dac,
		uint8_t channel, uint16_t offset);

void bl_acq_dac_enable(bl_acq_dac_t *dac);
void bl_acq_dac_disable(bl_acq_dac_t *dac);

#endif

