#ifndef BL_TOOLS_MSG_H
#define Bl_TOOLS_MSG_H

const char *bl_msg_type_to_str(enum bl_msg_type type);

void bl_msg_print(const union bl_msg_data *msg, FILE *file);

#endif /* BL_TOOLS_MSG_H */
