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
 * Block for given number of microseconds.
 *
 * bl_tick_init must be called first for this to work
 * Should not be called from interrupt
 * Minimum delay of 10us
 *
 * \param[in]  us  Delay in microseconds.
 */
void bl_delay_us(uint32_t us);

/**
 * Block for given number of milliseconds.
 *
 * bl_tick_init must be called first for this to work
 * Should not be called from interrupt
 *
 * \param[in]  us  Delay in milliseconds.
 */
void bl_delay_ms(uint32_t ms);

#endif
