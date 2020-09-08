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

#include <inttypes.h>
#include <stdbool.h>

/* fifo, a first-in first-out structure containing 32-bit integers */

struct fifo {
	uint32_t *values;
	uint16_t capacity;
	uint16_t write;
	uint16_t read;
	uint16_t used;
};

struct fifo *fifo_create(uint16_t capacity);
void fifo_destroy(struct fifo *fifo);
bool fifo_peek_back(const struct fifo *fifo, uint16_t steps, uint32_t *out);
bool fifo_write(struct fifo *fifo, uint32_t value);
bool fifo_read(struct fifo *fifo, uint32_t *value);

/* pfifo, a first-in first-out structure containing void pointers */
/* Since the contents of the fifo may be of arbitrary complexity,
 * responsibility for freeing the contents is left to the user */

struct pfifo {
	void **values;
	uint16_t capacity;
	uint16_t write;
	uint16_t read;
	uint16_t used;
};

struct pfifo *pfifo_create(uint16_t capacity);
void pfifo_destroy(struct pfifo *fifo);
bool pfifo_write(struct pfifo *fifo, void *value);
bool pfifo_read(struct pfifo *fifo, void **value);
bool pfifo_peek_back(const struct pfifo *fifo, uint16_t steps, void **out);
