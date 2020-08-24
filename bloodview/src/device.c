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

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>

#include "../../src/msg.h"
#include "../../tools/msg.h"
#include "../../tools/device.h"

#include "data.h"
#include "util.h"
#include "device.h"
#include "main-menu.h"

/** Special code for indicating the start of an acquisition. */
#define MSG_START_SPECIAL_CAL ((enum bl_msg_type) 255)

/** Special code for indicating the start of a calibration. */
#define MSG_START_SPECIAL_ACQ ((enum bl_msg_type) 254)

/** A locked unsigned integer. */
typedef struct locked_uint {
	pthread_mutex_t lock;
	unsigned        value;
} locked_uint_t;

/**
 * Check whether a locked value is equal to a given value.
 *
 * \param[in]  lu     Locked unsigned value to check.
 * \param[in]  value  The value to compare against.
 * \return true if the locked unsigned value is equal to the given value.
 */
static inline bool locked_uint_is_equal(
		locked_uint_t *lu,
		unsigned value)
{
	bool ret;

	pthread_mutex_lock(&lu->lock);
	ret = (lu->value == value);
	pthread_mutex_unlock(&lu->lock);

	return ret;
}

/**
 * Increment a locked unsigned value.
 *
 * \param[in]  lu  Locked unsigned value to change.
 */
static inline void locked_uint_inc(
		locked_uint_t *lu)
{
	pthread_mutex_lock(&lu->lock);
	lu->value++;
	pthread_mutex_unlock(&lu->lock);
}

/**
 * Decrement a locked unsigned value.
 *
 * \param[in]  lu  Locked unsigned value to change.
 */
static inline void locked_uint_dec(
		locked_uint_t *lu)
{
	pthread_mutex_lock(&lu->lock);
	lu->value--;
	pthread_mutex_unlock(&lu->lock);
}

/**
 * Set a locked unsigned value.
 *
 * \param[in]  lu     Locked unsigned value to change.
 * \param[in]  value  New value.
 * \return true if the locked value changed, false otherwise.
 */
static inline bool locked_uint_set(
		locked_uint_t *lu,
		unsigned value)
{
	bool ret = false;

	pthread_mutex_lock(&lu->lock);
	if (lu->value != value) {
		lu->value = value;
		ret = true;
	}
	pthread_mutex_unlock(&lu->lock);
	return ret;
}

/** Maximum number of messages that can be queued for sending. */
#define MSG_FIFO_MAX 16

/** Device module global context. */
static struct {
	locked_uint_t state;
	int           dev_fd;

	device_state_change_cb  cb;
	void                   *pw;

	volatile bool quit;
	pthread_t     thread_id;

	union bl_msg_data msg[MSG_FIFO_MAX];
	unsigned          msg_next_free;
	unsigned          msg_next_queued;
	locked_uint_t     msg_used;

	FILE *rec;
} bv_device_g;

/**
 * Advance the a send message queue position pointer.
 *
 * \param[in,out]  pos  The position to be advanced.
 */
static void device__msg_advance_ptr(unsigned *pos)
{
	unsigned new_pos = *pos + 1;

	*pos = (new_pos >= MSG_FIFO_MAX) ? 0 : new_pos;
}

/**
 * Get the next free message slot for sending.
 *
 * \return pointer to free message, or NULL on error.
 */
static inline union bl_msg_data *device__msg_get_next_free(void)
{
	if (locked_uint_is_equal(&bv_device_g.msg_used, MSG_FIFO_MAX)) {
		return NULL;
	}

	return &bv_device_g.msg[bv_device_g.msg_next_free];
}

/**
 * Queue a message for sending.
 *
 * \param[in]  msg  The message to be sent.
 */
static inline void device__msg_send(const union bl_msg_data *msg)
{
	BV_UNUSED(msg);

	device__msg_advance_ptr(&bv_device_g.msg_next_free);
	locked_uint_inc(&bv_device_g.msg_used);
}

/**
 * Get the next message queued for sending.
 *
 * \return pointer to the next queued message, or NULL on if there are none.
 */
