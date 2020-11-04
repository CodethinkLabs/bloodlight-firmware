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

#ifndef BL_COMMON_ERROR_H
#define BL_COMMON_ERROR_H

/** Error codes. */
enum bl_error {
	BL_ERROR_NONE,                /**< Success. */
	BL_ERROR_OUT_OF_RANGE,        /**< Value is out of allowed range. */
	BL_ERROR_BAD_MESSAGE_TYPE,    /**< Unknown message type. */
	BL_ERROR_BAD_MESSAGE_LENGTH,  /**< Unexpected message length. */
	BL_ERROR_BAD_SOURCE_MASK,     /**< No sources would be sampled. */
	BL_ERROR_MODE_MISMATCH,       /**< The acquisition mode mismatches with led_mask. */
	BL_ERROR_ACTIVE_ACQUISITION,  /**< There is an active acquisition. */
	BL_ERROR_BAD_FREQUENCY,       /**< Frequency combo not supported. */
	BL_ERROR_NOT_IMPLEMENTED,     /**< Feature not implemented. */
	BL_ERROR_HARDWARE_CONFLICT,   /**< Hardware conflict in config. */
	BL_ERROR_ADC_FREQ_TOO_HIGH,   /**< Frequency too high for ADC. */
	BL_ERROR_ADC_DMA_BUFFER,      /**< Config exceeds DMA buffer size. */
	BL_ERROR_DAC_BAD_CHANNEL,     /**< Bad DAC channel. */
	BL_ERROR_DAC_BAD_OFFSET,      /**< Bad DAC offset. */
	BL_ERROR_OPAMP_BAD_GAIN,      /**< Bad opamp gain. */
	BL_ERROR_TIMER_BAD_FREQUENCY, /**< Bad timer frequency. */
};

#endif
