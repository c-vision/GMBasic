/* ============================================================================
 * gmb_tui_concodes.c — ConCodes (porting completo di ConCodes.cs).
 * Encoder ANSI + decoder DEC state-machine (anes-rs port). TTY-free.
 * ============================================================================ */
#include "gmb_tui_concodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== Encoder ===================== */

#define CSI "\x1b["

static int csi1(unsigned int code, char final, char* out, int cap)
{
    return snprintf(out, cap, CSI "%u%c", code, final);
}

static int csi2(unsigned int a, unsigned int b, char final, char* out, int cap)
{
    return snprintf(out, cap, CSI "%u;%u%c", a, b, final);
}

int gmb_concodes_cuu(unsigned short n, char* out, int cap) { return csi1(n, 'A', out, cap); }
int gmb_concodes_cud(unsigned short n, char* out, int cap) { return csi1(n, 'B', out, cap); }
int gmb_concodes_cuf(unsigned short n, char* out, int cap) { return csi1(n, 'C', out, cap); }
int gmb_concodes_cub(unsigned short n, char* out, int cap) { return csi1(n, 'D', out, cap); }
int gmb_concodes_cnl(unsigned short n, char* out, int cap) { return csi1(n, 'E', out, cap); }
int gmb_concodes_cpl(unsigned short n, char* out, int cap) { return csi1(n, 'F', out, cap); }
int gmb_concodes_cha(unsigned short n, char* out, int cap) { return csi1(n, 'G', out, cap); }
int gmb_concodes_cup(unsigned short row, unsigned short col, char* out, int cap) { return csi2(row, col, 'H', out, cap); }
int gmb_concodes_ed(unsigned short mode, char* out, int cap) { return csi1(mode, 'J', out, cap); }
int gmb_concodes_el(unsigned short mode, char* out, int cap) { return csi1(mode, 'K', out, cap); }
int gmb_concodes_sm(unsigned int mode, char* out, int cap) { return csi1(mode, 'h', out, cap); }
int gmb_concodes_rm(unsigned int mode, char* out, int cap) { return csi1(mode, 'l', out, cap); }
int gmb_concodes_psm(unsigned int mode, char* out, int cap) { return snprintf(out, cap, CSI "?%uh", mode); }
int gmb_concodes_prm(unsigned int mode, char* out, int cap) { return snprintf(out, cap, CSI "?%ul", mode); }

int gmb_concodes_sgr_raw(const unsigned int* codes, int n, char* out, int cap)
{
    if (n <= 0 || !codes) { snprintf(out, cap, CSI "m"); return (int)strlen(out); }
    int o = snprintf(out, cap, CSI);
    for (int i = 0; i < n; i++)
        o += snprintf(out + o, cap - o, "%s%u", i > 0 ? ";" : "", codes[i]);
    if (o < cap - 1) out[o++] = 'm';
    out[o] = '\0';
    return o;
}

static int standard_index(gmb_cell_color_t c)
{
    if (c.is_default) return -1;
    if (c.r == 0 && c.g == 0 && c.b == 0) return 0;
    if (c.r == 255 && c.g == 0 && c.b == 0) return 1;
    if (c.r == 0 && c.g == 255 && c.b == 0) return 2;
    if (c.r == 255 && c.g == 255 && c.b == 0) return 3;
    if (c.r == 0 && c.g == 0 && c.b == 255) return 4;
    if (c.r == 255 && c.g == 0 && c.b == 255) return 5;
    if (c.r == 0 && c.g == 255 && c.b == 255) return 6;
    if (c.r == 255 && c.g == 255 && c.b == 255) return 7;
    return -1;
}

static int fg_segment(gmb_cell_color_t c, bool bright, char* out, int cap)
{
    if (c.is_default) { snprintf(out, cap, "39"); return 2; }
    int idx = standard_index(c);
    if (idx >= 0) return snprintf(out, cap, "%d", (bright ? 90 : 30) + idx);
    return snprintf(out, cap, "38;2;%u;%u;%u", c.r, c.g, c.b);
}

static int bg_segment(gmb_cell_color_t c, bool bright, char* out, int cap)
{
    if (c.is_default) { snprintf(out, cap, "49"); return 2; }
    int idx = standard_index(c);
    if (idx >= 0) return snprintf(out, cap, "%d", (bright ? 100 : 40) + idx);
    return snprintf(out, cap, "48;2;%u;%u;%u", c.r, c.g, c.b);
}

int gmb_concodes_sgr_fg(gmb_cell_color_t c, char* out, int cap)
{
    char seg[32];
    fg_segment(c, false, seg, (int)sizeof(seg));
    return snprintf(out, cap, CSI "%sm", seg);
}
int gmb_concodes_sgr_bright_fg(gmb_cell_color_t c, char* out, int cap)
{
    char seg[32];
    fg_segment(c, true, seg, (int)sizeof(seg));
    return snprintf(out, cap, CSI "%sm", seg);
}
int gmb_concodes_sgr_bg(gmb_cell_color_t c, char* out, int cap)
{
    char seg[32];
    bg_segment(c, false, seg, (int)sizeof(seg));
    return snprintf(out, cap, CSI "%sm", seg);
}
int gmb_concodes_sgr_bright_bg(gmb_cell_color_t c, char* out, int cap)
{
    char seg[32];
    bg_segment(c, true, seg, (int)sizeof(seg));
    return snprintf(out, cap, CSI "%sm", seg);
}
int gmb_concodes_sgr_fg256(unsigned char idx, char* out, int cap) { return snprintf(out, cap, CSI "38;5;%um", idx); }
int gmb_concodes_sgr_bg256(unsigned char idx, char* out, int cap) { return snprintf(out, cap, CSI "48;5;%um", idx); }
int gmb_concodes_sgr_fg24(unsigned char r, unsigned char g, unsigned char b, char* out, int cap) { return snprintf(out, cap, CSI "38;2;%u;%u;%um", r, g, b); }
int gmb_concodes_sgr_bg24(unsigned char r, unsigned char g, unsigned char b, char* out, int cap) { return snprintf(out, cap, CSI "48;2;%u;%u;%um", r, g, b); }

