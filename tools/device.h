#ifndef BL_TOOLS_DEVICE_H
#define BL_TOOLS_DEVICE_H

/* The path name of the matched /dev node */
extern char dev_node[32];

struct bl_device {
	char *device_path;
	char *device_serial;
};

/**
 * Get the /dev nodes that matches the right ACM devices
 *
 * If a device node is provided from command line, it will
 * be used, otherwise, program will try to match the product
 * and manufacturer fields of USB device to guess the best
 * device node to use.
 */
void get_dev(int dev, char *argv[]);

/**
 * Open a bloodlight device.
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

#endif /* BL_TOOLS_DEVICE_H */
