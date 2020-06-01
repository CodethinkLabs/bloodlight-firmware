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
 * Handle the LED Test message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_led_test(const union bl_msg_data *msg)
{
	return bl_led_set(msg->led_test.led_mask);
}

/**
 * Handle the Acquisition Setup message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_acq_setup(const union bl_msg_data *msg)
{
	return bl_acq_setup(
			msg->acq_setup.period,
			msg->acq_setup.oversample,
			msg->acq_setup.src_mask,
			msg->acq_setup.gain);
}

/**
 * Handle the Acquisition Start message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_acq_start(const union bl_msg_data *msg)
{
	BL_UNUSED(msg);

	return bl_acq_start();
}

/**
 * Handle the Acquisition Abort message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_acq_abort(const union bl_msg_data *msg)
{
	BL_UNUSED(msg);

	return bl_acq_abort();
}

/**
 * Handle the Reset message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_reset(const union bl_msg_data *msg)
{
	BL_UNUSED(msg);

	bl_init();

	return BL_ERROR_NONE;
}

/* Exported function, documented in msg.h */
enum bl_error bl_msg_handle(const union bl_msg_data *msg)
{
	typedef enum bl_error (* bl_msg_handle_fn)(
			const union bl_msg_data *msg);

	static const bl_msg_handle_fn fns[] = {
		[BL_MSG_LED_TEST]  = bl_msg_led_test,
		[BL_MSG_ACQ_SETUP] = bl_msg_acq_setup,
		[BL_MSG_ACQ_START] = bl_msg_acq_start,
		[BL_MSG_ACQ_ABORT] = bl_msg_acq_abort,
		[BL_MSG_RESET]     = bl_msg_reset,
	};

	if (msg->type >= BL_ARRAY_LEN(fns) || fns[msg->type] == NULL) {
		return BL_ERROR_BAD_MESSAGE_TYPE;
	}

	return fns[msg->type](msg);
}
