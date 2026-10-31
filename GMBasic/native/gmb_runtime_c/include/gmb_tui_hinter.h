/* gmb_tui_hinter.h — LineEditor.cs DefaultHinter (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_HINTER_H
#define GMB_TUI_HINTER_H
#include "gmb_tui_history.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct gmb_default_hinter {
    int min_chars;
    gmb_basic_history_t* history;
} gmb_default_hinter_t;
void gmb_default_hinter_init(gmb_default_hinter_t* h, int min_chars, gmb_basic_history_t* history);
int  gmb_default_hinter_hint(gmb_default_hinter_t* h, const char* text, char* out, int cap);
#ifdef __cplusplus
}
#endif
#endif
