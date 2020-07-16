#ifndef BL_TOOLS_FIND_DEVICE_H
#define Bl_TOOLS_FIND_DEVICE_H

/* The path name of the matched /dev node */
extern char dev_node[32];

/**
 * Scan the /dev nodes and match the right ACM devices
 *
 * Return: number of matched devices
 */
int scan(void);

#endif /* BL_TOOLS_FIND_DEVICE_H */
