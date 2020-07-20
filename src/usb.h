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

#ifndef BL_USB_H
#define BL_USB_H

#include "msg.h"

#define BL_STR_MANUFACTURER "Codethink"
#define BL_STR_PRODUCT      "Medical Plethysmograph Device"
#define BL_STR_SERIAL_NUM   "ct-mpd:000000"

/**
 * Initialise the USB module.
 */
void bl_usb_init(void);

/**
 * Poll the USB.
 */
void bl_usb_poll(void);

/**
 * Send a message to the host.
 *
 * \param[in]  msg  Message to send to host.
 * \return True on success.
 */
bool usb_send_message(union bl_msg_data *msg);

#endif