static int osc(const char* body, char* out, int cap)
{
    return snprintf(out, cap, "\x1b]%s\x07", body);
}
int gmb_concodes_set_title(const char* s, char* out, int cap) { char b[600]; snprintf(b, sizeof(b), "2;%s", s ? s : ""); return osc(b, out, cap); }
int gmb_concodes_set_icon_name(const char* s, char* out, int cap) { char b[600]; snprintf(b, sizeof(b), "1;%s", s ? s : ""); return osc(b, out, cap); }
int gmb_concodes_set_both(const char* s, char* out, int cap) { char b[600]; snprintf(b, sizeof(b), "0;%s", s ? s : ""); return osc(b, out, cap); }
int gmb_concodes_set_working_dir(const char* s, char* out, int cap) { char b[600]; snprintf(b, sizeof(b), "7;%s", s ? s : ""); return osc(b, out, cap); }
int gmb_concodes_notification(const char* s, char* out, int cap) { char b[600]; snprintf(b, sizeof(b), "9;%s", s ? s : ""); return osc(b, out, cap); }
int gmb_concodes_enable_mouse(unsigned int mode, char* out, int cap) { return snprintf(out, cap, CSI "?%uh", mode); }
int gmb_concodes_disable_mouse(unsigned int mode, char* out, int cap) { return snprintf(out, cap, CSI "?%ul", mode); }
int gmb_concodes_save_cursor(char* out, int cap) { snprintf(out, cap, "\x1b" "7"); return 2; }
int gmb_concodes_restore_cursor(char* out, int cap) { snprintf(out, cap, "\x1b" "8"); return 2; }
int gmb_concodes_scroll_region(unsigned short top, unsigned short bottom, char* out, int cap) { return snprintf(out, cap, CSI "%hu;%hur", top, bottom); }
int gmb_concodes_insert_lines(unsigned short n, char* out, int cap) { return snprintf(out, cap, CSI "%huL", n); }
int gmb_concodes_delete_lines(unsigned short n, char* out, int cap) { return snprintf(out, cap, CSI "%huM", n); }
int gmb_concodes_insert_chars(unsigned short n, char* out, int cap) { return snprintf(out, cap, CSI "%hu@", n); }
int gmb_concodes_delete_chars(unsigned short n, char* out, int cap) { return snprintf(out, cap, CSI "%huP", n); }
int gmb_concodes_erase_chars(unsigned short n, char* out, int cap) { return snprintf(out, cap, CSI "%huX", n); }

/* ===================== Decoder: Parser state machine ===================== */

enum {
    ST_GROUND, ST_ESCAPE, ST_ESCAPE_INTERMEDIATE, ST_CSI_ENTRY, ST_CSI_IGNORE,
    ST_CSI_PARAMETER, ST_CSI_INTERMEDIATE, ST_UTF8, ST_OSC_STRING
};

void gmb_parser_init(gmb_parser_t* p, bool emulator)
{
    memset(p, 0, sizeof(*p));
    p->emulator = emulator;
    p->state = ST_GROUND;
}

static bool is_c0(unsigned char b)
{
    return b <= 0x17 || b == 0x19 || (b >= 0x1C && b <= 0x1F);
}

static void parser_set_state(gmb_parser_t* p, int s)
{
    if (s == ST_GROUND)
    {
        p->param_count = 0;
        p->parameter = 0;
        p->ignored_count = 0;
        p->priv = false;
        p->sgr = false;
        p->utf8_count = 0;
        p->utf8_expected = 0;
        p->osc_count = 0;
    }
    p->state = s;
}

static int parser_emit(gmb_parser_t* p, const gmb_sequence_t* seq)
{
    if (p->seq_count < GMB_SEQ_MAX)
        p->seqs[p->seq_count++] = *seq;
    return 1;
}

static void parser_store_parameter(gmb_parser_t* p)
{
    if (p->param_count < 30)
    {
        p->params[p->param_count] = p->parameter;
        p->param_count++;
    }
    else
    {
        p->ignored_count++;
    }
    p->parameter = 0;
}

static void parser_provide_char(gmb_parser_t* p, char ch);
static void parser_provide_execute(gmb_parser_t* p, unsigned char b);
static void parser_provide_esc(gmb_parser_t* p, char ch);
static void parser_provide_csi(gmb_parser_t* p, unsigned char b);
static void parser_provide_osc(gmb_parser_t* p);

/* Parsers (specchio Parsers.cs) */
static int pn(unsigned long* pp, int np, int i)
{
    unsigned long v = i < np ? pp[i] : 0;
    if (v < 1) v = 1;
    if (v > 65535) v = 65535;
    return (int)v;
}

static unsigned long P(unsigned long* pp, int np, int i, unsigned long def)
{
    unsigned long v = i < np ? pp[i] : 0;
    return v == 0 ? def : v;
}

static gmb_key_modifiers_t decode_modifier_param(unsigned long mod)
{
    unsigned long bits = mod == 0 ? 0 : mod - 1;
    gmb_key_modifiers_t m = GMB_MODKEY_NONE;
    if (bits & 1) m |= GMB_MODKEY_SHIFT;
    if (bits & 2) m |= GMB_MODKEY_ALT;
    if (bits & 4) m |= GMB_MODKEY_CONTROL;
    if (bits & 8) m |= GMB_MODKEY_META;
    if (bits & 16) m |= GMB_MODKEY_SUPER;
    if (bits & 32) m |= GMB_MODKEY_HYPER;
    return m;
}

