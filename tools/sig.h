#ifndef BL_TOOLS_SIG_H
#define BL_TOOLS_SIG_H

extern volatile bool bl_sig_killed;

/**
 * Initialise the sig module.
 * \return true on success, false otherwise.
 */
bool bl_sig_init(void);

#endif /* BL_TOOLS_SIG_H */
