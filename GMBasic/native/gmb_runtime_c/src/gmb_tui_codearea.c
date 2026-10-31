/* ============================================================================
 * gmb_tui_codearea.c — CodeArea.cs (specchio ClipperLib/Controls): gutter e
 * selezione testo.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gmb_codearea_format_line_number(int n, int width, char* out, int cap)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d|", n);
    int len = (int)strlen(buf);
    int pad = width - (len - 1);
    int total = (pad > 0 ? pad : 0) + len;
    if (total >= cap) total = cap - 1;
    int p = 0;
    for (int i = 0; i < pad && p < total; i++) out[p++] = ' ';
    for (int i = 0; buf[i] && p < total; i++) out[p++] = buf[i];
    out[p] = 0;
}

int gmb_codearea_gutter_inner_width(int lines)
{
    int digits = 1;
    int n = lines > 0 ? lines : 1;
    while (n >= 10) { n /= 10; digits++; }
    return digits < 2 ? 2 : digits;
}

int gmb_codearea_gutter_width(int lines)
{
    return gmb_codearea_gutter_inner_width(lines) + 1;
}

void gmb_text_selection_set(gmb_text_selection_t* s, int al, int ac, int el, int ec)
{
    s->anchor_line = al; s->anchor_col = ac;
    s->end_line = el; s->end_col = ec;
    s->active = true;
}
void gmb_text_selection_clear(gmb_text_selection_t* s, int cur_line, int cur_col)
{
    s->anchor_line = cur_line; s->anchor_col = cur_col;
    s->end_line = cur_line; s->end_col = cur_col;
    s->active = false;
}
bool gmb_text_selection_has(const gmb_text_selection_t* s)
{
    return s->active && !(s->anchor_line == s->end_line && s->anchor_col == s->end_col);
}
void gmb_text_selection_select_line(gmb_text_selection_t* s, int line, int line_len)
{
    gmb_text_selection_set(s, line, 0, line, line_len);
}