static gmb_key_modifiers_t parse_kitty_mods(unsigned long mod)
{
    gmb_key_modifiers_t m = GMB_MODKEY_NONE;
    if (mod & 1) m |= GMB_MODKEY_SHIFT;
    if (mod & 2) m |= GMB_MODKEY_ALT;
    if (mod & 4) m |= GMB_MODKEY_CONTROL;
    if (mod & 8) m |= GMB_MODKEY_SUPER;
    if (mod & 16) m |= GMB_MODKEY_HYPER;
    if (mod & 32) m |= GMB_MODKEY_META;
    return m;
}

int gmb_concodes_decode_sgr(const unsigned long* params, int np, gmb_sgr_attr_t* out, int max)
{
    int o = 0;
    int i = 0;
    unsigned long local[32];
    int ln = np;
    if (ln == 0) { local[0] = 0; ln = 1; }
    else for (int k = 0; k < np && k < 32; k++) local[k] = params[k];
    unsigned long* p = local;
    while (i < ln && o < max)
    {
        unsigned long code = p[i];
        gmb_sgr_attr_t* a = &out[o];
        memset(a, 0, sizeof(*a));
        if (code == 0) a->type = GMB_SA_RESET;
        else if (code == 1) a->type = GMB_SA_BOLD;
        else if (code == 2) a->type = GMB_SA_DIM;
        else if (code == 3) a->type = GMB_SA_ITALIC;
        else if (code == 4) a->type = GMB_SA_UNDERLINE;
        else if (code == 5) a->type = GMB_SA_BLINK;
        else if (code == 6) a->type = GMB_SA_RAPIDBLINK;
        else if (code == 7) a->type = GMB_SA_REVERSE;
        else if (code == 8) a->type = GMB_SA_HIDDEN;
        else if (code == 9) a->type = GMB_SA_STRIKE;
        else if (code == 10) a->type = GMB_SA_PRIMARY_FONT;
        else if (code >= 11 && code <= 19) { a->type = GMB_SA_ALT_FONT; a->alt_font_n = (unsigned char)(code - 10); }
        else if (code == 20) a->type = GMB_SA_FRAKTUR;
        else if (code == 22) a->type = GMB_SA_NOT_BOLD_OR_DIM;
        else if (code == 23) a->type = GMB_SA_NOT_ITALIC;
        else if (code == 24) a->type = GMB_SA_NOT_UNDERLINE;
        else if (code == 25) a->type = GMB_SA_NOT_BLINK;
        else if (code == 27) a->type = GMB_SA_NOT_REVERSE;
        else if (code == 28) a->type = GMB_SA_NOT_HIDDEN;
        else if (code == 29) a->type = GMB_SA_NOT_STRIKE;
        else if (code >= 30 && code <= 37) { a->type = GMB_SA_FG; a->color.type = GMB_SGRCOL_INDEXED; a->color.index = (unsigned char)(code - 30); }
        else if (code == 38)
        {
            int next = i + 1;
            a->type = GMB_SA_FG_EXTENDED;
            if (next < ln && p[next] == 2)
            {
                a->color.type = GMB_SGRCOL_RGB;
                a->color.r = (unsigned char)(next + 1 < ln ? p[next + 1] : 0);
                a->color.g = (unsigned char)(next + 2 < ln ? p[next + 2] : 0);
                a->color.b = (unsigned char)(next + 3 < ln ? p[next + 3] : 0);
                i = next + 3;
            }
            else if (next < ln && p[next] == 5)
            {
                a->color.type = GMB_SGRCOL_INDEXED;
                a->color.index = (unsigned char)(next + 1 < ln ? p[next + 1] : 0);
                i = next + 1;
            }
            else { a->color.type = GMB_SGRCOL_DEFAULT; i = next; }
            o++;
            i++;
            continue;
        }
        else if (code == 39) a->type = GMB_SA_DEFAULT_FG;
        else if (code >= 40 && code <= 47) { a->type = GMB_SA_BG; a->color.type = GMB_SGRCOL_INDEXED; a->color.index = (unsigned char)(code - 40); }
        else if (code == 48)
        {
            int next = i + 1;
            a->type = GMB_SA_BG_EXTENDED;
            if (next < ln && p[next] == 2)
            {
                a->color.type = GMB_SGRCOL_RGB;
                a->color.r = (unsigned char)(next + 1 < ln ? p[next + 1] : 0);
                a->color.g = (unsigned char)(next + 2 < ln ? p[next + 2] : 0);
                a->color.b = (unsigned char)(next + 3 < ln ? p[next + 3] : 0);
                i = next + 3;
            }
            else if (next < ln && p[next] == 5)
            {
                a->color.type = GMB_SGRCOL_INDEXED;
                a->color.index = (unsigned char)(next + 1 < ln ? p[next + 1] : 0);
                i = next + 1;
            }
            else { a->color.type = GMB_SGRCOL_DEFAULT; i = next; }
            o++;
            i++;
            continue;
        }
        else if (code == 49) a->type = GMB_SA_DEFAULT_BG;
        else if (code >= 90 && code <= 97) { a->type = GMB_SA_FG; a->color.type = GMB_SGRCOL_INDEXED; a->color.index = (unsigned char)(code - 90 + 8); }
        else if (code >= 100 && code <= 107) { a->type = GMB_SA_BG; a->color.type = GMB_SGRCOL_INDEXED; a->color.index = (unsigned char)(code - 100 + 8); }
        else if (code == 51) a->type = GMB_SA_FRAMED;
        else if (code == 52) a->type = GMB_SA_ENCIRCLED;
        else if (code == 53) a->type = GMB_SA_OVERLINED;
        else if (code == 54) a->type = GMB_SA_NOT_FRAMED_OR_ENCIRCLED;
        else if (code == 55) a->type = GMB_SA_NOT_OVERLINED;
        else { o--; }
        o++;
        i++;
    }
    return o;
}

