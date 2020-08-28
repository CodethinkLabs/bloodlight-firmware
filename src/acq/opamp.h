#ifndef BL_ACQ_OPAMP_H
#define BL_ACQ_OPAMP_H

#include <stdint.h>

#include "../error.h"


typedef struct bl_acq_opamp_s bl_acq_opamp_t;

#if (BL_REVISION == 1)
extern bl_acq_opamp_t *bl_acq_opamp2;
extern bl_acq_opamp_t *bl_acq_opamp4;
#else
extern bl_acq_opamp_t *bl_acq_opamp1;
extern bl_acq_opamp_t *bl_acq_opamp2;
extern bl_acq_opamp_t *bl_acq_opamp3;
extern bl_acq_opamp_t *bl_acq_opamp4;
extern bl_acq_opamp_t *bl_acq_opamp5;
extern bl_acq_opamp_t *bl_acq_opamp6;
#endif

void bl_acq_opamp_calibrate(bl_acq_opamp_t *opamp);

enum bl_error bl_acq_opamp_configure(bl_acq_opamp_t *opamp, uint8_t gain);

void bl_acq_opamp_enable(bl_acq_opamp_t *opamp);
void bl_acq_opamp_disable(bl_acq_opamp_t *opamp);

#if (BL_REVISION >= 2)
#include "dac.h"
bl_acq_dac_t *bl_acq_opamp_get_dac(const bl_acq_opamp_t *opamp,
		uint8_t *dac_channel);
#endif

#include "adc.h"
bl_acq_adc_t *bl_acq_opamp_get_adc(const bl_acq_opamp_t *opamp,
		uint8_t *adc_channel);

#endif

