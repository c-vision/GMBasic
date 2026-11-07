/* ============================================================================
 * gmb_tui_drawapi.c — API di disegno dei controlli per i client C# (P/Invoke).
 *
 * Il client C# (ClipperLib/Controls) NON disegna più: costruisce una
 * `gmb_draw_req_t` e chiama `gmb_tui_draw_control`. Qui si istanzia il widget
 * reale del runtime C, gli si applica lo stato e si esegue il render canonico
 * (`gmb_control_draw`) nel buffer del client. Una sola implementazione.
 * ========================================================================== */
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_tui_drawapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- utilità --------------------------------------------------------------- */

/* Divide `s` su un separatore, scrivendo i puntatori delle parti in `out`.
 * Le parti sono NUL-terminate in-place su una copia (`scratch`). */
static int dk_split(char* scratch, int cap_scratch, char sep, char** out, int cap_out)
{
    int n = 0;
    if (!scratch || !*scratch || cap_out <= 0) return 0;
    (void)cap_scratch;
    char* p = scratch;
    out[n++] = p;
    while (*p && n < cap_out)
    {
        if (*p == sep) { *p = '\0'; out[n++] = p + 1; }
        p++;
    }
    return n;
}

/* Applica lo stato comune + i colori espliciti al controllo temporaneo. */
static void dk_apply(gmb_control_t* c, const gmb_draw_req_t* r)
{
    c->x = r->x; c->y = r->y;
    c->width = r->w;
    if (r->h > 0) c->height = r->h;
    c->focused = (r->state & GMB_DS_FOCUSED) != 0;
    c->has_error = (r->state & GMB_DS_ERROR) != 0;
    c->uppercase = (r->state & GMB_DS_UPPERCASE) != 0;
    c->window_kind = GMB_WIN_STANDARD;   /* client C#: buffer a celle */
    if (r->has_fg || r->has_bg)
    {
        gmb_cell_color_t fg = { (uint8_t)((r->fg_rgb >> 16) & 0xFF), (uint8_t)((r->fg_rgb >> 8) & 0xFF),
                                (uint8_t)(r->fg_rgb & 0xFF), 0 };
        gmb_cell_color_t bg = { (uint8_t)((r->bg_rgb >> 16) & 0xFF), (uint8_t)((r->bg_rgb >> 8) & 0xFF),
                                (uint8_t)(r->bg_rgb & 0xFF), 0 };
        gmb_control_style_with_colors(&c->style, fg, bg);
        c->style.has_fg = r->has_fg != 0;
        c->style.has_bg = r->has_bg != 0;
    }
}

static void dk_draw_and_free(gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    if (c) { gmb_control_draw(c, buf); free(c); }
}

/* --- disegno per kind ------------------------------------------------------ */

