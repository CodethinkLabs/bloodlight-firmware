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

/**
 * \file
 * \brief Interface to calibration filter.
 *
 * This module works out the correct channel settings for 16-bit acquisitions.
 */

#ifndef BV_DATA_CAL_H
#define BV_DATA_CAL_H

/**
 * Finalise a given data calibration filter session.
 *
 * \param[in]  pw  Data calibration filter session context.
 */
void data_cal_fini(void *pw);

/**
 * Create a data calibration filter session.
 *
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  src_mask   Mask of enabled sources.
 * \return a context pointer on success, or NULL on error.
 */
void *data_cal_init(
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask);

/**
 * Handle a sample for a given channel.
 *
 * \param[in]  pw       The data calibration filter context.
 * \param[in]  channel  The channel index.
 * \param[in]  sample   The sample value to filter.
 * \return true on success, false on error.
 */
uint32_t data_cal_proc(
		void *pw,
		unsigned channel,
		uint32_t sample);

#endif /* BV_DATA_CAL_H */
