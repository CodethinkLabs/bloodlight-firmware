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
 * \brief Interface to the data processing pipeline value handling.
 */

#ifndef BV_DPP_VALUE_H
#define BV_DPP_VALUE_H

#include <assert.h>
#include <stdbool.h>

/** Value data. */
struct bv_value {
	enum bv_value_e {
		BV_VALUE_BOOL,
		BV_VALUE_DOUBLE,
		BV_VALUE_UNSIGNED,
	} type;
	union {
		bool     type_bool;       /**< Data for bool values */
		double   type_double;     /**< Data for double values */
		unsigned type_unsigned;   /**< Data for unsigned values */
	};
};

/**
 * Get a boolean value from a bv_value.
 *
 * \param[in]  v  The bv_value to extract the value from.
 * \return The value.
 */
static inline bool bv_value_bool(const struct bv_value *v)
{
	assert(v != NULL);
	assert(v->type == BV_VALUE_BOOL);

	return v->type_bool;
}

/**
 * Get a double value from a bv_value.
 *
 * \param[in]  v  The bv_value to extract the value from.
 * \return The value.
 */
static inline double bv_value_double(const struct bv_value *v)
{
	assert(v != NULL);
	assert(v->type == BV_VALUE_DOUBLE);

	return v->type_double;
}

/**
 * Get an unsigned value from a bv_value.
 *
 * \param[in]  v  The bv_value to extract the value from.
 * \return The value.
 */
static inline unsigned bv_value_unsigned(const struct bv_value *v)
{
	assert(v != NULL);
	assert(v->type == BV_VALUE_UNSIGNED);

	return v->type_unsigned;
}

#endif
