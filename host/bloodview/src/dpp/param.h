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
 * \brief Interface to the data processing pipeline parameter handling.
 */

#ifndef BV_DPP_PARAM_H
#define BV_DPP_PARAM_H

#include "value.h"

/** Bloodview parameter. */
struct bv_param {
	char *name;            /**< Parameter name. */
	struct bv_value value; /**< Parameter value. */
};

/** Bloodview parameter specification. */
struct bv_param_spec {
	char *name;           /**< Parameter name. */
	enum bv_value_e kind; /**< Parameter kind. */
};

/**
 * Find named typed parameter in array of parameters.
 *
 * \param[in] param        Array of parameters.
 * \param[in] param_count  Number of parameters.
 * \param[in] name         Parameter name to look up.
 * \param[in] kind         Parameter kind to look up.
 * \return The matching parameter or NULL if not found.
 */
const struct bv_param *param_lookup(
		const struct bv_param *param,
		unsigned param_count,
		const char *name,
		enum bv_value_e kind);

/**
 * Find named parameter spec in array of parameter specs.
 *
 * \param[in] param        Array of parameters.
 * \param[in] param_count  Number of parameters.
 * \param[in] name         Parameter name to look up.
 * \return The matching parameter spec or NULL if not found.
 */
const struct bv_param_spec *param_spec_lookup(
		const struct bv_param_spec *param,
		unsigned param_count,
		const char *name);

#endif
