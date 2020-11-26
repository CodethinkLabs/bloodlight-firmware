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
 * \brief Interface to the FIFO module.
 *
 * This provides a simple generic FIFO.
 */

#ifndef BL_HOST_COMMON_FIFO_H
#define BL_HOST_COMMON_FIFO_H

#include <stdint.h>
#include <stdbool.h>

/**
 * A generic FIFO.
 */
struct fifo {
	uint8_t  *values;       /**< Array of FIFO values. */
	uint16_t  element_size; /**< Size of a FIFO value. */
	uint16_t  capacity;     /**< Maximum number of values FIFO can store. */
	uint16_t  write;        /**< Write pointer. */
	uint16_t  read;         /**< Read pointer. */
	uint16_t  used;         /**< Number of values currently in the FIFO. */
};

/**
 * Create a FIFO.
 *
 * The fifo does not own the stored elements, so the caller is responsible
 * for any element-specific destruction.
 *
 * \param[in]  capacity      Maximum number of elements FIFO can store.
 * \param[in]  element_size  Size of a fifo entry in bytes.
 * \return FIFO object or NULL on error.
 */
struct fifo *fifo_create(
		uint16_t capacity,
		uint16_t element_size);

/**
 * Destroy a FIFO object.
 *
 * If FIFO elements need specific destruction, drain the fifo with
 * \ref fifo_read before destroying it.
 *
 * \param[in]  fifo  A FIFO object.
 */
void fifo_destroy(
		struct fifo *fifo);

/**
 * Put a value to a FIFO.
 *
 * \param[in]  fifo   A FIFO object.
 * \param[in]  value  Pointer to value to write into fifo.
 * \return true on success, and false if the FIFO is full.
 */
bool fifo_write(
		struct fifo *fifo,
		const void *value);

/**
 * Extract a value from a FIFO.
 *
 * \param[in]  fifo   A FIFO object.
 * \param[out] value  Pointer to storage to move value into.
 * \return true on success, and false if the FIFO is empty.
 */
bool fifo_read(
		struct fifo *fifo,
		void *value);

/**
 * Peek back at a previously written value.
 *
 * \param[in]  fifo   A FIFO object.
 * \param[in]  steps  Number of steps to look back from most recently written.
 * \param[out] value  Pointer to storage to copy value into.
 * \return true on success, and false if the FIFO doesn't have enough values.
 */
bool fifo_peek_back(
		const struct fifo *fifo,
		uint16_t steps,
		void *value);

#endif
