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

#include "common/error.h"
#include "common/util.h"

#include "delay.h"
#include "mq.h"
#include "usb.h"
#include "spi.h"


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
		enum bl_acq_detection_mode detection_mode,
		enum bl_acq_flash_mode flash_mode,
		uint16_t frequency,
		uint16_t led_mask,
		uint16_t src_mask)
{
	uint32_t acq_chan_mask;

	if (src_mask == 0x00) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (src_mask >= (1U << BL_ACQ__SRC_COUNT)) {
		return BL_ERROR_BAD_SOURCE_MASK;
	}

	if (bl_acq_is_active) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	if (flash_mode == BL_ACQ_FLASH) {
		if ((led_mask & (led_mask - 1)) == 0) {
			return BL_ERROR_MODE_MISMATCH;
		}

		enum bl_error status = bl_led_setup(led_mask);
		if (status != BL_ERROR_NONE) {
			return status;
		}

		/* Channels correspond to LEDs, and the 3 non-LED sources */
		acq_chan_mask = led_mask | (src_mask & 0xF0) << 16;

	} else {
		/* Channels map to sources. */
		acq_chan_mask = src_mask;
	}

	/* Detection mode casting to SPI mode:
	 * reflective   -> none SPI
	 * transmissive -> SPI mother mode
	 * Changing from SPI mother mode to daughter requires
	 * reconnecting USB cable, thus power cycle, so this routine
	 * is not valid in firmware
	 */
	if (bl_spi_mode != (enum bl_acq_spi_mode)detection_mode) {
		bl_spi_mode = detection_mode;
		if (bl_spi_mode == BL_ACQ_SPI_MOTHER) {
			bl_spi_init();
		}
	}

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
		config->frequency = frequency;

		unsigned multiplex = flash_mode ? bl_led_count : 1;
		config->frequency_trigger = (config->frequency *
				config->sw_oversample * multiplex);
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
	bool is_first = true;
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

		bl_acq_adc_flash_init(adc, flash_mode, is_first);
		is_first = false;
	}

	/* Enable all of the channels. */
	for (unsigned i = 0; i < BL_ACQ_CHANNEL_COUNT; i++) {
		if (acq_chan_mask & (1U << i)) {
			bl_acq_channel_enable(i);
		}
	}

	/* Disable status LED to avoid light pollution. */
	bl_led_status_set(false);

	/* Start timer(s) to begin acquisition. */
	bl_acq_is_active = true;
	bl_acq_timer_start_all();

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_source_conf(
		uint8_t  source,
		uint8_t  opamp_gain,
		uint16_t opamp_offset,
		uint16_t sw_oversample,
		uint8_t  hw_oversample,
		uint8_t  hw_shift)
{
	enum bl_error err;

	if (bl_acq_source_is_enabled(source)) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	bl_acq_source_config_t *src_config = bl_acq_source_get_config(source);

	/* Config setting is incomplete here so validation is done later. */

	src_config->opamp_gain    = opamp_gain;
	src_config->opamp_offset  = opamp_offset;
	src_config->sw_oversample = sw_oversample;
	src_config->oversample    = hw_oversample;
	src_config->shift         = hw_shift;

	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_source_cap(
		uint8_t  source,
		bl_msg_source_cap_t *response)
{
	if (source >= BL_ACQ__SRC_COUNT) {
		return BL_ERROR_OUT_OF_RANGE;
	}

	bl_acq_opamp_t *opamp = bl_acq_source_get_opamp(source);
	if (opamp == NULL) {
		response->opamp_gain_cnt = 0;
		response->opamp_offset   = false;
	} else {
#if (BL_REVISION == 1)
		response->opamp_offset = false;

		response->opamp_gain_cnt = 4;
		response->opamp_gain[0] =  2;
		response->opamp_gain[1] =  4;
		response->opamp_gain[2] =  8;
		response->opamp_gain[3] = 16;
#else
		bl_acq_dac_t *dac = bl_acq_source_get_dac(source, NULL);
		response->opamp_offset = (dac != NULL);

		if (response->opamp_offset)
		{
			response->opamp_gain_cnt = 5;
			response->opamp_gain[0] =  3;
			response->opamp_gain[1] =  7;
			response->opamp_gain[2] = 15;
			response->opamp_gain[3] = 31;
			response->opamp_gain[4] = 63;
		} else {
			response->opamp_gain_cnt = 6;
			response->opamp_gain[0] =  2;
			response->opamp_gain[1] =  4;
			response->opamp_gain[2] =  8;
			response->opamp_gain[3] = 16;
			response->opamp_gain[4] = 32;
			response->opamp_gain[5] = 64;
		}
#endif
	}

#if (BL_REVISION == 1)
	response->hw_oversample = false;
#else
	response->hw_oversample = true;
#endif

	response->source = source;
	response->type = BL_MSG_SOURCE_CAP;
	return BL_ERROR_NONE;
}

/* Exported function, documented in acq.h */
enum bl_error bl_acq_channel_conf(
		uint8_t  channel,
		uint8_t  source,
		uint8_t  shift,
		uint32_t offset,
		bool     sample32)
{
	enum bl_error err;

	if (bl_acq_channel_is_enabled(channel)) {
		return BL_ERROR_ACTIVE_ACQUISITION;
	}

	/* Config setting is incomplete here so validation is done later. */

	err = bl_acq_channel_configure(channel, source, sample32,
			offset, shift);
	if (err != BL_ERROR_NONE) {
		return err;
	}

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

	/* Re-enable status LED. */
	bl_led_status_set(true);

	return BL_ERROR_NONE;
}

