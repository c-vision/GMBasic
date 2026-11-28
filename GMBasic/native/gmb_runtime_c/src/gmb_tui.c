/* ============================================================================
 * gmb_tui.c — Terminal User Interface (porting C della ClipperLib + TUI)
 * File: native/gmb_runtime_c/src/gmb_tui.c
 * FASE 0/1: CellColor + ScreenCell + encoder ANSI. Niente dipendenze OS.
 * ============================================================================ */
#include "gmb_tui.h"
#include "gmb_screen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



static void grid_compute_sizes(const int* sizes, int count, int total,
                               gmb_divider_strategy_t divider,
                               int* out)
{
    if (count == 0) return;
    long sum = 0;
    for (int i = 0; i < count; i++) sum += sizes[i];
    if (sum <= 0) sum = 1;
    int used = 0;
    for (int i = 0; i < count; i++)
    {
        int sz;
        if (divider == GMB_DIVIDER_EVEN)
            sz = total / count;
        else
            sz = (int)((long)sizes[i] * total / sum);
        if (i == count - 1)
            sz = total - used;   /* l'ultimo track assorbe il resto */
        if (sz < 0) sz = 0;
        out[i] = sz;
        used += sz;
    }
}

int gmb_grid_layout(gmb_rect_t area,
                    const int* cols, int col_count,
                    const int* rows, int row_count,
                    gmb_split_strategy_t split,
                    gmb_divider_strategy_t divider,
                    gmb_rect_t* out_rects, int out_cap)
{
    (void)split; /* per ora la griglia è sempre colonne-prima (Horizontal),
                    identica per split in presenza di colonne+righe esplicite */
    int n = col_count * row_count;
    if (n > out_cap) n = out_cap;
    if (n <= 0) return 0;
    int* colW = (int*)malloc((size_t)col_count * sizeof(int));
    int* rowH = (int*)malloc((size_t)row_count * sizeof(int));
    if (!colW || !rowH) { free(colW); free(rowH); return 0; }
    grid_compute_sizes(cols, col_count, area.w, divider, colW);
    grid_compute_sizes(rows, row_count, area.h, divider, rowH);

    int idx = 0;
    int x = area.x;
    for (int c = 0; c < col_count && idx < n; c++)
    {
        int y = area.y;
        for (int r = 0; r < row_count && idx < n; r++)
        {
            int cw = colW[c] < 0 ? 0 : colW[c];
            int rh = rowH[r] < 0 ? 0 : rowH[r];
            out_rects[idx].x = x; out_rects[idx].y = y;
            out_rects[idx].w = cw; out_rects[idx].h = rh;
            idx++;
            y += rh;
        }
        x += colW[c];
    }
    free(colW);
    free(rowH);
    return idx;
}

/* ============================================================================
 * Layout — solver direzionale (specchio Layout.cs, pulito).
 * ============================================================================ */
static int clamp_int(long v, int lo, int hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int)v;
}

int gmb_constraint_base(gmb_constraint_t c, int total)
{
    switch (c.tag)
    {
        case GMB_CONST_LENGTH: return c.a;
        case GMB_CONST_MIN: return c.a;
        case GMB_CONST_MAX: return c.a < total ? c.a : total;
        case GMB_CONST_PERCENTAGE: return clamp_int((long)c.a * total / 100, 0, total);
        case GMB_CONST_RATIO: return c.b == 0 ? 0 : clamp_int((long)c.a * total / c.b, 0, total);
        case GMB_CONST_MINMAX: return c.a;
        case GMB_CONST_PROPORTION: return 0;
        default: return 0;
    }
}

int gmb_constraint_minsize(gmb_constraint_t c)
{
    if (c.tag == GMB_CONST_MINMAX) return c.a;
    return gmb_constraint_base(c, 0x7FFFFFFF);
}

static void layout_solve(const gmb_constraint_t* cs, int n, int available,
                         int* sizes)
{
    long used = 0;
    for (int i = 0; i < n; i++)
    {
        sizes[i] = gmb_constraint_base(cs[i], available);
        used += sizes[i];
    }

    if (used < available)
    {
        long remaining = available - used;

        /* 1) Proportion (weighted). */
        long prop_weight = 0;
        for (int i = 0; i < n; i++)
            if (cs[i].tag == GMB_CONST_PROPORTION) prop_weight += cs[i].a;
        if (prop_weight > 0)
        {
            long total_extra = remaining;
            long distributed = 0;
            int last_prop = -1;
            for (int i = 0; i < n; i++)
            {
                if (cs[i].tag == GMB_CONST_PROPORTION)
                {
                    long share = total_extra * cs[i].a / prop_weight;
                    sizes[i] += (int)share;
                    distributed += share;
                    last_prop = i;
                }
            }
            if (last_prop >= 0)
            {
                sizes[last_prop] += (int)(total_extra - distributed);
            }
            remaining = 0;
        }

        /* 2) Min / MinMax (equal parts, remainder to first). */
        if (remaining > 0)
        {
            int grow_idx[64]; int grow_count = 0;
            for (int i = 0; i < n && grow_count < 64; i++)
                if (cs[i].tag == GMB_CONST_MIN || cs[i].tag == GMB_CONST_MINMAX)
                    grow_idx[grow_count++] = i;
            if (grow_count > 0)
            {
                long per = remaining / grow_count;
                long extra = remaining % grow_count;
                for (int k = 0; k < grow_count; k++)
                {
                    int i = grow_idx[k];
                    long add = per + (extra > 0 ? 1 : 0);
                    if (extra > 0) extra--;
                    long cap = cs[i].tag == GMB_CONST_MINMAX ? cs[i].b : 0x7FFFFFFF;
                    long v = (long)sizes[i] + add;
                    if (v > cap) v = cap;
                    sizes[i] = (int)v;
                }
                remaining = 0;
            }
        }

        /* 3) last segment. */
        if (remaining > 0)
        {
            sizes[n - 1] += (int)remaining;
        }
    }
    else if (used > available)
    {
        /* Over-subscribed: shrink from the last, never below min. */
        long overflow = used - available;
        for (int i = n - 1; i >= 0 && overflow > 0; i--)
        {
            long min = gmb_constraint_minsize(cs[i]);
            long reduce = (long)sizes[i] - min;
            if (reduce < 0) reduce = 0;
            if (reduce > overflow) reduce = overflow;
            sizes[i] -= (int)reduce;
            overflow -= reduce;
        }
    }
}

int gmb_layout_split(gmb_rect_t area, gmb_margin_t margin,
                     const gmb_constraint_t* cs, int n,
                     gmb_layout_direction_t dir, int spacing, gmb_flex_mode_t flex,
                     gmb_rect_t* out_rects, int out_cap)
{
    if (n <= 0 || n > out_cap) return 0;

    int inner_x = area.x + margin.horizontal;
    int inner_y = area.y + margin.vertical;
    int inner_w = area.w - margin.horizontal * 2; if (inner_w < 0) inner_w = 0;
    int inner_h = area.h - margin.vertical * 2;   if (inner_h < 0) inner_h = 0;

    int total_span = dir == GMB_DIR_HORIZONTAL ? inner_w : inner_h;
    int spacing_total = spacing * (n > 1 ? n - 1 : 0);
    int available = total_span - spacing_total; if (available < 0) available = 0;
    int base_start = dir == GMB_DIR_HORIZONTAL ? inner_x : inner_y;

    int sizes[64];
    int lead = 0, gap_extra = 0;

    if (flex == GMB_FLEX_LEGACY)
    {
        layout_solve(cs, n, available, sizes);
    }
    else
    {
        for (int i = 0; i < n; i++) sizes[i] = gmb_constraint_base(cs[i], available);
        long used = 0;
        for (int i = 0; i < n; i++) used += sizes[i];
        long leftover = available - used; if (leftover < 0) leftover = 0;
        switch (flex)
        {
            case GMB_FLEX_START: lead = 0; gap_extra = 0; break;
            case GMB_FLEX_END: lead = (int)leftover; gap_extra = 0; break;
            case GMB_FLEX_CENTER: lead = (int)(leftover / 2); gap_extra = 0; break;
            case GMB_FLEX_SPACE_BETWEEN:
                lead = 0;
                gap_extra = n > 1 ? (int)(leftover / (n - 1)) : (int)(leftover / 2);
                break;
            case GMB_FLEX_SPACE_AROUND:
                lead = (int)(leftover / (2 * n));
                gap_extra = (int)(leftover / n);
                break;
            default: lead = 0; gap_extra = 0; break;
        }
    }

    int offset = base_start + lead;
    for (int i = 0; i < n; i++)
    {
        int size = sizes[i];
        if (dir == GMB_DIR_HORIZONTAL)
        {
            out_rects[i].x = offset; out_rects[i].y = inner_y;
            out_rects[i].w = size;   out_rects[i].h = inner_h;
        }
        else
        {
            out_rects[i].x = inner_x; out_rects[i].y = offset;
            out_rects[i].w = inner_w; out_rects[i].h = size;
        }
        offset += size;
        if (i + 1 < n) offset += spacing + gap_extra;
    }
    return n;
}

/* ============================================================================
 * Input Mask — parsing template + apply (specchio Inputs/Mask.cs).
 * ============================================================================ */
#define GMB_MASK_MAX_SLOTS 256

struct gmb_mask {
    char* raw;
    char* template_body;
    gmb_mask_slot_t slots[GMB_MASK_MAX_SLOTS];
    int slot_count;
    bool force_uppercase;
    bool european_numbers;
    bool zero_as_blank;
    int visible_size;
    char fill_char;
    gmb_mask_datatype_t datatype;
};

/* mask spostato in gmb_tui_mask.c per evitare monolite */

static bool color_tok_contains(const char* t, char c)
{
    while (*t) { if (*t == c) return true; t++; }
    return false;
}

