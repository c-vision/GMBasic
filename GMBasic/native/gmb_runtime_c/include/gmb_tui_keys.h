/* gmb_tui_keys.h — Macros.cs Keys (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_KEYS_H
#define GMB_TUI_KEYS_H
#include "gmb_tui.h"
#ifdef __cplusplus
extern "C" {
#endif
int  gmb_keys_key_name(const char* name);
bool gmb_keys_matches_char(int last_key, char c);
bool gmb_keys_matches_name(int last_key, const char* name);
#ifdef __cplusplus
}
#endif
#endif
