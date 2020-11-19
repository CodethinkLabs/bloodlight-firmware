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

static int hex_to_int(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/**
 * Handle the LED message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_led(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_UNUSED(response);
	return bl_led_set(msg->led.led_mask);
}

/**
 * Handle the source config message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_source_conf(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_UNUSED(response);
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
static enum bl_error bl_msg_channel_conf(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_UNUSED(response);
	return bl_acq_channel_conf(
			msg->channel_conf.channel,
			msg->channel_conf.source,
			msg->channel_conf.shift,
			msg->channel_conf.offset,
			(msg->channel_conf.sample32 != 0));
}

/**
 * Handle the source capability request message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_source_cap(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	return bl_acq_source_cap(
			msg->source_cap_req.source, &response->source_cap);
}

static enum bl_error bl_msg_version(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_STATIC_ASSERT(sizeof(BL_COMMIT_SHA) - 1 == COMMIT_SHA_LENGTH * 8);
	/* commit sha needs to be interpreted as a string then stored in a buffer to be interpreted as hex */
	char commit_sha[COMMIT_SHA_LENGTH * 8] = BL_COMMIT_SHA;
	BL_UNUSED(msg);

	response->version.type = BL_MSG_VERSION;
	response->version.revision = BL_REVISION;
	for (unsigned i = 0; i < COMMIT_SHA_LENGTH; i++) {
		response->version.commit_sha[i] = 0;
		/* Convert 8-character hex string into uint32 */
		for (unsigned j = 0; j < 8; j++) {
			int v = hex_to_int(BL_COMMIT_SHA[i * 8 + j]);
			if (v < 0) {
				/* If this happens, BL_COMMIT_SHA is probably malformed */
				return BL_ERROR_OUT_OF_RANGE;
			}
			response->version.commit_sha[i] |= v << 4 * (7 - j);
		}
	}
	return BL_ERROR_NONE;
}

/**
 * Handle the Acquisition Start message.
 *
 * \param[in]  msg  Message to handle.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
static enum bl_error bl_msg_start(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_UNUSED(response);
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
static enum bl_error bl_msg_abort(
		const union bl_msg_data *msg,
		union bl_msg_data *response)
{
	BL_UNUSED(msg);
	BL_UNUSED(response);

	return bl_acq_abort();
}

/* Exported function, documented in msg.h */
bool bl_msg_handle(const union bl_msg_data *msg, union bl_msg_data *response)
{
	typedef enum bl_error (* bl_msg_handle_fn)(
			const union bl_msg_data *msg,
			union bl_msg_data *response);

	/* This may be overwritten by handling functions. */
	response->response.type        = BL_MSG_RESPONSE;
	response->response.response_to = msg->type;
	response->response.error_code  = BL_ERROR_NONE;

	static const bl_msg_handle_fn fns[] = {
		[BL_MSG_LED]            = bl_msg_led,
		[BL_MSG_START]          = bl_msg_start,
		[BL_MSG_ABORT]          = bl_msg_abort,
		[BL_MSG_SOURCE_CONF]    = bl_msg_source_conf,
		[BL_MSG_CHANNEL_CONF]   = bl_msg_channel_conf,
		[BL_MSG_SOURCE_CAP_REQ] = bl_msg_source_cap,
		[BL_MSG_VERSION_REQ]    = bl_msg_version,
	};

	if (msg->type >= BL_ARRAY_LEN(fns) || fns[msg->type] == NULL) {
		response->response.error_code = BL_ERROR_BAD_MESSAGE_TYPE;
		return true;
	}

	enum bl_error error = fns[msg->type](msg, response);
	if (error != BL_ERROR_NONE) {
		response->response.error_code = error;
	}

	return true;
}
