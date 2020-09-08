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

#include "bl.h"
#include "led.h"

static void bl_spi_test(void)
{
	uint16_t counter = 0;
	bl_led_set(0xFFFF);
	while (1)
	{
		for (int i = 0; i < 1000000; i++)
		{
			__asm__("nop");
		}

		bl_led_set(counter << 8);
		counter++;
	}
}

int main(void)
{
	bl_init();
	bl_spi_test();
}
