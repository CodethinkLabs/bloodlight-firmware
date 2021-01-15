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
 * \brief Interface to the data processing pipeline derivative filter.
 */

#ifndef BV_DPP_FILTER_DERIVATIVE_H
#define BV_DPP_FILTER_DERIVATIVE_H

#include <stdbool.h>

/**
 * Register the existence of the derivative filter.
 *
 * This can be called once on startup to register the filter.
 *
 * \return true on success, or false on error.
 */
bool filter_derivative_register(void);

#endif
