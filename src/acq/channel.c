#include "channel.h"
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
	bool             enable;

	union bl_msg_data *msg;

	bl_acq_channel_config_t config;
} bl_acq_channel_t;

static bl_acq_channel_t bl_acq_channel[] =
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
		.enable       = false,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 5,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 1,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 2,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 2,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = 3,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = false,
	},
#else
	[BL_ACQ_PD1] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 2,
		.opamp        = &bl_acq_opamp3,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD2] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 10,
		.opamp        = &bl_acq_opamp4,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD3] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 1,
		.opamp        = &bl_acq_opamp6,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_PD4] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 3,
		.opamp        = &bl_acq_opamp1,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = false,
	},
	[BL_ACQ_3V3] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_VBAT,
		.adc_ccr_flag = ADC_CCR_VBATEN,
		.enable       = false,
	},
	[BL_ACQ_5V0] = {
		.gpio_port    = GPIOA,
		.gpio_pin     = 7,
		.opamp        = &bl_acq_opamp2,
		.opamp_used   = false,
		.adc          = &bl_acq_adc2,
		.adc_channel  = 4,
		.adc_ccr_flag = 0,
		.enable       = false,
	},
	[BL_ACQ_TMP] = {
		.gpio_port    = 0,
		.opamp        = NULL,
		.opamp_used   = false,
		.adc          = &bl_acq_adc1,
		.adc_channel  = ADC_CHANNEL_TEMP,
		.adc_ccr_flag = ADC_CCR_TSEN,
		.enable       = false,
	},
	[BL_ACQ_EXT] = {
		.gpio_port    = GPIOB,
		.gpio_pin     = 15,
		.opamp        = &bl_acq_opamp5,
		.opamp_used   = true,
		.adc          = NULL,
		.enable       = false,
	},
#endif
};


void bl_acq_channel_calibrate(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if (channel->opamp) {
		bl_acq_opamp_calibrate(*(channel->opamp));
	}

	if (channel->adc) {
		bl_acq_adc_calibrate(*(channel->adc));
	}
}

enum bl_error bl_acq_channel_configure(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];
	bl_acq_channel_config_t *config = &channel->config;

	bool opamp_needed = (config->opamp_gain > 1) ||
			(config->opamp_offset != 0);
	if (opamp_needed && (channel->opamp == NULL)) {
		return BL_ERROR_HARDWARE_CONFLICT;
	}

	channel->opamp_used = opamp_needed || (channel->adc == NULL);
	return BL_ERROR_NONE;
}

void bl_acq_channel_enable(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if (channel->enable) {
		/* Already enabled. */
		return;
	}

	if (channel->gpio_port != 0) {
		gpio_mode_setup(channel->gpio_port, GPIO_MODE_ANALOG,
				GPIO_PUPD_NONE, (1U << channel->gpio_pin));
	}

	const bl_acq_channel_config_t *config = &channel->config;

	if (channel->opamp_used && (channel->opamp != NULL)) {
		bl_acq_opamp_enable(*(channel->opamp));
	} else if (channel->adc != NULL) {
		bl_acq_adc_enable(*(channel->adc), channel->adc_ccr_flag);
	}

	/* Initialize message queue. */
	channel->msg = bl_mq_acquire(source);
	if (channel->msg != NULL) {
		channel->msg->type = channel->config.sample32 ?
			BL_MSG_SAMPLE_DATA32 : BL_MSG_SAMPLE_DATA16;
		channel->msg->sample_data.channel  = source;
		channel->msg->sample_data.count    = 0;
		channel->msg->sample_data.reserved = 0x00;
	}

	channel->enable = true;
}

void bl_acq_channel_disable(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if ((channel->adc != NULL) && (channel->config.opamp_gain <= 1)) {
		bl_acq_adc_disable(*(channel->adc), channel->adc_ccr_flag);
	} else if (channel->opamp != NULL) {
		bl_acq_opamp_disable(*(channel->opamp));
	}

	/* Don't need to do anything here to disable GPIO (it is floating). */

	channel->enable = false;

	/* Send remaining queued samples. */
	union bl_msg_data *msg = channel->msg;
		if (msg != NULL) {
		bool pending;
		switch (msg->type) {
		case BL_MSG_SAMPLE_DATA16:
		case BL_MSG_SAMPLE_DATA32:
			pending = (msg->sample_data.count > 0);
			break;
		default:
			pending = false;
			break;
		}

		if (pending) {
			bl_mq_commit(source);
		}

		channel->msg = NULL;
	}
}


bool bl_acq_channel_is_enabled(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];
	return channel->enable;
}


bl_acq_channel_config_t *bl_acq_channel_get_config(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];
	return &channel->config;
}

bl_acq_timer_t * bl_acq_channel_get_timer(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if (!channel->config.enable) {
		return NULL;
	}

	if (channel->opamp_used) {
		bl_acq_adc_t *adc = bl_acq_opamp_get_adc(
				*(channel->opamp), NULL);
		return bl_acq_adc_get_timer(adc);
	}

	return bl_acq_adc_get_timer(*(channel->adc));
}

bl_acq_opamp_t * bl_acq_channel_get_opamp(enum bl_acq_source source)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if (!channel->config.enable || !channel->opamp_used) {
		return NULL;
	}

	return *(channel->opamp);
}

#if (BL_REVISION >= 2)
bl_acq_dac_t * bl_acq_channel_get_dac(enum bl_acq_source source,
	uint8_t *dac_channel)
{
	bl_acq_opamp_t *opamp = bl_acq_channel_get_opamp(source);
	if (opamp == NULL) {
		return NULL;
	}

	return bl_acq_opamp_get_dac(opamp, dac_channel);
}
#endif

bl_acq_adc_t * bl_acq_channel_get_adc(enum bl_acq_source source,
	uint8_t *adc_channel)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];

	if (!channel->config.enable) {
		return NULL;
	}

	if (channel->opamp_used) {
		return bl_acq_opamp_get_adc(*(channel->opamp), adc_channel);
	}

	if (adc_channel != NULL) {
		*adc_channel = channel->adc_channel;
	}
	return *(channel->adc);
}


static inline uint16_t bl_acq_sample_pack16(
		uint32_t sample, uint32_t offset, uint8_t shift)
{
	if (sample < offset) return 0;
	sample -= offset;
	sample >>= shift;
	return (sample > 0xFFFF) ? 0xFFFF : sample;
}

void bl_acq_channel_commit_sample(enum bl_acq_source source, uint32_t sample)
{
	bl_acq_channel_t *channel = &bl_acq_channel[source];
	const bl_acq_channel_config_t *config = &channel->config;

	union bl_msg_data *msg = channel->msg;
	/* Note: We don't check for NULL due to cost. */

	if (config->sample32) {
		uint8_t count = msg->sample_data.count++;
		msg->sample_data.data32[count] = sample;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA32_MAX) {
			bl_mq_commit(source);
			msg = bl_mq_acquire(source);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA32;
			msg->sample_data.channel  = source;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			channel->msg = msg;
		}
	} else {
		uint8_t count = msg->sample_data.count++;
		uint16_t sample16 = bl_acq_sample_pack16(sample,
				config->sw_offset, config->sw_shift);
		msg->sample_data.data16[count] = sample16;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA16_MAX) {
			bl_mq_commit(source);
			msg = bl_mq_acquire(source);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA16;
			msg->sample_data.channel  = source;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			channel->msg = msg;
		}
	}
}

