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

#include <stdbool.h>

#include "common/acq.h"
#include "common/msg.h"

#include "led.h"

union bl_msg_data;

#if (BL_REVISION > 1)
#define BL_ACQ__SRC_COUNT 8
#else
#define BL_ACQ__SRC_COUNT 7
#endif

/**
 * Initialise the acquisition module.
 */
void bl_acq_init(uint32_t clock);

/**
 * Start an acquisition.
 *
 * \param[in]  detection_mode Acquisition detection mode.
 * \param[in]  flash_mode     Acquisition flash mode.
 * \param[in]  frequency      Sampling frequency.
 * \param[in]  led_mask       Mask of LEDs to enable.
 * \param[in]  src_mask       Mask of sources to enable.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_start(
		enum bl_acq_detection_mode detection_mode,
		enum bl_acq_flash_mode flash_mode,
		uint16_t frequency,
		uint16_t led_mask,
		uint16_t src_mask);

/**
 * Set the per-source configuration
 *
 * \param[in]  source         Channel (source) to configure
 * \param[in]  opamp_gain     Hardware gain.
 * \param[in]  opamp_offset   Hardware offset.
 * \param[in]  sw_oversample  Software oversample.
 * \param[in]  hw_oversample  Hardware oversample.
 * \param[in]  hw_shift       Hardware shift.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_source_conf(
		uint8_t  source,
		uint8_t  opamp_gain,
		uint16_t opamp_offset,
		uint16_t sw_oversample,
		uint8_t  hw_oversample,
		uint8_t  hw_shift);

/**
 * Get the per-source capabilities
 *
 * \param[in]   source    Channel (source) to get capabilites of.
 * \param[out]  response  Response message to request.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_source_cap(
		uint8_t  source,
		bl_msg_source_cap_t *response);

/**
 * Set the per-channel configuration
 *
 * \param[in]  channel   Channel to configure
 * \param[in]  source    Acquisition source associated with the channel.
 * \param[in]  shift     Bits to shift sample values by (divides by (2^shift)
 * \param[in]  offset    Amount to offset sample values by
 * \param[in]  saturate  Whether to enable sample saturation.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_channel_conf(
		uint8_t  channel,
		uint8_t  source,
		uint8_t  shift,
		uint32_t offset,
		bool     sample32);

/**
 * Abort an acquisition.
 *
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_abort(void);

#endif
