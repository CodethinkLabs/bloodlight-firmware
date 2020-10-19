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

#include "dac.h"
#include "rcc.h"

#include <libopencm3/stm32/dac.h>


typedef struct
{
	uint8_t  channel_mask;
	uint16_t offset[2];
} bl_acq_dac_config_t;

struct bl_acq_dac_s
{
	const uint32_t rcc_unit;
	const uint32_t base;
	unsigned       enable;

	bl_acq_dac_config_t config;
};

static bl_acq_dac_t bl_acq__dac3 =
{
	.rcc_unit = RCC_DAC3,
	.base     = DAC3,
	.enable   = 0,
};
bl_acq_dac_t *bl_acq_dac3 = &bl_acq__dac3;

static bl_acq_dac_t bl_acq__dac4 =
{
	.rcc_unit = RCC_DAC4,
	.base     = DAC4,
	.enable   = 0,
};
bl_acq_dac_t *bl_acq_dac4 = &bl_acq__dac4;


static uint32_t bl_acq_dac__bus_freq = 0;

void bl_acq_dac_init(uint32_t bus_freq)
{
	bl_acq_dac__bus_freq = bus_freq;
}

void bl_acq_dac_calibrate(bl_acq_dac_t *dac)
{
	if (dac->enable) return;

	/* TODO: Implement by reading Section 22.4.13 of RM04440. */
}

enum bl_error bl_acq_dac_channel_configure(bl_acq_dac_t *dac,
		uint8_t channel, uint16_t offset)
{
	if (dac->enable > 0) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	if (channel > DAC_CHANNEL2) {
		return BL_ERROR_DAC_BAD_CHANNEL;
	}

	if (offset > 0xFFF) {
		return BL_ERROR_DAC_BAD_OFFSET;
	}

	bl_acq_dac_config_t *config = &dac->config;

	config->channel_mask |= channel;
	config->offset[channel] = offset;
	return BL_ERROR_NONE;
}

void bl_acq_dac_enable(bl_acq_dac_t *dac)
{
	if (!bl_acq__rcc_enable_ref(dac->rcc_unit, &dac->enable)) {
		/* DAC already enabled. */
		return;
	}

	const bl_acq_dac_config_t *config = &dac->config;

	uint32_t hfsel = DAC_MCR_HFSEL_DIS;
	if (bl_acq_dac__bus_freq >= 160000000) {
		hfsel = DAC_MCR_HFSEL_AHB160;
	} else if (bl_acq_dac__bus_freq >= 80000000) {
		hfsel = DAC_MCR_HFSEL_AHB80;
	}
	dac_set_high_frequency_mode(dac->base, hfsel);

	uint32_t mode = 0;
	if (config->channel_mask & DAC_CHANNEL1) {
		mode |= DAC_MCR_MODE1_EP;
	}
	if (config->channel_mask & DAC_CHANNEL2) {
		mode |= DAC_MCR_MODE2_EP;
	}
	dac_set_mode(dac->base, mode);

	dac_enable(dac->base, dac->config.channel_mask);
	dac_wait_on_ready(dac->base, dac->config.channel_mask);

	if (config->channel_mask & DAC_CHANNEL1) {
		dac_load_data_buffer_single(dac->base,
				config->offset[DAC_CHANNEL1],
				DAC_RIGHT12, DAC_CHANNEL1);
	}
	if (config->channel_mask & DAC_CHANNEL2) {
		dac_load_data_buffer_single(dac->base,
				config->offset[DAC_CHANNEL2],
				DAC_RIGHT12, DAC_CHANNEL2);
	}
}

void bl_acq_dac_disable(bl_acq_dac_t *dac)
{
	if (bl_acq__rcc_disable_ref(dac->rcc_unit, &dac->enable)) {
		dac_disable(dac->base, DAC_CHANNEL_BOTH);
	}
}

