/*
 * Copyright 2020 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "opamp.h"
#include "adc.h"

#if (BL_REVISION >= 2)
#include "dac.h"
#endif

#include "../delay.h"

#include <stddef.h>
#include <stdbool.h>

#include <libopencm3/stm32/opamp.h>
#include <libopencm3/stm32/rcc.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/dac.h>
#endif


#if (BL_REVISION == 1)
	/* ADC calibration max trimming time in uS. */
	#define TOFFTRIM_MAX 2000
#else
	/* ADC calibration max trimming time in uS. */
	#define TOFFTRIM_MAX 1000
#endif


typedef struct
{
	uint32_t pga_gain;
} bl_acq_opamp_config_t;

struct bl_acq_opamp_s
{
	const uint32_t base;
#if (BL_REVISION >= 2)
	bl_acq_dac_t **dac;
	const uint8_t  dac_channel;
#endif
	bl_acq_adc_t **adc;
	const uint8_t  adc_channel;
	uint32_t       enable;

	uint8_t vinp;
	bool    inverted;

	bool    calibrated;
	uint8_t trimoffsetn;
	uint8_t trimoffsetp;

	bl_acq_opamp_config_t config;
};

#if (BL_REVISION == 1)
static bl_acq_opamp_t bl_acq__opamp2 =
{
	.base        = OPAMP2,
	.adc         = &bl_acq_adc2,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP2_CSR_VP_SEL_PA7,
	.inverted    = false,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp2 = &bl_acq__opamp2;

static bl_acq_opamp_t bl_acq__opamp4 =
{
	.base        = OPAMP4,
	.adc         = &bl_acq_adc4,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP4_CSR_VP_SEL_PA4,
	.inverted    = false,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp4 = &bl_acq__opamp4;
#else
static bl_acq_opamp_t bl_acq__opamp1 =
{
	.base        = OPAMP1,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc1,
	.adc_channel = 13,
	.enable      = 0,
	.vinp        = OPAMP1_CSR_VP_SEL_DAC3CH1,
	.inverted    = true,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp1 = &bl_acq__opamp1;

static bl_acq_opamp_t bl_acq__opamp2 =
{
	.base        = OPAMP2,
	.dac         = NULL,
	.adc         = &bl_acq_adc2,
	.adc_channel = 16,
	.enable      = 0,
	.vinp        = OPAMP_CSR_VP_SEL_VINP0,
	.inverted    = false,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp2 = &bl_acq__opamp2;

static bl_acq_opamp_t bl_acq__opamp3 =
{
	.base        = OPAMP3,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL2,
	.adc         = &bl_acq_adc3,
	.adc_channel = 13,
	.enable      = 0,
	.vinp        = OPAMP3_CSR_VP_SEL_DAC3CH2,
	.inverted    = true,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp3 = &bl_acq__opamp3;

static bl_acq_opamp_t bl_acq__opamp4 =
{
	.base        = OPAMP4,
	.dac         = &bl_acq_dac4,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc5,
	.adc_channel = 5,
	.enable      = 0,
	.vinp        = OPAMP4_CSR_VP_SEL_DAC4CH1,
	.inverted    = true,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp4 = &bl_acq__opamp4;

static bl_acq_opamp_t bl_acq__opamp5 =
{
	.base        = OPAMP5,
	.dac         = &bl_acq_dac4,
	.dac_channel = DAC_CHANNEL2,
	.adc         = &bl_acq_adc5,
	.adc_channel = 3,
	.enable      = 0,
	.vinp        = OPAMP5_CSR_VP_SEL_DAC4CH2,
	.inverted    = true,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp5 = &bl_acq__opamp5;

static bl_acq_opamp_t bl_acq__opamp6 =
{
	.base        = OPAMP6,
	.dac         = &bl_acq_dac3,
	.dac_channel = DAC_CHANNEL1,
	.adc         = &bl_acq_adc4,
	.adc_channel = 17,
	.enable      = 0,
	.vinp        = OPAMP6_CSR_VP_SEL_DAC3CH1,
	.inverted    = true,
	.calibrated  = false,
};
bl_acq_opamp_t *bl_acq_opamp6 = &bl_acq__opamp6;
#endif


void bl_acq_opamp_calibrate(bl_acq_opamp_t *opamp)
{
	if (opamp->enable > 0) {
		return;
	}

#if (BL_REVISION >= 2)
	if (opamp->dac) {
		bl_acq_dac_calibrate(*(opamp->dac));
	}
#endif

	if (opamp->adc) {
		bl_acq_adc_calibrate(*(opamp->adc));
	}

	rcc_periph_clock_enable(RCC_SYSCFG);

	opamp_enable(opamp->base);

	opamp_user_trim_enable(opamp->base);
	opamp_cal_enable(opamp->base);
	opamp_set_calsel(opamp->base, OPAMP_CSR_CALSEL_90_PERCENT);

	for (unsigned n = 0; n < OPAMP_CSR_TRIMOFFSETN_MASK; n++) {
		opamp_trimoffsetn_set(opamp->base, n);

		bl_delay_us(TOFFTRIM_MAX);

		if (!opamp_read_outcal(opamp->base)) {
			opamp->trimoffsetn = n;
			break;
		}
	}

	opamp_set_calsel(opamp->base, OPAMP_CSR_CALSEL_10_PERCENT);

	for (unsigned p = 0; p < OPAMP_CSR_TRIMOFFSETN_MASK; p++) {
		opamp_trimoffsetp_set(opamp->base, p);

		bl_delay_us(TOFFTRIM_MAX);

		if (!opamp_read_outcal(opamp->base)) {
			opamp->trimoffsetp = p;
			break;
		}
	}

	opamp_user_trim_disable(opamp->base);
	opamp_cal_disable(opamp->base);
	opamp_disable(opamp->base);
}

enum bl_error bl_acq_opamp_configure(bl_acq_opamp_t *opamp, uint8_t gain)
{
	if (opamp->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	bl_acq_opamp_config_t *config = &opamp->config;

	uint32_t pga_gain;
	if (opamp->inverted) {
		switch (gain) {
#if (BL_REVISION >= 2)
		case 1:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_1_GAIN_2_VM0;
			break;

		case 3:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_3_GAIN_4_VM0;
			break;

		case 7:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_7_GAIN_8_VM0;
			break;

		case 15:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_15_GAIN_16_VM0;
			break;

		case 31:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_31_GAIN_32_VM0;
			break;

		case 63:
			pga_gain = OPAMP_CSR_PGA_INV_GAIN_MINUS_63_GAIN_64_VM0;
			break;
#endif

		default:
			return BL_ERROR_OPAMP_BAD_GAIN;
		}
	} else {
		switch (gain) {
		case 2:
			pga_gain = OPAMP_CSR_PGA_GAIN_2;
			break;

		case 4:
			pga_gain = OPAMP_CSR_PGA_GAIN_4;
			break;

		case 8:
			pga_gain = OPAMP_CSR_PGA_GAIN_8;
			break;

		case 16:
			pga_gain = OPAMP_CSR_PGA_GAIN_16;
			break;

#if (BL_REVISION >= 2)
		case 32:
			pga_gain = OPAMP_CSR_PGA_GAIN_32;
			break;

		case 64:
			pga_gain = OPAMP_CSR_PGA_GAIN_64;
			break;
#endif

		default:
			return BL_ERROR_OPAMP_BAD_GAIN;
		}
	}

	config->pga_gain = pga_gain;
	return BL_ERROR_NONE;
}

void bl_acq_opamp_enable(bl_acq_opamp_t *opamp)
{
	opamp->enable++;
	if (opamp->enable != 1) {
		/* Already enabled. */
		return;
	}

	rcc_periph_clock_enable(RCC_SYSCFG);

	const bl_acq_opamp_config_t *config = &opamp->config;

#if (BL_REVISION >= 2)
	if (opamp->dac) {
		bl_acq_dac_enable(*(opamp->dac));
	}
#endif

	if (opamp->calibrated) {
		opamp_user_trim_enable(opamp->base);
		opamp_trimoffsetn_set(opamp->base, opamp->trimoffsetn);
		opamp_trimoffsetp_set(opamp->base, opamp->trimoffsetp);
	}

	opamp_pga_gain_select(opamp->base, config->pga_gain);

	opamp_vm_select(opamp->base, OPAMP_CSR_VM_SEL_PGA_MODE);
	opamp_vp_select(opamp->base, opamp->vinp);

#if (BL_REVISION >= 2)
	opamp_output_set_internal(opamp->base);
#endif

	opamp_enable(opamp->base);

	if (opamp->adc) {
		bl_acq_adc_enable(*(opamp->adc), 0);
	}
}

void bl_acq_opamp_disable(bl_acq_opamp_t *opamp)
{
	if (opamp->enable == 0) {
		return;
	}
	opamp->enable--;

	/* Don't disable SYSCFG as it may be used elsewhere. */

	if (opamp->adc != NULL) {
		bl_acq_adc_disable(*(opamp->adc), 0);
	}

	opamp_disable(opamp->base);

#if (BL_REVISION >= 2)
	if (opamp->dac != NULL) {
		bl_acq_dac_disable(*(opamp->dac));
	}
#endif
}


#if (BL_REVISION >= 2)
bl_acq_dac_t *bl_acq_opamp_get_dac(const bl_acq_opamp_t *opamp,
		uint8_t *dac_channel)
{
	if ((opamp->dac != NULL) && (dac_channel != NULL)) {
		*dac_channel = opamp->dac_channel;
	}
	return *(opamp->dac);
}
#endif

bl_acq_adc_t *bl_acq_opamp_get_adc(const bl_acq_opamp_t *opamp,
		uint8_t *adc_channel)
{
	if ((opamp->adc != NULL) && (adc_channel != NULL)) {
		*adc_channel = opamp->adc_channel;
	}
	return *(opamp->adc);
}