static int decode_mode(unsigned long code)
{
    switch (code)
    {
        case 1: return GMB_TM_KEYPAD;
        case 2: return GMB_TM_KEYBOARD_ACTION;
        case 4: return GMB_TM_INSERT_REPLACEMENT;
        case 6: return GMB_TM_ORIGIN;
        case 7: return GMB_TM_AUTOWRAP;
        case 12: return GMB_TM_SEND_RECEIVE;
        case 25: return GMB_TM_CURSOR_VISIBLE;
        case 47: case 1047: case 1049: return GMB_TM_ALTSCREEN;
        case 9: return GMB_TM_MOUSE_X10;
        case 1000: return GMB_TM_MOUSE_X11;
        case 1002: return GMB_TM_MOUSE_BUTTON_EVENT;
        case 1003: return GMB_TM_MOUSE_ANY_EVENT;
        case 1004: return GMB_TM_FOCUS_EVENT;
        case 1005: return GMB_TM_MOUSE_UTF8;
        case 1006: return GMB_TM_MOUSE_SGR;
        case 1015: return GMB_TM_MOUSE_URXVT;
        case 1016: return GMB_TM_MOUSE_SGR_PIXELS;
        case 2004: return GMB_TM_BRACKETED_PASTE;
        default: return GMB_TM_UNKNOWN;
    }
}

static void parse_csi_keyboard(gmb_parser_t* p, unsigned long* params, int np)
{
    if (np == 0) return;
    unsigned long cp = params[0];
    if (cp > 0x10FFFF) return;
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_KEY;
    seq.key.type = GMB_KEY_CHAR;
    seq.key.ch = (char)cp;
    seq.mods = parse_kitty_mods(np > 1 ? params[1] : 0);
    parser_emit(p, &seq);
}

static void parse_csi_tilde(gmb_parser_t* p, unsigned long* params, int np)
{
    if (np == 0) return;
    gmb_key_modifiers_t mods = decode_modifier_param(np > 1 ? params[1] : 1);
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_KEY;
    seq.mods = mods;
    unsigned long c = params[0];
    if (c == 1 || c == 7) seq.key.type = GMB_KEY_HOME;
    else if (c == 2) seq.key.type = GMB_KEY_INSERT;
    else if (c == 3) seq.key.type = GMB_KEY_DELETE;
    else if (c == 4 || c == 8) seq.key.type = GMB_KEY_END;
    else if (c == 5) seq.key.type = GMB_KEY_PAGEUP;
    else if (c == 6) seq.key.type = GMB_KEY_PAGEDOWN;
    else if (c >= 11 && c <= 15) { seq.key.type = GMB_KEY_F; seq.key.f_num = (unsigned char)(c - 10); }
    else if (c >= 17 && c <= 21) { seq.key.type = GMB_KEY_F; seq.key.f_num = (unsigned char)(c - 11); }
    else if (c >= 23 && c <= 24) { seq.key.type = GMB_KEY_F; seq.key.f_num = (unsigned char)(c - 12); }
    else return;
    parser_emit(p, &seq);
}

static void parse_csi_cursor_position(gmb_parser_t* p, unsigned long* params, int np)
{
    if (np < 2) return;
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_CURSOR_POSITION;
    seq.x = (unsigned short)params[1];
    seq.y = (unsigned short)params[0];
    parser_emit(p, &seq);
}

static void parse_csi_window_op(gmb_parser_t* p, unsigned long* params, int np)
{
    if (np == 0) return;
    unsigned long code = params[0];
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_WINDOW_OP;
    switch (code)
    {
        case 1: seq.window.type = GMB_WIN_DEICONIFY; break;
        case 2: seq.window.type = GMB_WIN_ICONIFY; break;
        case 3: seq.window.type = GMB_WIN_MOVE; seq.window.x = (int)(1 < np ? params[1] : 0); seq.window.y = (int)(2 < np ? params[2] : 0); break;
        case 4: seq.window.type = GMB_WIN_RESIZE_PIXELS; seq.window.w = (unsigned short)(2 < np ? params[2] : 0); seq.window.h = (unsigned short)(1 < np ? params[1] : 0); break;
        case 5: seq.window.type = GMB_WIN_RAISE; break;
        case 6: seq.window.type = GMB_WIN_LOWER; break;
        case 7: seq.window.type = GMB_WIN_REFRESH; break;
        case 8: seq.window.type = GMB_WIN_RESIZE_CHARS; seq.window.w = (unsigned short)(2 < np ? params[2] : 0); seq.window.h = (unsigned short)(1 < np ? params[1] : 0); break;
        case 9: seq.window.type = GMB_WIN_MAXIMIZE_VERT; break;
        case 10: seq.window.type = GMB_WIN_MAXIMIZE_HORZ; break;
        case 11: seq.window.type = GMB_WIN_MAXIMIZE; break;
        case 13: seq.window.type = GMB_WIN_RESTORE; break;
        default: seq.window.type = GMB_WIN_UNKNOWN; seq.window.code = code; break;
    }
    parser_emit(p, &seq);
}

