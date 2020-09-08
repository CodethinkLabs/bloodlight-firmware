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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>

#include "acq.h"
#include "acq/timer.h"
#include "acq/adc.h"
#include "acq/opamp.h"
#include "acq/source.h"

#if (BL_REVISION >= 2)
#include "acq/dac.h"
#endif

#include "delay.h"
#include "error.h"
#include "util.h"
#include "msg.h"
#include "mq.h"
#include "usb.h"


static uint32_t bl_acq__ahb_freq = 0;
static uint32_t bl_acq__adc_freq = 0;

static bool bl_acq_is_active = false;



/* Exported function, documented in acq.h */
void bl_acq_init(uint32_t clock)
{
	rcc_periph_clock_enable(RCC_SYSCFG);

	bl_mq_init();

	/* TODO: Differentiate clock frequencies in input. */
	bl_acq__adc_freq = clock;
	bl_acq__ahb_freq = clock;

	bl_acq_timer_init(bl_acq__ahb_freq);

#if (BL_REVISION >= 2)
	bl_acq_dac_init(bl_acq__ahb_freq);
#endif

	/* Configure ADC groups to get ADC clocks for calibration. */
	bool async = (bl_acq__adc_freq != bl_acq__ahb_freq);
	bl_acq_adc_group_configure_all(bl_acq__adc_freq, async, false);

	for (unsigned src = 0; src < BL_ACQ__SRC_COUNT; src++) {
		bl_acq_source_config_t *config
			= bl_acq_source_get_config(src);

		config->enable = true;

		bl_acq_source_calibrate(src);

		config->enable = false;
	}

}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_start(
		uint16_t frequency,
		uint32_t oversample,
		uint16_t src_mask)
{
	if (src_mask == 0x00) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (src_mask >= (1U << BL_ACQ__SRC_COUNT)) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (bl_acq_is_active) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	/* For now, channels have a direct mapping to sources. */
	uint16_t acq_chan_mask = src_mask;

	/* Get source list from channels */
	uint16_t acq_src_mask = 0;
	for (unsigned i = 0; i < BL_ACQ_CHANNEL_COUNT; i++) {
		if ((acq_chan_mask & (1U << i)) != 0) {
			acq_src_mask |= (1U << bl_acq_channel_get_source(i));
		}
	}

	enum bl_acq_source src[BL_ACQ__SRC_COUNT];
	unsigned           src_count = 0;

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if ((acq_src_mask & (1U << i)) != 0) {
			src[src_count++] = i;
		}
	}

	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		/* Finalize Config. */
		config->frequency     = frequency;
		config->sw_oversample = oversample;
		config->oversample    = 0;

		config->frequency_trigger = (config->frequency *
				config->sw_oversample);
		config->frequency_sample = (config->frequency_trigger <<
				config->oversample);

		/* Finalize configuration. */
		enum bl_error status = bl_acq_source_configure(src[i]);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure timer(s). */
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_timer_t *timer = bl_acq_source_get_timer(src[i]);
		if (timer == NULL) {
			continue;
		}

		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		/* Ensure same frequency for all users of the timer. */
		for (unsigned j = (i + 1); j < src_count; j++) {
			if (bl_acq_source_get_timer(src[j]) != timer) {
				continue;
			}

			bl_acq_source_config_t *config_b =
					bl_acq_source_get_config(src[j]);

			if (config->frequency_trigger !=
					config_b->frequency_trigger) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_timer_configure(timer,
				config->frequency_trigger);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

#if (BL_REVISION >= 2)
	/* Configure DACs. */
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		uint8_t dac_channel;
		bl_acq_dac_t *dac = bl_acq_source_get_dac(src[i], &dac_channel);
		if (dac == NULL) {
			continue;
		}

		/* Ensure same offsets for all users of same DAC channel. */
		for (unsigned j = (i + 1); j < src_count; j++) {
			bl_acq_source_config_t *config_b =
					bl_acq_source_get_config(src[j]);

			uint8_t dac_channel_b;
			bl_acq_dac_t *dac_b = bl_acq_source_get_dac(
					src[j], &dac_channel_b);

			if ((dac_b != dac) || (dac_channel_b != dac_channel)) {
				continue;
			}

			if (config->opamp_offset !=
				config_b->opamp_offset) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_dac_channel_configure(
			dac, dac_channel, config->opamp_offset);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}
#endif

	/* Configure OPAMPs. */
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		bl_acq_opamp_t *opamp = bl_acq_source_get_opamp(src[i]);
		if (opamp == NULL) {
			continue;
		}

		/* OPAMPs are never shared so no conflicts possible. */

		enum bl_error status = bl_acq_opamp_configure(opamp,
				config->opamp_gain);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure ADC groups so we know the frequency. */
	bool async = (bl_acq__adc_freq != bl_acq__ahb_freq);

	bool multi = false;
	bl_acq_adc_t *first_adc = NULL;
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_adc_t *chan_adc = bl_acq_source_get_adc(src[i], NULL);
		if (chan_adc == NULL) {
			continue;
		}

		if (first_adc && (chan_adc != first_adc)) {
			multi = true;
			break;
		}
		first_adc = chan_adc;
	}

	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_adc_t *adc = bl_acq_source_get_adc(src[i], NULL);
		if (adc == NULL) {
			continue;
		}

		bl_acq_adc_group_t *adc_group = bl_acq_adc_get_group(adc);
		if (adc_group == NULL) {
			continue;
		}

		enum bl_error status = bl_acq_adc_group_configure(
				adc_group, bl_acq__adc_freq, async, multi);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Configure ADC Channels. */
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		uint8_t adc_channel;
		bl_acq_adc_t *adc = bl_acq_source_get_adc(src[i], &adc_channel);
		if (adc == NULL) {
			continue;
		}

		/* Ensure same ADC configs for all users of same ADC. */
		for (unsigned j = (i + 1); j < src_count; j++) {
			bl_acq_adc_t *adc_b = bl_acq_source_get_adc(
					src[j], NULL);
			if (adc_b != adc) {
				continue;
			}

			bl_acq_source_config_t *config_b =
					bl_acq_source_get_config(src[j]);

			if (config->oversample != config_b->oversample) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}

			if (config->shift != config_b->shift) {
				return BL_ERROR_HARDWARE_CONFLICT;
			}
		}

		enum bl_error status = bl_acq_adc_channel_configure(
				adc, adc_channel, src[i]);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	for (unsigned i = 0; i < BL_ACQ_CHANNEL_COUNT; i++) {
		if (acq_chan_mask & (1U << i)) {
			enum bl_acq_source source = bl_acq_channel_get_source(i);
			bl_acq_source_assign_channel(source, i);
		}
	}

	/* Configure ADCs. */
	for (unsigned i = 0; i < src_count; i++) {
		bl_acq_source_config_t *config =
				bl_acq_source_get_config(src[i]);

		uint8_t adc_channel;
		bl_acq_adc_t *adc = bl_acq_source_get_adc(src[i], &adc_channel);
		if (adc == NULL) {
			continue;
		}

		enum bl_error status = bl_acq_adc_configure(adc,
				config->frequency_sample,
				config->sw_oversample,
				config->oversample,
				config->shift);
		if (status != BL_ERROR_NONE) {
			return status;
		}
	}

	/* Enable all of the channels. */
	for (unsigned i = 0; i < BL_ACQ_CHANNEL_COUNT; i++) {
		if (acq_chan_mask & (1U << i)) {
			bl_acq_channel_enable(i);
		}
	}

	/* Start timer(s) to begin acquisition. */
	bl_acq_is_active = true;
	bl_acq_timer_start_all();

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_channel_conf(
		uint8_t  index,
		uint8_t  gain,
		uint8_t  shift,
		uint32_t offset,
		bool     sample32)
{
	enum bl_error err;

	if (bl_acq_channel_is_enabled(index)) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	err = bl_acq_channel_configure(index, index, sample32, offset, shift);
	if (err != BL_ERROR_NONE) {
		return err;
	}

	/* TODO: Split out source config command into new host msg. */
	if (bl_acq_source_is_enabled(index)) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	bl_acq_source_config_t *src_config = bl_acq_source_get_config(index);

	/* Config setting is incomplete here so validation is done later. */

	src_config->opamp_gain   = gain;
	src_config->opamp_offset = 0;
	src_config->shift        = 0;

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_abort(void)
{
	/* Stop acquisition timer(s). */
	bl_acq_timer_stop_all();
	bl_acq_is_active = false;

	/* Disable all channels. */
	for (unsigned i = 0; i < BL_ACQ_CHANNEL_COUNT; i++) {
		if (bl_acq_channel_is_enabled(i)) {
			bl_acq_channel_disable(i);
		}
	}

	return BL_ERROR_NONE;
}

