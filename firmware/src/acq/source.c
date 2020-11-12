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

#include "source.h"
#include "opamp.h"
#include "adc.h"

#include "../mq.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>

typedef struct
{
	uint32_t         gpio_port;
	uint8_t          gpio_pin;
	bl_acq_opamp_t **opamp;
	bool             opamp_used;
	bl_acq_adc_t   **adc;
	uint8_t          adc_channel;
	uint32_t         adc_ccr_flag;
	uint32_t         enable;

	uint8_t channel_count;
	uint8_t channel[BL_ACQ_CHANNEL_COUNT];
	uint8_t channel_current;

	bl_acq_source_config_t config;
} bl_acq_source_t;

static bl_acq_source_t bl_acq_source[] =
{
#if (BL_REVISION == 1)
	[BL_ACQ_PD1] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 4,
		.opamp        = &bl_acq_opamp4,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 1,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 5,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 1,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 2,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 3,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = 0,
	},
#else
	[BL_ACQ_PD1] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 2,
		.opamp        = &bl_acq_opamp3,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = 0,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 10,
		.opamp        = &bl_acq_opamp4,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = 0,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 1,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc3,
		.adc_channel  = 1,
		.enable       = 0,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = &bl_acq_opamp1,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = 0,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_VBAT,
		.adc_ccr_flag = ADC_CCR_VBATEN,
		.enable       = 0,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = 0,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = 0,
	},
	[BL_ACQ_EXT] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 15,
		.opamp        = &bl_acq_opamp5,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = 0,
	},
#endif
};

void bl_acq_source_calibrate(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	if (src->opamp) {
		bl_acq_opamp_calibrate(*(src->opamp));
	}

	if (src->adc) {
		bl_acq_adc_calibrate(*(src->adc));
	}
}

enum bl_error bl_acq_source_configure(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];
	bl_acq_source_config_t *config = &src->config;

	bool opamp_needed = (config->opamp_gain > 1) ||
			(config->opamp_offset != OPAMP_OFFSET_ZERO);
	if (opamp_needed && (src->opamp == NULL)) {
		return BL_ERROR_HARDWARE_CONFLICT;
	}

	src->opamp_used = opamp_needed || (src->adc == NULL);
	return BL_ERROR_NONE;
}

void bl_acq_source_assign_channel(enum bl_acq_source source, unsigned channel)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	for (unsigned i = 0; i < src->channel_count; i++) {
		if (src->channel[i] == channel) {
			return;
		}
	}

	src->channel[src->channel_count++] = channel;
}

void bl_acq_source_enable(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	src->enable++;
	if (src->enable > 1) {
		/* Already enabled. */
		return;
	}

	if (src->gpio_port != 0) {
		gpio_mode_setup(src->gpio_port, GPIO_MODE_ANALOG,
				GPIO_PUPD_NONE, (1U << src->gpio_pin));
	}

	const bl_acq_source_config_t *config = &src->config;

	if (src->opamp_used && (src->opamp != NULL)) {
		bl_acq_opamp_enable(*(src->opamp));
	} else if (src->adc != NULL) {
		bl_acq_adc_enable(*(src->adc), src->adc_ccr_flag);
	}
}

void bl_acq_source_disable(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	if (src->enable == 0) {
		return;
	}

	src->enable--;
	if (src->enable > 0) {
		return;
	}

	if ((src->adc != NULL) && (src->config.opamp_gain <= 1)) {
		bl_acq_adc_disable(*(src->adc), src->adc_ccr_flag);
	} else if (src->opamp != NULL) {
		bl_acq_opamp_disable(*(src->opamp));
	}

	/* Clear configuration so we're ready for re-configuring. */
	src->channel_count = 0;
	src->channel_current = 0;

	/* Don't need to do anything here to disable GPIO (it is floating). */
}

bool bl_acq_source_is_enabled(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];
	return src->enable;
}


bl_acq_source_config_t *bl_acq_source_get_config(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];
	return &src->config;
}

bl_acq_timer_t *bl_acq_source_get_timer(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	if (src->opamp_used) {
		bl_acq_adc_t *adc = bl_acq_opamp_get_adc(
				*(src->opamp), NULL);
		return bl_acq_adc_get_timer(adc);
	}

	return bl_acq_adc_get_timer(*(src->adc));
}

bl_acq_opamp_t *bl_acq_source_get_opamp(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	if (!src->opamp_used) {
		return NULL;
	}

	return *(src->opamp);
}

#if (BL_REVISION >= 2)
bl_acq_dac_t *bl_acq_source_get_dac(enum bl_acq_source source,
	uint8_t *dac_channel)
{
	bl_acq_opamp_t *opamp = bl_acq_source_get_opamp(source);
	if (opamp == NULL) {
		return NULL;
	}

	return bl_acq_opamp_get_dac(opamp, dac_channel);
}
#endif

bl_acq_adc_t *bl_acq_source_get_adc(enum bl_acq_source source,
	uint8_t *adc_channel)
{
	bl_acq_source_t *src = &bl_acq_source[source];

	if (src->opamp_used) {
		return bl_acq_opamp_get_adc(*(src->opamp), adc_channel);
	}

	if (adc_channel != NULL) {
		*adc_channel = src->adc_channel;
	}
	return *(src->adc);
}

static inline uint8_t bl_acq_source__current_idx(bl_acq_source_t *src)
{
	uint8_t current = src->channel_current++;

	if (src->channel_current == src->channel_count) {
		src->channel_current = 0;
	}

	return current;
}

uint8_t bl_acq_source_get_channel(enum bl_acq_source source)
{
	bl_acq_source_t *src = &bl_acq_source[source];
	return src->channel[bl_acq_source__current_idx(src)];
}
