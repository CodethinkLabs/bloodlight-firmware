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

#ifndef BL_MSG_H
#define BL_MSG_H

#include "acq.h"
#include "led.h"

enum bl_error;

/** Message type. */
enum bl_msg_type {
	BL_MSG_RESPONSE,
	BL_MSG_LED,
	BL_MSG_SET_GAINS,
	BL_MSG_SET_OVERSAMPLE,
	BL_MSG_SET_FIXEDOFFSET,
	BL_MSG_START,
	BL_MSG_ABORT,
	BL_MSG_SAMPLE_DATA,
	BL_MSG__COUNT,
};

/** Message data */
union bl_msg_data {
	/** Message type. */
	uint8_t type;

	/** Data for \ref BL_MSG_RESPONSE. */
	struct {
		uint8_t  type;        /**< Must be \ref BL_MSG_RESPONSE */
		uint8_t  response_to; /**< Type of message response is to. */
		uint16_t error_code;  /**< Result of command. */
	} response;

	/**
	 * Data for \ref BL_MSG_LED.
	 *
	 * This simply turns LEDs on and off.
	 *
	 * While the interface allows multiple LEDs to be on simultaniously,
	 * it may not be possible to achieve on the hardware, due to power
	 * limitations.  Setting a value of 0x00 turns all LEDs off.  The
	 * least significant bit is \ref BL_LED_ID_0.
	 */
	struct {
		uint8_t  type;     /**< Must be \ref BL_MSG_LED */
		uint16_t led_mask; /**< One bit per LED. */
	} led;

	struct {
		uint8_t type;
		uint8_t gain[BL_ACQ_PD__COUNT]; /**< Photodiode gains. */
	} gain;

	struct {
		uint8_t type;
		uint8_t  oversample;
	} oversample;

	struct {
		uint8_t type;
		uint16_t  offset;
	} offset;
	/**
	 * Data for \ref BL_MSG_SETUP.
	 */
	struct {
		uint8_t  type;           /**< Must be \ref BL_MSG_SETUP */
		uint16_t frequency;      /**< Sampling rate in Hz. */
		uint16_t src_mask;       /**< Mask of sources to enable. */
	} start;

	/** Data for \ref BL_MSG_ABORT. */
	struct {
		uint8_t type; /**< Must be \ref BL_MSG_ABORT */
	} abort;

	/** Data for \ref BL_MSG_SAMPLE_DATA. */
	struct {
		uint8_t  type;     /**< Must be \ref BL_MSG_SAMPLE_DATA */
		uint8_t  count;    /**< Number of samples in packet. */
		uint16_t src_mask; /**< Mask of sources in this message. */
		uint16_t data[];   /**< Sample data for \ref count samples. */
	} sample_data;
};

/**
 * Helper macro to get the size of a given \ref union bl_msg_data member.
 *
 * \param[in]  _msg  Union member to get size of.
 * \return size of given member.
 */
#define BL_SIZEOF_MSG(_msg) \
	(sizeof(((union bl_msg_data *)NULL)->_msg))

/**
 * Get the byte length of the given message type.
 *
 * Note, in the case of \ref BL_MSG_SAMPLE_DATA, this assumes a count of
 * zero samples in the message.
 *
 * \param[in]  type  Message type to get the byte length of.
 * \return byte length of message type, or zero for invalid type.
 */
static inline uint8_t bl_msg_type_to_len(enum bl_msg_type type)
{
	static const uint8_t len[BL_MSG__COUNT] = {
		[BL_MSG_RESPONSE]    = BL_SIZEOF_MSG(response),
		[BL_MSG_LED]         = BL_SIZEOF_MSG(led),
		[BL_MSG_START]       = BL_SIZEOF_MSG(start),
		[BL_MSG_ABORT]       = BL_SIZEOF_MSG(abort),
		[BL_MSG_SAMPLE_DATA] = BL_SIZEOF_MSG(sample_data),
		[BL_MSG_SET_GAINS]   = BL_SIZEOF_MSG(gain),
		[BL_MSG_SET_OVERSAMPLE]   = BL_SIZEOF_MSG(oversample),
		[BL_MSG_SET_FIXEDOFFSET]   = BL_SIZEOF_MSG(offset),
	};

	if (type >= BL_MSG__COUNT) {
		return 0;
	}

	return len[type];
}

/**
 * Get full byte length of a message.
 *
 * Includes any sample data payload.
 *
 * \param[in]  msg  Message data to get length of.
 * \return byte length of message type, or zero for invalid type.
 */
static inline uint8_t bl_msg_len(union bl_msg_data *msg)
{
	uint8_t len = bl_msg_type_to_len(msg->type);

	if (msg->type == BL_MSG_SAMPLE_DATA) {
		len += msg->sample_data.count * sizeof(uint16_t);
	}

	return len;
}

/**
 * Helper to get message type, even if we've not "decoded" yet.
 *
 * \param[in]  msg  Message data to extract type from.
 * \return type message.
 */
static inline enum bl_msg_type bl_msg_get_type(void *msg)
{
	return ((union bl_msg_data *)msg)->type;
}

/**
 * Convert to decoded message.
 *
 * \param[in]  data  Message data to decode.
 * \param[in]  len   Length of message data.
 * \return Cast of `data` to the \ref union bl_msg_data, or NULL if the
 *         message length did not match type.
 */
static inline union bl_msg_data * bl_msg_decode(uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0) {
		return NULL;
	}

	if (bl_msg_type_to_len(bl_msg_get_type(data)) != len) {
		return NULL;
	}

	return (void *)data;
}

/**
 * Handle an incomming message.
 *
 * \param[in]  msg  Incomming message to be handled.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_msg_handle(const union bl_msg_data *msg);

#endif
