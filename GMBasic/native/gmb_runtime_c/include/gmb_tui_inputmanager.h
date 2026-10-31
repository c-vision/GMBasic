/* gmb_tui_inputmanager.h — Manager.cs (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_INPUTMANAGER_H
#define GMB_TUI_INPUTMANAGER_H
#include "gmb_tui.h"
#include "gmb_tui_inputmask.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { GMB_IF_MASKED = 0, GMB_IF_CURRENCY, GMB_IF_TEXT, GMB_IF_DATE };
typedef struct gmb_input_field {
    int x, y;
    int kind;
    char value[512];
    char mask[256];
    double num_value;
    int decimals;
    double min_value, max_value;
    int min_len, max_len;
    bool century;
    bool clipper;
    gmb_input_mask_t use_key_mask;
    bool restore_cursor_pos;
    bool cursor_at_start;
    bool (*validator)(void* ctx, const char* value);
    void* validator_ctx;
} gmb_input_field_t;
void gmb_input_field_init(gmb_input_field_t* f);
void gmb_input_field_text(gmb_input_field_t* f, int x, int y, const char* value, const char* allowed,
                          int min_len, int max_len, bool clipper, gmb_input_mask_t mask, bool restore);
void gmb_input_field_masked(gmb_input_field_t* f, int x, int y, const char* value, const char* mask,
                            int min_len, int max_len, bool clipper, gmb_input_mask_t msk, bool restore);
void gmb_input_field_currency(gmb_input_field_t* f, int x, int y, double value, int decimals,
                              double min_value, double max_value, bool clipper, gmb_input_mask_t mask, bool restore);
void gmb_input_field_date(gmb_input_field_t* f, int x, int y, const char* date, bool century,
                          bool clipper, gmb_input_mask_t mask, bool restore);
typedef struct gmb_input_manager {
    gmb_input_field_t fields[16];
    int field_count;
    gmb_cell_color_t colors_fg, colors_bg;
    gmb_cell_color_t colors_exit_fg, colors_exit_bg;
} gmb_input_manager_t;
void gmb_input_manager_init(gmb_input_manager_t* m);
void gmb_input_manager_add(gmb_input_manager_t* m, const gmb_input_field_t* f);
gmb_input_mask_t gmb_input_manager_mask_default(void);
#ifdef __cplusplus
}
#endif
#endif
