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

#include "error.h"
#include "util.h"
#include "bl.h"
#include "msg.h"

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
 * Handle the set gains message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_set_gains(const union bl_msg_data *msg)
{
	return bl_acq_set_gains_setting(
			msg->gain.gain);
}

/**
 * Handle the set oversample message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_set_oversample(const union bl_msg_data *msg)
{
	return bl_acq_set_oversample_setting(
			msg->oversample.oversample);
}

/**
 * Handle the set fixed offset message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_set_fixed_offset(const union bl_msg_data *msg)
{
	return bl_acq_set_fixed_offset_setting(
			msg->offset.offset);
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
			msg->start.frequency,
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
		[BL_MSG_LED]   = bl_msg_led,
		[BL_MSG_START] = bl_msg_start,
		[BL_MSG_ABORT] = bl_msg_abort,
		[BL_MSG_SET_GAINS] = bl_msg_set_gains,
		[BL_MSG_SET_OVERSAMPLE] = bl_msg_set_oversample,
		[BL_MSG_SET_FIXEDOFFSET] = bl_msg_set_fixed_offset,
	};

	if (msg->type >= BL_ARRAY_LEN(fns) || fns[msg->type] == NULL) {
		return BL_ERROR_BAD_MESSAGE_TYPE;
	}

	return fns[msg->type](msg);
}
