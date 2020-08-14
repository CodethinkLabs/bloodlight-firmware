#ifndef BL_TOOLS_MSG_H
#define BL_TOOLS_MSG_H

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

#endif /* BL_TOOLS_MSG_H */
