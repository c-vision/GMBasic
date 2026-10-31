/* gmb_tui_history.h — LineEditor.cs BasicHistory (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_HISTORY_H
#define GMB_TUI_HISTORY_H
#include "gmb_tui.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GMB_HISTORY_MAX 64
#define GMB_HISTORY_ENTRY 256
typedef struct gmb_basic_history {
    char entries[GMB_HISTORY_MAX][GMB_HISTORY_ENTRY];
    int count;
    int capacity;
    int cursor;
} gmb_basic_history_t;
void gmb_basic_history_init(gmb_basic_history_t* h, int capacity);
int  gmb_basic_history_count(const gmb_basic_history_t* h);
bool gmb_basic_history_is_empty(const gmb_basic_history_t* h);
void gmb_basic_history_add(gmb_basic_history_t* h, const char* entry);
int  gmb_basic_history_previous(gmb_basic_history_t* h, char* out, int cap);
int  gmb_basic_history_next(gmb_basic_history_t* h, char* out, int cap);
int  gmb_basic_history_search(const gmb_basic_history_t* h, const char* prefix, char out[][GMB_HISTORY_ENTRY], int cap);
void gmb_basic_history_clear(gmb_basic_history_t* h);
#ifdef __cplusplus
}
#endif
#endif
