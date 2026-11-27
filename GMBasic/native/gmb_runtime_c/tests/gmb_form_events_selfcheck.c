/* gmb_form_events_selfcheck.c — eventi Control base (VB6-like), headless.
 * Simula il mouse e verifica CLICK/DBLCLICK/CHANGE/UPCLICK/DOWNCLICK/SCROLL/
 * DROPDOWN/SELCHANGE/NODECLICK/ROWCOLCHANGE (deterministico, nessuna finestra). */
#include "gmb_form.h"
#include "gmb_gfx_input.h"
#include <stdio.h>

static int g_click = 0, g_dbl = 0, g_change = 0, g_gridch = 0, g_sel = -1;
static int g_up = 0, g_dn = 0, g_scroll = 0, g_dd = 0, g_selch = 0, g_node = 0, g_rowcol = 0;
static int g_kd = 0, g_kp = 0, g_ku = 0, g_val = 0, g_dover = 0, g_ddrop = 0, g_exp = 0, g_col = 0;
static int g_foc_kd = 0;
static gmb_double_t cb_foc_kd(void) { g_foc_kd++; return 0; }
static int g_btn_cb = 0;
static gmb_double_t cb_btn(void) { g_btn_cb++; return 0; }
static gmb_double_t cb_click(void) { g_click++; return 0; }
static gmb_double_t cb_dbl(void) { g_dbl++; return 0; }
static gmb_double_t cb_change(void) { g_change++; return 0; }
static gmb_double_t cb_gridch(void) { g_gridch++; return 0; }
static gmb_double_t cb_up(void) { g_up++; return 0; }
static gmb_double_t cb_dn(void) { g_dn++; return 0; }
static gmb_double_t cb_scroll(void) { g_scroll++; return 0; }
static gmb_double_t cb_dd(void) { g_dd++; return 0; }
static gmb_double_t cb_selch(void) { g_selch++; return 0; }
static gmb_double_t cb_node(void) { g_node++; return 0; }
static gmb_double_t cb_rowcol(void) { g_rowcol++; return 0; }
static gmb_double_t cb_kd(void) { g_kd++; return 0; }
static gmb_double_t cb_kp(void) { g_kp++; return 0; }
static gmb_double_t cb_ku(void) { g_ku++; return 0; }
static gmb_double_t cb_val(void) { g_val++; return 0; }
static gmb_double_t cb_dover(void) { g_dover++; return 0; }
static gmb_double_t cb_ddrop(void) { g_ddrop++; return 0; }
static gmb_double_t cb_exp(void) { g_exp++; return 0; }
static gmb_double_t cb_col(void) { g_col++; return 0; }
static int g_fire_ctl = -100, g_fire_ev = 0;
static gmb_double_t cb_fire(void) { g_fire_ctl = gmb_form_firing_control(); g_fire_ev = gmb_form_firing_event(); return 0; }

static void down(int x, int y) { gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, x, y, 1, 0); gmb_form_poll(); }
static void up(int x, int y) { gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_UP, x, y, 0, 0); gmb_form_poll(); }
static void click(int x, int y) { down(x, y); up(x, y); }
static void move(int x, int y) { gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_MOVE, x, y, 0, 0); gmb_form_poll(); }

#define FAIL(...) do { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)

