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
    out->flow    = -1;
    out->cols    = -1;
    out->gap     = -1;
    out->pad     = -1;
    out->has_group = 0;
    out->group[0] = '\0';

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

    /* FLOW (contenitore): `flow=wrap` / `flow=grid`, con `cols=`, `gap=`, `pad=`.
     * Alias comodi: `wrap=1` (flow=wrap) e `grid=3` (flow=grid cols=3). */
    if (layout_find_val(spec, "flow", v, sizeof v)) {
        any = 1;
        if      (!strcasecmp(v, "wrap") || !strcasecmp(v, "flow") || !strcmp(v, "1")) out->flow = GMB_LAYOUT_FLOW_WRAP;
        else if (!strcasecmp(v, "grid") || !strcasecmp(v, "table") || !strcmp(v, "2")) out->flow = GMB_LAYOUT_FLOW_GRID;
        else if (!strcasecmp(v, "none") || !strcmp(v, "0")) out->flow = GMB_LAYOUT_FLOW_NONE;
    }
    if (layout_find_val(spec, "cols", v, sizeof v)) { any = 1; out->cols = atoi(v); if (out->cols < 0) out->cols = 0; }
    if (layout_find_val(spec, "gap", v, sizeof v))  { any = 1; out->gap  = atoi(v); if (out->gap  < 0) out->gap  = 0; }
    if (layout_find_val(spec, "pad", v, sizeof v))  { any = 1; out->pad  = atoi(v); if (out->pad  < 0) out->pad  = 0; }
    if (layout_find_val(spec, "wrap", v, sizeof v)) {
        any = 1;
        if (atoi(v) != 0 || !strcasecmp(v, "wrap")) out->flow = GMB_LAYOUT_FLOW_WRAP;
        else out->flow = GMB_LAYOUT_FLOW_NONE;
    }
    if (layout_find_val(spec, "grid", v, sizeof v)) {
        any = 1;
        out->flow = GMB_LAYOUT_FLOW_GRID;
        out->cols = atoi(v);
        if (out->cols < 0) out->cols = 0;
    }
    /* GRUPPO: il figlio dichiara a quale contenitore appartiene. La chiave è il
     * NOME del contenitore, quindi il contenitore deve avere un `name=`. */
    if (layout_find_val(spec, "group", v, sizeof v)) {
        any = 1;
        out->has_group = 1;
        snprintf(out->group, sizeof out->group, "%s", v);
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
        && ls->min_w < 0 && ls->min_h < 0 && ls->max_w < 0 && ls->max_h < 0 && ls->grow < 0
        && ls->flow < 0 && ls->cols < 0 && ls->gap < 0 && ls->pad < 0 && !ls->has_group)
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
        c->layout_flow      = GMB_LAYOUT_FLOW_NONE;
        c->layout_cols      = 0;
        c->layout_gap       = 0;
        c->layout_pad       = 0;
        c->layout_group[0]  = '\0';
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
    if (ls->flow    >= 0) c->layout_flow    = ls->flow;
    if (ls->cols    >= 0) c->layout_cols    = ls->cols;
    if (ls->gap     >= 0) c->layout_gap     = ls->gap;
    if (ls->pad     >= 0) c->layout_pad     = ls->pad;
    if (ls->has_group)    snprintf(c->layout_group, sizeof c->layout_group, "%s", ls->group);
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

/* ---------------------------------------------------------------------------
 * FLOW (WrapBox / Grid) — dispone i FIGLI di un contenitore.
 *
 * Il modello delta per-controllo non può sapere che i fratelli si spingono a
 * capo: serve una funzione che li guardi insieme. Qui è pura (nessun registro):
 * il chiamante le passa i controlli del gruppo e il rect del contenitore.
 *
 * WRAP: riga per riga, ognuno con la sua larghezza dichiarata; quando la riga è
 * piena si va a capo. L'altezza della riga è quella del figlio più alto.
 * GRID: `cols` colonne di celle uguali; righe = ceil(n/cols); ogni figlio riempie
 * la cella. In entrambi i casi valgono i limiti min/max del figlio.
 * ------------------------------------------------------------------------- */
static int flow_clamp(int v, int lo, int hi)
{
    if (v < 1) v = 1;
    if (hi > 0 && v > hi) v = hi;
    if (lo > 0 && v < lo) v = lo;
    if (v < 1) v = 1;
    return v;
}

int gmb_layout_flow_apply(struct gmb_control* const* kids, int n,
                          int area_x, int area_y, int area_w, int area_h,
                          int mode, int cols, int gap, int pad)
{
    if (!kids || n <= 0) return 0;
    if (mode != GMB_LAYOUT_FLOW_WRAP && mode != GMB_LAYOUT_FLOW_GRID) return 0;
    if (gap < 0) gap = 0;
    if (pad < 0) pad = 0;
    if (area_w < 1) area_w = 1;
    if (area_h < 1) area_h = 1;

    int ox = area_x + pad;
    int oy = area_y + pad;
    int inner_w = area_w - pad * 2;
    int inner_h = area_h - pad * 2;
    if (inner_w < 1) inner_w = 1;
    if (inner_h < 1) inner_h = 1;

    int moved = 0;

    if (mode == GMB_LAYOUT_FLOW_GRID) {
        if (cols < 1) cols = 1;
        int rows = (n + cols - 1) / cols;
        int cell_w = (inner_w - gap * (cols - 1)) / cols;
        int cell_h = (inner_h - gap * (rows - 1)) / rows;
        if (cell_w < 1) cell_w = 1;
        if (cell_h < 1) cell_h = 1;
        for (int i = 0; i < n; i++) {
            gmb_control_t* c = (gmb_control_t*)kids[i];
            if (!c) continue;
            int r = i / cols, col = i % cols;
            int x = ox + col * (cell_w + gap);
            int y = oy + r * (cell_h + gap);
            int w = flow_clamp(cell_w, c->layout_min_w, c->layout_max_w);
            int h = flow_clamp(cell_h, c->layout_min_h, c->layout_max_h);
            if (c->x != x || c->y != y || c->width != w || c->height != h) {
                c->x = x; c->y = y; c->width = w; c->height = h;
                moved++;
            }
        }
        return moved;
    }

    /* WRAP */
    int x = ox, y = oy, row_h = 0;
    int right = ox + inner_w;
    for (int i = 0; i < n; i++) {
        gmb_control_t* c = (gmb_control_t*)kids[i];
        if (!c) continue;
        int w = c->layout_on ? c->layout_base_w : c->width;
        int h = c->layout_on ? c->layout_base_h : c->height;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        w = flow_clamp(w, c->layout_min_w, c->layout_max_w);
        h = flow_clamp(h, c->layout_min_h, c->layout_max_h);
        if (w > inner_w) w = inner_w;      /* un figlio più largo del contenitore non esce */
        /* Va a capo se non è il primo della riga e non ci sta più. */
        if (x > ox && x + w > right) { x = ox; y += row_h + gap; row_h = 0; }
        if (c->x != x || c->y != y || c->width != w || c->height != h) {
            c->x = x; c->y = y; c->width = w; c->height = h;
            moved++;
        }
        x += w + gap;
        if (h > row_h) row_h = h;
    }
    return moved;
}
