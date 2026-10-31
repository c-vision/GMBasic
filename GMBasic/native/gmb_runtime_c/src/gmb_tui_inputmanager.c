/* ============================================================================
 * gmb_tui_inputmanager.c — Manager.cs (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_inputmanager.h"
#include <stdio.h>
#include <string.h>

void gmb_input_field_init(gmb_input_field_t* f)
{
    memset(f, 0, sizeof(*f));
    f->min_len = 1;
    f->century = true;
}

void gmb_input_field_text(gmb_input_field_t* f, int x, int y, const char* value, const char* allowed,
                          int min_len, int max_len, bool clipper, gmb_input_mask_t mask, bool restore)
{
    gmb_input_field_init(f);
    f->x = x; f->y = y; f->kind = GMB_IF_TEXT;
    if (value) snprintf(f->value, sizeof(f->value), "%s", value);
    if (allowed) snprintf(f->mask, sizeof(f->mask), "%s", allowed);
    f->min_len = min_len; f->max_len = max_len; f->clipper = clipper;
    f->use_key_mask = mask; f->restore_cursor_pos = restore;
}

void gmb_input_field_masked(gmb_input_field_t* f, int x, int y, const char* value, const char* mask,
                            int min_len, int max_len, bool clipper, gmb_input_mask_t msk, bool restore)
{
    gmb_input_field_init(f);
    f->x = x; f->y = y; f->kind = GMB_IF_MASKED;
    if (value) snprintf(f->value, sizeof(f->value), "%s", value);
    if (mask) snprintf(f->mask, sizeof(f->mask), "%s", mask);
    f->min_len = min_len; f->max_len = max_len; f->clipper = clipper;
    f->use_key_mask = msk; f->restore_cursor_pos = restore;
    f->cursor_at_start = true;
}

void gmb_input_field_currency(gmb_input_field_t* f, int x, int y, double value, int decimals,
                              double min_value, double max_value, bool clipper, gmb_input_mask_t mask,
                              bool restore)
{
    gmb_input_field_init(f);
    f->x = x; f->y = y; f->kind = GMB_IF_CURRENCY;
    f->num_value = value; f->decimals = decimals;
    f->min_value = min_value; f->max_value = max_value; f->clipper = clipper;
    f->use_key_mask = mask; f->restore_cursor_pos = restore;
    f->cursor_at_start = true;
}

void gmb_input_field_date(gmb_input_field_t* f, int x, int y, const char* date, bool century,
                          bool clipper, gmb_input_mask_t mask, bool restore)
{
    gmb_input_field_init(f);
    f->x = x; f->y = y; f->kind = GMB_IF_DATE;
    if (date) snprintf(f->value, sizeof(f->value), "%s", date);
    f->century = century; f->clipper = clipper;
    f->use_key_mask = mask; f->restore_cursor_pos = restore;
    f->cursor_at_start = true;
}

void gmb_input_manager_init(gmb_input_manager_t* m)
{
    memset(m, 0, sizeof(*m));
    m->colors_fg = GMB_CELLCOLOR_GREEN;
    m->colors_bg = GMB_CELLCOLOR_BLACK;
    m->colors_exit_fg = GMB_CELLCOLOR_GREEN;
    m->colors_exit_bg = GMB_CELLCOLOR_BLACK;
}

void gmb_input_manager_add(gmb_input_manager_t* m, const gmb_input_field_t* f)
{
    if (m->field_count >= 16 || !f) return;
    m->fields[m->field_count++] = *f;
}

gmb_input_mask_t gmb_input_manager_mask_default(void) { return gmb_inputmask_default(); }
