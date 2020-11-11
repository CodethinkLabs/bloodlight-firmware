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

#include <stdlib.h>
#include <assert.h>

#include "fifo.h"

// Start fifo methods

struct fifo *fifo_create(uint16_t capacity)
{
	struct fifo *fifo = calloc(1, sizeof(*fifo));
	if (fifo == NULL) {
		return NULL;
	}
	fifo->values = malloc(capacity * sizeof(*fifo->values));
	if (fifo->values == NULL) {
		free(fifo);
		return NULL;
	}
	fifo->capacity = capacity;
	return fifo;
}

void fifo_destroy(struct fifo *fifo)
{
	free(fifo->values);
	free(fifo);
}

static inline uint16_t fifo_advance(const struct fifo *fifo, uint16_t pos)
{
	pos++;
	return (pos >= fifo->capacity) ? 0 : pos;
}

static uint16_t fifo_seek(const struct fifo *fifo, int32_t pos, int32_t steps)
{
	pos += steps;
	while (pos >= fifo->capacity) {
		pos -= fifo->capacity;
	}
	while (pos < 0) {
		pos += fifo->capacity;
	}
	return pos;
}

// Returns the nth previously added element from the fifo
bool fifo_peek_back(const struct fifo *fifo, uint16_t steps, uint32_t *out)
{
	if (fifo->used <= steps) {
		// Can't peek back further than we've written
		return false;
	}

	// fifo->read points to an as-yet unused index
	*out = fifo->values[fifo_seek(fifo, fifo->read - 1, -steps)];
	return true;
}

bool fifo_write(struct fifo *fifo, uint32_t value)
{
	if (fifo->used == fifo->capacity) {
		return false;
	}

	fifo->values[fifo->read] = value;
	fifo->read = fifo_advance(fifo, fifo->read);
	fifo->used++;

	return true;
}

bool fifo_read(struct fifo *fifo, uint32_t *value)
{
	if (fifo->used == 0) {
		return false;
	}

	*value = fifo->values[fifo->write];
	fifo->write = fifo_advance(fifo, fifo->write);
	fifo->used--;

	return true;
}

// Start pfifo methods

struct pfifo *pfifo_create(uint16_t capacity)
{
	struct pfifo *fifo = calloc(1, sizeof(*fifo));

	if (fifo == NULL) {
		return NULL;
	}
	fifo->values = malloc(capacity * sizeof(*fifo->values));
	if (fifo->values == NULL) {
		free(fifo);
		return NULL;
	}
	fifo->capacity = capacity;
	return fifo;
}

void pfifo_destroy(struct pfifo *fifo)
{
	assert(fifo->used == 0); // Empty the pfifo before destroying it!
	free(fifo->values);
	free(fifo);
}

static inline uint16_t pfifo_advance(const struct pfifo *fifo, uint16_t pos)
{
	pos++;
	return (pos >= fifo->capacity) ? 0 : pos;
}

static uint16_t pfifo_seek(const struct pfifo *fifo, int32_t pos, int32_t steps)
{
	pos += steps;
	while (pos >= fifo->capacity) {
		pos -= fifo->capacity;
	}
	while (pos < 0) {
		pos += fifo->capacity;
	}
	return pos;
}

bool pfifo_write(struct pfifo *fifo, void *value)
{
	if (fifo->used == fifo->capacity) {
		return false;
	}

	fifo->values[fifo->read] = value;
	fifo->read = pfifo_advance(fifo, fifo->read);
	fifo->used++;

	return true;
}

bool pfifo_read(struct pfifo *fifo, void **value)
{
	if (fifo->used == 0) {
		return false;
	}

	*value = fifo->values[fifo->write];
	fifo->write = pfifo_advance(fifo, fifo->write);
	fifo->used--;

	return true;
}

bool pfifo_peek_back(const struct pfifo *fifo, uint16_t steps, void **out)
{
	if (fifo->used <= steps) {
		// Can't peek further than we've written
		return false;
	}

	*out = fifo->values[pfifo_seek(fifo, fifo->read - 1, -steps)];
	return true;
}