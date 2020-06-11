#ifndef BL_COMMON_H
#define Bl_COMMON_H

const char *bl_msg_type_to_str(enum bl_msg_type type);

void bl_msg_print(const union bl_msg_data *msg, FILE *file);

#endif /* BL_COMMON_H */