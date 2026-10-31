/* ============================================================================
 * gmb_tui_tbrowse.h — TBrowse (specchio ClipperLib/TBrowse/TBrowse.cs).
 * Tabella browsabile con header, colonne, pan, freeze, editing inline, render
 * headless e Draw su ScreenBuffer.
 * ============================================================================ */
#ifndef GMB_TUI_TBROWSE_H
#define GMB_TUI_TBROWSE_H

#include "gmb_tui.h"
#include "gmb_tui_widgets.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_BROWSE_MAX_COLS  32
#define GMB_BROWSE_MAX_BUF   512
#define GMB_BROWSE_MAX_SORT  4

typedef enum gmb_browse_alignment {
    GMB_ALIGN_LEFT = 0, GMB_ALIGN_CENTER, GMB_ALIGN_RIGHT
} gmb_browse_alignment_t;

typedef enum gmb_data_type {
    GMB_DT_TEXT = 0, GMB_DT_INT, GMB_DT_FLOAT, GMB_DT_BOOL, GMB_DT_DATE, GMB_DT_CHOICE
} gmb_data_type_t;

typedef struct gmb_color_pair {
    gmb_cell_color_t foreground;
    gmb_cell_color_t background;
} gmb_color_pair_t;

typedef struct gmb_browse_column {
    char header[GMB_BROWSE_MAX_BUF];
    int width;
    gmb_browse_alignment_t align;
    bool editable;
    bool visible;
    gmb_data_type_t data_type;
} gmb_browse_column_t;

/* Data source (interfaccia IBrowseDataSource). */
typedef struct gmb_browse_datasource {
    void* user;
    int  (*get_value)(void* user, int row, int col, char* out, int cap);
    int  (*row_count)(void* user);
    int  (*skip)(void* user, int delta);
    void (*go_top)(void* user);
    void (*go_bottom)(void* user);
    int  (*bookmark)(void* user);
    void (*go_to)(void* user, int n);
    void (*set_value)(void* user, int row, int col, const char* val);
} gmb_browse_datasource_t;

/* VecDataSource: matrice di stringhe. */
typedef struct gmb_vec_ds {
    int rows, cols;
    char* data[GMB_BROWSE_MAX_COLS];  /* per colonna: buffer a righe, out of band */
    /* semplificazione: matrice flat rows*cols stringhe (fixed 256) */
    char cells[64][GMB_BROWSE_MAX_COLS][256];
    int current;
} gmb_vec_ds_t;

typedef enum gmb_browse_action {
    GMB_BA_NONE = 0, GMB_BA_SELECTED, GMB_BA_CANCELLED, GMB_BA_REFRESHED, GMB_BA_EDIT
} gmb_browse_action_t;

typedef struct gmb_browse gmb_browse_t;

struct gmb_browse {
    gmb_control_t ctrl;                 /* TBrowse : Control (X/Y/Width/Height) */
    gmb_rect_t area;
    gmb_browse_column_t columns[GMB_BROWSE_MAX_COLS];
    int col_count;
    int frozen_cols;
    int current_row;
    int current_col;
    int top_row;
    int left_col;
    int* view;          /* view[display_row] = data_row (sort/filter) */
    int view_count;
    int sort_col;
    int sort_dir;
    int sort_cols[GMB_BROWSE_MAX_SORT];  /* chiavi multiple (multi-colonna) */
    int sort_dirs[GMB_BROWSE_MAX_SORT];
    int sort_nkeys;
    char filter[256];
    bool mouse_enabled;
    bool show_header;
    bool show_border;
    bool highlight_row;
    gmb_color_pair_t header_colors;
    gmb_color_pair_t header_highlight_colors;
    gmb_color_pair_t row_colors;
    gmb_color_pair_t highlight_colors;
    gmb_browse_datasource_t ds;
    bool focused;

    /* Clipper 5.3 state */
    bool stabilized;
    bool has_last_render_pos, has_last_render_top;
    int last_render_row, last_render_col, last_render_top, last_render_left;
    bool invalidated;
    bool auto_lite;
    bool force_hilite;
    char color_spec_str[256];
    char col_sep[8];
    char head_sep[8];
    char foot_sep[8];
    char footer_text[256];
    bool has_footer;
    void* cargo;

    /* cell lock */
    int locked_cells[64][2];
    int locked_count;
    bool (*cell_lock_fn)(void* ctx, int row, int col);
    void* cell_lock_ctx;

    /* validators */
    bool (*pre_validator)(void* ctx, int row, int col);
    void* pre_validator_ctx;
    bool (*post_validator)(void* ctx, int row, int col);
    void* post_validator_ctx;
    bool (*stable_block)(void* ctx, struct gmb_browse* b);
    void* stable_block_ctx;

    /* hooks / key handlers (per callback semplici) */
    void (*hook_fn)(void* ctx, struct gmb_browse* b);
    void* hook_ctx;
    bool has_hook;

