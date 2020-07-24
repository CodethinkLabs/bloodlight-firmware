#ifndef BL_TOOLS_DEVICE_H
#define BL_TOOLS_DEVICE_H

/* The path name of the matched /dev node */
extern char dev_node[32];

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

#endif /* BL_TOOLS_DEVICE_H */
