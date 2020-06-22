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

/** OpAmp count */
#define BL_ACQ_OPAMP__COUNT 4

/** Photodiode count */
#define BL_ACQ_PD__COUNT 4

/**
 * Acquisition sources.
 *
 * These are the four photodiodes and anything else we want to sample for
 * debug / reference.
 */
enum bl_acq_source {
	BL_ACQ_PD1,
	BL_ACQ_PD2,
	BL_ACQ_PD3,
	BL_ACQ_PD4,
	BL_ACQ_3V3,
	BL_ACQ_5V0,
	BL_ACQ_TMP,
};

/**
 * Initialise the acquisition module.
 */
void bl_acq_init(void);

/**
 * Start an acquisition.
 *
 * \param[in]  period      Sample timer perieod.
 * \param[in]  prescale    Sample timer prescale.
 * \param[in]  oversample  Number of bits to oversample by.
 * \param[in]  src_mask    Mask of sources to enable.
 * \param[in]  gain        Gain value for each photodiode.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_start(
		uint16_t period,
		uint16_t prescale,
		uint8_t  oversample,
		uint16_t src_mask,
		const uint8_t gain[BL_ACQ_PD__COUNT]);

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
