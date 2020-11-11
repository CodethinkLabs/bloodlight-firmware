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

#ifndef BL_COMMON_ACQ_H
#define BL_COMMON_ACQ_H

/** SPI mode **/
extern enum bl_acq_spi_mode {
	BL_ACQ_SPI_NONE,
	BL_ACQ_SPI_MOTHER,
	BL_ACQ_SPI_DAUGHTER,
	BL_ACQ_SPI_INIT,
} bl_spi_mode;

/** Acquisition detection mode **/
enum bl_acq_detection_mode {
	BL_ACQ_REFLECTIVE,
	BL_ACQ_TRANSMISSIVE,
};

/** Acquisition flash mode */
enum bl_acq_flash_mode {
	BL_ACQ_CONTINUOUS,
	BL_ACQ_FLASH,
};

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
	BL_ACQ_EXT,

	BL_ACQ_SOURCE_MAX
};

#endif
