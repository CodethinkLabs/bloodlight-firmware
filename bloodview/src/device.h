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

#ifndef BV_DEVICE_H
#define BV_DEVICE_H

typedef enum device_state {
	DEVICE_STATE_NONE,
	DEVICE_STATE_IDLE,
	DEVICE_STATE_ACTIVE,
} device_state_t;

/**
 * Callback for notification of device state changes.
 *
 * \param[in]  pw     Client private context data.
 * \param[in]  state  The state that we've changed to.
 */
typedef void (* device_state_change_cb)(
		void *pw, device_state_t state);

/**
 * Initialise the device module.
 *
 * This will try to connect to a device, and fail if it can't.
 *
 * \param[in]  device Node to open, or NULL for device discovery.
 * \param[in]  cb     Callback to handle 
 * \param[in]  pw     Client private context data.
 * \return true on success, or false on error.
 */
bool device_init(
		const char *dev_path,
		device_state_change_cb cb,
		void *pw);

/**
 * Finalise the device module.
 */
void device_fini(
		void);

/**
 * Start calibration.
 *
 * \return true on success, or false on error.
 */
bool device_calibrate_start(
		void);

/**
 * Start acquisition.
 *
 * \return true on success, or false on error.
 */
bool device_acquisition_start(
		void);

/**
 * Stop acquisition or calibration.
 *
 * \return true on success, or false on error.
 */
bool device_stop(
		void);

#endif /* BV_DEVICE_H */
