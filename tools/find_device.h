#ifndef BL_TOOLS_FIND_DEVICE_H
#define Bl_TOOLS_FIND_DEVICE_H

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
extern void get_dev(int dev, char *argv[]);

#endif /* BL_TOOLS_FIND_DEVICE_H */
