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

#ifndef BL_MSG_H
#define BL_MSG_H

#include "common/msg.h"
#include "common/error.h"

/**
 * Handle an incomming message.
 *
 * \param[in]  msg  Incomming message to be handled.
 * \return \ref BL_ERROR_NONE on success, or appropriate error otherwise.
 */
enum bl_error bl_msg_handle(const union bl_msg_data *msg);

#endif
