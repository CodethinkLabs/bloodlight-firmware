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

#include "channel.h"
#include "source.h"
#include "opamp.h"
#include "adc.h"

#include "../mq.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>

typedef struct  {
	uint32_t sw_offset;
	uint8_t  sw_shift;
	bool     sample32;
} bl_acq_channel_config_t;

typedef struct
{
	enum bl_acq_source source;
	unsigned           led;
	bool               enable;

	union bl_msg_data *msg;

	bl_acq_channel_config_t config;
} bl_acq_channel_t;

static bl_acq_channel_t bl_acq_channel[BL_ACQ_CHANNEL_COUNT] = { 0 };

enum bl_error bl_acq_channel_configure(unsigned channel,
		enum bl_acq_source source,
		bool sample32, uint32_t sw_offset, uint8_t sw_shift)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];
	bl_acq_channel_config_t *config = &chan->config;

	chan->source = source;

	config->sample32  = sample32;
	config->sw_offset = sw_offset;
	config->sw_shift  = sw_shift;
	return BL_ERROR_NONE;
}

void bl_acq_channel_enable(unsigned channel)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];

	if (chan->enable) {
		/* Already enabled. */
		return;
	}

	bl_acq_source_enable(chan->source);

	/* Initialize message queue. */
	chan->msg = bl_mq_acquire(channel);
	if (chan->msg != NULL) {
		chan->msg->type = chan->config.sample32 ?
			BL_MSG_SAMPLE_DATA32 : BL_MSG_SAMPLE_DATA16;
		chan->msg->sample_data.channel  = channel;
		chan->msg->sample_data.count    = 0;
		chan->msg->sample_data.reserved = 0x00;
	}

	chan->enable = true;
}

void bl_acq_channel_disable(unsigned channel)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];

	bl_acq_source_disable(chan->source);

	chan->enable = false;

	/* Send remaining queued samples. */
	union bl_msg_data *msg = chan->msg;
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
			bl_mq_commit(channel);
		}

		chan->msg = NULL;
	}
}

bool bl_acq_channel_is_enabled(unsigned channel)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];
	return chan->enable;
}


enum bl_acq_source bl_acq_channel_get_source(unsigned channel)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];
	return chan->source;
}

static inline uint16_t bl_acq_sample_pack16(
		uint32_t sample, uint32_t offset, uint8_t shift)
{
	if (sample < offset) return 0;
	sample -= offset;
	sample >>= shift;
	return (sample > 0xFFFF) ? 0xFFFF : sample;
}

void bl_acq_channel_commit_sample(unsigned channel, uint32_t sample)
{
	bl_acq_channel_t *chan = &bl_acq_channel[channel];
	const bl_acq_channel_config_t *config = &chan->config;

	union bl_msg_data *msg = chan->msg;
	/* Note: We don't check for NULL due to cost. */

	if (config->sample32) {
		uint8_t count = msg->sample_data.count++;
		msg->sample_data.data32[count] = sample;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA32_MAX) {
			bl_mq_commit(channel);
			msg = bl_mq_acquire(channel);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA32;
			msg->sample_data.channel  = channel;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			chan->msg = msg;
		}
	} else {
		uint8_t count = msg->sample_data.count++;
		uint16_t sample16 = bl_acq_sample_pack16(sample,
				config->sw_offset, config->sw_shift);
		msg->sample_data.data16[count] = sample16;

		if (msg->sample_data.count >= MSG_SAMPLE_DATA16_MAX) {
			bl_mq_commit(channel);
			msg = bl_mq_acquire(channel);

			/* Note: We dont' check for failure to acquire a msg
			 * due to the cost of checking in an interrupt. */

			msg->type = BL_MSG_SAMPLE_DATA16;
			msg->sample_data.channel  = channel;
			msg->sample_data.count    = 0;
			msg->sample_data.reserved = 0;

			chan->msg = msg;
		}
	}
}
