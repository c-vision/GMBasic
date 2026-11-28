/* ============================================================================
 * gmb_layout.c — LAYOUT MANAGER condiviso (VM/C + C99Native), vedi gmb_layout.h.
 *
 * Una sola implementazione per entrambi i percorsi dei controlli:
 *   - GRAPHICS/PIXEL: gmb_form.c (g_btns[] / g_wid[]), rect in PIXEL;
 *   - TEXT/TUI:      registry gmb_tui_* (g_reg_ctrl[]), rect in CELLE.
 * Il chiamante passa `is_tui` e le dimensioni correnti del contenitore, così
 * questo file non dipende né da SDL né dalla grafica (resta nel core).
 * ============================================================================ */
#include "gmb_layout.h"
#include "gmb_tui_widgets.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

/* Valore di una chiave `key=valore` nella spec; il valore può essere fra
 * virgolette. Ritorna 1 se trovata. La ricerca è per parola intera, così
 * `expand=` non viene confuso con `…expand=`. */
static int layout_find_val(const char* spec, const char* key, char* out, size_t cap)
{
    if (!spec || !key || !out || cap == 0) return 0;
    size_t kl = strlen(key);
    const char* p = spec;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (strncasecmp(p, key, kl) == 0 && p[kl] == '=') {
            p += kl + 1;
            size_t n = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
                if (*p == '"') p++;
            } else {
                while (*p && *p != ' ' && n + 1 < cap) out[n++] = *p++;
            }
            out[n] = 0;
            return 1;
        }
        /* salta il token corrente (gestendo le virgolette) */
        while (*p && *p != ' ') {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') p++;
                if (*p) p++;
            } else p++;
        }
    }
    return 0;
}

/* Divide `v` su '+' '|' ',' e applica `fn` a ogni parola. Usa una copia locale
 * perché strtok_r modifica la stringa. */
static int layout_anchor_mask(const char* v)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", v);
    int m = 0;
    char* sv = NULL;
    for (char* it = strtok_r(buf, "+|,", &sv); it; it = strtok_r(NULL, "+|,", &sv)) {
        while (*it == ' ') it++;
        if      (!strcasecmp(it, "left"))   m |= GMB_LAYOUT_LEFT;
        else if (!strcasecmp(it, "right"))  m |= GMB_LAYOUT_RIGHT;
        else if (!strcasecmp(it, "top"))    m |= GMB_LAYOUT_TOP;
        else if (!strcasecmp(it, "bottom")) m |= GMB_LAYOUT_BOTTOM;
        else if (!strcasecmp(it, "all") || !strcasecmp(it, "fill")) m |= GMB_LAYOUT_ALL;
        else if (!strcasecmp(it, "none"))   m = 0;
    }
    return m;
}

int gmb_layout_parse(const char* spec, gmb_layout_spec_t* out)
{
    if (!spec || !out) return 0;
    char v[96];
    int any = 0;

    out->expand  = -1;
    out->anchor  = -1;
    out->align_h = -1;
    out->align_v = -1;
    out->min_w   = -1;
    out->min_h   = -1;
    out->max_w   = -1;
    out->max_h   = -1;
    out->grow    = -1;

    if (layout_find_val(spec, "expand", v, sizeof v)) {
        any = 1;
        if      (!strcasecmp(v, "none") || !strcmp(v, "0")) out->expand = GMB_LAYOUT_EXPAND_NONE;
        else if (!strcasecmp(v, "h") || !strcasecmp(v, "horizontal") || !strcmp(v, "1")) out->expand = GMB_LAYOUT_EXPAND_H;
        else if (!strcasecmp(v, "v") || !strcasecmp(v, "vertical")   || !strcmp(v, "2")) out->expand = GMB_LAYOUT_EXPAND_V;
        else out->expand = GMB_LAYOUT_EXPAND_BOTH;   /* both/all/qualunque valore esplicito */
    }
    if (layout_find_val(spec, "anchor", v, sizeof v)) {
        any = 1;
        out->anchor = layout_anchor_mask(v);
    }
    if (layout_find_val(spec, "align", v, sizeof v)) {
        any = 1;
        char buf[96];
        snprintf(buf, sizeof(buf), "%s", v);
        char* sv = NULL;
        for (char* it = strtok_r(buf, "+|,", &sv); it; it = strtok_r(NULL, "+|,", &sv)) {
            while (*it == ' ') it++;
            if      (!strcasecmp(it, "left")   || !strcasecmp(it, "start"))  out->align_h = GMB_LAYOUT_ALIGN_START;
            else if (!strcasecmp(it, "center") || !strcasecmp(it, "centre")) out->align_h = GMB_LAYOUT_ALIGN_CENTER;
            else if (!strcasecmp(it, "right")  || !strcasecmp(it, "end"))    out->align_h = GMB_LAYOUT_ALIGN_END;
            else if (!strcasecmp(it, "top"))                                 out->align_v = GMB_LAYOUT_ALIGN_START;
            else if (!strcasecmp(it, "middle") || !strcasecmp(it, "vcenter"))out->align_v = GMB_LAYOUT_ALIGN_CENTER;
            else if (!strcasecmp(it, "bottom"))                              out->align_v = GMB_LAYOUT_ALIGN_END;
        }
    }
    /* Limiti di dimensione e quota del delta. 0 = nessun limite / assorbe tutto. */
    if (layout_find_val(spec, "minw", v, sizeof v)) { any = 1; out->min_w = atoi(v); if (out->min_w < 0) out->min_w = 0; }
    if (layout_find_val(spec, "minh", v, sizeof v)) { any = 1; out->min_h = atoi(v); if (out->min_h < 0) out->min_h = 0; }
    if (layout_find_val(spec, "maxw", v, sizeof v)) { any = 1; out->max_w = atoi(v); if (out->max_w < 0) out->max_w = 0; }
    if (layout_find_val(spec, "maxh", v, sizeof v)) { any = 1; out->max_h = atoi(v); if (out->max_h < 0) out->max_h = 0; }
    if (layout_find_val(spec, "grow", v, sizeof v)) {
        any = 1;
        out->grow = atoi(v);
        if (out->grow < 0)   out->grow = 0;
        if (out->grow > 200) out->grow = 200;
    }
    return any;
}

