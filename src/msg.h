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
#include <inttypes.h>
#include <stddef.h>

/**
 * Maximum number of channels a \ref BL_MSG_SAMPLE_DATA16 message can contain.
 */
#define MSG_SAMPLE_DATA16_MAX 30

/**
 * Maximum number of channels a \ref BL_MSG_SAMPLE_DATA32 message can contain.
 */
#define MSG_SAMPLE_DATA32_MAX 15

/** Message type. */
enum bl_msg_type {
	BL_MSG_RESPONSE,
	BL_MSG_LED,
	BL_MSG_SOURCE_CONF,
	BL_MSG_CHANNEL_CONF,
	BL_MSG_START,
	BL_MSG_ABORT,
	BL_MSG_SAMPLE_DATA16,
	BL_MSG_SAMPLE_DATA32,

	BL_MSG__COUNT
};

/** Data for \ref BL_MSG_RESPONSE. */
typedef struct {
	uint8_t  type;        /**< Must be \ref BL_MSG_RESPONSE */
	uint8_t  response_to; /**< Type of message response is to. */
	uint16_t error_code;  /**< Result of command. */
} bl_msg_response_t;

/**
 * Data for \ref BL_MSG_LED.
 *
 * This simply turns LEDs on and off.
 *
 * While the interface allows multiple LEDs to be on simultaniously,
 * it may not be possible to achieve on the hardware, due to power
 * limitations.  Setting a value of 0x00 turns all LEDs off.  The
 * least significant bit is LD1.
 */
typedef struct {
	uint8_t  type;     /**< Must be \ref BL_MSG_LED */
	uint16_t led_mask; /**< One bit per LED. */
} bl_msg_led_t;

typedef struct {
	uint8_t  type;
	uint8_t  source;
	uint8_t  opamp_gain;
	uint16_t opamp_offset;
	uint16_t sw_oversample;
	uint8_t  hw_oversample;
	uint8_t  hw_shift;
} bl_msg_source_conf_t;

typedef struct {
	uint8_t  type;
	uint8_t  channel;
	uint8_t  source;
	uint8_t  shift;
	uint32_t offset;
	uint8_t  sample32;
} bl_msg_channel_conf_t;

/**
 * Data for \ref BL_MSG_START.
 */
typedef struct {
	uint8_t  type;       /**< Must be \ref BL_MSG_START */
	uint16_t frequency;  /**< Sampling rate in Hz. */
	uint16_t led_mask;   /**< Mask of LEDs to use. */
	uint16_t src_mask;   /**< Mask of sources to enable. */
} bl_msg_start_t;

/** Data for \ref BL_MSG_ABORT. */
typedef struct {
	uint8_t type; /**< Must be \ref BL_MSG_ABORT */
} bl_msg_abort_t;

/** Data for \ref BL_MSG_SAMPLE_DATA16 and \ref BL_MSG_SAMPLE_DATA32. */
typedef struct {
	uint8_t  type;     /**< Must be \ref BL_MSG_SAMPLE_DATA */
	uint8_t  channel;  /**< Channel of sample data. */
	uint8_t  count;    /**< Number of samples in packet. */
	uint8_t  reserved;

	union {
		uint16_t data16[MSG_SAMPLE_DATA16_MAX]; /**< Sample data for \ref count samples. */
		uint32_t data32[MSG_SAMPLE_DATA32_MAX]; /**< Sample data for \ref count samples. */
	};
} bl_msg_sample_data_t;

/** Message data */
union bl_msg_data {
	/** Message type. */
	uint8_t type;

	bl_msg_response_t     response;
	bl_msg_led_t          led;
	bl_msg_source_conf_t  source_conf;
	bl_msg_channel_conf_t channel_conf;
	bl_msg_start_t        start;
	bl_msg_abort_t        abort;
	bl_msg_sample_data_t  sample_data;
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
	static const uint8_t len_table[BL_MSG__COUNT] = {
		[BL_MSG_RESPONSE]      = BL_SIZEOF_MSG(response),
		[BL_MSG_LED]           = BL_SIZEOF_MSG(led),
		[BL_MSG_SOURCE_CONF]   = BL_SIZEOF_MSG(source_conf),
		[BL_MSG_CHANNEL_CONF]  = BL_SIZEOF_MSG(channel_conf),
		[BL_MSG_START]         = BL_SIZEOF_MSG(start),
		[BL_MSG_ABORT]         = BL_SIZEOF_MSG(abort),
		[BL_MSG_SAMPLE_DATA16] = BL_SIZEOF_MSG(sample_data),
		[BL_MSG_SAMPLE_DATA32] = BL_SIZEOF_MSG(sample_data),
	};

	if (type >= BL_MSG__COUNT) {
		return 0;
	}

	uint8_t len = len_table[type];

	switch (type) {
	case BL_MSG_SAMPLE_DATA16:
		len -= sizeof(((union bl_msg_data *)NULL)->sample_data.data16);
		break;
	case BL_MSG_SAMPLE_DATA32:
		len -= sizeof(((union bl_msg_data *)NULL)->sample_data.data32);
		break;
	default:
		break;
	}

	return len;
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

	switch (msg->type) {
	case BL_MSG_SAMPLE_DATA16:
		len += msg->sample_data.count * sizeof(uint16_t);
		break;
	case BL_MSG_SAMPLE_DATA32:
		len += msg->sample_data.count * sizeof(uint32_t);
		break;
	default:
		break;
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
