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

#ifndef BL_HOST_COMMON_MSG_H
#define BL_HOST_COMMON_MSG_H

#include <stdbool.h>

/**
 * Parse from given file stream into given message data structure.
 *
 * \param[in]     file  Stream to read message from.
 * \param[in,out] msg   Message structure to populate.
 * \return true on success, false otherwise.
 */
bool bl_msg_yaml_parse(
		FILE *file,
		union bl_msg_data *msg);

/**
 * Print message to given file stream.
 *
 * \param[in] file  Stream to print message to.
 * \param[in] msg   Message to print.
 */
void bl_msg_yaml_print(
		FILE *file,
		const union bl_msg_data *msg);

/**
 * Read raw msg from given file descriptor into given message data structure.
 *
 * \param[in]     fd       File descriptor to read message from.
 * \param[in]     timeout  Timeout in ms.
 * \param[in,out] msg      Message structure to populate.
 * \return true on success, false otherwise.
 */
bool bl_msg_read(
		int fd,
		int timeout,
		union bl_msg_data *msg);

/**
 * Write message to given file descriptor as raw data.
 *
 * \param[in] fd    File descriptor to write message to.
 * \param[in] path  Path to fd, (only used for error logging).
 * \param[in] msg   Message to write.
 */
bool bl_msg_write(
		int fd,
		const char *path,
		const union bl_msg_data *msg);

#endif /* BL_HOST_COMMON_MSG_H */
