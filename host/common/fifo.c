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
 * \brief Implementation of the FIFO module.
 *
 * This provides a simple generic FIFO.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "fifo.h"

/**
 * Advance a position pointer value.
 *
 * \param[in]  fifo  The FIFO object.
 * \param[in]  pos   Position value to increase by one.
 * \return new pointer position.
 */
static inline uint16_t fifo__pos_advance(
		const struct fifo *fifo,
		uint16_t pos)
{
	pos++;
	return (pos >= fifo->capacity) ? 0 : pos;
}

/**
 * Seek a position pointer value by a given offset.
 *
 * \param[in]  fifo  The FIFO object.
 * \param[in]  pos   Position value to seek to a new offset.
 * \param[in]  n     Signed number of setps to seek pos by.
 * \return new pointer position.
 */
static inline uint16_t fifo__pos_seek(
		const struct fifo *fifo,
		int32_t pos,
		int32_t n)
{
	assert(abs(n) < fifo->used);

	pos += n;
	if (pos >= fifo->capacity) {
		pos -= fifo->capacity;
	} else if (pos < 0) {
		pos += fifo->capacity;
	}
	return pos;
}

/**
 * Copy a value from a FIFO.
 *
 * \param[in]  fifo   The FIFO object.
 * \param[in]  pos    FIFO position to read from.
 * \param[out] value  Pointer to storage to copy value into.
 */
static inline void fifo__copy_from(
		const struct fifo *fifo,
		uint16_t pos,
		void *value)
{
	size_t element_size = fifo->element_size;

	memcpy(value, fifo->values + pos * element_size, element_size);
}

/**
 * Copy a value to a FIFO.
 *
 * \param[in]  fifo   The FIFO object.
 * \param[in]  pos    FIFO position to write to.
 * \param[in]  value  Pointer to value to write into fifo.
 */
static inline void fifo__copy_to(
		const struct fifo *fifo,
		uint16_t pos,
		const void *value)
{
	size_t element_size = fifo->element_size;

	memcpy(fifo->values + pos * element_size, value, element_size);
}

/* Exported interface, documented in fifo.h */
struct fifo *fifo_create(
		uint16_t capacity,
		uint16_t element_size)
{
	struct fifo *fifo = calloc(1, sizeof(*fifo));
	if (fifo == NULL) {
		return NULL;
	}
	fifo->values = malloc(capacity * element_size);
	if (fifo->values == NULL) {
		free(fifo);
		return NULL;
	}
	fifo->capacity = capacity;
	fifo->element_size = element_size;
	return fifo;
}

/* Exported interface, documented in fifo.h */
void fifo_destroy(
		struct fifo *fifo)
{
	if (fifo != NULL) {
		free(fifo->values);
		free(fifo);
	}
}

/* Exported interface, documented in fifo.h */
bool fifo_write(
		struct fifo *fifo,
		const void *value)
{
	if (fifo->used == fifo->capacity) {
		return false;
	}

	fifo__copy_to(fifo, fifo->write, value);
	fifo->write = fifo__pos_advance(fifo, fifo->write);
	fifo->used++;

	return true;
}

/* Exported interface, documented in fifo.h */
bool fifo_read(
		struct fifo *fifo,
		void *value)
{
	if (fifo->used == 0) {
		return false;
	}

	fifo__copy_from(fifo, fifo->read, value);
	fifo->read = fifo__pos_advance(fifo, fifo->read);
	fifo->used--;

	return true;
}

/* Exported interface, documented in fifo.h */
bool fifo_peek_back(
		const struct fifo *fifo,
		uint16_t steps,
		void *value)
{
	uint16_t peek_pos;

	if (fifo->used <= steps) {
		return false;
	}

	peek_pos = fifo__pos_seek(fifo, fifo->write, - (steps + 1));
	fifo__copy_from(fifo, peek_pos, value);
	return true;
}