void gmb_color_parse_token(const char* tok, gmb_cell_color_t* out)
{
    if (!out) return;
    char t[64];
    size_t i = 0;
    if (!tok) tok = "N";
    while (tok[i] && i < sizeof(t) - 1)
    {
        char c = tok[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        t[i++] = c;
    }
    t[i] = '\0';

    bool bright = i > 0 && t[i - 1] == '+';
    if (bright) t[i - 1] = '\0';
    size_t core_len = strlen(t);
    if (core_len == 0 || (core_len == 1 && t[0] == 'N'))
    {
        *out = bright ? GMB_CELLCOLOR_GREY : GMB_CELLCOLOR_BLACK;
        return;
    }
    if (core_len == 1 && t[0] == 'W') { *out = GMB_CELLCOLOR_WHITE; return; }

    bool r = color_tok_contains(t, 'R');
    bool g = color_tok_contains(t, 'G');
    bool b = color_tok_contains(t, 'B');

    if (r && g && b) { *out = bright ? GMB_CELLCOLOR_WHITE : GMB_CELLCOLOR_GREY; return; }
    if (r && g) { *out = bright ? GMB_CELLCOLOR_YELLOW : GMB_CELLCOLOR_DARKYELLOW; return; }
    if (r && b) { *out = bright ? GMB_CELLCOLOR_MAGENTA : GMB_CELLCOLOR_DARKMAGENTA; return; }
    if (g && b) { *out = bright ? GMB_CELLCOLOR_CYAN : GMB_CELLCOLOR_DARKCYAN; return; }
    if (r) { *out = bright ? GMB_CELLCOLOR_RED : GMB_CELLCOLOR_DARKRED; return; }
    if (g) { *out = bright ? GMB_CELLCOLOR_GREEN : GMB_CELLCOLOR_DARKGREEN; return; }
    if (b) { *out = bright ? GMB_CELLCOLOR_BLUE : GMB_CELLCOLOR_DARKBLUE; return; }
    *out = GMB_CELLCOLOR_DEFAULT;
}

void gmb_color_parse_pair(const char* spec, gmb_cell_color_t* out_fg, gmb_cell_color_t* out_bg)
{
    char fg[64], bg[64];
    if (!spec) spec = "W/N";
    const char* slash = strchr(spec, '/');
    if (slash)
    {
        size_t fl = (size_t)(slash - spec);
        if (fl >= sizeof(fg)) fl = sizeof(fg) - 1;
        memcpy(fg, spec, fl); fg[fl] = '\0';
        size_t bl = strlen(slash + 1);
        if (bl >= sizeof(bg)) bl = sizeof(bg) - 1;
        memcpy(bg, slash + 1, bl); bg[bl] = '\0';
    }
    else
    {
        snprintf(fg, sizeof(fg), "%s", spec);
        snprintf(bg, sizeof(bg), "N");
    }
    gmb_color_parse_token(fg, out_fg);
    gmb_color_parse_token(bg, out_bg);
}

/* ============================================================================
 * FormControl / FormButton (specchio FormControl.cs / FormButton.cs).
 * ============================================================================ */
void gmb_form_control_init(gmb_form_control_t* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->width = 10;
    c->height = 1;
    c->visible = true;
    c->enabled = true;
    c->tabstop = true;
}

void gmb_form_control_free(gmb_form_control_t* c)
{
    if (!c) return;
    free(c->label);
    c->label = NULL;
}

void gmb_form_button_init(gmb_form_button_t* b)
{
    if (!b) return;
    memset(b, 0, sizeof(*b));
    gmb_form_control_init(&b->ctrl);
}

void gmb_form_button_free(gmb_form_button_t* b)
{
    if (!b) return;
    free(b->text);
    free(b->color);
    free(b->focus_color);
    gmb_form_control_free(&b->ctrl);
    memset(b, 0, sizeof(*b));
}

void gmb_form_button_draw(const gmb_form_button_t* b, gmb_screen_buffer_t* buf,
                          gmb_cell_color_t base_fg, gmb_cell_color_t base_bg,
                          bool blink_on)
{
    (void)blink_on;
    if (!b || !buf) return;
    if (!b->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (b->ctrl.focused && b->focus_color)
        gmb_color_parse_pair(b->focus_color, &fg, &bg);
    else if (b->color)
        gmb_color_parse_pair(b->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    const char* label = b->text && *b->text ? b->text
                       : (b->ctrl.label && *b->ctrl.label ? b->ctrl.label : "Ok");
    int cap_len = (int)strlen(label);
    int inner = b->ctrl.width - 2;
    if (cap_len > inner) cap_len = inner;
    if (cap_len < 0) cap_len = 0;
    int pad = inner - cap_len;
    int left_pad = pad / 2;
    int right_pad = pad - left_pad;

    char line[512];
    int n = 0;
    line[n++] = '<';
    for (int i = 0; i < left_pad && n < (int)sizeof(line) - 1; i++) line[n++] = ' ';
    for (int i = 0; i < cap_len && n < (int)sizeof(line) - 1; i++) line[n++] = label[i];
    for (int i = 0; i < right_pad && n < (int)sizeof(line) - 1; i++) line[n++] = ' ';
    line[n++] = '>';
    line[n] = '\0';

    gmb_screen_setstring(buf, b->ctrl.col, b->ctrl.row, line, fg, bg);
}

bool gmb_form_button_process_key(gmb_form_button_t* b, int key_code)
{
    if (!b || !b->ctrl.enabled) return false;
    if (key_code == 13 || key_code == ' ')
    {
        b->clicked = true;
        if (b->on_click) b->on_click(b->user_data);
        return true;
    }
    return false;
}

/* ============================================================================
 * FormCheckBox / FormProgressBar (specchio FormCheckBox.cs / FormProgressBar.cs)
 * ============================================================================ */
void gmb_form_checkbox_init(gmb_form_checkbox_t* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    gmb_form_control_init(&c->ctrl);
}

void gmb_form_checkbox_free(gmb_form_checkbox_t* c)
{
    if (!c) return;
    free(c->color);
    free(c->focus_color);
    gmb_form_control_free(&c->ctrl);
    memset(c, 0, sizeof(*c));
}

void gmb_form_checkbox_draw(const gmb_form_checkbox_t* c, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!c || !buf || !c->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (c->ctrl.focused && c->focus_color)
        gmb_color_parse_pair(c->focus_color, &fg, &bg);
    else if (c->color)
        gmb_color_parse_pair(c->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    const char* label = c->ctrl.label ? c->ctrl.label : "";
    int cap = (int)strlen(label);
    int inner = c->ctrl.width - 2;
    if (cap > inner) cap = inner;
    int pad = c->ctrl.width - cap - 2; if (pad < 0) pad = 0;

    char line[512];
    int n = 0;
    if (c->checked) { line[n++] = (char)0xE2; line[n++] = (char)0x97; line[n++] = (char)0x8F; } /* ● */
    else            { line[n++] = (char)0xE2; line[n++] = (char)0x97; line[n++] = (char)0x8B; } /* ○ */
    line[n++] = ' ';
    for (int i = 0; i < cap && n < (int)sizeof(line) - 1; i++) line[n++] = label[i];
    for (int i = 0; i < pad && n < (int)sizeof(line) - 1; i++) line[n++] = ' ';
    line[n] = '\0';

    gmb_screen_setstring(buf, c->ctrl.col, c->ctrl.row, line, fg, bg);
}

bool gmb_form_checkbox_process_key(gmb_form_checkbox_t* c, int key_code)
{
    if (!c || !c->ctrl.enabled) return false;
    if (key_code == ' ')
    {
        c->checked = !c->checked;
        if (c->on_toggle) c->on_toggle(c->user_data, c->checked);
        return true;
    }
    return false;
}

void gmb_form_progress_init(gmb_form_progress_t* p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    gmb_form_control_init(&p->ctrl);
    p->max = 100;
    p->show_percent = true;
}

void gmb_form_progress_free(gmb_form_progress_t* p)
{
    if (!p) return;
    free(p->color);
    free(p->fill_char);
    gmb_form_control_free(&p->ctrl);
    memset(p, 0, sizeof(*p));
}

void gmb_form_progress_draw(const gmb_form_progress_t* p, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!p || !buf || !p->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (p->color) gmb_color_parse_pair(p->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    double pct = (p->max > p->min) ? (double)(p->value - p->min) / (double)(p->max - p->min) : 0.0;
    if (pct < 0) pct = 0;
    if (pct > 1) pct = 1;
    int inner = p->ctrl.width - 2; if (inner < 0) inner = 0;
    int fill_w = (int)(inner * pct);
    if (fill_w > inner) fill_w = inner;

    /* "[" + fill + dot + "]" con fill ■ (3 byte) e dot · (2 byte) */
    char line[512];
    int n = 0;
    line[n++] = '[';
    for (int i = 0; i < fill_w; i++)
    {
        if (n + 3 >= (int)sizeof(line)) break;
        line[n++] = (char)0xE2; line[n++] = (char)0x96; line[n++] = (char)0xA0; /* ■ */
    }
    for (int i = fill_w; i < inner; i++)
    {
        if (n + 2 >= (int)sizeof(line)) break;
        line[n++] = (char)0xC2; line[n++] = (char)0xB7; /* · */
    }
    line[n++] = ']';
    line[n] = '\0';
    gmb_screen_setstring(buf, p->ctrl.col, p->ctrl.row, line, fg, bg);

    if (p->show_percent)
    {
        char pct_s[16];
        snprintf(pct_s, sizeof(pct_s), "%d%%", (int)(pct * 100));
        int pct_x = p->ctrl.col + (p->ctrl.width - (int)strlen(pct_s)) / 2;
        gmb_screen_setstring(buf, pct_x, p->ctrl.row, pct_s, fg, bg);
    }
}

/* ============================================================================
 * FormNumeric (specchio FormNumeric.cs).
 * ============================================================================ */
void gmb_form_numeric_init(gmb_form_numeric_t* n)
{
    if (!n) return;
    memset(n, 0, sizeof(*n));
    gmb_form_control_init(&n->ctrl);
    n->max = 100;
    n->step = 1;
    n->fill = ' ';
}

void gmb_form_numeric_free(gmb_form_numeric_t* n)
{
    if (!n) return;
    free(n->color);
    free(n->focus_color);
    gmb_form_control_free(&n->ctrl);
    memset(n, 0, sizeof(*n));
}

double gmb_form_numeric_clamp(const gmb_form_numeric_t* n, double v)
{
    if (!n) return v;
    if (v < n->min) return n->min;
    if (v > n->max) return n->max;
    return v;
}

static void numeric_format(double v, int decimals, char* out, size_t cap)
{
    if (decimals > 0)
        snprintf(out, cap, "%.*f", decimals, v);
    else
        snprintf(out, cap, "%.0f", v);
}

void gmb_form_numeric_draw(const gmb_form_numeric_t* n, gmb_screen_buffer_t* buf,
                           gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!n || !buf || !n->ctrl.visible) return;

    gmb_cell_color_t norm_fg, norm_bg, hl_fg, hl_bg;
    if (n->color) gmb_color_parse_pair(n->color, &norm_fg, &norm_bg);
    else { norm_fg = base_fg; norm_bg = base_bg; }
    if (n->ctrl.focused && n->focus_color) gmb_color_parse_pair(n->focus_color, &hl_fg, &hl_bg);
    else { hl_fg = norm_fg; hl_bg = norm_bg; }

    char val[64];
    numeric_format(n->value, n->decimals, val, sizeof(val));
    int val_max = n->ctrl.width - 3; if (val_max < 1) val_max = 1;
    if ((int)strlen(val) > val_max)
    {
        memset(val, '#', val_max);
        val[val_max] = '\0';
    }
    int fill_len = val_max - (int)strlen(val);
    if (fill_len < 0) fill_len = 0;

    char value_part[128];
    int n2 = 0;
    for (int i = 0; i < fill_len && n2 < (int)sizeof(value_part) - 1; i++)
        value_part[n2++] = n->fill ? n->fill : ' ';
    for (int i = 0; val[i] && n2 < (int)sizeof(value_part) - 1; i++)
        value_part[n2++] = val[i];
    value_part[n2] = '\0';

    if (n->ctrl.width > 0)
    {
        char spaces[128];
        int wn = n->ctrl.width; if (wn > (int)sizeof(spaces) - 1) wn = (int)sizeof(spaces) - 1;
        memset(spaces, ' ', (size_t)wn); spaces[wn] = '\0';
        gmb_screen_setstring(buf, n->ctrl.col, n->ctrl.row, spaces, norm_fg, norm_bg);
    }
    gmb_screen_setstring(buf, n->ctrl.col, n->ctrl.row, value_part, hl_fg, hl_bg);
    /* arrow: " ▲▼" — ▲=E2 96 B2 ▼=E2 96 BC */
    int ax = n->ctrl.col + (int)strlen(value_part);
    char arrows[8];
    arrows[0] = ' ';
    arrows[1] = (char)0xE2; arrows[2] = (char)0x96; arrows[3] = (char)0xB2;
    arrows[4] = (char)0xE2; arrows[5] = (char)0x96; arrows[6] = (char)0xBC;
    arrows[7] = '\0';
    gmb_screen_setstring(buf, ax, n->ctrl.row, arrows, norm_fg, norm_bg);
}

bool gmb_form_numeric_process_key(gmb_form_numeric_t* n, int key_code)
{
    if (!n || !n->ctrl.enabled) return false;
    double proposed;
    switch (key_code)
    {
        case 5:   /* K_UP */
            proposed = gmb_form_numeric_clamp(n, n->value + n->step);
            break;
        case 24:  /* K_DOWN */
            proposed = gmb_form_numeric_clamp(n, n->value - n->step);
            break;
        case 18:  /* K_PGUP */
            proposed = gmb_form_numeric_clamp(n, n->value + n->step * 10);
            break;
        case 3:   /* K_PGDN */
            proposed = gmb_form_numeric_clamp(n, n->value - n->step * 10);
            break;
        default:
            if ((key_code >= '0' && key_code <= '9') || key_code == '.')
            {
                char cur[64];
                numeric_format(n->value, n->decimals, cur, sizeof(cur));
                char proposed_str[128];
                if (n->value < 1e-10 && n->value > -1e-10 && key_code != '.')
                {
                    proposed_str[0] = (char)key_code;
                    proposed_str[1] = '\0';
                }
                else
                {
                    snprintf(proposed_str, sizeof(proposed_str), "%s%c", cur, (char)key_code);
                }
                /* dedup del punto: conserva solo la prima occorrenza */
                char dedup[128];
                int d = 0;
                bool seen_dot = false;
                for (int i = 0; proposed_str[i] && d < (int)sizeof(dedup) - 1; i++)
                {
                    if (proposed_str[i] == '.')
                    {
                        if (seen_dot) continue;
                        seen_dot = true;
                    }
                    dedup[d++] = proposed_str[i];
                }
                dedup[d] = '\0';
                double parsed;
                if (sscanf(dedup, "%lf", &parsed) == 1)
                {
                    proposed = gmb_form_numeric_clamp(n, parsed);
                    break;
                }
                return true;
            }
            return false;
    }
    n->value = proposed;
    if (n->on_value_change) n->on_value_change(n->user_data, proposed);
    return true;
}

/* ============================================================================
 * FormRadioButton (specchio FormRadioButton.cs).
 * ============================================================================ */
void gmb_form_radio_init(gmb_form_radio_t* r)
{
    if (!r) return;
    memset(r, 0, sizeof(*r));
    gmb_form_control_init(&r->ctrl);
}

void gmb_form_radio_free(gmb_form_radio_t* r)
{
    if (!r) return;
    free(r->group_name);
    free(r->color);
    free(r->focus_color);
    gmb_form_control_free(&r->ctrl);
    memset(r, 0, sizeof(*r));
}

void gmb_form_radio_draw(const gmb_form_radio_t* r, gmb_screen_buffer_t* buf,
                         gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!r || !buf || !r->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (r->ctrl.focused && r->focus_color)
        gmb_color_parse_pair(r->focus_color, &fg, &bg);
    else if (r->color)
        gmb_color_parse_pair(r->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    const char* label = r->ctrl.label ? r->ctrl.label : "";
    int cap = (int)strlen(label);
    int inner = r->ctrl.width - 2;
    if (cap > inner) cap = inner;
    int pad = r->ctrl.width - cap - 2; if (pad < 0) pad = 0;

    char line[512];
    int n = 0;
    if (r->checked) { line[n++] = (char)0xE2; line[n++] = (char)0x97; line[n++] = (char)0x86; } /* ◆ U+25C6 */
    else            { line[n++] = (char)0xE2; line[n++] = (char)0x97; line[n++] = (char)0x87; } /* ◇ U+25C7 */
    line[n++] = ' ';
    for (int i = 0; i < cap && n < (int)sizeof(line) - 1; i++) line[n++] = label[i];
    for (int i = 0; i < pad && n < (int)sizeof(line) - 1; i++) line[n++] = ' ';
    line[n] = '\0';

    gmb_screen_setstring(buf, r->ctrl.col, r->ctrl.row, line, fg, bg);
}

bool gmb_form_radio_process_key(gmb_form_radio_t* r, int key_code)
{
    if (!r || !r->ctrl.enabled) return false;
    if (key_code == ' ')
    {
        if (r->checked) return true;
        r->checked = true;
        if (r->on_check) r->on_check(r->user_data, true);
        return true;
    }
    return false;
}

/* ============================================================================
 * FormListBox (specchio FormListBox.cs).
 * ============================================================================ */
void gmb_form_listbox_init(gmb_form_listbox_t* l)
{
    if (!l) return;
    memset(l, 0, sizeof(*l));
    gmb_form_control_init(&l->ctrl);
    l->selected_index = -1;
}

void gmb_form_listbox_free(gmb_form_listbox_t* l)
{
    if (!l) return;
    for (int i = 0; i < l->item_count; i++) free(l->items[i]);
    free(l->items);
    free(l->color);
    free(l->focus_color);
    free(l->selected_color);
    gmb_form_control_free(&l->ctrl);
    memset(l, 0, sizeof(*l));
}

int gmb_form_listbox_set_items(gmb_form_listbox_t* l, const char* const* items, int count)
{
    if (!l || count < 0) return 0;
    for (int i = 0; i < l->item_count; i++) free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->item_count = 0;
    l->selected_index = -1;
    l->top_index = 0;
    if (count == 0) return 1;
    char** arr = (char**)calloc((size_t)count, sizeof(char*));
    if (!arr) return 0;
    for (int i = 0; i < count; i++)
    {
        arr[i] = items && items[i] ? strdup(items[i]) : strdup("");
        if (!arr[i]) { for (int j = 0; j < i; j++) free(arr[j]); free(arr); return 0; }
    }
    l->items = arr;
    l->item_count = count;
    return 1;
}

const char* gmb_form_listbox_selected_value(const gmb_form_listbox_t* l)
{
    if (!l || l->selected_index < 0 || l->selected_index >= l->item_count) return NULL;
    return l->items[l->selected_index];
}

void gmb_form_listbox_draw(const gmb_form_listbox_t* l, gmb_screen_buffer_t* buf,
                           gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!l || !buf || !l->ctrl.visible) return;

    gmb_cell_color_t bg, sel_fg, sel_bg;
    gmb_cell_color_t unused_fg;
    if (l->ctrl.focused && l->focus_color)
        gmb_color_parse_pair(l->focus_color, &unused_fg, &bg);
    else if (l->color)
        gmb_color_parse_pair(l->color, &unused_fg, &bg);
    else bg = base_bg;

    if (l->selected_color) gmb_color_parse_pair(l->selected_color, &sel_fg, &sel_bg);
    else { sel_fg = base_fg; sel_bg = base_bg; }

    for (int i = 0; i < l->ctrl.height; i++)
    {
        int idx = l->top_index + i;
        const char* line = (idx >= 0 && idx < l->item_count) ? l->items[idx] : "";
        int len = (int)strlen(line);
        char buf_line[512];
        int n = 0;
        int cap = len > l->ctrl.width ? l->ctrl.width : len;
        for (int k = 0; k < cap && n < (int)sizeof(buf_line) - 1; k++) buf_line[n++] = line[k];
        int pad = l->ctrl.width - cap; if (pad < 0) pad = 0;
        for (int k = 0; k < pad && n < (int)sizeof(buf_line) - 1; k++) buf_line[n++] = ' ';
        buf_line[n] = '\0';

        bool is_sel = (idx == l->selected_index);
        gmb_cell_color_t fg = is_sel ? sel_fg : base_fg;
        gmb_cell_color_t bgc = is_sel ? sel_bg : bg;
        gmb_screen_setstring(buf, l->ctrl.col, l->ctrl.row + i, buf_line, fg, bgc);
    }
}

bool gmb_form_listbox_process_key(gmb_form_listbox_t* l, int key_code)
{
    if (!l || !l->ctrl.enabled) return false;
    if (l->item_count == 0) return false;

    int proposed = l->selected_index;
    switch (key_code)
    {
        case 5:   /* K_UP */
            if (l->selected_index > 0) proposed = l->selected_index - 1;
            else return true;
            break;
        case 24:  /* K_DOWN */
            if (l->selected_index < l->item_count - 1) proposed = l->selected_index + 1;
            else return true;
            break;
        case 18:  /* K_PGUP */
            proposed = l->selected_index - l->ctrl.height;
            if (proposed < 0) proposed = 0;
            break;
        case 3:   /* K_PGDN */
            proposed = l->selected_index + l->ctrl.height;
            if (proposed > l->item_count - 1) proposed = l->item_count - 1;
            break;
        case 1:   /* K_HOME */
            proposed = 0;
            break;
        case 6:   /* K_END */
            proposed = l->item_count - 1;
            break;
        default:
            return false;
    }

    if (proposed == l->selected_index) return true;

    l->selected_index = proposed;
    if (l->selected_index < l->top_index) l->top_index = l->selected_index;
    if (l->selected_index >= l->top_index + l->ctrl.height)
        l->top_index = l->selected_index - l->ctrl.height + 1;
    if (l->top_index < 0) l->top_index = 0;

    if (l->on_select) l->on_select(l->user_data, l->selected_index);
    return true;
}

/* ============================================================================
 * FormComboBox (specchio FormComboBox.cs).
 * ============================================================================ */
void gmb_form_combobox_init(gmb_form_combobox_t* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    gmb_form_control_init(&c->ctrl);
    c->selected_index = -1;
    c->open_sel = -1;
    c->drop_down_height = 5;
}

void gmb_form_combobox_free(gmb_form_combobox_t* c)
{
    if (!c) return;
    for (int i = 0; i < c->item_count; i++) free(c->items[i]);
    free(c->items);
    free(c->color);
    free(c->focus_color);
    free(c->list_color);
    gmb_form_control_free(&c->ctrl);
    memset(c, 0, sizeof(*c));
}

int gmb_form_combobox_set_items(gmb_form_combobox_t* c, const char* const* items, int count)
{
    if (!c || count < 0) return 0;
    for (int i = 0; i < c->item_count; i++) free(c->items[i]);
    free(c->items);
    c->items = NULL;
    c->item_count = 0;
    c->selected_index = -1;
    c->open_sel = -1;
    if (count == 0) return 1;
    char** arr = (char**)calloc((size_t)count, sizeof(char*));
    if (!arr) return 0;
    for (int i = 0; i < count; i++)
    {
        arr[i] = items && items[i] ? strdup(items[i]) : strdup("");
        if (!arr[i]) { for (int j = 0; j < i; j++) free(arr[j]); free(arr); return 0; }
    }
    c->items = arr;
    c->item_count = count;
    return 1;
}

const char* gmb_form_combobox_selected_value(const gmb_form_combobox_t* c)
{
    if (!c || c->selected_index < 0 || c->selected_index >= c->item_count) return NULL;
    return c->items[c->selected_index];
}

void gmb_form_combobox_draw(const gmb_form_combobox_t* c, gmb_screen_buffer_t* buf,
                            gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!c || !buf || !c->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (c->ctrl.focused && c->focus_color)
        gmb_color_parse_pair(c->focus_color, &fg, &bg);
    else if (c->color)
        gmb_color_parse_pair(c->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    const char* val = gmb_form_combobox_selected_value(c);
    if (!val) val = "";
    int inner = c->ctrl.width - 2;
    if (inner < 0) inner = 0;
    char head[512];
    int n = 0;
    head[n++] = '[';
    int vl = (int)strlen(val);
    int cap = vl > inner ? inner : vl;
    for (int i = 0; i < cap && n < (int)sizeof(head) - 1; i++) head[n++] = val[i];
    int pad = inner - cap;
    for (int i = 0; i < pad && n < (int)sizeof(head) - 1; i++) head[n++] = ' ';
    /* ▼ U+25BC = E2 96 BC */
    head[n++] = (char)0xE2; head[n++] = (char)0x96; head[n++] = (char)0xBC;
    head[n] = '\0';
    gmb_screen_setstring(buf, c->ctrl.col, c->ctrl.row, head, fg, bg);

    if (c->dropped_down)
    {
        gmb_cell_color_t list_bg;
        gmb_cell_color_t unused_fg;
        if (c->list_color) gmb_color_parse_pair(c->list_color, &unused_fg, &list_bg);
        else list_bg = base_bg;
        gmb_cell_color_t list_fg = base_fg;
        int visible = c->drop_down_height < c->item_count ? c->drop_down_height : c->item_count;
        for (int i = 0; i < visible; i++)
        {
            const char* line = c->items[i];
            int len = (int)strlen(line);
            char buf_line[512];
            int n2 = 0;
            int capI = len > c->ctrl.width ? c->ctrl.width : len;
            for (int k = 0; k < capI && n2 < (int)sizeof(buf_line) - 1; k++) buf_line[n2++] = line[k];
            int padI = c->ctrl.width - capI; if (padI < 0) padI = 0;
            for (int k = 0; k < padI && n2 < (int)sizeof(buf_line) - 1; k++) buf_line[n2++] = ' ';
            buf_line[n2] = '\0';
            bool is_sel = (i == c->open_sel);
            gmb_cell_color_t fgi = is_sel ? list_bg : list_fg;
            gmb_cell_color_t bgi = is_sel ? list_fg : list_bg;
            gmb_screen_setstring(buf, c->ctrl.col, c->ctrl.row + 1 + i, buf_line, fgi, bgi);
        }
    }
}

bool gmb_form_combobox_process_key(gmb_form_combobox_t* c, int key_code)
{
    if (!c || !c->ctrl.enabled) return false;
    if (c->item_count == 0) return false;

    if (key_code == 13 || key_code == ' ')
    {
        if (!c->dropped_down)
        {
            c->dropped_down = true;
            c->open_sel = c->selected_index >= 0 ? c->selected_index : 0;
        }
        else
        {
            int prop = c->open_sel;
            if (prop != c->selected_index)
            {
                c->selected_index = prop;
                if (c->on_change) c->on_change(c->user_data, prop);
            }
            c->dropped_down = false;
            c->open_sel = -1;
        }
        return true;
    }

    if (key_code == 27 && c->dropped_down)
    {
        c->dropped_down = false;
        c->open_sel = -1;
        return true;
    }

    if (c->dropped_down)
    {
        int prop = c->open_sel;
        switch (key_code)
        {
            case 5:  /* UP */
                if (c->open_sel > 0) prop = c->open_sel - 1;
                else return true;
                break;
            case 24: /* DOWN */
                if (c->open_sel < c->item_count - 1) prop = c->open_sel + 1;
                else return true;
                break;
            case 1:  /* HOME */
                prop = 0;
                break;
            case 6:  /* END */
                prop = c->item_count - 1;
                break;
            default:
                return false;
        }
        if (prop != c->open_sel) c->open_sel = prop;
        return true;
    }

    return false;
}

/* ============================================================================
 * FormMemo (specchio FormMemo.cs).
 * ============================================================================ */
void gmb_form_memo_init(gmb_form_memo_t* m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    gmb_form_control_init(&m->ctrl);
    m->word_wrap = true;
}

void gmb_form_memo_free(gmb_form_memo_t* m)
{
    if (!m) return;
    free(m->value);
    free(m->color);
    free(m->focus_color);
    gmb_form_control_free(&m->ctrl);
    memset(m, 0, sizeof(*m));
}

void gmb_form_memo_set_value(gmb_form_memo_t* m, const char* value)
{
    if (!m) return;
    free(m->value);
    m->value = value ? strdup(value) : strdup("");
    /* normalizza il cursore */
    int cnt = gmb_form_memo_line_count(m);
    if (m->cur_row > cnt - 1) m->cur_row = cnt - 1;
    if (m->cur_row < 0) m->cur_row = 0;
    int len;
    char tmp[1];
    len = gmb_form_memo_line(m, m->cur_row, tmp, 1);
    (void)len;
}

const char* gmb_form_memo_value(const gmb_form_memo_t* m) { return m ? (m->value ? m->value : "") : ""; }

int gmb_form_memo_line_count(const gmb_form_memo_t* m)
{
    if (!m || !m->value) return 1;
    int n = 1;
    for (const char* p = m->value; *p; p++) if (*p == '\n') n++;
    return n;
}

int gmb_form_memo_line(const gmb_form_memo_t* m, int r, char* out, int out_cap)
{
    if (!m || !m->value || out_cap <= 0) return 0;
    const char* p = m->value;
    int cur = 0;
    int written = 0;
    while (*p)
    {
        if (*p == '\n')
        {
            cur++;
            p++;
            continue;
        }
        if (cur == r)
        {
            if (written < out_cap - 1) out[written++] = *p;
        }
        if (cur > r) break;
        p++;
    }
    if (out_cap > 0) out[written] = '\0';
    return written;
}

static void memo_replace(gmb_form_memo_t* m, char* new_value)
{
    free(m->value);
    m->value = new_value ? new_value : strdup("");
}

void gmb_form_memo_draw(const gmb_form_memo_t* m, gmb_screen_buffer_t* buf,
                        gmb_cell_color_t base_fg, gmb_cell_color_t base_bg, bool blink_on)
{
    (void)blink_on;
    if (!m || !buf || !m->ctrl.visible) return;

    gmb_cell_color_t fg, bg;
    if (m->ctrl.focused && m->focus_color)
        gmb_color_parse_pair(m->focus_color, &fg, &bg);
    else if (m->color)
        gmb_color_parse_pair(m->color, &fg, &bg);
    else { fg = base_fg; bg = base_bg; }

    int lines = gmb_form_memo_line_count(m);
    for (int i = 0; i < m->ctrl.height; i++)
    {
        int idx = m->scroll_row + i;
        char line[512];
        int len = gmb_form_memo_line(m, idx, line, (int)sizeof(line));
        char buf_line[512];
        int n = 0;
        int cap = len > m->ctrl.width ? m->ctrl.width : len;
        for (int k = 0; k < cap && n < (int)sizeof(buf_line) - 1; k++) buf_line[n++] = line[k];
        int pad = m->ctrl.width - cap; if (pad < 0) pad = 0;
        for (int k = 0; k < pad && n < (int)sizeof(buf_line) - 1; k++) buf_line[n++] = ' ';
        buf_line[n] = '\0';
        gmb_screen_setstring(buf, m->ctrl.col, m->ctrl.row + i, buf_line, fg, bg);
        (void)lines;
    }
}

static void memo_ensure_scroll(gmb_form_memo_t* m)
{
    if (m->cur_row < m->scroll_row) m->scroll_row = m->cur_row;
    if (m->cur_row >= m->scroll_row + m->ctrl.height)
        m->scroll_row = m->cur_row - m->ctrl.height + 1;
}

bool gmb_form_memo_process_key(gmb_form_memo_t* m, int key_code)
{
    if (!m || !m->ctrl.enabled) return false;
    if (!m->value) m->value = strdup("");

    int lines = gmb_form_memo_line_count(m);
    int cur_r = m->cur_row;
    if (cur_r > lines - 1) cur_r = lines - 1;
    if (cur_r < 0) cur_r = 0;

    char line[1024];
    int line_len = gmb_form_memo_line(m, cur_r, line, (int)sizeof(line));

    /* funzioni che ricostruiscono il value da righe */
    switch (key_code)
    {
        case 127: /* K_BS */
        {
            if (m->cur_col > 0)
            {
                /* riga: line[0..cur-1] + line[cur..] */
                char* nv = (char*)malloc(strlen(m->value) + 2);
                if (!nv) return true;
                /* costruzione: sostituisci nel value la riga cur_r */
                char prefix[4096], suffix[4096];
                /* troviamo gli offset della riga cur_r nel value */
                const char* p = m->value;
                int r = 0;
                size_t start = 0, end = strlen(m->value);
                while (*p)
                {
                    if (r == cur_r) break;
                    if (*p == '\n') r++;
                    p++;
                }
                start = (size_t)(p - m->value);
                const char* q = p;
                while (*q && *q != '\n') q++;
                end = (size_t)(q - m->value);
                size_t plen = start, slen = strlen(m->value) - end;
                memcpy(prefix, m->value, plen); prefix[plen] = '\0';
                memcpy(suffix, m->value + end, slen + 1);
                snprintf(nv, strlen(m->value) + 2, "%s%.*s%s", prefix, m->cur_col - 1, line, suffix);
                memo_replace(m, nv);
                m->cur_col--;
                if (m->on_change) m->on_change(m->user_data, m->value);
                return true;
            }
            if (cur_r > 0)
            {
                int prev_len = 0;
                char pl[1024];
                prev_len = gmb_form_memo_line(m, cur_r - 1, pl, (int)sizeof(pl));
                const char* p = m->value;
                int r = 0;
                while (*p && r < cur_r - 1) { if (*p == '\n') r++; p++; }
                size_t start = (size_t)(p - m->value);
                const char* q = p; while (*q && *q != '\n') q++;
                size_t end = (size_t)(q - m->value);
                char prefix[4096], suffix[4096];
                size_t plen = start, slen = strlen(m->value) - end;
                memcpy(prefix, m->value, plen); prefix[plen] = '\0';
                memcpy(suffix, m->value + end, slen + 1);
                char* nv = (char*)malloc(strlen(m->value) + 2);
                if (!nv) return true;
                snprintf(nv, strlen(m->value) + 2, "%s%s%s", prefix, pl, suffix);
                memo_replace(m, nv);
                m->cur_row = cur_r - 1;
                m->cur_col = prev_len;
                memo_ensure_scroll(m);
                if (m->on_change) m->on_change(m->user_data, m->value);
                return true;
            }
            return true;
        }
        case 7: /* K_DEL */
        {
            if (m->cur_col < line_len)
            {
                const char* p = m->value;
                int r = 0;
                while (*p && r < cur_r) { if (*p == '\n') r++; p++; }
                size_t start = (size_t)(p - m->value);
                const char* q = p; while (*q && *q != '\n') q++;
                size_t end = (size_t)(q - m->value);
                char prefix[4096], suffix[4096];
                size_t plen = start, slen = strlen(m->value) - end;
                memcpy(prefix, m->value, plen); prefix[plen] = '\0';
                memcpy(suffix, m->value + end, slen + 1);
                char* nv = (char*)malloc(strlen(m->value) + 2);
                if (!nv) return true;
                snprintf(nv, strlen(m->value) + 2, "%s%.*s%s", prefix, m->cur_col, line, suffix + (m->cur_col < (int)slen ? m->cur_col : 0));
                /* approccio più semplice: prefix + line senza il char a cur + suffix */
                char mid[2048];
                int mn = 0;
                for (int i = 0; i < line_len && mn < (int)sizeof(mid) - 1; i++)
                    if (i != m->cur_col) mid[mn++] = line[i];
                mid[mn] = '\0';
                snprintf(nv, strlen(m->value) + 2, "%s%s%s", prefix, mid, suffix);
                memo_replace(m, nv);
                if (m->on_change) m->on_change(m->user_data, m->value);
                return true;
            }
            return true;
        }
        case 13: /* ENTER */
        {
            const char* p = m->value;
            int r = 0;
            while (*p && r < cur_r) { if (*p == '\n') r++; p++; }
            size_t start = (size_t)(p - m->value);
            const char* q = p; while (*q && *q != '\n') q++;
            size_t end = (size_t)(q - m->value);
            char prefix[4096], suffix[4096];
            size_t plen = start, slen = strlen(m->value) - end;
            memcpy(prefix, m->value, plen); prefix[plen] = '\0';
            memcpy(suffix, m->value + end, slen + 1);
            char* nv = (char*)malloc(strlen(m->value) + 3);
            if (!nv) return true;
            snprintf(nv, strlen(m->value) + 3, "%s%.*s\n%s%s", prefix, m->cur_col, line, line + m->cur_col, suffix);
            memo_replace(m, nv);
            m->cur_row = cur_r + 1;
            m->cur_col = 0;
            memo_ensure_scroll(m);
            if (m->on_change) m->on_change(m->user_data, m->value);
            return true;
        }
        case 5: /* UP */
            if (cur_r > 0)
            {
                m->cur_row = cur_r - 1;
                int l2 = gmb_form_memo_line(m, m->cur_row, line, (int)sizeof(line));
                if (m->cur_col > l2) m->cur_col = l2;
            }
            memo_ensure_scroll(m);
            return true;
        case 24: /* DOWN */
            if (cur_r < lines - 1)
            {
                m->cur_row = cur_r + 1;
                int l2 = gmb_form_memo_line(m, m->cur_row, line, (int)sizeof(line));
                if (m->cur_col > l2) m->cur_col = l2;
            }
            memo_ensure_scroll(m);
            return true;
        case 19: /* LEFT */
            if (m->cur_col > 0) { m->cur_col--; return true; }
            if (cur_r > 0)
            {
                m->cur_row = cur_r - 1;
                m->cur_col = gmb_form_memo_line(m, m->cur_row, line, (int)sizeof(line));
            }
            memo_ensure_scroll(m);
            return true;
        case 4: /* RIGHT */
        {
            int l2 = gmb_form_memo_line(m, cur_r, line, (int)sizeof(line));
            if (m->cur_col < l2) { m->cur_col++; return true; }
            if (cur_r < lines - 1) { m->cur_row = cur_r + 1; m->cur_col = 0; }
            memo_ensure_scroll(m);
            return true;
        }
        case 1: /* HOME */
            m->cur_col = 0;
            memo_ensure_scroll(m);
            return true;
        case 6: /* END */
        {
            int l2 = gmb_form_memo_line(m, m->cur_row, line, (int)sizeof(line));
            m->cur_col = l2;
            memo_ensure_scroll(m);
            return true;
        }
        default:
            if (key_code >= 32 && key_code <= 126)
            {
                const char* p = m->value;
                int r = 0;
                while (*p && r < cur_r) { if (*p == '\n') r++; p++; }
                size_t start = (size_t)(p - m->value);
                const char* q = p; while (*q && *q != '\n') q++;
                size_t end = (size_t)(q - m->value);
                char prefix[4096], suffix[4096];
                size_t plen = start, slen = strlen(m->value) - end;
                memcpy(prefix, m->value, plen); prefix[plen] = '\0';
                memcpy(suffix, m->value + end, slen + 1);
                char* nv = (char*)malloc(strlen(m->value) + 3);
                if (!nv) return true;
                /* riga modificata: line[0..cur) + ch + line[cur..] */
                char mid[2048];
                int mn = 0;
                for (int i = 0; i < m->cur_col && mn < (int)sizeof(mid) - 1; i++) mid[mn++] = line[i];
                mid[mn++] = (char)key_code;
                for (int i = m->cur_col; i < line_len && mn < (int)sizeof(mid) - 1; i++) mid[mn++] = line[i];
                mid[mn] = '\0';
                snprintf(nv, strlen(m->value) + 3, "%s%s%s", prefix, mid, suffix);
                memo_replace(m, nv);
                m->cur_col++;
                if (m->on_change) m->on_change(m->user_data, m->value);
                return true;
            }
            return false;
    }
}

/* ============================================================================
 * Data — logica pura FormDatePicker (specchio FormDatePicker.cs).
 * ============================================================================ */
bool gmb_date_is_leap_year(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int gmb_date_days_in_month(int y, int m)
{
    switch (m)
    {
        case 2: return gmb_date_is_leap_year(y) ? 29 : 28;
        case 4: case 6: case 9: case 11: return 30;
        default: return 31;
    }
}

/* Algoritmo di Sakamoto per il giorno della settimana (0=Domenica..6=Sabato),
   equivalente a System.DateTime.DayOfWeek. */
int gmb_date_day_of_week_idx(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

bool gmb_date_is_valid(int d, int m, int y, int min_year, int max_year)
{
    return m >= 1 && m <= 12 && d >= 1 && d <= gmb_date_days_in_month(y, m)
        && y >= min_year && y <= max_year;
}

int gmb_date_format(int d, int m, int y, const char* fmt, bool century, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    int yy = century ? y : y % 100;
    const char* f = (fmt && *fmt) ? fmt : "mm-dd-yyyy";
    char out_fmt[32];
    size_t i = 0;
    while (f[i] && i < sizeof(out_fmt) - 1)
    {
        char c = f[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out_fmt[i++] = c;
    }
    out_fmt[i] = '\0';

    int n;
    if (strcmp(out_fmt, "dd/mm/yyyy") == 0) n = snprintf(out, out_cap, "%02d/%02d/%04d", d, m, y);
    else if (strcmp(out_fmt, "dd/mm/yy") == 0) n = snprintf(out, out_cap, "%02d/%02d/%02d", d, m, yy);
    else if (strcmp(out_fmt, "mm/dd/yyyy") == 0) n = snprintf(out, out_cap, "%02d/%02d/%04d", m, d, y);
    else if (strcmp(out_fmt, "mm/dd/yy") == 0) n = snprintf(out, out_cap, "%02d/%02d/%02d", m, d, yy);
    else if (strcmp(out_fmt, "yyyy-mm-dd") == 0) n = snprintf(out, out_cap, "%04d-%02d-%02d", y, m, d);
    else if (strcmp(out_fmt, "yy-mm-dd") == 0) n = snprintf(out, out_cap, "%02d-%02d-%02d", yy, m, d);
    else if (strcmp(out_fmt, "dd-mm-yyyy") == 0) n = snprintf(out, out_cap, "%02d-%02d-%04d", d, m, y);
    else if (strcmp(out_fmt, "dd-mm-yy") == 0) n = snprintf(out, out_cap, "%02d-%02d-%02d", d, m, yy);
    else if (strcmp(out_fmt, "mm-dd-yyyy") == 0) n = snprintf(out, out_cap, "%02d-%02d-%04d", m, d, y);
    else if (strcmp(out_fmt, "mm-dd-yy") == 0) n = snprintf(out, out_cap, "%02d-%02d-%02d", m, d, yy);
    else n = snprintf(out, out_cap, "%02d/%02d/%04d", d, m, y);
    return n < 0 ? 0 : n;
}

/* ============================================================================
 * ClipperStrings (specchio ClipperStrings.cs).
 * ============================================================================ */
int gmb_str_at(const char* search, const char* target)
{
    if (!search || !*search || !target) return 0;
    const char* p = strstr(target, search);
    return p ? (int)(p - target) + 1 : 0;
}

int gmb_str_rat(const char* search, const char* target)
{
    if (!search || !*search || !target) return 0;
    const char* last = NULL;
    const char* p = target;
    size_t sl = strlen(search);
    while ((p = strstr(p, search)) != NULL)
    {
        last = p;
        p += sl;
    }
    return last ? (int)(last - target) + 1 : 0;
}

/* Substring per CARATTERI (code point), la stessa unità di LEN/LEFT$/MID$/RIGHT$/INSTR.
 * Prima indicizzava i BYTE (s[i]): una sottostringa che cade su un carattere
 * multibyte (una "à", una emoji) ne prendeva solo una parte, producendo UTF-8 non
 * valido. Oggi nessun chiamante usa questa funzione: la correzione è di coerenza,
 * così chi la collegherà in futuro non eredita il difetto. start è 1-based, come
 * in BASIC. */
int gmb_str_substr(const char* s, int start, int count, char* out, int out_cap)
{
    /* Unità "carattere" del runtime (gmb_strings.c); dichiarate qui perché
     * gmb_runtime.h non è includibile in questo file (gmb_str_space ha
     * dichiarazioni static/non-static in conflitto). */
    extern int64_t gmb_chars_len(const char* s);
    extern size_t  gmb_chars_to_byte(const char* s, int64_t chars);
    if (!out || out_cap <= 0) return 0;
    out[0] = '\0';
    if (!s || count <= 0) return 0;
    int chars = (int)gmb_chars_len(s);
    if (start > chars) return 0;
    if (start < 1)
    {
        count += start - 1;
        start = 1;
        if (count <= 0) return 0;
    }
    if (start + count - 1 > chars) count = chars - start + 1;
    size_t from = gmb_chars_to_byte(s, start - 1);
    size_t to   = gmb_chars_to_byte(s, start - 1 + count);
    size_t n = 0;
    for (size_t i = from; i < to && n + 1 < (size_t)out_cap; i++) out[n++] = s[i];
    out[n] = '\0';
    return (int)n;
}

int gmb_str_stuff(const char* s, int start, int delete, const char* insert, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!insert) insert = "";
    if (!s || !*s) { int n = (int)strlen(insert); if (n >= out_cap) n = out_cap - 1; memcpy(out, insert, (size_t)n); out[n] = '\0'; return n; }
    if (start < 1) start = 1;
    if (delete < 0) delete = 0;
    int len = (int)strlen(s);
    if (start > len + 1) { snprintf(out, out_cap, "%s%s", s, insert); return (int)strlen(out); }
    int idx = start - 1;
    int tail = delete < (len - idx) ? delete : (len - idx);
    char prefix[4096], suffix[4096];
    int pn = 0; for (int i = 0; i < idx && i < len && pn < (int)sizeof(prefix) - 1; i++) prefix[pn++] = s[i];
    prefix[pn] = '\0';
    int sn = 0; for (int i = idx + tail; i < len && sn < (int)sizeof(suffix) - 1; i++) suffix[sn++] = s[i];
    suffix[sn] = '\0';
    snprintf(out, out_cap, "%s%s%s", prefix, insert, suffix);
    return (int)strlen(out);
}

int gmb_str_strtran(const char* s, const char* search, const char* replace, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!replace) replace = "";
    if (!s || !search || !*search) { snprintf(out, out_cap, "%s", s ? s : ""); return (int)strlen(out); }
    char* tmp = (char*)malloc((size_t)out_cap);
    if (!tmp) { out[0] = '\0'; return 0; }
    const char* p = s;
    size_t sl = strlen(search);
    int n = 0;
    while (*p && n < out_cap - 1)
    {
        const char* found = strstr(p, search);
        if (found == p)
        {
            const char* r = replace;
            while (*r && n < out_cap - 1) tmp[n++] = *r++;
            p += sl;
        }
        else
        {
            tmp[n++] = *p++;
        }
    }
    tmp[n] = '\0';
    memcpy(out, tmp, (size_t)n + 1);
    free(tmp);
    return n;
}

int gmb_str_padl(const char* s, int length, char fill, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (len >= length) { snprintf(out, out_cap, "%s", s); return len; }
    int pad = length - len;
    int n = 0;
    for (int i = 0; i < pad && n < out_cap - 1; i++) out[n++] = fill;
    for (int i = 0; i < len && n < out_cap - 1; i++) out[n++] = s[i];
    out[n] = '\0';
    return n;
}

int gmb_str_padc(const char* s, int length, char fill, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (len >= length) { snprintf(out, out_cap, "%s", s); return len; }
    int total = length - len;
    int l = total / 2;
    int n = 0;
    for (int i = 0; i < l && n < out_cap - 1; i++) out[n++] = fill;
    for (int i = 0; i < len && n < out_cap - 1; i++) out[n++] = s[i];
    for (int i = 0; i < total - l && n < out_cap - 1; i++) out[n++] = fill;
    out[n] = '\0';
    return n;
}

int gmb_str_padr(const char* s, int length, char fill, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!s) s = "";
    int len = (int)strlen(s);
    if (len >= length) { snprintf(out, out_cap, "%s", s); return len; }
    int n = 0;
    for (int i = 0; i < len && n < out_cap - 1; i++) out[n++] = s[i];
    for (int i = 0; i < length - len && n < out_cap - 1; i++) out[n++] = fill;
    out[n] = '\0';
    return n;
}

int gmb_str_replicate_str(const char* s, int n, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (n <= 0 || !s || !*s) { out[0] = '\0'; return 0; }
    int len = (int)strlen(s);
    int wn = 0;
    for (int k = 0; k < n; k++)
        for (int i = 0; i < len && wn < out_cap - 1; i++) out[wn++] = s[i];
    out[wn] = '\0';
    return wn;
}

int gmb_str_replicate_char(char c, int n, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (n <= 0) { out[0] = '\0'; return 0; }
    int wn = 0;
    for (int i = 0; i < n && wn < out_cap - 1; i++) out[wn++] = c;
    out[wn] = '\0';
    return wn;
}

static int gmb_str_space(int n, char* out, int out_cap)
{
    return gmb_str_replicate_char(' ', n, out, out_cap);
}

static int str_is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }





double gmb_str_val(const char* s)
{
    if (!s || !*s) return 0.0;
    const char* p = s;
    while (*p && str_is_space(*p)) p++;
    char num[128];
    int n = 0;
    if (*p == '+' || *p == '-') num[n++] = *p++;
    bool digits = false, dot = false;
    while (*p && n < (int)sizeof(num) - 1 && ((*p >= '0' && *p <= '9') || *p == '.'))
    {
        if (*p == '.')
        {
            if (dot) break;
            dot = true;
            num[n++] = *p;
        }
        else { num[n++] = *p; digits = true; }
        p++;
    }
    num[n] = '\0';
    if (!digits) return 0.0;
    return atof(num);
}

int gmb_str_str(double value, int length, int decimals, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (length < 1) { out[0] = '\0'; return 0; }
    if (decimals < 0) decimals = 0;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%.*f", decimals, value);
    int tl = (int)strlen(tmp);
    if (tl > length) { for (int i = 0; i < length; i++) out[i] = '*'; out[length] = '\0'; return length; }
    int pad = length - tl;
    int n = 0;
    for (int i = 0; i < pad && n < out_cap - 1; i++) out[n++] = ' ';
    for (int i = 0; i < tl && n < out_cap - 1; i++) out[n++] = tmp[i];
    out[n] = '\0';
    return n;
}

int gmb_str_strzero(double value, int length, int decimals, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (length < 1) { out[0] = '\0'; return 0; }
    if (decimals < 0) decimals = 0;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%.*f", decimals, value);
    bool neg = tmp[0] == '-';
    const char* body = neg ? tmp + 1 : tmp;
    int bl = (int)strlen(body);
    if (bl > length) { for (int i = 0; i < length; i++) out[i] = '*'; out[length] = '\0'; return length; }
    int n = 0;
    if (neg && n < out_cap - 1) out[n++] = '-';
    int pad = length - bl;
    for (int i = 0; i < pad && n < out_cap - 1; i++) out[n++] = '0';
    for (int i = 0; i < bl && n < out_cap - 1; i++) out[n++] = body[i];
    out[n] = '\0';
    return n;
}

int gmb_str_asc(const char* s)
{
    if (!s || !*s) return 0;
    const unsigned char* u = (const unsigned char*)s;
    return u[0];
}

int gmb_str_chr(int n, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    out[0] = (char)(n & 0xFF);
    out[1] = '\0';
    return 1;
}

/* ============================================================================
 * StripAnsi (specchio StripAnsi.cs).
 * ============================================================================ */
int gmb_strip_ansi(const char* input, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    if (!input) { out[0] = '\0'; return 0; }
    int n = 0;
    const char* p = input;
    while (*p && n < out_cap - 1)
    {
        if (*p == 0x1B && p[1] == '[')
        {
            p += 2;
            while (*p && n < out_cap - 1)
            {
                if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
                {
                    p++;
                    break;
                }
                p++;
            }
        }
        else
        {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return n;
}

/* ============================================================================
 * ClipperNumbers (specchio ClipperNumbers.cs).
 * ============================================================================ */
double gmb_num_abs(double n) { return n < 0 ? -n : n; }

int gmb_num_int(double n) { return n >= 0 ? (int)n : (int)ceil(n); }

double gmb_num_round(double n, int decimals)
{
    if (decimals < 0) decimals = 0;
    if (decimals > 15) decimals = 15;
    double factor = pow(10.0, decimals);
    double v = n * factor;
    double r = (v >= 0) ? floor(v + 0.5) : ceil(v - 0.5);  /* AwayFromZero */
    return r / factor;
}

double gmb_num_sqrt(double n) { return sqrt(n); }
double gmb_num_exp(double n) { return exp(n); }
double gmb_num_log(double n) { return log(n); }
double gmb_num_log10(double n) { return log10(n); }
double gmb_num_max(double a, double b) { return a > b ? a : b; }
int gmb_num_max_i(int a, int b) { return a > b ? a : b; }
double gmb_num_min(double a, double b) { return a < b ? a : b; }
int gmb_num_min_i(int a, int b) { return a < b ? a : b; }

double gmb_num_mod(double dividend, double divisor)
{
    if (divisor == 0) return 0;
    double r = fmod(dividend, divisor);
    if (r != 0 && ((r < 0) != (divisor < 0))) r += divisor;
    return r;
}

int gmb_num_mod_i(int dividend, int divisor)
{
    if (divisor == 0) return 0;
    int r = dividend % divisor;
    if (r != 0 && ((r < 0) != (divisor < 0))) r += divisor;
    return r;
}

int gmb_num_sign(double n)
{
    if (n > 0) return 1;
    if (n < 0) return -1;
    return 0;
}

/* ============================================================================
 * Quantize — mapping colore ANSI (specchio Quantize.cs).
 * ============================================================================ */
const gmb_cell_color_t gmb_ansi16[16] = {
    {  0,   0,   0, 0}, /* 0 black */
    {205,   0,   0, 0}, /* 1 red */
    {  0, 205,   0, 0}, /* 2 green */
    {205, 205,   0, 0}, /* 3 yellow */
    {  0,   0, 205, 0}, /* 4 blue */
    {205,   0, 205, 0}, /* 5 magenta */
    {  0, 205, 205, 0}, /* 6 cyan */
    {205, 205, 205, 0}, /* 7 white */
    {127, 127, 127, 0}, /* 8 bright black */
    {255,   0,   0, 0}, /* 9 */
    {  0, 255,   0, 0}, /* 10 */
    {255, 255,   0, 0}, /* 11 */
    {  0,   0, 255, 0}, /* 12 */
    {255,   0, 255, 0}, /* 13 */
    {  0, 255, 255, 0}, /* 14 */
    {255, 255, 255, 0}, /* 15 */
};

const gmb_cell_color_t gmb_ansi256[256] = {
#include "gmb_tui_ansi256.inc"
};

int gmb_quantize_palette(gmb_cell_color_t c)
{
    int r = c.r * 5 / 255;
    int g = c.g * 5 / 255;
    int b = c.b * 5 / 255;
    return 16 + 36 * r + 6 * g + b;
}

int gmb_quantize_greyscale(gmb_cell_color_t c)
{
    int offset = (c.r + c.g + c.b) / (23 * 3);
    return 232 + offset;
}

int gmb_quantize_to16(gmb_cell_color_t c)
{
    const int third = 255 / 3;
    int cr = c.r / third;
    int cg = c.g / third;
    int cb = c.b / third;

    if (cr == 0 && cg == 0 && cb == 0) return 0;
    if (cr == 1 && cg == 0 && cb == 0) return 1;
    if (cr == 0 && cg == 1 && cb == 0) return 2;
    if (cr == 0 && cg == 0 && cb == 1) return 4;
    if (cr == 1 && cg == 1 && cb == 0) return 3;
    if (cr == 0 && cg == 1 && cb == 1) return 6;
    if (cr == 1 && cg == 0 && cb == 1) return 5;
    if (cr == 1 && cg == 1 && cb == 1) return 8;

#define Q_HI(v) ((v) == 2 || (v) == 3)
#define Q_LO(v) ((v) == 0 || (v) == 1)
    if (Q_HI(cr) && Q_HI(cg) && Q_HI(cb))
        return (cr == 2 && cg == 2 && cb == 2) ? 7 : 15;
    if (Q_HI(cr) && Q_LO(cg) && Q_LO(cb)) return 9;
    if (Q_LO(cr) && Q_HI(cg) && Q_LO(cb)) return 10;
    if (Q_LO(cr) && Q_LO(cg) && Q_HI(cb)) return 12;
    if (Q_HI(cr) && Q_HI(cg) && Q_LO(cb)) return 11;
    if (Q_LO(cr) && Q_HI(cg) && Q_HI(cb)) return 14;
    if (Q_HI(cr) && Q_LO(cg) && Q_HI(cb)) return 13;
    return 0;
}

static uint32_t q_sqdist(gmb_cell_color_t a, gmb_cell_color_t b)
{
    int r = a.r - b.r;
    int g = a.g - b.g;
    int bl = a.b - b.b;
    return (uint32_t)(r * r + g * g + bl * bl);
}

int gmb_quantize_to256(gmb_cell_color_t c)
{
    int p = gmb_quantize_palette(c);
    int g = gmb_quantize_greyscale(c);
    return q_sqdist(c, gmb_ansi256[p]) <= q_sqdist(c, gmb_ansi256[g]) ? p : g;
}

/* ============================================================================
 * FormatPicture — PICTURE Clipper (specchio FormatPicture.cs).
 * ============================================================================ */
static bool pic_is_func(char c)
{
    return strchr("ABCDEFIJKLNRSTXZ()$!", c) != NULL;
}

gmb_picture_spec_t gmb_picture_parse(const char* picture)
{
    gmb_picture_spec_t p;
    memset(&p, 0, sizeof(p));
    p.fill_char = ' ';
    p.thousands_sep = ',';
    p.decimal_sep = '.';

    if (!picture || !*picture) return p;

    const char* s = picture;
    int idx = 0;
    if (s[0] == '@')
    {
        idx = 1;
        while (s[idx])
        {
            char c = s[idx];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            if (c == '@') { idx++; continue; }
            if (c == ' ')
            {
                int j = idx;
                while (s[j] == ' ') j++;
                if (s[j] == '@') { idx = j; continue; }
                break;
            }
            if (c == 'S' && s[idx + 1] && s[idx + 1] >= '0' && s[idx + 1] <= '9')
            {
                p.scroll = true;
                int j = idx + 1;
                while (s[j] >= '0' && s[j] <= '9') j++;
                p.scroll_width = atoi(s + idx + 1);
                idx = j;
                continue;
            }
            if (c == 'L' && s[idx + 1]) { p.fill_char = s[idx + 1]; idx += 2; continue; }
            if (pic_is_func(c))
            {
                switch (c)
                {
                    case 'A': p.alpha = true; break;
                    case 'B': p.left_justify_numbers = true; break;
                    case 'C': p.cr_after_positive = true; break;
                    case 'D': p.date_set_format = true; break;
                    case 'E': p.date_british = true; break;
                    case 'F': p.european = true; break;
                    case 'I': p.insert_mode = true; break;
                    case 'J': case 'N': p.justify_right = true; break;
                    case 'K': p.smart = true; break;
                    case 'R': p.literal_insert = true; break;
                    case 'T': p.trim = true; break;
                    case 'X': p.db_after_negatives = true; break;
                    case 'Z': p.zero_blank = true; break;
                    case '(': p.parens_with_leading = true; break;
                    case ')': p.parens_no_leading = true; break;
                    case '$': p.currency = true; break;
                    case '!': p.upper = true; break;
                }
                idx++;
                continue;
            }
            break;
        }
        if (s[idx] == ' ') idx++;
    }

    p.template = strdup(s + idx);
    const char* t = p.template;

    /* IsNumeric: solo "9#$*., 0()" e almeno un '9' o '#'. */
    bool num_chars = true, has_num = false;
    for (const char* q = t; *q; q++)
    {
        if (!strchr("9#$*., 0()", *q)) { num_chars = false; break; }
        if (*q == '9' || *q == '#') has_num = true;
    }
    p.is_numeric = t[0] && num_chars && has_num;

    /* IsDate: ≥6 char, tutti digit o separatori "/-.: " (specchio C#) */
    if (p.date_set_format || p.date_british) p.is_date = true;
    else if (strlen(t) >= 6)
    {
        bool all_dt = true, any_sep = false;
        for (const char* q = t; *q; q++)
        {
            bool is_digit = (*q >= '0' && *q <= '9');
            if (!(is_digit || strchr("/-.: ", *q))) { all_dt = false; break; }
            if (strchr("/-.:", *q)) any_sep = true;
        }
        if (all_dt && any_sep) p.is_date = true;
    }

    if (p.is_numeric)
    {
        char dec_sep = p.european ? ',' : '.';
        char th_sep = p.european ? '.' : ',';
        p.decimal_sep = dec_sep;
        p.thousands_sep = th_sep;

        bool has_dec = strchr(t, dec_sep) != NULL;
        bool has_th = strchr(t, th_sep) != NULL;
        int dec_idx = -1;
        if (has_dec && has_th)
        {
            const char* ld = strrchr(t, dec_sep);
            const char* lt = strrchr(t, th_sep);
            dec_idx = ld > lt ? (int)(ld - t) : (int)(lt - t);
        }
        else if (has_dec)
        {
            const char* ld = strrchr(t, dec_sep);
            const char* after = ld + 1;
            bool has_digit_after = false;
            while (*after) { if (*after == '9' || *after == '#' || *after == '0') { has_digit_after = true; break; } after++; }
            dec_idx = has_digit_after ? (int)(ld - t) : -1;
        }

        if (dec_idx >= 0)
        {
            p.decimals = 0;
            for (const char* q = t + dec_idx + 1; *q; q++)
                if (*q == '9' || *q == '#' || *q == '0') p.decimals++;
            p.has_thousands = memchr(t, th_sep, (size_t)dec_idx) != NULL;
        }
        else
        {
            p.decimals = 0;
            p.has_thousands = has_th;
        }

        if (p.currency) p.floating_fill = '$';
        else if (t[0] == '*') p.floating_fill = '*';
    }

    return p;
}

void gmb_picture_free(gmb_picture_spec_t* p)
{
    if (!p) return;
    free(p->template);
    p->template = NULL;
}

static int pic_find_first_digit(const char* s)
{
    for (int i = 0; s[i]; i++)
        if (s[i] >= '0' && s[i] <= '9') return i;
    return (int)strlen(s);
}

static int pic_add_thousands(const char* int_part, char sep, char* out, int out_cap)
{
    int len = (int)strlen(int_part);
    bool neg = len > 0 && int_part[0] == '-';
    const char* digits = neg ? int_part + 1 : int_part;
    int dl = len - (neg ? 1 : 0);
    int n = 0;
    if (neg && n < out_cap - 1) out[n++] = '-';
    int count = 0;
    for (int i = dl - 1; i >= 0; i--)
    {
        if (count > 0 && count % 3 == 0 && n < out_cap - 1) out[n++] = sep;
        if (n < out_cap - 1) out[n++] = digits[i];
        count++;
    }
    out[n] = '\0';
    /* inverti (abbiamo costruito da destra) */
    char* start = out + (neg ? 1 : 0);
    int sn = n - (neg ? 1 : 0);
    for (int i = 0; i < sn / 2; i++)
    {
        char tmp = start[i];
        start[i] = start[sn - 1 - i];
        start[sn - 1 - i] = tmp;
    }
    return n;
}

static int pic_insert_minus(const char* s, char fill, char* out, int out_cap)
{
    (void)fill;
    int first = pic_find_first_digit(s);
    int len = (int)strlen(s);
    if (first == 0) { snprintf(out, out_cap, "-%s", s); return len + 1; }
    if (first < len && (s[first - 1] == ' ' || s[first - 1] == '$' || s[first - 1] == '*'))
    {
        memcpy(out, s, (size_t)len + 1);
        out[first - 1] = '-';
        return len;
    }
    snprintf(out, out_cap, "%.*s-%s", first, s, s + first);
    return len + 1;
}

static bool pic_is_placeholder(char t)
{
    return strchr("ANX9#LY!0", t) != NULL;
}

static bool pic_match_template(char c, char t)
{
    switch (t)
    {
        case 'A': return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        case 'N': return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        case 'X': return true;
        case '9': return (c >= '0' && c <= '9') || c == '-';
        case '#': return (c >= '0' && c <= '9') || c == '-' || c == ' ';
        case 'L': return c == 'T' || c == 'F';
        case 'Y': return c == 'Y' || c == 'N';
        case '!': return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        case '0': return c >= '0' && c <= '9';
        default: return false;
    }
}

static int pic_format_number(double value, const gmb_picture_spec_t* p, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    int width = (int)strlen(p->template); if (width < 1) width = 1;
    if (p->zero_blank && value == 0)
    {
        for (int i = 0; i < width && i < out_cap - 1; i++) out[i] = ' ';
        out[width < out_cap ? width : out_cap - 1] = '\0';
        return width < out_cap ? width : out_cap - 1;
    }

    int dec = p->decimals; if (dec < 0) dec = 0; if (dec > 15) dec = 15;
    char s[128];
    snprintf(s, sizeof(s), "%.*f", dec, value < 0 ? -value : value);
    char* dot = strchr(s, '.');
    char int_part[64], frac[64] = "";
    if (dot)
    {
        size_t il = (size_t)(dot - s);
        memcpy(int_part, s, il); int_part[il] = '\0';
        snprintf(frac, sizeof(frac), "%s", dot + 1);
    }
    else snprintf(int_part, sizeof(int_part), "%s", s);

    char int_p[128] = "";
    if (p->has_thousands && (int)strlen(int_part) > 3)
        pic_add_thousands(int_part, p->thousands_sep, int_p, (int)sizeof(int_p));
    else snprintf(int_p, sizeof(int_p), "%s", int_part);

    char body[256];
    if (dec > 0) snprintf(body, sizeof(body), "%s%c%s", int_p, p->decimal_sep, frac);
    else snprintf(body, sizeof(body), "%s", int_p);

    bool neg = value < 0;
    char fill = p->fill_char == ' ' ? ' ' : p->fill_char;
    char base_padded[256];
    int bl = (int)strlen(body);
    if (p->left_justify_numbers)
    {
        int n = 0;
        for (int i = 0; i < bl && n < (int)sizeof(base_padded) - 1; i++) base_padded[n++] = body[i];
        for (int i = 0; i < width - bl && n < (int)sizeof(base_padded) - 1; i++) base_padded[n++] = fill;
        base_padded[n] = '\0';
    }
    else
    {
        int n = 0;
        for (int i = 0; i < width - bl && n < (int)sizeof(base_padded) - 1; i++) base_padded[n++] = fill;
        for (int i = 0; i < bl && n < (int)sizeof(base_padded) - 1; i++) base_padded[n++] = body[i];
        base_padded[n] = '\0';
    }

    /* floating fill ($ o *) prima della prima cifra */
    if (p->floating_fill != 0)
    {
        int fd = pic_find_first_digit(base_padded);
        for (int i = 0; i < fd && (base_padded[i] == ' ' || base_padded[i] == fill); i++)
            base_padded[i] = p->floating_fill;
    }

    if (neg)
    {
        if (p->parens_with_leading) snprintf(out, out_cap, "(%s )", base_padded);
        else if (p->parens_no_leading) snprintf(out, out_cap, "(%s)", base_padded);
        else if (p->db_after_negatives) snprintf(out, out_cap, "%s DB", base_padded);
        else pic_insert_minus(base_padded, fill, out, out_cap);
    }
    else if (p->cr_after_positive)
    {
        snprintf(out, out_cap, "%s CR", base_padded);
    }
    else
    {
        snprintf(out, out_cap, "%s", base_padded);
    }
    return (int)strlen(out);
}

static int pic_format_text(const char* value, const gmb_picture_spec_t* p, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    char v[512];
    snprintf(v, sizeof(v), "%s", value ? value : "");
    if (p->upper)
        for (int i = 0; v[i]; i++) if (v[i] >= 'a' && v[i] <= 'z') v[i] = (char)(v[i] - 32);

    const char* t = p->template;
    if (!*t)
    {
        if (p->justify_right)
        {
            const char* start = v;
            while (*start == ' ') start++;
            int len = (int)strlen(start); if (len < 1) len = 1;
            int pad = len - (int)strlen(start);
            int n = 0;
            for (int i = 0; i < pad && n < out_cap - 1; i++) out[n++] = ' ';
            for (int i = 0; start[i] && n < out_cap - 1; i++) out[n++] = start[i];
            out[n] = '\0';
            return n;
        }
        snprintf(out, out_cap, "%s", v);
        return (int)strlen(out);
    }

    /* JustifyRight: riempi da destra */
    if (p->justify_right)
    {
        char buf[512];
        int bn = 0;
        int vidx = (int)strlen(v) - 1;
        char stack[512];
        int sn2 = 0;
        int tl = (int)strlen(t);
        for (int i = tl - 1; i >= 0; i--)
        {
            char tt = t[i];
            if (!pic_is_placeholder(tt)) { stack[sn2++] = tt; continue; }
            char c = vidx >= 0 ? v[vidx--] : ' ';
            if (p->upper && c >= 'a' && c <= 'z') c = (char)(c - 32);
            stack[sn2++] = pic_match_template(c, tt) ? c : ' ';
        }
        bn = 0;
        for (int i = sn2 - 1; i >= 0; i--) buf[bn++] = stack[i];
        buf[bn] = '\0';
        int reslen = bn;
        if (p->trim)
        {
            char* st = buf;
            int e = bn;
            while (*st == ' ') st++;
            while (e > 0 && buf[e - 1] == ' ') e--;
            reslen = e - (int)(st - buf);
            memmove(buf, st, (size_t)reslen);
            buf[reslen] = '\0';
        }
        if (reslen >= out_cap) reslen = out_cap - 1;
        memcpy(out, buf, (size_t)reslen);
        out[reslen] = '\0';
        return reslen;
    }

    /* normale: sinistra → destra */
    int n = 0;
    int vi2 = 0;
    int vlen = (int)strlen(v);
    for (const char* q = t; *q && n < out_cap - 1; q++)
    {
        char tt = *q;
        if (pic_is_placeholder(tt))
        {
            char c = vi2 < vlen ? v[vi2++] : ' ';
            bool ok = pic_match_template(c, tt);
            if (p->upper && c >= 'a' && c <= 'z') c = (char)(c - 32);
            out[n++] = ok ? c : ' ';
        }
        else
        {
            out[n++] = tt;
        }
    }
    out[n] = '\0';
    if (p->trim)
    {
        /* trim del risultato (spazi ai lati) */
        int e = n;
        while (e > 0 && out[e - 1] == ' ') e--;
        const char* st = out;
        while (*st == ' ') st++;
        int rl = e - (int)(st - out);
        memmove(out, st, (size_t)rl);
        out[rl] = '\0';
        return rl;
    }
    return n;
}

static int pic_format_date(int d, int m, int y, const gmb_picture_spec_t* p, char* out, int out_cap)
{
    if (!out || out_cap <= 0) return 0;
    const char* t = p->template;
    int tl = (int)strlen(t);
    if (tl < 6) { snprintf(out, out_cap, "%02d/%02d/%04d", d, m, y); return (int)strlen(out); }

    /* gruppi di cifre nel template */
    int groups[4]; int gc = 0; int cur = 0;
    char seps[8]; int sc = 0;
    for (int i = 0; i < tl; i++)
    {
        if (t[i] >= '0' && t[i] <= '9') cur++;
        else
        {
            if (cur > 0 && gc < 4) { groups[gc++] = cur; cur = 0; }
            if (sc < (int)sizeof(seps) - 1) seps[sc++] = t[i];
        }
    }
    if (cur > 0 && gc < 4) groups[gc++] = cur;
    if (gc < 3) { snprintf(out, out_cap, "%02d/%02d/%04d", d, m, y); return (int)strlen(out); }

    char parts[3][8];
    snprintf(parts[0], 8, "%02d", d);
    snprintf(parts[1], 8, "%02d", m);
    snprintf(parts[2], 8, groups[2] >= 4 ? "%04d" : "%02d", groups[2] >= 4 ? y : y % 100);

    int n = 0, out_idx = 0, g_idx = 0;
    for (int i = 0; i < tl && n < out_cap - 1; i++)
    {
        if (t[i] >= '0' && t[i] <= '9')
        {
            if (g_idx < 3 && out_idx < (int)strlen(parts[g_idx]))
                out[n++] = parts[g_idx][out_idx];
            else
                out[n++] = ' ';
            out_idx++;
            if (out_idx >= groups[g_idx])
            {
                out_idx = 0;
                g_idx++;
            }
        }
        else
        {
            out[n++] = t[i];
        }
    }
    out[n] = '\0';
    return n;
}

int gmb_picture_apply_number(double value, const char* picture, char* out, int out_cap)
{
    gmb_picture_spec_t p = gmb_picture_parse(picture);
    int r = pic_format_number(value, &p, out, out_cap);
    gmb_picture_free(&p);
    return r;
}

int gmb_picture_apply_text(const char* value, const char* picture, char* out, int out_cap)
{
    gmb_picture_spec_t p = gmb_picture_parse(picture);
    int r = pic_format_text(value, &p, out, out_cap);
    gmb_picture_free(&p);
    return r;
}

int gmb_picture_apply_date(int d, int m, int y, const char* picture, char* out, int out_cap)
{
    gmb_picture_spec_t p = gmb_picture_parse(picture);
    int r = pic_format_date(d, m, y, &p, out, out_cap);
    gmb_picture_free(&p);
    return r;
}

/* ============================================================================
 * KeyConstants (specchio KeyConstants.cs).
 * ============================================================================ */
bool gmb_key_decode_mouse(int key, int* x, int* y, int* button, bool* pressed)
{
    if (key >= -1000)
    {
        if (x) *x = 0; if (y) *y = 0; if (button) *button = 0; if (pressed) *pressed = false;
        return false;
    }
    int raw = -1000 - key;
    int p = raw % 10;
    int btn = (raw / 10) % 100;
    int yy = (raw / 1000) % 1000;
    int xx = raw / 1000000;
    if (x) *x = xx;
    if (y) *y = yy;
    if (button) *button = btn;
    if (pressed) *pressed = p != 0;
    return true;
}

/* ============================================================================
 * Codepage437 (specchio Codepage437.cs).
 * ============================================================================ */
const uint32_t gmb_cp437[256] = {
#include "gmb_tui_cp437.inc"
};

uint32_t gmb_cp437_tounicode(uint8_t b)
{
    return gmb_cp437[b];
}

int gmb_cp437_fromunicode(uint32_t cp)
{
    for (int i = 0; i < 256; i++)
        if (gmb_cp437[i] == cp) return i;
    return -1;
}

int gmb_cp437_encode(const char* utf8, uint8_t* out, int out_cap)
{
    if (!utf8 || !out || out_cap <= 0) return 0;
    int n = 0;
    const unsigned char* u = (const unsigned char*)utf8;
    while (*u && n < out_cap)
    {
        uint32_t cp;
        int len;
        if (u[0] < 0x80) { cp = u[0]; len = 1; }
        else if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) { cp = ((u[0] & 0x1F) << 6) | (u[1] & 0x3F); len = 2; }
        else if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) { cp = ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F); len = 3; }
        else if ((u[0] & 0xF8) == 0xF0) { cp = 0xFFFD; len = 4; }
        else { cp = 0xFFFD; len = 1; }
        int b = gmb_cp437_fromunicode(cp);
        out[n++] = b >= 0 ? (uint8_t)b : 0x3F;   /* unrepresentable → '?' */
        u += len;
    }
    return n;
}

/* ============================================================================
 * ConsoleRenderer — ScreenBuffer → stringa ANSI (headless, deterministico).
 * Per ogni riga: "\x1b[{y+1};1H", poi SGR differenziale (fg/bg), poi i char.
 * Gli attributi (bold/...) non sono ancora emessi (Fase 2 base).
 * ============================================================================ */
size_t gmb_render_screen(const gmb_screen_buffer_t* b, char* out, size_t out_cap)
{
    if (!b || !out || out_cap == 0) return 0;
    size_t used = 0;
    gmb_cell_color_t prev_fg = GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t prev_bg = GMB_CELLCOLOR_DEFAULT;
    char tmp[64];

    for (int y = 0; y < b->height; y++)
    {
        /* posizionamento riga (1-based) */
        int n = snprintf(tmp, sizeof(tmp), "\x1b[%d;1H", y + 1);
        if (used + (size_t)n >= out_cap) return 0;
        memcpy(out + used, tmp, (size_t)n); used += (size_t)n;

        for (int x = 0; x < b->width; x++)
        {
            const gmb_screen_cell_t* c = &b->cells[(size_t)y * b->width + x];
            gmb_cell_color_t fg = c->fg_rgb;
            gmb_cell_color_t bg = c->bg_rgb;

            if (!gmb_cellcolor_eq(fg, prev_fg))
            {
                if (fg.is_default) n = snprintf(tmp, sizeof(tmp), "\x1b[39m");
                else n = snprintf(tmp, sizeof(tmp), "\x1b[38;2;%u;%u;%um", fg.r, fg.g, fg.b);
                if (used + (size_t)n >= out_cap) return 0;
                memcpy(out + used, tmp, (size_t)n); used += (size_t)n;
                prev_fg = fg;
            }
            if (!gmb_cellcolor_eq(bg, prev_bg))
            {
                if (bg.is_default) n = snprintf(tmp, sizeof(tmp), "\x1b[49m");
                else n = snprintf(tmp, sizeof(tmp), "\x1b[48;2;%u;%u;%um", bg.r, bg.g, bg.b);
                if (used + (size_t)n >= out_cap) return 0;
                memcpy(out + used, tmp, (size_t)n); used += (size_t)n;
                prev_bg = bg;
            }
            /* char: cella vuota (\0) → spazio */
            uint32_t ch = c->character ? c->character : ' ';
            if (ch < 0x80)
            {
                if (used + 1 >= out_cap) return 0;
                out[used++] = (char)ch;
            }
            else
            {
                /* codepoint → UTF-8 (semplicistico ma sufficiente per codepage 437/test) */
                if (ch <= 0x7FF)
                {
                    if (used + 2 >= out_cap) return 0;
                    out[used++] = (char)(0xC0 | (ch >> 6));
                    out[used++] = (char)(0x80 | (ch & 0x3F));
                }
                else
                {
                    if (used + 3 >= out_cap) return 0;
                    out[used++] = (char)(0xE0 | (ch >> 12));
                    out[used++] = (char)(0x80 | ((ch >> 6) & 0x3F));
                    out[used++] = (char)(0x80 | (ch & 0x3F));
                }
            }
        }
    }
    if (used + 1 >= out_cap) return 0;
    out[used] = '\0';
    return used;
}

/* Confronto cella-per-cella di due buffer (dimensioni + carattere + fg/bg). */
bool gmb_screen_buffer_equal(const gmb_screen_buffer_t* a, const gmb_screen_buffer_t* b)
{
    if (!a || !b) return false;
    if (a->width != b->width || a->height != b->height) return false;
    size_t n = (size_t)a->width * (size_t)a->height;
    for (size_t i = 0; i < n; i++)
    {
        if (a->cells[i].character != b->cells[i].character) return false;
        if (!gmb_cellcolor_eq(a->cells[i].fg_rgb, b->cells[i].fg_rgb)) return false;
        if (!gmb_cellcolor_eq(a->cells[i].bg_rgb, b->cells[i].bg_rgb)) return false;
    }
    return true;
}

/* Copia src in dst (rialloca le celle se la dimensione cambia). */
void gmb_screen_buffer_copy_to(gmb_screen_buffer_t* dst, const gmb_screen_buffer_t* src)
{
    if (!dst || !src) return;
    size_t n = (size_t)src->width * (size_t)src->height;
    if (dst->width != src->width || dst->height != src->height || !dst->cells)
    {
        free(dst->cells);
        dst->cells = (gmb_screen_cell_t*)malloc(n * sizeof(gmb_screen_cell_t));
        if (!dst->cells) { dst->width = dst->height = 0; return; }
        dst->width = src->width;
        dst->height = src->height;
    }
    memcpy(dst->cells, src->cells, n * sizeof(gmb_screen_cell_t));
}

/* Render DIFFERENZIALE: emette solo le celle cambiate rispetto a prev (prev=NULL = tutto).
 * `*em_fg`/`*em_bg` = ultimo colore emesso (persistente tra le chiamate: il terminale li
 * conserva quando si sposta solo il cursore). */
size_t gmb_render_screen_diff(const gmb_screen_buffer_t* cur, const gmb_screen_buffer_t* prev,
                              gmb_cell_color_t* em_fg, gmb_cell_color_t* em_bg,
                              char* out, size_t out_cap)
{
    if (!cur || !out || out_cap == 0) return 0;
    size_t used = 0;
    gmb_cell_color_t prev_fg = em_fg ? *em_fg : GMB_CELLCOLOR_DEFAULT;
    gmb_cell_color_t prev_bg = em_bg ? *em_bg : GMB_CELLCOLOR_DEFAULT;
    int same_size = prev && prev->width == cur->width && prev->height == cur->height;
    char tmp[64];
    for (int y = 0; y < cur->height; y++)
    {
        for (int x = 0; x < cur->width; x++)
        {
            const gmb_screen_cell_t* c = &cur->cells[(size_t)y * cur->width + x];
            if (same_size)
            {
                const gmb_screen_cell_t* p = &prev->cells[(size_t)y * cur->width + x];
                if (c->character == p->character &&
                    gmb_cellcolor_eq(c->fg_rgb, p->fg_rgb) &&
                    gmb_cellcolor_eq(c->bg_rgb, p->bg_rgb)) continue;
            }
            int n = snprintf(tmp, sizeof(tmp), "\x1b[%d;%dH", y + 1, x + 1);
            if (used + (size_t)n >= out_cap) return used;
            memcpy(out + used, tmp, (size_t)n); used += (size_t)n;
            gmb_cell_color_t fg = c->fg_rgb;
            gmb_cell_color_t bg = c->bg_rgb;
            if (!gmb_cellcolor_eq(fg, prev_fg))
            {
                if (fg.is_default) n = snprintf(tmp, sizeof(tmp), "\x1b[39m");
                else n = snprintf(tmp, sizeof(tmp), "\x1b[38;2;%u;%u;%um", fg.r, fg.g, fg.b);
                if (used + (size_t)n >= out_cap) return used;
                memcpy(out + used, tmp, (size_t)n); used += (size_t)n;
                prev_fg = fg;
            }
            if (!gmb_cellcolor_eq(bg, prev_bg))
            {
                if (bg.is_default) n = snprintf(tmp, sizeof(tmp), "\x1b[49m");
                else n = snprintf(tmp, sizeof(tmp), "\x1b[48;2;%u;%u;%um", bg.r, bg.g, bg.b);
                if (used + (size_t)n >= out_cap) return used;
                memcpy(out + used, tmp, (size_t)n); used += (size_t)n;
                prev_bg = bg;
            }
            uint32_t ch = c->character ? c->character : ' ';
            if (ch < 0x80) { if (used + 1 >= out_cap) return used; out[used++] = (char)ch; }
            else if (ch <= 0x7FF) { if (used + 2 >= out_cap) return used; out[used++] = (char)(0xC0 | (ch >> 6)); out[used++] = (char)(0x80 | (ch & 0x3F)); }
            else { if (used + 3 >= out_cap) return used; out[used++] = (char)(0xE0 | (ch >> 12)); out[used++] = (char)(0x80 | ((ch >> 6) & 0x3F)); out[used++] = (char)(0x80 | (ch & 0x3F)); }
        }
    }
    if (em_fg) *em_fg = prev_fg;
    if (em_bg) *em_bg = prev_bg;
    if (used + 1 >= out_cap) return used;
    out[used] = '\0';
    return used;
}
