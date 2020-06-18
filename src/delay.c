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

#include <stdint.h>

#include "delay.h"
#include "tick.h"


void bl_delay_us(uint32_t us)
{
	uint32_t timer;
	bl_set_us_timer(&timer);
	while(bl_get_us_timer_elapsed(timer) < us);
}

void bl_delay_ms(uint32_t ms) 
{
	uint32_t timer;
	bl_set_ms_timer(&timer);
	while(bl_get_ms_timer_elapsed(timer) < ms);
}