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
 * \brief Interface to the data module.
 *
 * This module handles incoming sample data.
 */

#ifndef BV_DATA_H
#define BV_DATA_H

/**
 * Finish a data processing session.
 */
void data_finish(void);

/**
 * Start a data processing session.
 *
 * \param[in]  calibrate     Whether this is a calibration acquisition.
 * \param[in]  frequency     The sampling frequency.
 * \param[in]  channel_mask  The channel mask.
 * \return true on success, false on error.
 */
bool data_start(bool calibrate, unsigned frequency, unsigned channel_mask);

/**
 * Handle a BL_MSG_SAMPLE_DATA16 message.
 *
 * \param[in]  msg  The sample message to process.
 * \return true on success, false on error.
 */
bool data_handle_msg_u16(const bl_msg_sample_data_t *msg);

/**
 * Handle a BL_MSG_SAMPLE_DATA32 message.
 *
 * \param[in]  msg  The sample message to process.
 * \return true on success, false on error.
 */
bool data_handle_msg_u32(const bl_msg_sample_data_t *msg);

#endif /* BV_DATA_H */