int main(void)
{
    /* CHECK: CLICK + DBLCLICK + CHANGE */
    gmb_form_reset();
    double chk = 0;
    gmb_form_add_checkbox(10, 10, "ok", &chk);
    gmb_form_set_event(GMB_EV_CLICK, cb_click);
    gmb_form_set_event(GMB_EV_DBLCLICK, cb_dbl);
    gmb_form_set_event(GMB_EV_CHANGE, cb_change);
    click(20, 20);
    if (g_click != 1 || g_change != 1 || chk != 1) FAIL("check click=%d change=%d chk=%.0f", g_click, g_change, chk);
    click(20, 20);
    if (g_dbl != 1) FAIL("dbl=%d", g_dbl);

    /* TOUCH: un tap (TOUCH_DOWN + TOUCH_UP) e' un click completo sul controllo,
     * senza long-press. Un tap su una checkbox cambia lo stato. */
    gmb_form_reset();
    double tchk = 0;
    gmb_form_add_checkbox(10, 10, "ok", &tchk);
    gmb_form_set_event(GMB_EV_CLICK, cb_click);
    gmb_form_set_event(GMB_EV_CHANGE, cb_change);
    g_click = 0; g_change = 0;
    gmb_gfx_input_push_touch(1, 20, 20, 0); gmb_form_poll();
    gmb_gfx_input_push_touch(0, 20, 20, 100); gmb_form_poll();
    if (g_click != 1 || g_change != 1 || tchk != 1)
        FAIL("touch tap click=%d change=%d chk=%.0f", g_click, g_change, tchk);

    /* TOUCH + long-press: TOUCH_DOWN, LONGPRESS, TOUCH_UP. Il controllo deve
     * ricevere UN solo click (il LONGPRESS non e' una seconda pressione). */
    gmb_form_reset();
    double tchk2 = 0;
    gmb_form_add_checkbox(10, 10, "ok", &tchk2);
    gmb_form_set_event(GMB_EV_CLICK, cb_click);
    gmb_form_set_event(GMB_EV_CHANGE, cb_change);
    g_click = 0; g_change = 0;
    gmb_gfx_input_push_touch(1, 20, 20, 0); gmb_form_poll();
    gmb_gfx_input_pump_touch(GMB_GFX_TOUCH_HOLD_MS + 1);   /* emette LONGPRESS + UP */
    gmb_form_poll();   /* LONGPRESS (assorbito) */
    gmb_form_poll();   /* TOUCH_UP -> click */
    if (g_click != 1 || g_change != 1)
        FAIL("touch longpress click=%d change=%d (attesi 1/1)", g_click, g_change);

    /* TOUCH sulla corsia TUI (i bottoni in headless vivono nel registry TUI e sono
     * guidati dal frame loop, non da gmb_form_poll: qui si verifica il percorso
     * tocco->pressione->rilascio, che e' cio' che i controlli consumano). */
    gmb_form_reset();
    gmb_form_add_button(10, 10, 100, 30, "OK", NULL);
    gmb_gfx_input_push_touch(1, 30, 20, 0); gmb_form_poll();
    if (gmb_gfx_input_mouse_down() != 1) FAIL("touch button: pressione attiva");
    gmb_gfx_input_push_touch(0, 30, 20, 100); gmb_form_poll();
    if (gmb_gfx_input_mouse_down() != 0) FAIL("touch button: pressione rilasciata");

    /* SPINNER: UPCLICK/DOWNCLICK/CHANGE */
    gmb_form_reset();
    double sv = 10;
    gmb_form_add_spinner(10, 10, 150, 0, 100, 1, &sv);
    gmb_form_set_event(GMB_EV_UPCLICK, cb_up);
    gmb_form_set_event(GMB_EV_DOWNCLICK, cb_dn);
    gmb_form_set_event(GMB_EV_CHANGE, cb_change);
    g_change = 0;
    click(10 + 150 - 5, 20);          /* + */
    click(10 + 150 - 5 - 26, 20);     /* - */
    if (g_up != 1 || g_dn != 1 || g_change != 2 || sv != 10) FAIL("spin up=%d dn=%d ch=%d v=%.0f", g_up, g_dn, g_change, sv);

    /* SLIDER: SCROLL a trascinamento */
    gmb_form_reset();
    double sl = 50;
    gmb_form_add_track(10, 10, 200, 0, 100, &sl);
    gmb_form_set_event(GMB_EV_SCROLL, cb_scroll);
    down(60, 22); move(150, 22); up(150, 22);
    if (g_scroll < 1) FAIL("scroll=%d", g_scroll);

    /* DROPDOWN: DROPDOWN all'apertura */
    gmb_form_reset();
    double dd = 0;
    gmb_form_add_dropdown(10, 10, 150, "A|B|C", &dd);
    gmb_form_set_event(GMB_EV_DROPDOWN, cb_dd);
    click(20, 20);
    if (g_dd != 1) FAIL("dropdown=%d", g_dd);

    /* LISTBOX: SELCHANGE */
    gmb_form_reset();
    double lb = -1;
    gmb_form_add_listbox(10, 10, 150, 100, "x|y|z", &lb);
    gmb_form_set_event(GMB_EV_SELCHANGE, cb_selch);
    down(20, 30); up(20, 30);
    if (g_selch < 1) FAIL("listbox selchange=%d", g_selch);

    /* TREE: NODECLICK */
    gmb_form_reset();
    double tr = -1;
    gmb_form_add_tree(10, 10, 200, 120, "a/b|c/d", &tr);
    gmb_form_set_event(GMB_EV_NODECLICK, cb_node);
    down(20, 30); up(20, 30);
    if (g_node < 1) FAIL("tree node=%d", g_node);

    /* GRID: CHANGE + ROWCOLCHANGE + SELCHANGE */
    gmb_form_reset();
    gmb_form_add_grid(10, 10, 200, 100, "A|B", "r0a|r0b;r1a|r1b;r2a|r2b", &g_sel);
    gmb_form_set_event(GMB_EV_CHANGE, cb_gridch);
    gmb_form_set_event(GMB_EV_ROWCOLCHANGE, cb_rowcol);
    gmb_form_set_event(GMB_EV_SELCHANGE, cb_selch);
    click(20, 60);
    if (g_gridch != 1 || g_rowcol != 1) FAIL("grid ch=%d rowcol=%d", g_gridch, g_rowcol);

    /* KEYDOWN/KEYPRESS/KEYUP sul controllo focalizzato */
    gmb_form_reset();
    double kb = 0;
    gmb_form_add_checkbox(10, 10, "ok", &kb);
    gmb_form_set_event(GMB_EV_KEYDOWN, cb_kd);
    gmb_form_set_event(GMB_EV_KEYPRESS, cb_kp);
    gmb_form_set_event(GMB_EV_KEYUP, cb_ku);
    click(20, 20);                        /* focus */
    gmb_gfx_input_push_key("a"); gmb_form_poll();
    if (g_kd != 1 || g_kp != 1 || g_ku != 1) FAIL("key kd=%d kp=%d ku=%d", g_kd, g_kp, g_ku);

    /* VALIDATE su perdita di focus */
    gmb_form_reset();
    double v1 = 0, v2 = 0;
    gmb_form_add_checkbox(10, 10, "a", &v1);
    gmb_form_set_event(GMB_EV_VALIDATE, cb_val);
    gmb_form_add_checkbox(10, 40, "b", &v2);
    click(20, 20);          /* focus A */
    click(20, 50);          /* focus B -> VALIDATE su A */
    if (g_val != 1) FAIL("validate=%d", g_val);

    /* DRAGOVER / DRAGDROP: down su A, move su B, up su B */
    gmb_form_reset();
    double d1 = 0, d2 = 0;
    gmb_form_add_checkbox(10, 10, "a", &d1);
    gmb_form_add_checkbox(10, 40, "b", &d2);
    gmb_form_set_event(GMB_EV_DRAGOVER, cb_dover);
    gmb_form_set_event(GMB_EV_DRAGDROP, cb_ddrop);
    down(20, 20); move(20, 50); up(20, 50);
    if (g_dover < 1 || g_ddrop != 1) FAIL("dragover=%d dragdrop=%d", g_dover, g_ddrop);

    /* EXPAND / COLLAPSE (tree con gerarchia) */
    gmb_form_reset();
    double tr2 = -1;
    gmb_form_add_tree(10, 10, 200, 140, "a|a/b|a/c|d", &tr2);
    gmb_form_set_event(GMB_EV_COLLAPSE, cb_col);
    gmb_form_set_event(GMB_EV_EXPAND, cb_exp);
    down(16, 20); up(16, 20);      /* marker del ramo "a" -> collapse */
    if (g_col != 1) FAIL("collapse=%d", g_col);
    down(16, 20); up(16, 20);      /* -> expand */
    if (g_exp != 1) FAIL("expand=%d", g_exp);

    /* PROPRIETA base (nella spec di STYLE) + SetFocus */
    gmb_form_reset();
    double pv = 0;
    int ci = gmb_form_add_checkbox(10, 10, "x", &pv);
    gmb_form_set_last_style("name=\"chk1\" tooltip=\"tip qui\" tag=\"g\" visible=0 enabled=0 tabindex=5 tabstop=0");
    if (strcmp(gmb_form_widget_name(ci), "chk1") || strcmp(gmb_form_widget_tooltip(ci), "tip qui")
        || strcmp(gmb_form_widget_tag(ci), "g") || gmb_form_widget_visible(ci) != 0
        || gmb_form_widget_enabled(ci) != 0 || gmb_form_widget_tabindex(ci) != 5 || gmb_form_widget_tabstop(ci) != 0)
        FAIL("props name=%s vis=%d tabidx=%d", gmb_form_widget_name(ci), gmb_form_widget_visible(ci), gmb_form_widget_tabindex(ci));

    /* SetFocus via spec: i tasti vanno al controllo focalizzato */
    gmb_form_reset();
    double fv = 0;
    gmb_form_add_checkbox(10, 10, "y", &fv);
    gmb_form_set_event(GMB_EV_KEYDOWN, cb_foc_kd);
    gmb_form_set_last_style("focus");
    gmb_gfx_input_push_key("z"); gmb_form_poll();
    if (g_foc_kd != 1) FAIL("setfocus kd=%d", g_foc_kd);

    /* TEXTBOX: editing con caret (cursore comune della base) */
    gmb_form_reset();
    gmb_string_t txt = gmb_string_alloc("");
    gmb_form_add_textbox(10, 10, 220, 26, &txt);
    click(30, 20);                        /* focus + posizione caret */
    gmb_gfx_input_push_key("a"); gmb_gfx_input_push_key("b"); gmb_gfx_input_push_key("c"); gmb_form_poll();
    if (!txt.data || strcmp(txt.data, "abc")) FAIL("textbox=%s", txt.data ? txt.data : "(null)");
    gmb_gfx_input_push_key("\b"); gmb_form_poll();       /* backspace */
    if (strcmp(txt.data, "ab")) FAIL("textbox bs=%s", txt.data);

    /* BUTTON: proprietà base (Control) + visibilità */
    gmb_form_reset();
    int bi = gmb_form_add_button_css(10, 10, 100, 30, "Ok", cb_btn, "name=\"btnOk\" tooltip=\"premi\" visible=0 enabled=0 tabindex=2 tabstop=0");
    if (strcmp(gmb_form_button_name(bi), "btnOk") || strcmp(gmb_form_button_tooltip(bi), "premi")
        || gmb_form_button_visible(bi) != 0 || gmb_form_button_enabled(bi) != 0 || gmb_form_button_tabindex(bi) != 2)
        FAIL("button props name=%s vis=%d", gmb_form_button_name(bi), gmb_form_button_visible(bi));
    click(20, 20);   /* bottone invisibile: NON scatta */
    if (g_btn_cb != 0) FAIL("button hidden fired=%d", g_btn_cb);

    /* Metodi a runtime per nome: CONTROL SET/GET/GETS/MOVE/FOCUS/ZORDER */
    gmb_form_reset();
    double mv = 0;
    gmb_form_add_checkbox(10, 10, "A", &mv);
    gmb_form_set_last_style("name=\"chk1\" visible=1");
    gmb_form_add_button_css(10, 60, 120, 36, "Ok", cb_btn, "name=\"btn1\"");
    gmb_form_control_set("chk1", "visible", 0, "");
    if (gmb_form_control_get("chk1", "visible") != 0) FAIL("ctrl set/get visible");
    gmb_form_control_move("btn1", 200, 100, 140, 40);
    if (gmb_form_control_get("btn1", "left") != 200 || gmb_form_control_get("btn1", "width") != 140) FAIL("ctrl move");
    gmb_form_control_set("btn1", "caption", 0, "Nuovo");
    if (strcmp(gmb_form_control_get_str("btn1", "caption"), "Nuovo")) FAIL("ctrl caption=%s", gmb_form_control_get_str("btn1", "caption"));
    gmb_form_control_focus("chk1");
    gmb_form_set_event(GMB_EV_KEYDOWN, cb_foc_kd);
    g_foc_kd = 0; gmb_gfx_input_push_key("q"); gmb_form_poll();
    if (g_foc_kd != 1) FAIL("ctrl focus kd=%d", g_foc_kd);
    gmb_form_control_zorder("btn1", 1);   /* nessun crash, nome ancora trovato */
    if (gmb_form_control_find("btn1") < 1000) FAIL("ctrl zorder find");

    /* NAME-BASED (var NULL) + PULL-EVENT (gambe gestite) */
    gmb_form_reset();
    gmb_form_add_checkbox(10, 10, "z", NULL);      /* nessun INTO */
    gmb_form_set_last_style("name=\"nchk\"");
    gmb_form_control_set("nchk", "value", 1, "");
    if (gmb_form_control_get("nchk", "value") != 1) FAIL("name-based value=%g", gmb_form_control_get("nchk", "value"));
    /* textbox senza strptr */
    gmb_form_add_textbox(10, 40, 200, 28, NULL);
    gmb_form_set_last_style("name=\"ntxt\"");
    gmb_form_control_set("ntxt", "text", 0, "ciao");
    if (strcmp(gmb_form_control_get_str("ntxt", "text"), "ciao")) FAIL("name-based str=%s", gmb_form_control_get_str("ntxt", "text"));
    /* pull-event: click sul checkbox -> CHANGE in coda */
    { int pe = 0, pw = -1, saw = 0, any = 0; click(20, 20);
      while (gmb_form_poll_event(&pe, &pw)) { any = 1; if (pe == GMB_EV_CHANGE) saw = 1; }
      if (!any) FAIL("poll_event vuoto");
      if (!saw) FAIL("poll_event senza CHANGE"); }

    /* POLLING DI STATO (NON consuma): CLICKED/CHANGED per handle + globali. */
    gmb_form_reset();
    double pv2 = 0;
    int ph = gmb_form_add_checkbox(10, 10, "ok", &pv2);
    if (gmb_form_clicked(ph) != 0) FAIL("poll clicked init=%g", gmb_form_clicked(ph));
    click(20, 20);
    if (gmb_form_clicked(ph) != 1 && gmb_form_changed(ph) != 1) FAIL("poll click/change=%.0f/%.0f", gmb_form_clicked(ph), gmb_form_changed(ph));
    if (gmb_mouse_clicked() != 1) FAIL("poll mouse_clicked=%g", gmb_mouse_clicked());
    if (gmb_mouse_pressed() != 0) FAIL("poll pressed=%g", gmb_mouse_pressed());
    if (gmb_form_dblclicked(ph) != 0) FAIL("poll dbl init=%g", gmb_form_dblclicked(ph));
    /* bottone: handle = 1000+indice */
    gmb_form_reset();
    int pb = gmb_form_add_button(10, 10, 100, 30, "Ok", cb_btn);
    if (gmb_form_clicked(1000 + pb) != 0) FAIL("poll btn init=%g", gmb_form_clicked(1000 + pb));
    click(20, 20);
    if (gmb_form_clicked(1000 + pb) != 1) FAIL("poll btn clicked=%g", gmb_form_clicked(1000 + pb));
    if (gmb_mouse_clicked() != 1) FAIL("poll btn mouse_clicked=%g", gmb_mouse_clicked());
    /* KEYPRESSED globale */
    gmb_form_reset();
    double kk = 0;
    gmb_form_add_checkbox(10, 10, "k", &kk);
    click(20, 20);                        /* focus */
    gmb_gfx_input_push_key("a"); gmb_form_poll();
    if (gmb_key_pressed() != 1) FAIL("poll key_pressed=%g", gmb_key_pressed());

    /* gmb_form_fire_event + contesto "evento in corso" (usato dal thunk managed). */
    gmb_form_reset();
    { double fv2 = 0; int fh = gmb_form_add_checkbox(10, 10, "f", &fv2);
      gmb_form_set_event(GMB_EV_CHANGE, cb_fire);
      if (gmb_form_firing_control() != -1 || gmb_form_firing_event() != 0) FAIL("firing ctx init");
      gmb_form_fire_event(fh, GMB_EV_CHANGE);
      if (g_fire_ctl != fh || g_fire_ev != GMB_EV_CHANGE) FAIL("fire ctx %d/%d", g_fire_ctl, g_fire_ev);
      if (gmb_form_changed(fh) != 1) FAIL("fire changed=%g", gmb_form_changed(fh));
      if (gmb_form_firing_control() != -1) FAIL("firing ctx reset"); }
    /* bottone: handle 1000+i via gmb_form_set_event_id + fire */
    gmb_form_reset();
    { int fb = gmb_form_add_button(10, 10, 100, 30, "B", NULL);
      gmb_form_set_event_id(1000 + fb, GMB_EV_CLICK, cb_fire);
      g_fire_ctl = -100; g_fire_ev = 0;
      gmb_form_fire_event(1000 + fb, GMB_EV_CLICK);
      if (g_fire_ctl != 1000 + fb || g_fire_ev != GMB_EV_CLICK) FAIL("fire btn ctx %d/%d", g_fire_ctl, g_fire_ev);
      if (gmb_form_clicked(1000 + fb) != 1) FAIL("fire btn clicked=%g", gmb_form_clicked(1000 + fb)); }

    printf("FORM-EVENTS-OK check=%d dbl=%d spin=%d scroll=%d dd=%d listbox=%d tree=%d grid=%d sel=%.0f\n",
           g_click, g_dbl, g_up + g_dn, g_scroll, g_dd, g_selch, g_node, g_gridch, g_sel);
    return 0;
}
