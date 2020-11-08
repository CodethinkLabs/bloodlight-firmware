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

#ifndef BL_HOST_COMMON_DEVICE_H
#define BL_HOST_COMMON_DEVICE_H

struct bl_device {
	char *device_path;
	char *device_serial;
};

/**
 * Open a bloodlight device.
 *
 * If passed a `NULL` dev_path, this function will attempt to find a bloodlight
 * device to open.
 *
 * \param[in] dev_path  Path to device to open.
 * \return File descriptor for open device, or -1 on error.
 */
int bl_device_open(const char *dev_path);

/**
 * Close a bloodlight device.
 *
 * \param[in] dev_fd  File descriptor for device to close.
 */
void bl_device_close(int dev_fd);

/**
 * Get a list of connected bloodlight devices.
 *
 * \param[out] devices_out  Returns array of bloodlight devices on success.
 * \param[out] count_out    Returns number of entries in devices_out on success.
 * \return true on success, false otherwise.
 */
bool bl_device_list_get(
		struct bl_device **devices_out,
		unsigned *count_out);

/**
 * Free a list of connected bloodlight devices.
 *
 * \param[in] devices  Array of bloodlight devices to free.
 * \param[in] count    Number of entries in devices.
 */
void bl_device_list_free(
		struct bl_device *devices,
		unsigned count);

#endif /* BL_HOST_COMMON_DEVICE_H */