int gmb_layout_active(const struct gmb_control* cc)
{
    const gmb_control_t* c = (const gmb_control_t*)cc;
    return c && c->layout_on;
}

void gmb_layout_configure(struct gmb_control* cc, int is_tui,
                          const gmb_layout_spec_t* ls, int win_w, int win_h)
{
    gmb_control_t* c = (gmb_control_t*)cc;
    if (!c || !ls) return;
    if (ls->expand < 0 && ls->anchor < 0 && ls->align_h < 0 && ls->align_v < 0
        && ls->min_w < 0 && ls->min_h < 0 && ls->max_w < 0 && ls->max_h < 0 && ls->grow < 0)
        return;

    if (!c->layout_on) {
        /* Prima configurazione: il rect DICHIARATO e la finestra di adesso sono
         * la base del layout. Da qui in poi il reflow lavora sul delta. */
        c->layout_on        = 1;
        c->layout_expand    = GMB_LAYOUT_EXPAND_NONE;
        c->layout_anchor    = -1;                       /* non specificato */
        c->layout_align_h   = GMB_LAYOUT_ALIGN_START;
        c->layout_align_v   = GMB_LAYOUT_ALIGN_START;
        c->layout_min_w     = 0;
        c->layout_min_h     = 0;
        c->layout_max_w     = 0;
        c->layout_max_h     = 0;
        c->layout_grow      = 100;                      /* assorbe tutto il delta */
        c->layout_base_x    = c->x;
        c->layout_base_y    = c->y;
        c->layout_base_w    = c->width;
        c->layout_base_h    = c->height;
        c->layout_base_win_w = win_w > 0 ? win_w : 0;
        c->layout_base_win_h = win_h > 0 ? win_h : 0;
    }
    /* Se alla creazione la finestra non era ancora nota, la si fissa al primo
     * valore valido e non la si sovrascrive più (altrimenti il delta cambia). */
    if (c->layout_base_win_w <= 0 && win_w > 0) c->layout_base_win_w = win_w;
    if (c->layout_base_win_h <= 0 && win_h > 0) c->layout_base_win_h = win_h;

    if (ls->expand  >= 0) c->layout_expand  = ls->expand;
    if (ls->anchor  >= 0) c->layout_anchor  = ls->anchor;
    if (ls->align_h >= 0) c->layout_align_h = ls->align_h;
    if (ls->align_v >= 0) c->layout_align_v = ls->align_v;
    if (ls->min_w   >= 0) c->layout_min_w   = ls->min_w;
    if (ls->min_h   >= 0) c->layout_min_h   = ls->min_h;
    if (ls->max_w   >= 0) c->layout_max_w   = ls->max_w;
    if (ls->max_h   >= 0) c->layout_max_h   = ls->max_h;
    if (ls->grow    >= 0) c->layout_grow    = ls->grow;
    c->layout_is_tui = is_tui ? 1 : 0;

    /* I limiti sono proprietà del CONTROLLO, non solo del reflow: valgono anche
     * alla creazione (un minimo dichiarato non può essere ignorato fino al primo
     * resize). La base del delta resta il rect DICHIARATO. */
    if (c->layout_min_w > 0 && c->width  < c->layout_min_w) c->width  = c->layout_min_w;
    if (c->layout_min_h > 0 && c->height < c->layout_min_h) c->height = c->layout_min_h;
    if (c->layout_max_w > 0 && c->width  > c->layout_max_w) c->width  = c->layout_max_w;
    if (c->layout_max_h > 0 && c->height > c->layout_max_h) c->height = c->layout_max_h;
}

