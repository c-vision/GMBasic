/* ============================================================================
 * gmb_layout_flow_selfcheck.c — WrapBox / Grid del layout manager.
 *
 * Il solver `gmb_layout_flow_apply` è puro: prende i controlli del gruppo e il
 * rect del contenitore e scrive i loro rect. Qui si verificano le due modalità
 * con numeri esatti (celle), i limiti min/max e il caso "figlio più largo del
 * contenitore". Esce 1 se una posizione non è quella attesa.
 * ============================================================================ */
#include "gmb_layout.h"
#include "gmb_tui_widgets.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static void init_box(gmb_control_t* c, const char* name, int w, int h, int minw, int maxw)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->name, sizeof c->name, "%s", name);
    c->layout_on = 1;
    c->layout_base_w = w;
    c->layout_base_h = h;
    c->width = w;
    c->height = h;
    c->layout_min_w = minw;
    c->layout_max_w = maxw;
}

static void rect_is(const gmb_control_t* c, int x, int y, int w, int h, const char* msg)
{
    if (c->x != x || c->y != y || c->width != w || c->height != h) {
        printf("FAIL %s: atteso %d,%d %dx%d, ottenuto %d,%d %dx%d\n",
               msg, x, y, w, h, c->x, c->y, c->width, c->height);
        g_fails++;
    }
}

int main(void)
{
    /* ---- GRID: area 56x16 a (4,2), pad=1, gap=1, cols=3, 4 figli 8x2 ----
     * inner 54x14, cell_w=(54-2)/3=17, righe=2, cell_h=(14-1)/2=6
     * (5,3) (23,3) (41,3) (5,10), tutti 17x6. */
    {
        gmb_control_t a, b, c, d, *kids[4];
        init_box(&a, "a", 8, 2, 0, 0);
        init_box(&b, "b", 8, 2, 0, 0);
        init_box(&c, "c", 8, 2, 0, 0);
        init_box(&d, "d", 8, 2, 0, 0);
        kids[0] = &a; kids[1] = &b; kids[2] = &c; kids[3] = &d;
        int moved = gmb_layout_flow_apply((struct gmb_control* const*)kids, 4,
                                          4, 2, 56, 16, GMB_LAYOUT_FLOW_GRID, 3, 1, 1);
        check(moved == 4, "grid: quattro rect mossi");
        rect_is(&a,  5,  3, 17, 6, "grid a");
        rect_is(&b, 23,  3, 17, 6, "grid b");
        rect_is(&c, 41,  3, 17, 6, "grid c");
        rect_is(&d,  5, 10, 17, 6, "grid d");
    }

    /* ---- GRID: maxw=10 vince sulla cella (17) ---- */
    {
        gmb_control_t a, *kids[1];
        init_box(&a, "a", 8, 2, 0, 10);
        kids[0] = &a;
        gmb_layout_flow_apply((struct gmb_control* const*)kids, 1,
                              0, 0, 56, 10, GMB_LAYOUT_FLOW_GRID, 2, 0, 0);
        rect_is(&a, 0, 0, 10, 10, "grid maxw");
    }

    /* ---- WRAP: area 56x10 a (4,24), pad=0, gap=1, tre figli 20x2 ----
     * 4, 25, poi 46+20=66 > 60 -> a capo a (4,27). */
    {
        gmb_control_t a, b, c, *kids[3];
        init_box(&a, "a", 20, 2, 0, 0);
        init_box(&b, "b", 20, 2, 0, 0);
        init_box(&c, "c", 20, 2, 0, 0);
        kids[0] = &a; kids[1] = &b; kids[2] = &c;
        gmb_layout_flow_apply((struct gmb_control* const*)kids, 3,
                              4, 24, 56, 10, GMB_LAYOUT_FLOW_WRAP, 0, 1, 0);
        rect_is(&a, 4, 24, 20, 2, "wrap a");
        rect_is(&b, 25, 24, 20, 2, "wrap b");
        rect_is(&c, 4, 27, 20, 2, "wrap c (a capo)");
    }

    /* ---- WRAP: figlio più largo del contenitore -> clampi alla riga ---- */
    {
        gmb_control_t a, *kids[1];
        init_box(&a, "a", 200, 3, 0, 0);
        kids[0] = &a;
        gmb_layout_flow_apply((struct gmb_control* const*)kids, 1,
                              0, 0, 40, 10, GMB_LAYOUT_FLOW_WRAP, 0, 0, 0);
        rect_is(&a, 0, 0, 40, 3, "wrap clamp larghezza");
    }

    /* ---- minh: la riga usa l'altezza minima dichiarata ---- */
    {
        gmb_control_t a, b, *kids[2];
        init_box(&a, "a", 10, 2, 0, 0);
        init_box(&b, "b", 10, 2, 0, 0);
        b.layout_min_h = 5;
        kids[0] = &a; kids[1] = &b;
        gmb_layout_flow_apply((struct gmb_control* const*)kids, 2,
                              0, 0, 30, 20, GMB_LAYOUT_FLOW_WRAP, 0, 1, 0);
        rect_is(&b, 11, 0, 10, 5, "wrap minh");
    }

    /* ---- modalità sconosciuta: nessun movimento ---- */
    {
        gmb_control_t a, *kids[1];
        init_box(&a, "a", 10, 2, 0, 0);
        kids[0] = &a;
        check(gmb_layout_flow_apply((struct gmb_control* const*)kids, 1,
                                    0, 0, 40, 10, GMB_LAYOUT_FLOW_NONE, 0, 0, 0) == 0,
              "flow=none non muove nulla");
    }

    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_layout_flow_selfcheck: OK\n");
    return 0;
}