static int dk_draw(gmb_screen_buffer_t* buf, const gmb_draw_req_t* r)
{
    if (!buf || !r) return 0;
    const char* text = r->text ? r->text : "";
    const char* extra = r->extra ? r->extra : "";

    switch (r->kind)
    {
    case GMB_DK_BUTTON: {
        gmb_button_t* o = (gmb_button_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_button_init(o, r->x, r->y, r->w, text);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_CHECKBOX: {
        gmb_checkbox_t* o = (gmb_checkbox_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_checkbox_init(o, r->x, r->y, r->w, text);
        gmb_checkbox_set_checked(o, (r->state & GMB_DS_CHECKED) != 0);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_TOGGLE: {
        gmb_toggle_button_t* o = (gmb_toggle_button_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_toggle_button_init(o, r->x, r->y, r->w, text);
        o->pressed = (r->state & GMB_DS_CHECKED) != 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_LABEL: {
        gmb_label_t* o = (gmb_label_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_label_init(o, r->x, r->y, r->w, text);
        o->align = r->sel;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_TEXTINPUT: {
        gmb_text_input_t* o = (gmb_text_input_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_text_input_init(o, r->x, r->y, r->w);
        gmb_text_input_set_text(o, text);
        if (r->sel > 0 && r->sel <= (int)strlen(o->text)) o->cursor = r->sel;
        o->read_only = (r->state & GMB_DS_READONLY) != 0;
        o->password_mode = (r->state & GMB_DS_PASSWORD) != 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_LISTBOX: {
        gmb_listbox_t* o = (gmb_listbox_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[64];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 64);
        gmb_listbox_init(o, r->x, r->y, r->w, r->h);
        gmb_listbox_set_items(o, (const char* const*)v, n);
        o->selected = r->sel >= 0 && r->sel < n ? r->sel : (n > 0 ? 0 : -1);
        if (o->selected >= 0 && r->h > 0 && o->selected >= r->h) o->scroll_offset = o->selected - r->h + 1;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_COMBOBOX: {
        gmb_combobox_t* o = (gmb_combobox_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[16];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 16);
        gmb_combobox_init(o, r->x, r->y, r->w);
        gmb_combobox_set_items(o, (const char* const*)v, n);
        o->selected = r->sel >= 0 && r->sel < n ? r->sel : (n > 0 ? 0 : -1);
        o->highlight = o->selected;
        o->open = (r->state & GMB_DS_OPEN) != 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_RADIOGROUP: {
        gmb_radio_group_t* o = (gmb_radio_group_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[16];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 16);
        gmb_radio_group_init(o, r->x, r->y, r->w, r->h);
        gmb_radio_group_set_options(o, (const char* const*)v, n);
        o->selected = r->sel >= 0 && r->sel < n ? r->sel : (n > 0 ? 0 : -1);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_PROGRESS: {
        gmb_progress_bar_t* o = (gmb_progress_bar_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_progress_bar_init(o, r->x, r->y, r->w);
        if (r->max > r->min) { o->min = r->min; o->max = r->max; }
        gmb_progress_bar_set(o, (int)(r->value + 0.5));
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_SCROLLBAR: {
        gmb_scrollbar_t* o = (gmb_scrollbar_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        int vertical = (r->h > 1 && r->w <= 1) ? 1 : 0;
        int len = vertical ? r->h : r->w;
        gmb_scrollbar_init(o, r->x, r->y, len, vertical);
        gmb_scrollbar_set(o, (int)(r->value + 0.5), r->min, r->max > r->min ? r->max : 100);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_TAB: {
        gmb_tab_t* o = (gmb_tab_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[16];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 16);
        gmb_tab_init(o, r->x, r->y, r->w);
        gmb_tab_set_tabs(o, (const char* const*)v, n);
        o->selected = r->sel >= 0 && r->sel < n ? r->sel : 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_PANEL: {
        gmb_panel_t* o = (gmb_panel_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_panel_init(o, r->x, r->y, r->w, r->h, extra[0] ? extra : text);
        if (r->sel > 0) o->border = r->sel;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_SPINNER: {
        gmb_spinner_t* o = (gmb_spinner_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_spinner_init(o, r->x, r->y, r->w);
        if (text[0]) gmb_spinner_set_preset(o, text);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_SLIDER: {
        gmb_slider_t* o = (gmb_slider_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_slider_init(o, r->x, r->y, r->w);
        gmb_slider_set_range(o, r->min, r->max > r->min ? r->max : 100);
        gmb_slider_set_value(o, (int)(r->value + 0.5));
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_STATUSBAR: {
        gmb_status_bar_t* o = (gmb_status_bar_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_status_bar_init(o, r->x, r->y, r->w);
        gmb_status_bar_set_text(o, text);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_MENU: {
        gmb_menu_t* o = (gmb_menu_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[16];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 16);
        gmb_menu_init(o, r->x, r->y, r->w);
        for (int i = 0; i < n; i++)
        {
            if (strcmp(v[i], "-") == 0) { gmb_menu_add_item(o, ""); o->items[o->items_count - 1].separator = true; }
            else gmb_menu_add_item(o, v[i]);
        }
        o->selected_index = r->sel;
        o->is_open = (r->state & GMB_DS_OPEN) != 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_TEXTAREA: {
        gmb_text_area_t* o = (gmb_text_area_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_text_area_init(o, r->x, r->y, r->w, r->h);
        char scratch[8192]; char* v[64];
        snprintf(scratch, sizeof(scratch), "%s", text);
        int n = dk_split(scratch, (int)sizeof(scratch), '\n', v, 64);
        gmb_text_area_set_lines(o, (const char* const*)v, n);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_CODEAREA: {
        gmb_code_area_t* o = (gmb_code_area_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_code_area_init(o, r->x, r->y, r->w, r->h);
        static char scratch[GMB_CODE_MAX_LINES * GMB_CODE_MAX_COLS];
        static char* v[GMB_CODE_MAX_LINES];
        int n = 0;
        {
            const char* p = text;
            while (*p && n < GMB_CODE_MAX_LINES)
            {
                int i = 0;
                while (*p && *p != '\n' && i < GMB_CODE_MAX_COLS) scratch[n * GMB_CODE_MAX_COLS + i++] = *p++;
                scratch[n * GMB_CODE_MAX_COLS + i] = 0;
                v[n] = &scratch[n * GMB_CODE_MAX_COLS];
                n++;
                if (*p == '\n') p++;
            }
        }
        gmb_code_area_set_lines(o, (const char* const*)v, n);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_ALERTBOX: {
        gmb_alert_box_t* o = (gmb_alert_box_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_alert_box_init(o, r->x, r->y, r->w, r->h);
        snprintf(o->message, sizeof(o->message), "%s", text);
        if (extra[0]) snprintf(o->title, sizeof(o->title), "%s", extra);
        o->border = r->min;      /* 0 = single, 1 = double, 2 = rounded */
        o->align = r->sel;       /* 0 = left, 1 = center, 2 = right */
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_BIGTEXT: {
        gmb_big_text_t* o = (gmb_big_text_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_big_text_init(o, r->x, r->y);
        gmb_big_text_set_text(o, text);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_NUMERIC: {
        gmb_numeric_updown_t* o = (gmb_numeric_updown_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_numeric_updown_init(o, r->x, r->y, r->w);
        gmb_numeric_updown_set_range(o, r->min, r->max > r->min ? r->max : 100);
        gmb_numeric_updown_set_value(o, (int)(r->value + 0.5));
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_HLINE: {
        gmb_hline_t* o = (gmb_hline_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_hline_init(o, r->x, r->y, r->w, text);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_VLINE: {
        gmb_vline_t* o = (gmb_vline_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_vline_init(o, r->x, r->y, r->h > 0 ? r->h : r->w);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_STARRATING: {
        gmb_star_rating_t* o = (gmb_star_rating_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_star_rating_init(o, r->x, r->y, r->w);
        o->value = (int)(r->value + 0.5);
        o->max = r->max > 0 ? r->max : 5;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_FILEEXPLORER: {
        gmb_file_explorer_t* o = (gmb_file_explorer_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_file_explorer_init(o, extra[0] ? extra : ".");
        gmb_file_explorer_set_bounds(o, r->x, r->y, r->w, r->h);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_FKEYBAR: {
        gmb_function_key_bar_t* o = (gmb_function_key_bar_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_function_key_bar_init(o, r->x, r->y, r->w);
        if (text[0])
        {
            char tmp[10][24]; int n = 0; const char* p = text;
            while (*p && n < 10)
            {
                int i = 0;
                while (*p && *p != '|' && i < 23) tmp[n][i++] = *p++;
                tmp[n][i] = 0; n++;
                if (*p == '|') p++;
            }
            for (int i = 0; i < n; i++) snprintf(o->keys[i], sizeof(o->keys[i]), "%s", tmp[i]);
        }
        gmb_function_key_bar_draw(o, buf);
        free(o);
        return 1;
    }
    case GMB_DK_SPLITTER: {
        gmb_splitter_t* o = (gmb_splitter_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_splitter_init(o, r->x, r->y, r->w, r->h, r->sel);
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_CONTEXTMENU: {
        gmb_context_menu_t* o = (gmb_context_menu_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        char buf_items[4096]; char* v[16];
        snprintf(buf_items, sizeof(buf_items), "%s", text);
        int n = dk_split(buf_items, (int)sizeof(buf_items), '|', v, 16);
        gmb_context_menu_init(o, r->x, r->y, r->w, r->h);
        gmb_context_menu_set_items(o, (const char**)v, n);
        o->sel = r->sel;
        o->open = (r->state & GMB_DS_OPEN) ? 1 : 0;
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_SCROLLVIEW: {
        gmb_scroll_view_t* o = (gmb_scroll_view_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_scroll_view_init(o, r->x, r->y, r->w, r->h);
        char scratch[8192]; char* v[64];
        snprintf(scratch, sizeof(scratch), "%s", text);
        int n = dk_split(scratch, (int)sizeof(scratch), '\n', v, 64);
        gmb_scroll_view_set_content(o, (const char* const*)v, n);
        o->offset = r->sel > 0 ? r->sel : 0;
        { int maxoff = n - (r->h > 0 ? r->h : 1); if (maxoff < 0) maxoff = 0;
          if (o->offset > maxoff) o->offset = maxoff; }
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_WINDOW: {
        gmb_window_t* o = (gmb_window_t*)calloc(1, sizeof(*o));
        if (!o) return 0;
        gmb_window_init(o, r->x, r->y, r->w, r->h);
        snprintf(o->title, sizeof(o->title), "%s", extra[0] ? extra : text);
        o->border = r->sel >= 0 ? r->sel : 0;
        o->closable = (r->state & GMB_DS_OPEN) != 0;   /* bit OPEN = pulsante di chiusura */
        dk_apply(&o->ctrl, r);
        dk_draw_and_free(&o->ctrl, buf);
        return 1;
    }
    case GMB_DK_TBROWSE: {
        /* text = colonne "header[:w[:type[:align]]]|…", extra = righe "c1|c2;c1|c2;…" */
        gmb_control_t* c = gmb_tui_add_tbrowse(r->y, r->x, r->w, r->h, text, extra);
        if (!c) return 0;
        dk_apply(c, r);
        c->height = r->h > 0 ? r->h : c->height;
        gmb_control_draw(c, buf);
        gmb_tui_control_remove_last();
        gmb_browse_free((gmb_browse_t*)c);
        return 1;
    }
    default:
        return 0;
    }
}

int gmb_tui_draw_control(gmb_screen_buffer_t* buf, const gmb_draw_req_t* req)
{
    return dk_draw(buf, req);
}

int gmb_tui_draw_control_h(void* buf, const gmb_draw_req_t* req)
{
    return dk_draw((gmb_screen_buffer_t*)buf, req);
}