static void parse_csi_xterm_mouse(gmb_parser_t* p, unsigned long* params, int np, char ch)
{
    if (np < 4) return;
    unsigned long cb_ = params[1];
    unsigned long cx = params[2];
    unsigned long cy = params[3];
    bool up = ch == 'm';
    gmb_key_modifiers_t mods = GMB_MODKEY_NONE;
    if (cb_ & 0x04) mods |= GMB_MODKEY_SHIFT;
    if (cb_ & 0x08) mods |= GMB_MODKEY_ALT;
    if (cb_ & 0x10) mods |= GMB_MODKEY_CONTROL;
    int btn = (int)(cb_ & 0x03);
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_MOUSE;
    seq.mods = mods;
    seq.mouse.x = (unsigned short)cx;
    seq.mouse.y = (unsigned short)cy;
    if (cb_ & 0x40)
    {
        seq.mouse.kind = (cb_ & 0x01) ? GMB_MOUSE_SCROLLDOWN : GMB_MOUSE_SCROLLUP;
    }
    else if (cb_ & 0x20)
    {
        seq.mouse.kind = GMB_MOUSE_MOVE;
        seq.mouse.button = GMB_MB_LEFT;
    }
    else
    {
        bool drag = (cb_ & 0x20) != 0;
        if (btn == 0)
        {
            if (up) seq.mouse.kind = GMB_MOUSE_UP;
            else if (drag) seq.mouse.kind = GMB_MOUSE_DRAG;
            else seq.mouse.kind = GMB_MOUSE_DOWN;
            seq.mouse.button = GMB_MB_LEFT;
        }
        else if (btn == 1)
        {
            if (up) seq.mouse.kind = GMB_MOUSE_UP;
            else if (drag) seq.mouse.kind = GMB_MOUSE_DRAG;
            else seq.mouse.kind = GMB_MOUSE_DOWN;
            seq.mouse.button = GMB_MB_MIDDLE;
        }
        else if (btn == 2)
        {
            if (up) seq.mouse.kind = GMB_MOUSE_UP;
            else if (drag) seq.mouse.kind = GMB_MOUSE_DRAG;
            else seq.mouse.kind = GMB_MOUSE_DOWN;
            seq.mouse.button = GMB_MB_RIGHT;
        }
        else
        {
            seq.mouse.kind = GMB_MOUSE_UP;
            seq.mouse.button = GMB_MB_ANY;
        }
    }
    parser_emit(p, &seq);
}

static void parse_csi_rxvt_mouse(gmb_parser_t* p, unsigned long* params, int np)
{
    if (np < 3) return;
    unsigned long cb_ = params[0];
    unsigned long cx = params[1];
    unsigned long cy = params[2];
    gmb_key_modifiers_t mods = GMB_MODKEY_NONE;
    if (cb_ & 0x04) mods |= GMB_MODKEY_SHIFT;
    if (cb_ & 0x08) mods |= GMB_MODKEY_ALT;
    if (cb_ & 0x10) mods |= GMB_MODKEY_CONTROL;
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_MOUSE;
    seq.mods = mods;
    seq.mouse.x = (unsigned short)cx;
    seq.mouse.y = (unsigned short)cy;
    if ((cb_ & 0x60) == 0x60)
    {
        seq.mouse.kind = (cb_ & 0x01) ? GMB_MOUSE_SCROLLDOWN : GMB_MOUSE_SCROLLUP;
    }
    else
    {
        bool drag = (cb_ & 0x40) != 0;
        unsigned long btn = cb_ & 0x03;
        if (btn == 0 && !drag) { seq.mouse.kind = GMB_MOUSE_DOWN; seq.mouse.button = GMB_MB_LEFT; }
        else if (btn == 2 && !drag) { seq.mouse.kind = GMB_MOUSE_DOWN; seq.mouse.button = GMB_MB_RIGHT; }
        else if (btn == 1 && !drag) { seq.mouse.kind = GMB_MOUSE_DOWN; seq.mouse.button = GMB_MB_MIDDLE; }
        else if (btn == 0 && drag) { seq.mouse.kind = GMB_MOUSE_DRAG; seq.mouse.button = GMB_MB_LEFT; }
        else if (btn == 2 && drag) { seq.mouse.kind = GMB_MOUSE_DRAG; seq.mouse.button = GMB_MB_RIGHT; }
        else if (btn == 1 && drag) { seq.mouse.kind = GMB_MOUSE_DRAG; seq.mouse.button = GMB_MB_MIDDLE; }
        else if (btn == 3 && !drag) { seq.mouse.kind = GMB_MOUSE_UP; seq.mouse.button = GMB_MB_ANY; }
        else return;
    }
    parser_emit(p, &seq);
}