    /* editing inline */
    bool editing;
    int edit_row, edit_col;
    char edit_buf[GMB_BROWSE_MAX_BUF];
    int edit_cursor;
    gmb_data_type_t edit_dtype;
    char edit_initial[GMB_BROWSE_MAX_BUF];
    int edit_date_field;
};

/* ---- costruttori / distruttore ---- */
gmb_browse_t* gmb_browse_new(const gmb_rect_t* area, int col_count, const gmb_browse_column_t* cols,
                             const gmb_browse_datasource_t* ds);
void gmb_browse_free(gmb_browse_t* b);

/* ---- VecDataSource ---- */
void gmb_vec_ds_make(gmb_vec_ds_t* out, int rows, int cols);
void gmb_vec_ds_set(gmb_vec_ds_t* v, int row, int col, const char* val);
void gmb_vec_ds_datasource(gmb_vec_ds_t* v, gmb_browse_datasource_t* out);

/* ---- computed ---- */
int  gmb_browse_page_size(const gmb_browse_t* b);
int  gmb_browse_row_count(const gmb_browse_t* b);
int  gmb_browse_current_value(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_inner_width(const gmb_browse_t* b);
int  gmb_browse_visible_col_count(const gmb_browse_t* b);
int  gmb_browse_displayed_cols(const gmb_browse_t* b, int* out, int cap);
int  gmb_browse_max_left_col(const gmb_browse_t* b);
int  gmb_browse_scrollable_page(const gmb_browse_t* b);

/* ---- navigazione ---- */
void gmb_browse_move_down(gmb_browse_t* b);
void gmb_browse_move_up(gmb_browse_t* b);
void gmb_browse_page_down(gmb_browse_t* b);
void gmb_browse_page_up(gmb_browse_t* b);
void gmb_browse_go_top(gmb_browse_t* b);
void gmb_browse_go_bottom(gmb_browse_t* b);
void gmb_browse_move_right(gmb_browse_t* b);
void gmb_browse_move_left(gmb_browse_t* b);
void gmb_browse_up(gmb_browse_t* b, int n);
void gmb_browse_down(gmb_browse_t* b, int n);
void gmb_browse_left(gmb_browse_t* b, int n);
void gmb_browse_right(gmb_browse_t* b, int n);
void gmb_browse_page_left(gmb_browse_t* b);
void gmb_browse_page_right(gmb_browse_t* b);
void gmb_browse_top(gmb_browse_t* b);
void gmb_browse_bottom(gmb_browse_t* b);
void gmb_browse_goto(gmb_browse_t* b, int row, int col);
int  gmb_browse_row_pos(const gmb_browse_t* b);
void gmb_browse_set_row_pos(gmb_browse_t* b, int r);
int  gmb_browse_col_pos(const gmb_browse_t* b);
void gmb_browse_set_col_pos(gmb_browse_t* b, int c);
int  gmb_browse_left_visible(const gmb_browse_t* b);
int  gmb_browse_right_visible(const gmb_browse_t* b);
int  gmb_browse_top_visible(const gmb_browse_t* b);
int  gmb_browse_bottom_visible(const gmb_browse_t* b);

/* ---- pan ---- */
void gmb_browse_pan_home(gmb_browse_t* b);
void gmb_browse_pan_end(gmb_browse_t* b);
void gmb_browse_pan_left(gmb_browse_t* b, int n);
void gmb_browse_pan_right(gmb_browse_t* b, int n);
void gmb_browse_hill_up(gmb_browse_t* b);
void gmb_browse_hill_down(gmb_browse_t* b);
void gmb_browse_hill_left(gmb_browse_t* b);
void gmb_browse_hill_right(gmb_browse_t* b);

/* ---- colonne ---- */
/* ---- view: sorting / filtering / resize ---- */
void gmb_browse_sort(gmb_browse_t* b, int col, int desc);   /*desc!=0 -> decrescente*/
void gmb_browse_set_filter(gmb_browse_t* b, const char* text);
void gmb_browse_resize_col(gmb_browse_t* b, int col, int w);
void gmb_browse_clear_filter(gmb_browse_t* b);
int  gmb_browse_sort_col(const gmb_browse_t* b);
int  gmb_browse_sort_dir(const gmb_browse_t* b);
const char* gmb_browse_filter(const gmb_browse_t* b);
/* multi-colonna: spec "col[:asc|desc][,col[:asc|desc]]..." */
void gmb_browse_set_sortkeys(gmb_browse_t* b, const char* spec);
int  gmb_browse_sortkeys(const gmb_browse_t* b, char* out, int cap);
/* export CSV della vista corrente (colonne visibili), header incluso */
int  gmb_browse_csv(const gmb_browse_t* b, char* out, int cap);

void gmb_browse_add_column(gmb_browse_t* b, const gmb_browse_column_t* col);
void gmb_browse_del_column(gmb_browse_t* b, int pos);
void gmb_browse_ins_column(gmb_browse_t* b, int pos, const gmb_browse_column_t* col);
gmb_browse_column_t* gmb_browse_get_column(gmb_browse_t* b, int pos);
void gmb_browse_set_column(gmb_browse_t* b, int pos, const gmb_browse_column_t* col);
int  gmb_browse_col_count(const gmb_browse_t* b);

/* ---- cell lock ---- */
void gmb_browse_lock_cell(gmb_browse_t* b, int row, int col);
void gmb_browse_unlock_cell(gmb_browse_t* b, int row, int col);
bool gmb_browse_is_cell_locked(const gmb_browse_t* b, int row, int col);
void gmb_browse_set_cell_lock(gmb_browse_t* b, bool (*f)(void*, int, int), void* ctx);
bool gmb_browse_cell_editable(const gmb_browse_t* b, int row, int col);

/* ---- freeze / stability ---- */
int  gmb_browse_freeze(gmb_browse_t* b, int n);
int  gmb_browse_freeze_count(const gmb_browse_t* b);
bool gmb_browse_stable(const gmb_browse_t* b);
bool gmb_browse_stable_with_block(gmb_browse_t* b);
void gmb_browse_force_stable(gmb_browse_t* b);
void gmb_browse_set_stable_block(gmb_browse_t* b, bool (*f)(void*, gmb_browse_t*), void* ctx);
bool gmb_browse_moved(const gmb_browse_t* b);
void gmb_browse_invalidate(gmb_browse_t* b);
bool gmb_browse_invalid(const gmb_browse_t* b);

/* ---- auto-lite / hilite ---- */
bool gmb_browse_auto_lite(const gmb_browse_t* b);
void gmb_browse_set_auto_lite(gmb_browse_t* b, bool v);
void gmb_browse_hilite(gmb_browse_t* b);
void gmb_browse_dehilite(gmb_browse_t* b);

/* ---- color / separatori ---- */
const char* gmb_browse_color_spec(const gmb_browse_t* b);
void gmb_browse_set_color_spec(gmb_browse_t* b, const char* spec);
const char* gmb_browse_col_sep(const gmb_browse_t* b);
void gmb_browse_set_col_sep(gmb_browse_t* b, const char* s);
const char* gmb_browse_head_sep(const gmb_browse_t* b);
void gmb_browse_set_head_sep(gmb_browse_t* b, const char* s);
const char* gmb_browse_foot_sep(const gmb_browse_t* b);
void gmb_browse_set_foot_sep(gmb_browse_t* b, const char* s);
bool gmb_browse_has_footer(const gmb_browse_t* b);
const char* gmb_browse_footer_text(const gmb_browse_t* b);
void gmb_browse_set_footer(gmb_browse_t* b, const char* text);

/* ---- cella / bookmark ---- */
int  gmb_browse_cell_value(const gmb_browse_t* b, int row, int col, char* out, int cap);
void gmb_browse_bookmark(gmb_browse_t* b, int n);
void gmb_browse_set_cargo(gmb_browse_t* b, void* v);
void* gmb_browse_get_cargo(gmb_browse_t* b);

/* ---- focus / hooks / key ---- */
void gmb_browse_set_focus(gmb_browse_t* b);
void gmb_browse_kill_focus(gmb_browse_t* b);
void gmb_browse_register_hook(gmb_browse_t* b, void (*f)(void* ctx, gmb_browse_t*), void* ctx);
void gmb_browse_call_hook(gmb_browse_t* b);
gmb_browse_action_t gmb_browse_apply_key(gmb_browse_t* b, int k);
gmb_browse_action_t gmb_browse_navigate(gmb_browse_t* b, int key);

/* ---- mouse ---- */
void gmb_browse_handle_click(gmb_browse_t* b, int x, int y);

/* ---- editing inline ---- */
bool gmb_browse_begin_edit(gmb_browse_t* b);
gmb_browse_action_t gmb_browse_apply_edit_key(gmb_browse_t* b, int key);

/* ---- render ---- */
int  gmb_browse_render_inner(const gmb_browse_t* b, const char* sep, char* out, int cap,
                             void* ctx, void (*cell_fn)(void* ctx, int col, int width, char* out, int cap));
int  gmb_browse_render_rows(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_render_header(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_render_separator(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_render_footer_separator(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_render(const gmb_browse_t* b, char* out, int cap);
int  gmb_browse_refresh(gmb_browse_t* b, char* out, int cap);
int  gmb_browse_refresh_all(gmb_browse_t* b, char* out, int cap);

/* Draw su ScreenBuffer. */
void gmb_browse_draw(gmb_browse_t* b, gmb_screen_buffer_t* buffer);

/* ---- helper statici esposti ---- */
void gmb_browse_format_cell(const char* value, int width, gmb_browse_alignment_t align, char* out, int cap);
gmb_color_pair_t gmb_browse_parse_color_pair(const char* token);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_TBROWSE_H */
