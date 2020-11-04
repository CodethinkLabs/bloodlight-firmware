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

#include "common/util.h"

#include "msg.h"
#include "bl.h"
#include "led.h"
#include "acq.h"

/**
 * Handle the LED message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_led(const union bl_msg_data *msg)
{
	return bl_led_set(msg->led.led_mask);
}

/**
 * Handle the source config message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_source_conf(const union bl_msg_data *msg)
{
	return bl_acq_source_conf(
			msg->source_conf.source,
			msg->source_conf.opamp_gain,
			msg->source_conf.opamp_offset,
			msg->source_conf.sw_oversample,
			msg->source_conf.hw_oversample,
			msg->source_conf.hw_shift);
}

/**
 * Handle the channel config message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_channel_conf(const union bl_msg_data *msg)
{
	return bl_acq_channel_conf(
			msg->channel_conf.channel,
			msg->channel_conf.source,
			msg->channel_conf.shift,
			msg->channel_conf.offset,
			(msg->channel_conf.sample32 != 0));
}

/**
 * Handle the Acquisition Start message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_start(const union bl_msg_data *msg)
{
	return bl_acq_start(
			msg->start.detection_mode,
			msg->start.flash_mode,
			msg->start.frequency,
			msg->start.led_mask,
			msg->start.src_mask);
}

/**
 * Handle the Acquisition Abort message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_abort(const union bl_msg_data *msg)
{
	BL_UNUSED(msg);

	return bl_acq_abort();
}

/* Exported function, documented in msg.h */
enum bl_error bl_msg_handle(const union bl_msg_data *msg)
{
	typedef enum bl_error (* bl_msg_handle_fn)(
			const union bl_msg_data *msg);

	static const bl_msg_handle_fn fns[] = {
		[BL_MSG_LED]          = bl_msg_led,
		[BL_MSG_START]        = bl_msg_start,
		[BL_MSG_ABORT]        = bl_msg_abort,
		[BL_MSG_SOURCE_CONF]  = bl_msg_source_conf,
		[BL_MSG_CHANNEL_CONF] = bl_msg_channel_conf,
	};

	if (msg->type >= BL_ARRAY_LEN(fns) || fns[msg->type] == NULL) {
		return BL_ERROR_BAD_MESSAGE_TYPE;
	}

	return fns[msg->type](msg);
}
