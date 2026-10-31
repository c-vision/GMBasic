/* ============================================================================
 * gmb_tui_tablemodel.c — gmb_tui_tablemodel. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== TableModel ===================== */

int gmb_text_align_align(const char* text, int width, int align, char* out, int cap)
{
    if (!text) text = "";
    int tl = (int)strlen(text);
    if (tl > width)
    {
        snprintf(out, cap, "%.*s", width, text);
        return width;
    }
    if (tl == width || width <= 0) { snprintf(out, cap, "%s", text); return tl; }
    int pad = width - tl;
    int o = 0;
    if (align == GMB_HALIGN_RIGHT)
    {
        for (int i = 0; i < pad; i++) out[o++] = ' ';
        for (int i = 0; i < tl; i++) out[o++] = text[i];
    }
    else if (align == GMB_HALIGN_CENTER)
    {
        int half = pad / 2;
        for (int i = 0; i < half; i++) out[o++] = ' ';
        for (int i = 0; i < tl; i++) out[o++] = text[i];
        for (int i = half; i < pad; i++) out[o++] = ' ';
    }
    else
    {
        for (int i = 0; i < tl; i++) out[o++] = text[i];
        for (int i = 0; i < pad; i++) out[o++] = ' ';
    }
    out[o] = '\0';
    return o;
}

void gmb_table_columns_init(gmb_table_columns_t* c)
{
    memset(c, 0, sizeof(*c));
}
int gmb_table_columns_add(gmb_table_columns_t* c, const char* name)
{
    if (c->count >= 16) return -1;
    int idx = c->count;
    gmb_table_column_t* col = &c->cols[c->count++];
    snprintf(col->name, sizeof(col->name), "%s", name ? name : "");
    col->index = idx;
    return idx;
}
int gmb_table_columns_add_with_index(gmb_table_columns_t* c, const char* name, int index)
{
    if (c->count >= 16) return index;
    gmb_table_column_t* col = &c->cols[c->count++];
    snprintf(col->name, sizeof(col->name), "%s", name ? name : "");
    col->index = index;
    return index;
}
bool gmb_table_columns_remove(gmb_table_columns_t* c, int i)
{
    if (i < 0 || i >= c->count) return false;
    for (int j = i; j < c->count - 1; j++) c->cols[j] = c->cols[j + 1];
    c->count--;
    for (int j = i; j < c->count; j++) c->cols[j].index = c->cols[j].index - 1;
    return true;
}
bool gmb_table_columns_insert(gmb_table_columns_t* c, int i, const char* name)
{
    if (i < 0 || i > c->count || c->count >= 16) return false;
    for (int j = c->count; j > i; j--) c->cols[j] = c->cols[j - 1];
    gmb_table_column_t* col = &c->cols[i];
    snprintf(col->name, sizeof(col->name), "%s", name ? name : "");
    col->index = i;
    c->count++;
    for (int j = i + 1; j < c->count; j++) c->cols[j].index++;
    return true;
}
bool gmb_table_columns_reorder(gmb_table_columns_t* c, int from, int to)
{
    if (from < 0 || from >= c->count || to < 0 || to >= c->count) return false;
    gmb_table_column_t col = c->cols[from];
    for (int j = from; j < c->count - 1; j++) c->cols[j] = c->cols[j + 1];
    for (int j = c->count - 1; j > to; j--) c->cols[j] = c->cols[j - 1];
    c->cols[to] = col;
    for (int j = 0; j < c->count; j++) c->cols[j].index = j;
    return true;
}
gmb_table_column_t* gmb_table_columns_get(gmb_table_columns_t* c, int i)
{
    return (i >= 0 && i < c->count) ? &c->cols[i] : NULL;
}

int gmb_text_align(const char* text, int width, int align, char* out, int cap)
{
    return gmb_text_align_align(text, width, align, out, cap);
}

void gmb_column_init(gmb_column_t* c, const char* name, int index)
{
    if (!c) return;
    snprintf(c->name, sizeof(c->name), "%s", name ? name : "");
    c->index = index;
    c->width = 0;
    c->has_width = false;
    c->align = GMB_HALIGN_LEFT;
}