static inline union bl_msg_data *device__msg_get_next_queued(void)
{
	if (locked_uint_is_equal(&bv_device_g.msg_used, 0)) {
		return NULL;
	}

	return &bv_device_g.msg[bv_device_g.msg_next_queued];
}

/**
 * Mark a message on the queued list as sent.
 *
 * \param[in]  msg  The message that has been sent.
 */
static inline void device__msg_sent(const union bl_msg_data *msg)
{
	BV_UNUSED(msg);

	device__msg_advance_ptr(&bv_device_g.msg_next_queued);
	locked_uint_dec(&bv_device_g.msg_used);
}

/**
 * Check whether the device module has been initialised.
 *
 * \return true if the device module has been initialised.
 */
static inline bool device__is_initialised(void)
{
	return !locked_uint_is_equal(&bv_device_g.state, DEVICE_STATE_NONE);
}

/**
 * Ensure the device has been initialised.
 *
 * \return true if the device module has been initialised.
 */
static bool device__ensure_initialised(
		const char *func)
{
	if (!device__is_initialised()) {
		fprintf(stderr, "%s: Device module not initialised.\n", func);
		return false;
	}

	return true;
}

/**
 * Set the device state, and call client callback on changes.
 *
 * \param[in]  state  The state to change to.
 * \return true if the state changed, or false otherwise.
 */
static inline bool device__set_state(
		device_state_t state)
{
	bool changed = locked_uint_set(&bv_device_g.state, state);

	if (changed) {
		bv_device_g.cb(bv_device_g.pw, state);
	}

	return changed;
}

/**
 * Get the current device state.
 *
 * Note, this is just a snapshot of the state.  It can be changed in
 * another thread, and there is nothing to ensure that the state
 * will still be the same when the caller uses the returned value.
 *
 * \return the device state snapshot.
 */
static inline device_state_t device__get_current_state(void)
{
	return bv_device_g.state.value;
}

/**
 * Open a recording file.
 *
 * Filenames take the forms:
 *
 * * "YYYY-MM-DD HH:MM:SS-cal.yaml"
 * * "YYYY-MM-DD HH:MM:SS-acq.yaml"
 *
 * Files are created in the current working directory.
 *
 * \param[in]  calibrate  Whether the recording is for a calibration.
 * \return The opened file stream or NULL on error.
 */
static FILE *device__open_recording(bool calibrate)
{
	size_t len;
	time_t rawtime;
	struct tm *timeinfo;
	static char buf[80];

	rawtime = time(NULL);
	if (rawtime == (time_t)-1) {
		return NULL;
	}

	timeinfo = localtime(&rawtime);
	if (timeinfo == NULL) {
		fprintf(stderr, "Error calling localtime: %s\n",
				strerror(errno));
		return NULL;
	}

	len = strftime(buf, sizeof(buf), "%Y-%m-%d.%H:%M:%S", timeinfo);
	assert(len > 0);

	assert(sizeof(buf) > len + 4);
	memcpy(&buf[len], calibrate ? "-cal" : "-acq", 5);
	len += 4;

	assert(sizeof(buf) > len + 5);
	memcpy(&buf[len], ".yaml", 6);

	return fopen(buf, "w+");
}

/**
 * Send a queued message, if any.
 *
 * \param[out]  sent_type  Returns the type of any sent message.
 * \return false on error, or true otherwise.
 */
static bool device__thread_send_msg(
		enum bl_msg_type *sent_type)
{
	static bool calibrating;
	union bl_msg_data *send_msg = device__msg_get_next_queued();
	if (send_msg == NULL) {
		return true;
	}

	if (send_msg->type == MSG_START_SPECIAL_CAL ||
	    send_msg->type == MSG_START_SPECIAL_ACQ) {
		bv_device_g.rec = device__open_recording(
				send_msg->type == MSG_START_SPECIAL_CAL);
		if (bv_device_g.rec == NULL) {
			fprintf(stderr,
				"Warning: Failed to open recording file.\n");
		}

		calibrating = (send_msg->type == MSG_START_SPECIAL_CAL);
		device__msg_sent(send_msg);
		return true;
	}

