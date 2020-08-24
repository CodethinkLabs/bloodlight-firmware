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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include <libudev.h>

#include "../src/usb.h"

#include "device.h"

static bool bl_device__match(
		const char *sysname,
		char **device_serial_out)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *device;
	struct udev_device *parent_dev;
	struct udev_device *dev;
	const char *udev_manu;
	const char *udev_prod;
	const char *path;
	bool match = false;

	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Can't create udev\n");
		goto out;
	}

	/* Create a list of the devices with the name "ttyACM*". */
	enumerate = udev_enumerate_new(udev);
	if (!enumerate) {
		fprintf(stderr, "Can't create udev enumerate context.\n");
		goto udev_out;
	}

	if (udev_enumerate_add_match_sysname(enumerate, sysname) < 0) {
		fprintf(stderr, "Failed on adding udev enumerate filter.\n");
		goto enum_out;
	}

	if (udev_enumerate_scan_devices(enumerate) < 0) {
		fprintf(stderr, "Failed on scanning device.\n");
		goto enum_out;
	}

	/* udev_enumerate_get_list_entry returns a list of entry,
	but we only need the first entry, because we only cares
	about their parent which are the same for all. */
	device = udev_enumerate_get_list_entry(enumerate);

	/* Get the filename of the /sys entry for the device
	 and create a udev_device object (dev) representing it */
	path = udev_list_entry_get_name(device);
	if (!path) {
		printf("No matched device found.\n");
		goto enum_out;
	}

	dev = udev_device_new_from_syspath(udev, path);
	if (!dev) {
		fprintf(stderr, "Unable to find usb device.");
		goto enum_out;
	}

	/* In order to get information about the
	 USB device, get the parent device with the
	 subsystem/devtype pair of "usb"/"usb_device". This will
	 be several levels up the tree, but the function will find
	 it.*/
	parent_dev = udev_device_get_parent_with_subsystem_devtype(
			dev, "usb", "usb_device");
	if (!parent_dev) {
		fprintf(stderr, "Unable to find parent usb device.");
		goto dev_out;
	}

	/* From here, we can call get_sysattr_value() for each file
	 in the device's /sys entry. The strings passed into these
	 functions (idProduct, idVendor, serial, etc.) correspond
	 directly to the files in the directory which represents
	 the USB device. Note that USB strings are Unicode, UCS2
	 encoded, but the strings returned from
	 udev_device_get_sysattr_value() are UTF-8 encoded. */
	udev_manu = udev_device_get_sysattr_value(parent_dev, "manufacturer");
	udev_prod = udev_device_get_sysattr_value(parent_dev, "product");
	if ((udev_manu != NULL) &&
	    (udev_prod != NULL)) {
		if (!strcmp(udev_manu, BL_STR_MANUFACTURER) &&
		    !strcmp(udev_prod, BL_STR_PRODUCT)) {
			const char *serial = udev_device_get_sysattr_value(
					parent_dev, "serial");

			match = true;
			*device_serial_out = strdup(serial);
		}
	}

dev_out:
	/* Note: we don't get an additional ref for `parent_dev`,
	 * it is the original `dev` that must be unreffed. */
	udev_device_unref(dev);
enum_out:
	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
udev_out:
	udev_unref(udev);
out:
	return match;
}

/* filter function for /dev nodes match */
static int bl_device__is_ttyACM(const struct dirent *entry)
{
	return strncmp(entry->d_name, "ttyACM", 6) == 0;
}

static bool bl_device__list_append(
		struct bl_device **devices,
		unsigned *count,
		const char *device_path,
		const char *device_serial)
{
	struct bl_device *temp;

	temp = realloc(*devices, sizeof(**devices) * (*count + 1));
	if (temp == NULL) {
		return false;
	}
	*devices = temp;

	temp[*count].device_path = strdup(device_path);
	temp[*count].device_serial = strdup(device_serial);

	if (temp[*count].device_path   == NULL ||
	    temp[*count].device_serial == NULL) {
		free(temp[*count].device_path);
		free(temp[*count].device_serial);
		return false;
	}

	*count += 1;
	return true;
}

void bl_device_list_free(
		struct bl_device *devices,
		unsigned count)
{
	for (unsigned i = 0; i < count; i++) {
		free(devices[i].device_path);
		free(devices[i].device_serial);
	}

	free(devices);
}

bool bl_device_list_get(
		struct bl_device **devices_out,
		unsigned *count_out)
{
	struct bl_device *list = NULL;
	struct dirent **namelist;
	unsigned found = 0;
	bool res = false;
	int count;

	count = scandir("/dev/", &namelist, bl_device__is_ttyACM, NULL);
	if (count == -1) {
		fprintf(stderr, "scandir failed: %s\n", strerror(errno));
		return false;
	}

	for (int i = 0; i < count; i++) {
		char *serial;
		if (bl_device__match(namelist[i]->d_name, &serial)) {
			char dev_node[32];
			int ret = snprintf(dev_node, sizeof(dev_node) - 1,
					"/dev/%s", namelist[i]->d_name);
			if (ret < 0 || ret >= (int)(sizeof(dev_node) - 1)) {
				fprintf(stderr, "Error saving device path: %s\n",
						namelist[i]->d_name);
				free(serial);
				goto out;
			}

			if (!bl_device__list_append(&list, &found,
					dev_node, serial)) {
				free(serial);
				goto out;
			}
			free(serial);
		}
	}

	if (found) {
		*devices_out = list;
		*count_out = found;
		res = true;
	}

out:
	if (res == false) {
		bl_device_list_free(list, found);
	}
	for (int i = 0; i < count; i++) {
		free(namelist[i]);
	}
	free(namelist);
	return res;
}

int bl_device_open(const char *dev_path)
{
	int dev_fd;
	struct termios t;
	unsigned count = 0;
	struct bl_device *devices = NULL;

	if (dev_path == NULL) {
		if (!bl_device_list_get(&devices, &count) || count == 0) {
			fprintf(stderr, "Failed to get device list.\n");
			return -ENODEV;
		}

		fprintf(stderr, "Using device: %s (%s).\n",
				devices[0].device_path,
				devices[0].device_serial);
		dev_path = devices[0].device_path;
	}

	dev_fd = open(dev_path, O_RDWR);
	if (dev_fd == -1) {
		fprintf(stderr, "Failed to open '%s': %s\n",
				dev_path, strerror(errno));
		bl_device_list_free(devices, count);
		return -1;
	}

	if (tcgetattr(dev_fd, &t) == 0) {
		t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		t.c_oflag &= ~(OPOST);
		t.c_cflag |=  (CS8);
		t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

		if (tcsetattr(dev_fd, TCSANOW, &t) != 0) {
			fprintf(stderr, "Failed set terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
			bl_device_list_free(devices, count);
			return -1;
		}
	} else {
		fprintf(stderr, "Failed get terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
		bl_device_list_free(devices, count);
		return -1;
	}

	bl_device_list_free(devices, count);
	return dev_fd;
}

void bl_device_close(int dev_fd)
{
	/* Maybe we should ignore 0, 1 and 2. */
	if (dev_fd >= 0) {
		close(dev_fd);
	}
}
