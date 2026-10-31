/* Self-test headless del mouse TUI (SdlConsole): esercita close/zoom/minimize/chip/
 * drag/raise/resize via gmb_winstmt_handle_mouse_px e VERIFICA lo stato. */
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_widgets.h"
#include "gmb_gfx_input.h"
#include "gmb_sdl_console.h"
#include "gmb_screen_config.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <string.h>

static int CW = 8, CH = 17, GW = 100, GH = 40;
static int fails = 0;
static void chk(const char* what, int ok) { fprintf(stderr, "%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }

static gmb_managed_window_t* W(int id) { return gmb_window_manager_get(gmb_window_manager_static(), id); }
static void click(int cx, int cy) {
    int lx = cx * CW, ly = cy * CH;
    gmb_winstmt_handle_mouse_px(lx, ly, GMB_GFX_MOUSE_DOWN, 1, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px(lx, ly, GMB_GFX_MOUSE_UP, 1, CW, CH, GW * CW, GH * CH);
}
static void setup(void) {
    gmb_window_manager_init(gmb_window_manager_static(), gmb_rect_new(0, 0, GW, GH));
    gmb_winstmt_reset();
    gmb_winstmt_define("Editor", "Turbo Editor", 2, 2, 15, 44, GMB_WINSTMT_SHADOW);
    gmb_winstmt_content("Editor", "hello");
    gmb_winstmt_define("Info", "Stato", 9, 30, 20, 74, GMB_WINSTMT_DOUBLE | GMB_WINSTMT_SHADOW);
    gmb_winstmt_content("Info", "info");
    gmb_winstmt_select("Editor");
}

int main(void)
{
    /* 1) CLOSE (pulsante [■] a SINISTRA del titolo, celle 2..4) */
    setup();
    click(5, 2);
    chk("close Editor (winstmt chiusa -> count==1)", gmb_winstmt_count() == 1);
    chk("close: Info ancora presente", W(2) != NULL);

    /* 2) ZOOM maximize + restore (pulsante a DESTRA, celle right-3..right-1) */
    setup();
    click(42, 2);                      /* Editor right=44 -> zoom circa cella 42 */
    gmb_managed_window_t* e = W(1);
    chk("zoom maximize (rect = tutta griglia)", e && e->rect.x == 0 && e->rect.y == 0 && e->rect.w == GW && e->rect.h == GH);
    click(98, 0);                      /* massimizzata: pulsante zoom a cella 97..99 */
    e = W(1);
    chk("zoom restore (rect = size precedente)", e && e->rect.x == 2 && e->rect.y == 2 && e->rect.w == 43 && e->rect.h == 14);

    /* 3) MINIMIZE (pulsante [_], celle right-6..right-4) */
    setup();
    click(39, 2);
    e = W(1);
    chk("minimize (Editor.minimized)", e && e->minimized == 1);
    gmb_winstmt_restore_by_id(1);
    e = W(1);
    chk("chip restore (Editor non piu' minimizzata)", e && e->minimized == 0);

    /* 4) RAISE: click su Info (non coperta) la porta sopra Editor */
    setup();
    int z0 = W(2)->z;
    click(50, 10);
    chk("raise Info (z maggiore e focused)", W(2)->z > W(1)->z && W(2)->focused);

    /* 5) DRAG: trascina Editor per il titolo */
    setup();
    e = W(1);
    int ox = e->rect.x, oy = e->rect.y;
    gmb_winstmt_handle_mouse_px(10 * CW, 2 * CH, GMB_GFX_MOUSE_DOWN, 1, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px(24 * CW, 8 * CH, GMB_GFX_MOUSE_MOVE, 1, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px(24 * CW, 8 * CH, GMB_GFX_MOUSE_UP,   1, CW, CH, GW * CW, GH * CH);
    e = W(1);
    chk("drag (Editor si e' spostata)", e && (e->rect.x != ox || e->rect.y != oy));

    /* 6) RESIZE dal bordo (SE) */
    setup();
    e = W(1);
    int ow = e->rect.w, oh = e->rect.h;
    /* bordo destro-inferiore Editor: x ~ (2+43)*8, y ~ (2+14)*17 */
    gmb_winstmt_handle_mouse_px((44) * CW, (15) * CH, GMB_GFX_MOUSE_DOWN, 1, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px((55) * CW, (24) * CH, GMB_GFX_MOUSE_MOVE, 1, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px((55) * CW, (24) * CH, GMB_GFX_MOUSE_UP,   1, CW, CH, GW * CW, GH * CH);
    e = W(1);
    chk("resize (w o h aumentati)", e && (e->rect.w > ow || e->rect.h > oh));

    /* 7) GHOST: dopo CLOSE col mouse, la finestra NON deve restare nel WindowManager
     * (altrimenti il suo rect assorbe i click e le finestre sotto non ricevono piu' eventi). */
    setup();
    click(5, 2);                                     /* chiude Editor (celle 4..6) */
    chk("ghost: Editor rimossa dal WindowManager", W(1) == NULL);
    click(35, 12);                                   /* punto gia' coperto da Editor */
    chk("ghost: click su Info (sotto) riceve l'evento", gmb_window_manager_focused(gmb_window_manager_static()) == 2);

    /* 8) STATO ORFANO: drag avviato su Editor, poi Editor CHIUSA mentre il drag e' pendente;
     * i MOVE successivi non devono restare assorbiti e Info deve tornare a ricevere il drag. */
    setup();
    gmb_winstmt_handle_mouse_px(10 * CW, 2 * CH, GMB_GFX_MOUSE_DOWN, 1, CW, CH, GW * CW, GH * CH); /* drag Editor */
    gmb_winstmt_action("Editor", "Close");                                                        /* orfano */
    gmb_winstmt_handle_mouse_px(40 * CW, 8 * CH, GMB_GFX_MOUSE_MOVE, 0, CW, CH, GW * CW, GH * CH);/* MOVE senza bottone */
    gmb_managed_window_t* i2 = W(2);
    int ix = i2->rect.x, iy = i2->rect.y;
    gmb_winstmt_handle_mouse_px(40 * CW, 9 * CH, GMB_GFX_MOUSE_DOWN, 1, CW, CH, GW * CW, GH * CH); /* drag Info */
    gmb_winstmt_handle_mouse_px(48 * CW, 13 * CH, GMB_GFX_MOUSE_MOVE, 0, CW, CH, GW * CW, GH * CH);
    gmb_winstmt_handle_mouse_px(48 * CW, 13 * CH, GMB_GFX_MOUSE_UP, 0, CW, CH, GW * CW, GH * CH);
    i2 = W(2);
    chk("orphan: drag su Info dopo chiusura di Editor funziona", i2 && (i2->rect.x != ix || i2->rect.y != iy));

    fprintf(stderr, "\n%s (fails=%d)\n", fails ? "TEST FALLITI" : "TUTTI I TEST PASSANO", fails);
    return fails ? 1 : 0;
}