	if (bl_msg_write(bv_device_g.dev_fd, "Discovered device", send_msg)) {
		*sent_type = send_msg->type;
		if (send_msg->type == BL_MSG_START) {
			if (!data_start(calibrating,
					send_msg->start.frequency,
					send_msg->start.src_mask)) {
				/* TODO: If this fails, we'll block on
				 * never clearing this message. */
				return false;
			}
		}

		if (bv_device_g.rec != NULL) {
			bl_msg_yaml_print(bv_device_g.rec, send_msg);
		}
		bl_msg_yaml_print(stderr, send_msg);

		device__msg_sent(send_msg);
	}

	return true;
}

/**
 * Handle an incoming response message sent from the device.
 *
 * \param[in]  sent_type  The type of any outstanding sent message.
 * \param[in]  recv_msg   The incoming response message to handle.
 * \return BL_MSG__COUNT if the \ref recv_msg was a response to \ref sent_type,
 *         or \ref sent_type otherwise.
 */
static enum bl_msg_type device__thread_receive_msg_response(
		const enum bl_msg_type  *sent_type,
		const union bl_msg_data *recv_msg)
{
	if (recv_msg->response.response_to == *sent_type) {
		if (bv_device_g.rec != NULL) {
			bl_msg_yaml_print(bv_device_g.rec, recv_msg);
		}

		switch (*sent_type) {
		case BL_MSG_START:
			if (recv_msg->response.error_code == BL_ERROR_NONE) {
				device__set_state(DEVICE_STATE_ACTIVE);
			} else {
				data_finish();
			}
			break;

		case BL_MSG_ABORT:
			if (recv_msg->response.error_code == BL_ERROR_NONE) {
				device__set_state(DEVICE_STATE_IDLE);
				data_finish();

				if (bv_device_g.rec != NULL) {
					fclose(bv_device_g.rec);
					bv_device_g.rec = NULL;
				}
			}
			break;
		default:
			break;
		}

		bl_msg_yaml_print(stderr, recv_msg);
		return BL_MSG__COUNT;
	}

	bl_msg_yaml_print(stderr, recv_msg);
	return *sent_type;
}

/**
 * Handle an incoming message sent from the device.
 *
 * \param[in,out]  sent_type  The type of any outstanding sent message.
 *                            Returns BL_MSG__COUNT if the incoming message
 *                            was a response to this type.
 */
static void device__thread_receive_msg(
		enum bl_msg_type *sent_type)
{
	union bl_msg_data recv_msg;

	if (bl_msg_read(bv_device_g.dev_fd, 100, &recv_msg)) {
		switch (recv_msg.type) {
		case BL_MSG_RESPONSE:
			*sent_type = device__thread_receive_msg_response(
					sent_type, &recv_msg);

			break;
		case BL_MSG_SAMPLE_DATA16:
			data_handle_msg_u16(&recv_msg.sample_data);
			if (bv_device_g.rec != NULL) {
				bl_msg_yaml_print(bv_device_g.rec, &recv_msg);
			}

			break;
		case BL_MSG_SAMPLE_DATA32:
			data_handle_msg_u32(&recv_msg.sample_data);
			if (bv_device_g.rec != NULL) {
				bl_msg_yaml_print(bv_device_g.rec, &recv_msg);
			}

			break;
		default:
			fprintf(stderr, "Unexpected message from device:\n");
			bl_msg_yaml_print(stderr, &recv_msg);
		}
	}
}

/**
 * A separate thread for handling communication with the device.
 *
 * All of the messages sending and receiving happens in this thread.
 * It also drives the data pipeline to pass new data into the data
 * module.
 *
 * \param[in]  ctx  The device module global context.
 * \return Device module global context on successful exit, or NULL on error.
 */
