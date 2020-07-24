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

/* Valid Codethink Medical Plethysmograph Device path */
char dev_node[32];

static bool match(char *sysname)
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *device;
	struct udev_device *dev;
	const char *path;

	/* Create the udev object */
	udev = udev_new();
	if (!udev)
	{
		fprintf(stderr, "Can't create udev\n");
		goto out;
	}

	/* Create a list of the devices with the name "ttyACM*". */
	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
	{
		fprintf(stderr, "Can't create udev enumerate context.\n");
		goto udev_out;
	}

	if (udev_enumerate_add_match_sysname(enumerate, sysname) < 0)
	{
		fprintf(stderr, "Failed on adding udev enumerate filter.\n");
		goto enum_out;
	}

	if (udev_enumerate_scan_devices(enumerate) < 0)
	{
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
	if (!path)
	{
		printf("No matched device found.\n");
		goto enum_out;
	}
	dev = udev_device_new_from_syspath(udev, path);

	/* In order to get information about the
	 USB device, get the parent device with the
	 subsystem/devtype pair of "usb"/"usb_device". This will
	 be several levels up the tree, but the function will find
	 it.*/
	dev = udev_device_get_parent_with_subsystem_devtype(
		dev,
		"usb",
		"usb_device");
	if (!dev)
	{
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
	if (!strncmp(udev_device_get_sysattr_value(dev, "manufacturer"),
				 BL_STR_MANUFACTURER, 9) &&
		!strncmp(udev_device_get_sysattr_value(dev, "product"),
				 BL_STR_PRODUCT, 29))
		return true;

dev_out:
	udev_device_unref(dev);
enum_out:
	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
udev_out:
	udev_unref(udev);
out:
	return false;
}

/* filter function for /dev nodes match */
static int dev_match(const struct dirent *entry)
{
	return !strncmp(entry->d_name, "ttyACM", 6);
}

static int scan(void)
{
	struct dirent **namelist;
	int n, found = 0;

	n = scandir("/dev/", &namelist, dev_match, NULL);
	if (n == -1)
	{
		fprintf(stderr, "scandir fails\n");
		exit(EXIT_FAILURE);
	}

	while (n--)
	{
		if (match(namelist[n]->d_name))
		{
			int ret = snprintf(dev_node, sizeof(dev_node) - 1,
							   "/dev/%s", namelist[n]->d_name);
			if (ret < 0 || ret >= (int)(sizeof(dev_node) - 1))
			{
				fprintf(stderr, "Error on saving device path: %s\n",
						namelist[n]->d_name);

				free(namelist[n]);
				break;
			}
			found++;
		}
		free(namelist[n]);
	}
	free(namelist);

	return found;
}

void get_dev(int dev, char *argv[])
{
	if ((strncmp(argv[dev], "auto", 4) ||
	     strncmp(argv[dev], "--auto", 6) ||
	     strncmp(argv[dev], "-a", 2))) {
		int found = scan();
		switch (found) {
		case 0:
			fprintf(stderr, "No MPD device found.\n");
			exit(EXIT_FAILURE);
		case 1:
			argv[dev] = dev_node;
			break;
		default:
			fprintf(stderr, "More than one device found, please specify which device to use.\n");
			exit(EXIT_FAILURE);
		}
	}
}

int bl_device_open(const char *dev_path)
{
	int dev_fd;
	struct termios t;

	dev_fd = open(dev_path, O_RDWR);
	if (dev_fd == -1) {
		fprintf(stderr, "Failed to open '%s': %s\n",
				dev_path, strerror(errno));
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
			return -1;
		}
	} else {
		fprintf(stderr, "Failed get terminal attributes"
				" of '%s': %s\n", dev_path, strerror(errno));
		return -1;
	}
	return dev_fd;
}

void bl_device_close(int dev_fd)
{
	/* Maybe we should ignore 0, 1 and 2. */
	if (dev_fd >= 0) {
		close(dev_fd);
	}
}

/*
int main(void)
{
	int found = scan();
	switch (found)
	{
	case 0:
		printf("no device found");
		break;
	case 1:
		printf("valid device at: %s\n", dev_node);
		break;
	default:
		printf("more than one device found, please specify which device to use.\n");
	}
	exit(EXIT_SUCCESS);
}
*/
