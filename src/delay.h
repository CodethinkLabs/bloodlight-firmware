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

#ifndef BL_DELAY_H
#define BL_DELAY_H

/**
 * Initialise the delay module.
 */
void bl_delay_init(void);

/**
 * Block for given number of microseconds.
 *
 * \param[in]  us  Delay in mictroseconds.
 */
void bl_delay_us(uint32_t us);

#endif
