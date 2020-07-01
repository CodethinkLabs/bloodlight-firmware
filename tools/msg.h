#ifndef BL_TOOLS_MSG_H
#define Bl_TOOLS_MSG_H

/**
 * Parse from `stdin` into given message data structure.
 *
 * \param[in,out] msg  Message structure to populate.
 * \return true on success, false otherwise.
 */
bool bl_msg_parse(union bl_msg_data *msg);

/**
 * Print message to given file stream.
 *
 * \param[in] msg   Message to print.
 * \param[in] file  Stream to print message to.
 */
void bl_msg_print(const union bl_msg_data *msg, FILE *file);

#endif /* BL_TOOLS_MSG_H */
