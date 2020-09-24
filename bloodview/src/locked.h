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
 * \brief Interface to the locked module.
 *
 * This module provides helpers for using mutex locked values.
 */

#ifndef BV_LOCKED_H
#define BV_LOCKED_H

#include <pthread.h>

/** A locked unsigned integer. */
typedef struct locked_uint {
	pthread_mutex_t   lock;  /**< The mutex lock. */
	volatile unsigned value; /**< The locked value. */
} locked_uint_t;

/**
 * Initialise a locked uint.
 *
 * \param[in]  lu  Locked unsigned to initialise.
 * \return true if initialisation succeded.
 */
static inline bool locked_uint_init(
		locked_uint_t *lu)
{
	return pthread_mutex_init(&lu->lock, NULL) == 0;
}

/**
 * Finalise a locked uint.
 *
 * \param[in]  lu  Locked unsigned to finalise.
 * \return true if finalisation succeded.
 */
static inline bool locked_uint_fini(
		locked_uint_t *lu)
{
	return pthread_mutex_destroy(&lu->lock) == 0;
}

/**
 * Claim a locked uint.
 *
 * \param[in]  lu  Locked unsigned to lock.
 * \return true if the locked value changed, false otherwise.
 */
static inline bool locked_uint_claim(
		locked_uint_t *lu)
{
	return pthread_mutex_lock(&lu->lock) == 0;
}

/**
 * Release a locked uint.
 *
 * \param[in]  lu  Locked unsigned to unlock.
 * \return true if the locked value changed, false otherwise.
 */
static inline bool locked_uint_release(
		locked_uint_t *lu)
{
	return pthread_mutex_unlock(&lu->lock) == 0;
}

/**
 * Check whether a locked value is equal to a given value.
 *
 * \param[in]  lu     Locked unsigned value to check.
 * \param[in]  value  The value to compare against.
 * \return true if the locked unsigned value is equal to the given value.
 */
static inline bool locked_uint_is_equal(
		locked_uint_t *lu,
		unsigned value)
{
	bool ret;

	locked_uint_claim(lu);
	ret = (lu->value == value);
	locked_uint_release(lu);

	return ret;
}

/**
 * Increment a locked unsigned value.
 *
 * \param[in]  lu  Locked unsigned value to change.
 */
static inline void locked_uint_inc(
		locked_uint_t *lu)
{
	locked_uint_claim(lu);
	lu->value++;
	locked_uint_release(lu);
}

/**
 * Decrement a locked unsigned value.
 *
 * \param[in]  lu  Locked unsigned value to change.
 */
static inline void locked_uint_dec(
		locked_uint_t *lu)
{
	locked_uint_claim(lu);
	lu->value--;
	locked_uint_release(lu);
}

/**
 * Set a locked unsigned value.
 *
 * \param[in]  lu     Locked unsigned value to change.
 * \param[in]  value  New value.
 * \return true if the locked value changed, false otherwise.
 */
static inline bool locked_uint_set(
		locked_uint_t *lu,
		unsigned value)
{
	bool ret = false;

	locked_uint_claim(lu);
	if (lu->value != value) {
		lu->value = value;
		ret = true;
	}
	locked_uint_release(lu);
	return ret;
}

#endif /* BV_LOCKED_H */