static void *device__thread(void *ctx)
{
	enum bl_msg_type sent_type = BL_MSG__COUNT;

	if (ctx != &bv_device_g) {
		return NULL;
	}

	while (bv_device_g.quit == false ) {
		if (sent_type == BL_MSG__COUNT) {
			/* Not already waiting for outstanding response. */
			if (!device__thread_send_msg(&sent_type)) {
				fprintf(stderr, "Fatal error\n");
				return NULL;
			}
		}

		if ((sent_type != BL_MSG__COUNT) ||
		    (device__get_current_state() == DEVICE_STATE_ACTIVE)) {
			/* Awaiting response to sent message,
			 * or running an acquisition. */
			device__thread_receive_msg(&sent_type);
		}
	}

	return ctx;
}

/**
 * Send a start indicator to the device thread.
 *
 * This is a notification that we are about to start a sequence of messages
 * that should represent a recording, it is not a message to be sent itself.
 *
 * \param[in]  calibrate  Whether device is being configured for calibration.
 * \return true if a message has been queued for sending to the device.
 */
static bool device__queue_msg_special_start_indicator(bool calibrate)
{
	union bl_msg_data *msg;

	msg = device__msg_get_next_free();
	if (msg == NULL) {
		return false;
	}

	msg->type = calibrate ? MSG_START_SPECIAL_CAL : MSG_START_SPECIAL_ACQ;

	device__msg_send(msg);
	return true;
}

/**
 * Turn device LED(s) on/off.
 *
 * When turning on, the LED mask is obtained from the main menu configuration.
 *
 * \param[in]  enable_lights  Whether the light(s) should be turned on or off.
 * \return true if a message has been queued for sending to the device.
 */
static bool device__queue_msg_led(bool enable_lights)
{
	union bl_msg_data *msg;
	uint16_t led_mask = 0;

	if (enable_lights) {
		led_mask = main_menu_conifg_get_led_mask();
	}

	msg = device__msg_get_next_free();
	if (msg == NULL) {
		return false;
	}

	msg->type = BL_MSG_LED;
	msg->led.led_mask = led_mask;

	device__msg_send(msg);
	return true;
}

/**
 * Configure a channel on the device.
 *
 * The config is obtained from the main menu configuration.
 *
 * \param[in]  channel    The channel to configure.
 * \param[in]  calibrate  Whether device is being configured for calibration.
 * \return true if a message has been queued for sending to the device.
 */
static bool device__queue_msg_channel_conf(
		uint8_t channel,
		bool    calibrate)
{
	union bl_msg_data *msg;
	bool sample32 = main_menu_conifg_get_channel_sample32(channel);

	msg = device__msg_get_next_free();
	if (msg == NULL) {
		return false;
	}

	if (calibrate) {
		sample32 = true;
	}

	msg->type = BL_MSG_CHANNEL_CONF;
	msg->channel_conf.channel  = channel;
	msg->channel_conf.gain     = main_menu_conifg_get_channel_gain(channel);
	msg->channel_conf.shift    = main_menu_conifg_get_channel_shift(channel);
	msg->channel_conf.offset   = main_menu_conifg_get_channel_offset(channel);
	msg->channel_conf.sample32 = sample32;

	device__msg_send(msg);
	return true;
}

/**
 * Start acquisition on the device.
 *
 * The config is obtained from the main menu configuration.
 *
 * \return true if a message has been queued for sending to the device.
 */
static bool device__queue_msg_start(void)
{
	union bl_msg_data *msg;

	msg = device__msg_get_next_free();
	if (msg == NULL) {
		return false;
	}

	msg->type = BL_MSG_START;
	msg->start.frequency  = main_menu_conifg_get_frequency();
	msg->start.oversample = main_menu_conifg_get_oversample();
	msg->start.src_mask   = main_menu_conifg_get_source_mask();

	device__msg_send(msg);
	return true;
}

/**
 * Abort acquisition on the device.
 *
 * \return true if a message has been queued for sending to the device.
 */
static bool device__queue_msg_abort(void)
{
	union bl_msg_data *msg;

	msg = device__msg_get_next_free();
	if (msg == NULL) {
		return false;
	}

	msg->type = BL_MSG_ABORT;

	device__msg_send(msg);
	return true;
}

