#ifndef BL_ACQ_CHANNEL_H
#define BL_ACQ_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "../error.h"
#include "../acq.h"

typedef struct  {
	bool enable;

	/* Per-channel frequency so we can have per-channel timers in future. */
	uint32_t frequency;

	/* These values are calculated based on variables below. */
	uint32_t frequency_sample;
	uint32_t frequency_trigger;

	/* Gain and offset are parameters for OPAMPs and come with hardware
	 * limitations:
	 * gain: applied to the incoming voltage, power of 2 should be 1..64.
	 * offset: applied to the incoming voltage and is 12-bit. */
	uint8_t  opamp_gain;
	uint16_t opamp_offset;

	/* Hardware oversample occurs before anything else, and truncates the
	 * result to 16 bits.
	 * oversample is logarithmic and maxes out at 8.
	 * shift_hw maxes out at 8. */
	uint8_t oversample;
	uint8_t shift;

	/* TODO: Support hardware gain compensation and offset in ADC? */

	uint16_t sw_oversample;
	uint32_t sw_offset;
	uint8_t  sw_shift;

	bool     sample32;
} bl_acq_channel_config_t;


void bl_acq_channel_calibrate(enum bl_acq_source source);

enum bl_error bl_acq_channel_configure(enum bl_acq_source source);

void bl_acq_channel_enable(enum bl_acq_source source);
void bl_acq_channel_disable(enum bl_acq_source source);

bool bl_acq_channel_is_enabled(enum bl_acq_source source);

bl_acq_channel_config_t *bl_acq_channel_get_config(enum bl_acq_source source);

#include "timer.h"
bl_acq_timer_t *bl_acq_channel_get_timer(enum bl_acq_source source);

#include "opamp.h"
bl_acq_opamp_t *bl_acq_channel_get_opamp(enum bl_acq_source source);

#if (BL_REVISION >= 2)
#include "dac.h"
bl_acq_dac_t *bl_acq_channel_get_dac(enum bl_acq_source source,
		uint8_t *dac_channel);
#endif

#include "adc.h"
bl_acq_adc_t *bl_acq_channel_get_adc(enum bl_acq_source source,
		uint8_t *adc_channel);

void bl_acq_channel_commit_sample(enum bl_acq_source source, uint32_t sample);

#endif
