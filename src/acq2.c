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

#include "acq.h"
#include "util.h"

void bl_acq_init(uint32_t clock)
{
	BL_UNUSED(clock);
	/* TODO: Implement */
}

/**
 * Start an acquisition.
 *
 * \param[in]  frequency   Sampling frequency.
 * \param[in]  src_mask    Mask of sources to enable.
 * \param[in]  oversample  Number of bits to oversample by.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_start(
		uint16_t frequency,
		uint32_t oversample,
		uint16_t src_mask)
{
	BL_UNUSED(frequency);
	BL_UNUSED(oversample);
	BL_UNUSED(src_mask);
	/* TODO: Implement */
	return BL_ERROR_NOT_IMPLEMENTED;
}

/**
 * Set the per-channel configuration
 *
 * \param[in]  channel   Channel (source) to configure
 * \param[in]  gain      OpAMP gain to apply for the channel
 * \param[in]  shift     Bits to shift sample values by (divides by (2^shift)
 * \param[in]  offset    Amount to offset sample values by
 * \param[in]  saturate  Whether to enable sample saturation.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_channel_conf(
		uint8_t  channel,
		uint8_t  gain,
		uint8_t  shift,
		uint32_t offset,
		bool     sample32)
{
	BL_UNUSED(channel);
	BL_UNUSED(gain);
	BL_UNUSED(shift);
	BL_UNUSED(offset);
	BL_UNUSED(sample32);
	/* TODO: Implement */
	return BL_ERROR_NOT_IMPLEMENTED;
}

/**
 * Abort an acquisition.
 *
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_acq_abort(void)
{
	/* TODO: Implement */
	return BL_ERROR_NOT_IMPLEMENTED;
}