static void parse_csi_sequence(gmb_parser_t* p, unsigned char b)
{
    unsigned long* params = p->params;
    int np = p->param_count;
    char ch = (char)b;
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));

    if (p->priv)
    {
        if (ch == 'h')
        {
            seq.type = GMB_SEQ_SET_MODE;
            seq.mode = decode_mode(np > 0 ? params[0] : 0);
            parser_emit(p, &seq);
        }
        else if (ch == 'l')
        {
            seq.type = GMB_SEQ_RESET_MODE;
            seq.mode = decode_mode(np > 0 ? params[0] : 0);
            parser_emit(p, &seq);
        }
        return;
    }

    if (p->emulator)
    {
        switch (ch)
        {
            case 'A': seq.type = GMB_SEQ_CURSOR_UP; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'B': seq.type = GMB_SEQ_CURSOR_DOWN; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'C': seq.type = GMB_SEQ_CURSOR_FORWARD; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'D': seq.type = GMB_SEQ_CURSOR_BACKWARD; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'E': seq.type = GMB_SEQ_CURSOR_NEXT_LINE; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'F': seq.type = GMB_SEQ_CURSOR_PREV_LINE; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'G': case '`': seq.type = GMB_SEQ_CURSOR_COLUMN; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'd': seq.type = GMB_SEQ_CURSOR_LINE; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'H': case 'f':
                seq.type = GMB_SEQ_CURSOR_POSITION;
                seq.x = (unsigned short)P(params, np, 1, 1);
                seq.y = (unsigned short)P(params, np, 0, 1);
                break;
            case 'J': seq.type = GMB_SEQ_ERASE_IN_DISPLAY; seq.n1 = (unsigned short)P(params, np, 0, 0); break;
            case 'K': seq.type = GMB_SEQ_ERASE_IN_LINE; seq.n1 = (unsigned short)P(params, np, 0, 0); break;
            case 'L': seq.type = GMB_SEQ_INSERT_LINES; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'M': seq.type = GMB_SEQ_DELETE_LINES; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case '@': seq.type = GMB_SEQ_INSERT_CHARS; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'P': seq.type = GMB_SEQ_DELETE_CHARS; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'S': seq.type = GMB_SEQ_SCROLL_UP; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'T': seq.type = GMB_SEQ_SCROLL_DOWN; seq.n1 = (unsigned short)pn(params, np, 0); break;
            case 'r':
                seq.type = GMB_SEQ_SET_SCROLL_REGION;
                seq.n1 = (unsigned short)P(params, np, 0, 1);
                seq.n2 = (unsigned short)P(params, np, 1, 0);
                break;
            case 'm':
                seq.type = GMB_SEQ_SGR;
                seq.sgr_count = gmb_concodes_decode_sgr(params, np, seq.sgr, GMB_SEQ_SGR_MAX);
                break;
            case 's': seq.type = GMB_SEQ_SAVE_CURSOR; break;
            case 'u': seq.type = GMB_SEQ_RESTORE_CURSOR; break;
            default: return;
        }
        parser_emit(p, &seq);
        return;
    }

    switch (ch)
    {
        case 'A': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_UP; seq.mods = decode_modifier_param(np > 1 ? params[1] : 1); break;
        case 'B': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_DOWN; seq.mods = decode_modifier_param(np > 1 ? params[1] : 1); break;
        case 'C': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_RIGHT; seq.mods = decode_modifier_param(np > 1 ? params[1] : 1); break;
        case 'D': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_LEFT; seq.mods = decode_modifier_param(np > 1 ? params[1] : 1); break;
        case 'H': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_HOME; break;
        case 'F': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_END; break;
        case 'Z': seq.type = GMB_SEQ_KEY; seq.key.type = GMB_KEY_BACKTAB; break;
        case 'I': seq.type = GMB_SEQ_FOCUS_IN; break;
        case 'O': seq.type = GMB_SEQ_FOCUS_OUT; break;
        case 'R': parse_csi_cursor_position(p, params, np); return;
        case 'u': parse_csi_keyboard(p, params, np); return;
        case 't': parse_csi_window_op(p, params, np); return;
        case 'm': parse_csi_xterm_mouse(p, params, np, ch); return;
        case 'M':
            if (p->sgr) parse_csi_xterm_mouse(p, params, np, ch);
            else parse_csi_rxvt_mouse(p, params, np);
            return;
        case '~': parse_csi_tilde(p, params, np); return;
        default: return;
    }
    parser_emit(p, &seq);
}

