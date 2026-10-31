/* ============================================================================
 * gmb_tui_events.h — argomenti evento Form (specchio ClipperLib/Clipper
 * FormEvents.cs). Tipi dati puri per i callback dei controlli.
 * ============================================================================ */
#ifndef GMB_TUI_EVENTS_H
#define GMB_TUI_EVENTS_H

#include "gmb_tui.h"
#include "gmb_tui_mouse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- FormEventArgs (base) ---- */
typedef struct gmb_form_event_args {
    bool handled;
    bool cancel;
    gmb_key_modifiers_t mods;
} gmb_form_event_args_t;

typedef struct gmb_form_key_event_args {
    gmb_form_event_args_t base;
    int key_code;
    char character;
    int scan_code;
    int repeat_count;
    bool is_extended;
    bool is_system_key;
} gmb_form_key_event_args_t;

typedef struct gmb_form_mouse_event_args {
    gmb_form_event_args_t base;
    int x, y;
    int kind;          /* GMB_MA_* */
    int click_count;
    int wheel_delta;
} gmb_form_mouse_event_args_t;

typedef struct gmb_form_paint_event_args {
    gmb_form_event_args_t base;
    gmb_screen_buffer_t* buffer;
} gmb_form_paint_event_args_t;

typedef struct gmb_form_text_event_args {
    gmb_form_event_args_t base;
    char current_value[512];
    char proposed_value[512];
    bool has_proposed;
    int current_cursor;
    int proposed_cursor;
    bool has_proposed_cursor;
} gmb_form_text_event_args_t;

typedef struct gmb_form_check_event_args {
    gmb_form_event_args_t base;
    bool current_checked;
    bool proposed_checked;
    bool has_proposed;
} gmb_form_check_event_args_t;

typedef struct gmb_form_list_event_args {
    gmb_form_event_args_t base;
    int current_index;
    int proposed_index;
    bool has_proposed;
    char current_value[512];
    char proposed_value[512];
} gmb_form_list_event_args_t;

typedef struct gmb_form_date_event_args {
    gmb_form_event_args_t base;
    int current_y, current_m, current_d;
    int proposed_y, proposed_m, proposed_d;
    bool has_proposed;
} gmb_form_date_event_args_t;

typedef struct gmb_form_color_event_args {
    gmb_form_event_args_t base;
    gmb_cell_color_t current_color;
    gmb_cell_color_t proposed_color;
    bool has_proposed;
} gmb_form_color_event_args_t;

typedef struct gmb_form_numeric_event_args {
    gmb_form_event_args_t base;
    double current_value;
    double proposed_value;
    bool has_proposed;
    double min, max;
} gmb_form_numeric_event_args_t;

typedef struct gmb_form_file_event_args {
    gmb_form_event_args_t base;
    char current_path[512];
    char proposed_path[512];
    bool has_proposed;
    char filter[256];
} gmb_form_file_event_args_t;

typedef struct gmb_form_printer_event_args {
    gmb_form_event_args_t base;
    char current_printer[256];
    char proposed_printer[256];
    bool has_proposed;
    int current_copies;
    int proposed_copies;
    bool has_proposed_copies;
} gmb_form_printer_event_args_t;

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_EVENTS_H */