int gmb_layout_reflow(struct gmb_control* cc, int is_tui, int win_w, int win_h)
{
    gmb_control_t* c = (gmb_control_t*)cc;
    if (!c || !c->layout_on) return 0;
    if (c->layout_is_tui != (is_tui ? 1 : 0)) return 0;
    if (win_w <= 0 || win_h <= 0) return 0;
    if (c->layout_base_win_w <= 0 || c->layout_base_win_h <= 0) return 0;

    int dx = win_w - c->layout_base_win_w;
    int dy = win_h - c->layout_base_win_h;

    /* GROW = quota del delta assorbita dal controllo (100 = tutto). Serve a due
     * pannelli affiancati che crescono entrambi: ciascuno prende la sua parte
     * invece di sovrapporsi all'altro (il modello è per-controllo, senza
     * conoscenza dei fratelli). */
    if (c->layout_grow >= 0 && c->layout_grow != 100) {
        dx = dx * c->layout_grow / 100;
        dy = dy * c->layout_grow / 100;
    }

    int a = c->layout_anchor;   /* -1 = non specificato */
    int aleft = 0, aright = 0, atop = 0, abot = 0;
    if (a >= 0) {
        aleft  = (a & GMB_LAYOUT_LEFT)   != 0;
        aright = (a & GMB_LAYOUT_RIGHT)  != 0;
        atop   = (a & GMB_LAYOUT_TOP)    != 0;
        abot   = (a & GMB_LAYOUT_BOTTOM) != 0;
    }
    /* EXPAND è la scorciatoia che aggancia entrambi i bordi dell'asse; un ANCHOR
     * esplicito ha la precedenza su quell'asse. */
    if (c->layout_expand & GMB_LAYOUT_EXPAND_H) {
        if (a < 0 || (!aleft && !aright)) { aleft = 1; aright = 1; }
    }
    if (c->layout_expand & GMB_LAYOUT_EXPAND_V) {
        if (a < 0 || (!atop && !abot)) { atop = 1; abot = 1; }
    }

    int nx = c->layout_base_x, ny = c->layout_base_y;
    int nw = c->layout_base_w, nh = c->layout_base_h;

    if (aleft && aright)      nw = c->layout_base_w + dx;   /* si allarga */
    else if (aright)          nx = c->layout_base_x + dx;   /* resta a destra */
    else if (!aleft) {                                      /* nessun bordo: ALIGN */
        if (c->layout_align_h == GMB_LAYOUT_ALIGN_CENTER)    nx = c->layout_base_x + dx / 2;
        else if (c->layout_align_h == GMB_LAYOUT_ALIGN_END)  nx = c->layout_base_x + dx;
    }

    if (atop && abot)         nh = c->layout_base_h + dy;
    else if (abot)            ny = c->layout_base_y + dy;
    else if (!atop) {
        if (c->layout_align_v == GMB_LAYOUT_ALIGN_CENTER)    ny = c->layout_base_y + dy / 2;
        else if (c->layout_align_v == GMB_LAYOUT_ALIGN_END)  ny = c->layout_base_y + dy;
    }

    /* Limiti dichiarati: prima il minimo leggibile, poi il tetto (se i due si
     * contraddicono vince il tetto, che è il vincolo visivo). */
    if (c->layout_min_w > 0 && nw < c->layout_min_w) nw = c->layout_min_w;
    if (c->layout_min_h > 0 && nh < c->layout_min_h) nh = c->layout_min_h;
    if (c->layout_max_w > 0 && nw > c->layout_max_w) nw = c->layout_max_w;
    if (c->layout_max_h > 0 && nh > c->layout_max_h) nh = c->layout_max_h;

    /* Un controllo non può avere dimensione nulla: il layout non deve mai
     * renderlo invisibile (né dipingere un rect degenere). */
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;

    if (nx == c->x && ny == c->y && nw == c->width && nh == c->height) return 0;
    c->x = nx; c->y = ny; c->width = nw; c->height = nh;
    return 1;
}
