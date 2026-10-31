/* gmb_tui_support.h — Support.cs (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_SUPPORT_H
#define GMB_TUI_SUPPORT_H
#include "gmb_tui_inputmask.h"
#ifdef __cplusplus
extern "C" {
#endif
int  gmb_support_adjust_text_len(int len, int max_len);
gmb_input_mask_t gmb_support_adjust_key_mask(gmb_input_mask_t mask);
#ifdef __cplusplus
}
#endif
#endif