static void parse_osc(gmb_parser_t* p)
{
    char body[1100];
    int bl = p->osc_count < 1024 ? p->osc_count : 1024;
    memcpy(body, p->osc, (size_t)bl);
    body[bl] = '\0';
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_OSC;
    char* semi = strchr(body, ';');
    char code_part[64];
    if (semi)
    {
        int cn = (int)(semi - body);
        if (cn > 63) cn = 63;
        memcpy(code_part, body, (size_t)cn);
        code_part[cn] = '\0';
    }
    else
        snprintf(code_part, sizeof(code_part), "%s", body);
    char* colon = strchr(code_part, ':');
    if (colon) *colon = '\0';
    long code = atol(code_part);
    const char* rest = semi ? semi + 1 : "";
    switch (code)
    {
        case 0: seq.osc.type = GMB_OSC_SET_BOTH; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 1: seq.osc.type = GMB_OSC_SET_ICON_NAME; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 2: seq.osc.type = GMB_OSC_SET_TITLE; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 7: seq.osc.type = GMB_OSC_SET_WORKING_DIR; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 8:
            seq.osc.type = GMB_OSC_HYPERLINK;
            {
                char* s2 = strchr(rest, ';');
                if (s2)
                {
                    int pn2 = (int)(s2 - rest);
                    if (pn2 > 127) pn2 = 127;
                    memcpy(seq.osc.param, rest, (size_t)pn2);
                    seq.osc.param[pn2] = '\0';
                    snprintf(seq.osc.uri, sizeof(seq.osc.uri), "%s", s2 + 1);
                }
            }
            break;
        case 9: seq.osc.type = GMB_OSC_NOTIFICATION; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 4:
            seq.osc.type = GMB_OSC_SET_COLOR_INDEX;
            {
                char* s2 = strchr(rest, ';');
                if (s2) { seq.osc.index = (unsigned char)atoi(rest); snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", s2 + 1); }
                else { seq.osc.index = 0; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); }
            }
            break;
        case 10: seq.osc.type = GMB_OSC_SET_FG_COLOR; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 11: seq.osc.type = GMB_OSC_SET_BG_COLOR; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 12: seq.osc.type = GMB_OSC_SET_CURSOR_COLOR; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
        case 52:
            seq.osc.type = GMB_OSC_CLIPBOARD;
            {
                char* s2 = strchr(rest, ';');
                if (s2) { seq.osc.selector = rest[0] ? rest[0] : 'c'; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", s2 + 1); }
                else { seq.osc.selector = 'c'; seq.osc.text[0] = '\0'; }
            }
            break;
        default: seq.osc.type = GMB_OSC_UNKNOWN; seq.osc.index = (unsigned char)code; snprintf(seq.osc.text, sizeof(seq.osc.text), "%s", rest); break;
    }
    parser_emit(p, &seq);
}

static void parser_provide_char(gmb_parser_t* p, char ch)
{
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    if (p->esc_o)
    {
        p->esc_o = false;
        if (ch >= 'P' && ch <= 'S')
        {
            seq.type = GMB_SEQ_KEY;
            seq.key.type = GMB_KEY_F;
            seq.key.f_num = (unsigned char)(ch - 'P' + 1);
            parser_emit(p, &seq);
        }
        return;
    }
    if (p->emulator)
    {
        seq.type = GMB_SEQ_PRINT;
        seq.ch = ch;
        parser_emit(p, &seq);
        return;
    }
    seq.type = GMB_SEQ_KEY;
    switch (ch)
    {
        case '\r': case '\n': seq.key.type = GMB_KEY_ENTER; break;
        case '\t': seq.key.type = GMB_KEY_TAB; break;
        case ' ': seq.key.type = GMB_KEY_SPACE; break;
        case '\x7F': seq.key.type = GMB_KEY_BACKTAB; break;
        case '\x1B': seq.key.type = GMB_KEY_ESC; break;
        case '\0': seq.key.type = GMB_KEY_NULL; break;
        default: seq.key.type = GMB_KEY_CHAR; seq.key.ch = ch; break;
    }
    parser_emit(p, &seq);
}

static void parser_provide_execute(gmb_parser_t* p, unsigned char b)
{
    if (p->emulator)
    {
        gmb_sequence_t seq;
        memset(&seq, 0, sizeof(seq));
        seq.type = GMB_SEQ_EXECUTE;
        seq.ctrl = b;
        parser_emit(p, &seq);
        return;
    }
    parser_provide_char(p, (char)b);
}

static void parser_provide_esc(gmb_parser_t* p, char ch)
{
    if (ch == 'O') { p->esc_o = true; return; }
    p->esc_o = false;
    if (p->emulator)
    {
        if (ch == '7')
        {
            gmb_sequence_t seq;
            memset(&seq, 0, sizeof(seq));
            seq.type = GMB_SEQ_SAVE_CURSOR;
            parser_emit(p, &seq);
        }
        else if (ch == '8')
        {
            gmb_sequence_t seq;
            memset(&seq, 0, sizeof(seq));
            seq.type = GMB_SEQ_RESTORE_CURSOR;
            parser_emit(p, &seq);
        }
        else if (ch == 'c')
        {
            gmb_sequence_t seq;
            memset(&seq, 0, sizeof(seq));
            seq.type = GMB_SEQ_SOFT_RESET;
            parser_emit(p, &seq);
        }
        return;
    }
    gmb_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.type = GMB_SEQ_KEY;
    seq.key.type = GMB_KEY_CHAR;
    seq.key.ch = ch;
    seq.mods = GMB_MODKEY_ALT;
    parser_emit(p, &seq);
}

static void parser_provide_csi(gmb_parser_t* p, unsigned char b)
{
    parse_csi_sequence(p, b);
    p->esc_o = false;
    parser_set_state(p, ST_GROUND);
}

static void parser_provide_osc(gmb_parser_t* p)
{
    parse_osc(p);
    p->esc_o = false;
}

static bool parser_handle_possible_esc(gmb_parser_t* p, unsigned char b, bool more)
{
    if (b != 0x1B) return false;
    switch (p->state * 2 + (more ? 1 : 0))
    {
        case ST_GROUND * 2 + 1: p->state = ST_ESCAPE; break;
        case ST_GROUND * 2 + 0: parser_provide_char(p, '\x1B'); break;
        case ST_ESCAPE * 2 + 1: parser_provide_char(p, '\x1B'); break;
        case ST_ESCAPE * 2 + 0:
            parser_provide_char(p, '\x1B');
            parser_provide_char(p, '\x1B');
            p->state = ST_GROUND;
            break;
        default:
            if (more) p->state = ST_ESCAPE;
            else { parser_provide_char(p, '\x1B'); p->state = ST_GROUND; }
            break;
    }
    return true;
}

static bool parser_handle_possible_utf8(gmb_parser_t* p, unsigned char b)
{
    if ((b & 0x80) == 0x00)
    {
        if (b >= 0x20 && b <= 0x7E) parser_provide_char(p, (char)b);
        else parser_provide_execute(p, b);
        return true;
    }
    if ((b & 0xE0) == 0xC0) { p->utf8_count = 1; p->utf8_points[0] = b; p->utf8_expected = 2; p->state = ST_UTF8; return true; }
    if ((b & 0xF0) == 0xE0) { p->utf8_count = 1; p->utf8_points[0] = b; p->utf8_expected = 3; p->state = ST_UTF8; return true; }
    if ((b & 0xF8) == 0xF0) { p->utf8_count = 1; p->utf8_points[0] = b; p->utf8_expected = 4; p->state = ST_UTF8; return true; }
    return false;
}

static void parser_advance_ground(gmb_parser_t* p, unsigned char b)
{
    if (parser_handle_possible_utf8(p, b)) return;
    if (is_c0(b)) parser_provide_execute(p, b);
    else if (b >= 0x20 && b <= 0x7F) parser_provide_char(p, (char)b);
}

static void parser_advance_escape(gmb_parser_t* p, unsigned char b)
{
    if (b >= 0x20 && b <= 0x2F) p->state = ST_ESCAPE_INTERMEDIATE;
    else if (b == 0x5B) p->state = ST_CSI_ENTRY;
    else if (b == 0x5D) { p->osc_count = 0; p->state = ST_OSC_STRING; }
    else if ((b >= 0x30 && b <= 0x4F) || (b >= 0x51 && b <= 0x57) || b == 0x59 || b == 0x5A || b == 0x5C || (b >= 0x60 && b <= 0x7E))
    {
        parser_provide_esc(p, (char)b);
        p->state = ST_GROUND;
    }
    else if (is_c0(b)) parser_provide_execute(p, b);
    else if (b != 0x7F) p->state = ST_GROUND;
}

static void parser_advance_escape_intermediate(gmb_parser_t* p, unsigned char b)
{
    if (b >= 0x20 && b <= 0x2F) { }
    else if (b == 0x5B) p->state = ST_CSI_ENTRY;
    else if ((b >= 0x30 && b <= 0x5A) || (b >= 0x5C && b <= 0x7E))
    {
        parser_provide_esc(p, (char)b);
        p->state = ST_GROUND;
    }
    else if (is_c0(b)) parser_provide_execute(p, b);
    else if (b != 0x7F) p->state = ST_GROUND;
}

static void parser_advance_csi_entry(gmb_parser_t* p, unsigned char b)
{
    if (b == 0x3B) { parser_store_parameter(p); p->state = ST_CSI_PARAMETER; }
    else if (b >= 0x30 && b <= 0x39) { p->parameter = (unsigned long)(b - 0x30); p->state = ST_CSI_PARAMETER; }
    else if (b == 0x3A) p->state = ST_CSI_IGNORE;
    else if (b >= 0x3C && b <= 0x3F)
    {
        if (b == 0x3F) p->priv = true;
        if (b == 0x3C) p->sgr = true;
        p->state = ST_CSI_PARAMETER;
    }
    else if (b >= 0x40 && b <= 0x7E) { parser_provide_csi(p, b); }
    else if (is_c0(b)) parser_provide_execute(p, b);
    else if (b != 0x7F) { p->parameter = b; parser_store_parameter(p); }
}

static void parser_advance_csi_ignore(gmb_parser_t* p, unsigned char b)
{
    if (is_c0(b)) parser_provide_execute(p, b);
    else if (b >= 0x40 && b <= 0x7E) p->state = ST_GROUND;
    else p->state = ST_GROUND;
}

static void parser_advance_csi_parameter(gmb_parser_t* p, unsigned char b)
{
    if (b >= 0x30 && b <= 0x39) p->parameter = p->parameter * 10 + (unsigned long)(b - 0x30);
    else if (b == 0x3B) parser_store_parameter(p);
    else if (b >= 0x40 && b <= 0x7E) { parser_store_parameter(p); parser_provide_csi(p, b); }
    else if (b >= 0x20 && b <= 0x2F) { parser_store_parameter(p); p->state = ST_CSI_INTERMEDIATE; }
    else if (b == 0x3A) p->state = ST_CSI_IGNORE;
    else if (b >= 0x3C && b <= 0x3F) p->priv = true;
    else if (is_c0(b)) parser_provide_execute(p, b);
    else if (b != 0x7F) p->state = ST_GROUND;
}

static void parser_advance_csi_intermediate(gmb_parser_t* p, unsigned char b)
{
    if (b >= 0x20 && b <= 0x2F) { }
    else if (b >= 0x40 && b <= 0x7E) parser_provide_csi(p, b);
    else if (is_c0(b)) parser_provide_execute(p, b);
    else if (b != 0x7F) p->state = ST_GROUND;
}

static void parser_advance_utf8(gmb_parser_t* p, unsigned char b)
{
    if ((b & 0xC0) != 0x80) { p->state = ST_GROUND; return; }
    p->utf8_points[p->utf8_count] = b;
    p->utf8_count++;
    if (p->utf8_count == p->utf8_expected)
    {
        /* decodifica UTF-8 (1 char) */
        uint32_t cp;
        const unsigned char* s = p->utf8_points;
        if (p->utf8_expected == 2) cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        else if (p->utf8_expected == 3) cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        else cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        parser_provide_char(p, (char)cp);
        p->state = ST_GROUND;
    }
}

static void parser_advance_osc(gmb_parser_t* p, unsigned char b)
{
    if (b == 0x07) { parser_provide_osc(p); p->state = ST_GROUND; }
    else if (b == 0x1B) { parser_provide_osc(p); p->state = ST_ESCAPE; }
    else if (p->osc_count < 1023) p->osc[p->osc_count++] = b;
}

void gmb_parser_advance_byte(gmb_parser_t* p, unsigned char b, bool more)
{
    if (parser_handle_possible_esc(p, b, more)) return;
    switch (p->state)
    {
        case ST_GROUND: parser_advance_ground(p, b); break;
        case ST_ESCAPE: parser_advance_escape(p, b); break;
        case ST_ESCAPE_INTERMEDIATE: parser_advance_escape_intermediate(p, b); break;
        case ST_CSI_ENTRY: parser_advance_csi_entry(p, b); break;
        case ST_CSI_IGNORE: parser_advance_csi_ignore(p, b); break;
        case ST_CSI_PARAMETER: parser_advance_csi_parameter(p, b); break;
        case ST_CSI_INTERMEDIATE: parser_advance_csi_intermediate(p, b); break;
        case ST_UTF8: parser_advance_utf8(p, b); break;
        case ST_OSC_STRING: parser_advance_osc(p, b); break;
    }
}

void gmb_parser_advance(gmb_parser_t* p, const char* utf8, bool more)
{
    if (!utf8) return;
    int len = (int)strlen(utf8);
    for (int i = 0; i < len; i++)
        gmb_parser_advance_byte(p, (unsigned char)utf8[i], i < len - 1 || more);
}

int gmb_parser_pending(const gmb_parser_t* p) { return p->seq_count; }

int gmb_parser_drain(gmb_parser_t* p, gmb_sequence_t* out, int max)
{
    int n = p->seq_count < max ? p->seq_count : max;
    for (int i = 0; i < n; i++) out[i] = p->seqs[i];
    p->seq_count = 0;
    return n;
}