/**
 * Configure all enabled channels on the device.
 *
 * The config is obtained from the main menu configuration.
 *
 * \param[in]  calibrate  Whether device is being configured for calibration.
 * \return true if all messages have been queued for sending to the device.
 */
static bool device__queue_channel_conf_messages(bool calibrate)
{
	unsigned source_mask = main_menu_conifg_get_source_mask();

	for (unsigned i = 0; i < BL_ACQ__SRC_COUNT; i++) {
		if (source_mask & (1U << i)) {
			if (!device__queue_msg_channel_conf(i, calibrate)) {
				return false;
			}
		}
	}

	return true;
}

/* Exported function, documented in device.h */
static bool device__start(
		const char *ctx_string,
		bool calibrate)
{
	if (!device__ensure_initialised(ctx_string)) {
		return false;
	}

	if (!device__queue_msg_special_start_indicator(calibrate)) {
		goto error;
	}

	if (!device__queue_msg_led(true)) {
		goto error;
	}

	if (!device__queue_channel_conf_messages(calibrate)) {
		goto error;
	}

	if (!device__queue_msg_start()) {
		goto error;
	}

	return true;

error:
	data_finish();
	return false;
}

/* Exported function, documented in device.h */
bool device_calibrate_start(void)
{
	return device__start(__func__, true);
}

/* Exported function, documented in device.h */
bool device_acquisition_start(void)
{
	return device__start(__func__, false);
}

/* Exported function, documented in device.h */
bool device_stop()
{
	if (!device__is_initialised()) {
		return false;
	}

	/* Don't check for DEVICE_STATE_ACTIVE state here.  The device may
	 * have got out of sync, so it's always useful to be able to send
	 * the abort message.
	 */

	if (!device__queue_msg_abort()) {
		return false;
	}

	if (!device__queue_msg_led(false)) {
		return false;
	}

	return true;
}

/* Exported function, documented in device.h */
bool device_init(
		const char *dev_path,
		device_state_change_cb cb,
		void *pw)
{
	int dev_fd;
	int ret;

	if (device__is_initialised()) {
		fprintf(stderr, "%s: Device module is already initialised.\n",
				__func__);
		return false;
	}

	dev_fd = bl_device_open(dev_path);
	if (dev_fd < 0) {
		return false;
	}

	bv_device_g.dev_fd = dev_fd;
	bv_device_g.cb = cb;
	bv_device_g.pw = pw;

	if (!device__set_state(DEVICE_STATE_IDLE)) {
		memset(&bv_device_g.thread_id, 0,
				sizeof(bv_device_g.thread_id));
		bl_device_close(bv_device_g.dev_fd);
		return false;
	}

	bv_device_g.quit = false;
	ret = pthread_create(&bv_device_g.thread_id, NULL,
			&device__thread, &bv_device_g);
	if (ret != 0) {
		memset(&bv_device_g.thread_id, 0,
				sizeof(bv_device_g.thread_id));
		bl_device_close(bv_device_g.dev_fd);
		return false;
	}

	return true;
}

/* Exported function, documented in device.h */
void device_fini(void)
{
	void *thread_ret = NULL;
	int ret;

	if (!device__is_initialised()) {
		return;
	}

	if (locked_uint_is_equal(&bv_device_g.state, DEVICE_STATE_ACTIVE)) {
		device_stop();
	}

	while (device__msg_get_next_queued() != NULL) {
		/* Ensure the stop messages are sent before quitting. */
	}

	bv_device_g.quit = true;
	ret = pthread_join(bv_device_g.thread_id, &thread_ret);
	if (ret != 0) {
		fprintf(stderr, "Error: Failed to join device thread (%i)",
				ret);
	}
	device__set_state(DEVICE_STATE_NONE);

	memset(&bv_device_g.thread_id, 0,
			sizeof(bv_device_g.thread_id));

	if (bv_device_g.rec != NULL) {
		fclose(bv_device_g.rec);
		bv_device_g.rec = NULL;
	}

	bl_device_close(bv_device_g.dev_fd);
	bv_device_g.dev_fd = 0;
	bv_device_g.cb = NULL;
	bv_device_g.pw = NULL;
}