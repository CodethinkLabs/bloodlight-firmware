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

#ifndef BL_ACQ_H
#define BL_ACQ_H

#include "led.h"

union bl_msg_data;
enum bl_error;

#define BL_ACQ_PD__COUNT 4

/**
 * Acquisition sources.
 */
enum bl_acq_source {
	BL_ACQ_PD1,
	BL_ACQ_PD2,
	BL_ACQ_PD3,
	BL_ACQ_PD4,
	BL_ACQ_3V3,
	BL_ACQ_5V0,
};

/**
 * Initialise the acquisition module.
 */
void bl_acq_init(void);

/**
 * Handle the Sampling Setup message.
 *
 * \param[in]  gain      Gain for all photodiodes.
 * \param[in]  rate      Sampling rate in ms.
 * \param[in]  samples   Number of samples per enabled LED.
 * \param[in]  src_mask  Mask of sources to enable.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_setup(
		uint8_t gain,
		uint16_t rate,
		uint16_t samples,
		uint16_t src_mask);

/**
 * Start an acquisition.
 *
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_start(void);

/**
 * Abort an acquisition.
 *
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_abort(void);

/**
 * Poll the acquisition module.
 */
void bl_acq_poll(void);

#endif
