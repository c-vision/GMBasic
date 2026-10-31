/* gmb_tui_pathcompleter.h — LineEditor.cs PathCompleter (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_PATHCOMPLETER_H
#define GMB_TUI_PATHCOMPLETER_H
#include "gmb_tui.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GMB_PC_MAX_COMPLETIONS 64
typedef struct gmb_path_completer_result {
    char completions[GMB_PC_MAX_COMPLETIONS][256];
    int count;
    int replacement_start;
    int replacement_len;
} gmb_path_completer_result_t;
void gmb_path_completer_complete(const char* text, int pos, gmb_path_completer_result_t* out);
#ifdef __cplusplus
}
#endif
#endif
