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
 * \brief Interface to sample averaging filter.
 *
 * This module averages samples over a given range.
 */

#ifndef BV_DATA_AVG_H
#define BV_DATA_AVG_H

/**
 * The data averaging filter's configuration.
 *
 * The filter frequency indicates with width of the window to average samples
 * over.
 */
struct data_avg_config {
	unsigned filter_freq; /**< Filter frequency in 1/1024ths of a Hz */
	bool     normalise;   /**< Whether to normalise samples. */
};

/**
 * Finalise a given data averaging filter session.
 *
 * \param[in]  pw  Data averaging filter session context.
 */
void data_avg_fini(void *pw);

/**
 * Create a data averaging filter session.
 *
 * \param[in]  config     Filter-specific configuration.
 * \param[in]  frequency  The sampling frequency.
 * \param[in]  channels   The number of channels.
 * \param[in]  src_mask   Mask of enabled sources.
 * \return a context pointer on success, or NULL on error.
 */
void *data_avg_init(
		const struct data_avg_config *config,
		unsigned frequency,
		unsigned channels,
		uint32_t src_mask);

/**
 * Handle a sample for a given channel.
 *
 * \param[in]  pw       The data averaging filter context.
 * \param[in]  channel  The channel index.
 * \param[in]  sample   The sample value to filter.
 * \return true on success, false on error.
 */
uint32_t data_avg_proc(
		void *pw,
		unsigned channel,
		uint32_t sample);

#endif /* BV_DATA_AVG_H */
