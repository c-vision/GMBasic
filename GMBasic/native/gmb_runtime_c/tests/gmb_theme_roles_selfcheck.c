/* ============================================================================
 * gmb_theme_roles_selfcheck.c — verifica del modulo dei RUOLI SEMANTICI del tema
 * (gmb_theme_roles.h) e delle nuove chiavi del LAYOUT MANAGER
 * (STYLE "minw=… maxw=… grow=…"), senza interfaccia grafica.
 *
 * È il test dell'UNITÀ toccata: pura aritmetica sui colori e sui rect, nessun
 * SDL, nessun backend. Esce 1 se una verifica fallisce (gate per CI).
 * ============================================================================ */
#include "gmb_theme_roles.h"
#include "gmb_layout.h"
#include "gmb_a11y.h"       /* flag globali di accessibilità */
#include "gmb_runtime.h"    /* gmb_opt_set: registro opzioni (sorgente dei flag) */

#include <math.h>
#include <stdio.h>
#include <string.h>
static int g_fails = 0;

static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static gmb_control_t ctrl(int x, int y, int w, int h)
{
    gmb_control_t c;
    memset(&c, 0, sizeof c);
    c.x = x; c.y = y; c.width = w; c.height = h;
    return c;
}

/* ---- contrasto WCAG ---- */

static void test_contrast(void)
{
    gmb_cell_color_t white = crgb(255, 255, 255);
    gmb_cell_color_t black = crgb(0, 0, 0);
    gmb_cell_color_t grey  = crgb(119, 119, 119);

    check(fabs(gmb_theme_luminance(white) - 1.0) < 0.001, "luminanza del bianco = 1");
    check(fabs(gmb_theme_luminance(black)) < 0.001, "luminanza del nero = 0");
    check(fabs(gmb_theme_contrast_ratio(white, black) - 21.0) < 0.05, "bianco/nero = 21:1");
    check(fabs(gmb_theme_contrast_ratio(white, white) - 1.0) < 0.001, "stesso colore = 1:1");
    check(gmb_theme_contrast_ratio(white, black) == gmb_theme_contrast_ratio(black, white),
          "il rapporto è simmetrico");

    /* text_on sceglie sempre il polo leggibile */
    check(gmb_theme_contrast_ratio(gmb_theme_text_on(grey), grey) >= 4.0,
          "text_on(grigio) resta leggibile");

    /* ensure_contrast: già sufficiente -> colore INVARIATO (nessuna deriva) */
    check(gmb_theme_contrast_ratio(gmb_theme_ensure_contrast(white, black, 4.5), black) >= 4.5,
          "ensure_contrast su coppia già leggibile");
    gmb_cell_color_t fixed = gmb_theme_ensure_contrast(white, black, 4.5);
    check(fixed.r == 255 && fixed.g == 255 && fixed.b == 255, "ensure_contrast non tocca chi passa");

    /* coppia illeggibile -> viene corretta, in modo DETERMINISTICO */
    gmb_cell_color_t a = gmb_theme_ensure_contrast(crgb(80, 80, 80), crgb(70, 70, 70), 4.5);
    gmb_cell_color_t b = gmb_theme_ensure_contrast(crgb(80, 80, 80), crgb(70, 70, 70), 4.5);
    check(gmb_theme_contrast_ratio(a, crgb(70, 70, 70)) >= 4.5, "ensure_contrast corregge il contrasto");
    check(a.r == b.r && a.g == b.g && a.b == b.b, "ensure_contrast è deterministico");
}

/* ---- ruoli su TUTTI i preset ---- */

static const char* g_themes[] = {
    "turbovision", "clipper", "qbasic", "midnight", "nord", "light", "dracula",
    "monokai", "solarized", "retrogreen", "amber", "vb6", "soft", "neon", "glass"
};

static void test_roles(void)
{
    /* nomi: alias accettati e nomi canonici */
    check(gmb_theme_role_parse("danger") == GMB_ROLE_DANGER, "role=danger");
    check(gmb_theme_role_parse("ERROR") == GMB_ROLE_DANGER, "role=error (alias, case-insensitive)");
    check(gmb_theme_role_parse("sconosciuto") < 0, "ruolo sconosciuto = -1");
    check(gmb_theme_role_parse("muted") == GMB_ROLE_MUTED, "role=muted");
    check(!strcmp(gmb_theme_role_name(GMB_ROLE_DANGER), "danger"), "nome canonico del ruolo");
    check(gmb_theme_state_parse("hover") == GMB_STATE_HOVER, "state=hover");
    check(gmb_theme_part_parse("text") == GMB_PART_TEXT, "part=text");

    /* Ogni ruolo, su ogni tema, deve avere testo leggibile (AA) sulla superficie. */
    for (size_t i = 0; i < sizeof g_themes / sizeof g_themes[0]; i++)
    {
        gmb_theme_t t = gmb_theme_named(g_themes[i]);
        for (int role = GMB_ROLE_DEFAULT; role <= GMB_ROLE_MUTED; role++)
        {
            gmb_cell_color_t surface = gmb_theme_role_color(&t, role, GMB_STATE_NORMAL, GMB_PART_SURFACE);
            gmb_cell_color_t text    = gmb_theme_role_color(&t, role, GMB_STATE_NORMAL, GMB_PART_TEXT);
            double r = gmb_theme_contrast_ratio(text, surface);
            if (r < 4.4)
            {
                printf("FAIL contrasto %s/%s = %.2f\n", g_themes[i], gmb_theme_role_name(role), r);
                g_fails++;
            }
        }
    }

    /* I ruoli semantici sono DISTINTI: su un tema scuro e su uno chiaro. */
    gmb_theme_t dark = gmb_theme_named("midnight");
    gmb_theme_t lite = gmb_theme_named("light");
    gmb_cell_color_t dk = gmb_theme_role_color(&dark, GMB_ROLE_DANGER, GMB_STATE_NORMAL, GMB_PART_SURFACE);
    gmb_cell_color_t sk = gmb_theme_role_color(&dark, GMB_ROLE_SUCCESS, GMB_STATE_NORMAL, GMB_PART_SURFACE);
    check(!(dk.r == sk.r && dk.g == sk.g && dk.b == sk.b), "danger != success (tema scuro)");
    dk = gmb_theme_role_color(&lite, GMB_ROLE_DANGER, GMB_STATE_NORMAL, GMB_PART_SURFACE);
    sk = gmb_theme_role_color(&lite, GMB_ROLE_SUCCESS, GMB_STATE_NORMAL, GMB_PART_SURFACE);
    check(!(dk.r == sk.r && dk.g == sk.g && dk.b == sk.b), "danger != success (tema chiaro)");

    /* Gli stati cambiano la superficie (hover/pressed), disabilitato è attenuato. */
    gmb_cell_color_t n = gmb_theme_role_color(&dark, GMB_ROLE_PRIMARY, GMB_STATE_NORMAL,   GMB_PART_SURFACE);
    gmb_cell_color_t h = gmb_theme_role_color(&dark, GMB_ROLE_PRIMARY, GMB_STATE_HOVER,    GMB_PART_SURFACE);
    gmb_cell_color_t p = gmb_theme_role_color(&dark, GMB_ROLE_PRIMARY, GMB_STATE_PRESSED,  GMB_PART_SURFACE);
    check(!(n.r == h.r && n.g == h.g && n.b == h.b), "hover distingue dalla superficie normale");
    check(!(h.r == p.r && h.g == p.g && h.b == p.b), "pressed distingue da hover");

    /* La selezione è visibile anche dove accento e selezione coincidono (Turbo). */
    gmb_theme_t tv = gmb_theme_named("turbovision");
    gmb_cell_color_t sel = gmb_theme_role_color(&tv, GMB_ROLE_DEFAULT, GMB_STATE_SELECTED, GMB_PART_SURFACE);
    gmb_cell_color_t nrm = gmb_theme_role_color(&tv, GMB_ROLE_DEFAULT, GMB_STATE_NORMAL,   GMB_PART_SURFACE);
    check(!(sel.r == nrm.r && sel.g == nrm.g && sel.b == nrm.b), "selezione visibile su Turbo Vision");

    /* Il focus è una TINTA del ruolo, leggibile sullo sfondo. */
    gmb_cell_color_t fo = gmb_theme_role_color(&dark, GMB_ROLE_ACCENT, GMB_STATE_FOCUSED, GMB_PART_FOCUS);
    check(gmb_theme_contrast_ratio(fo, gmb_theme_role_color(&dark, GMB_ROLE_ACCENT, GMB_STATE_NORMAL, GMB_PART_SURFACE)) >= 2.9,
          "anello di focus contrastato sulla superficie");

    /* Percorso PIXEL: il ruolo ritinge la superficie ma NON azzera forma/raggio. */
    gmb_widget_style_t px;
    memset(&px, 0, sizeof px);
    px.shape = 1; px.radius = 12; px.shadow_on = 1;
    gmb_theme_apply_role_px(&px, &dark, GMB_ROLE_DANGER);
    check(px.shape == 1 && px.radius == 12 && px.shadow_on == 1, "il ruolo non distrugge forma/ombra");
    check(((px.fill >> 16) & 0xFF) > ((px.fill >> 8) & 0xFF), "danger tira al rosso");
    check(px.text != px.fill, "testo del ruolo diverso dalla superficie");

    /* Un ruolo sconosciuto non tocca nulla. */
    gmb_widget_style_t keep;
    memset(&keep, 0, sizeof keep);
    keep.fill = 0x12345678u;
    gmb_theme_apply_role_px(&keep, &dark, GMB_ROLE_NONE);
    check(keep.fill == 0x12345678u, "role=NONE è un no-op");
}

/* ---- layout: limiti e quota del delta ---- */

static void test_layout(void)
{
    gmb_layout_spec_t ls;

    /* Una spec senza chiavi di layout non attiva nulla (role= è dello stile, non del layout). */
    check(gmb_layout_parse("role=danger fg=#ff0000", &ls) == 0, "role= non è una chiave di layout");

    /* grow=50: il controllo assorbe metà del delta. */
    gmb_control_t c = ctrl(40, 40, 200, 40);
    check(gmb_layout_parse("expand=h grow=50", &ls) == 1, "parse di expand + grow");
    check(ls.grow == 50 && ls.expand == GMB_LAYOUT_EXPAND_H, "valori di grow/expand");
    gmb_layout_configure(&c, 0, &ls, 640, 320);
    check(c.width == 200 && c.layout_grow == 50, "base del controllo intatta alla creazione");
    gmb_layout_reflow(&c, 0, 960, 480);
    check(c.width == 360, "grow=50 → 200 + (320/2)");

    /* minw: il controllo non scende sotto il minimo leggibile anche senza crescita. */
    gmb_control_t m = ctrl(10, 10, 100, 30);
    check(gmb_layout_parse("expand=h grow=0 minw=240", &ls) == 1, "parse di minw");
    check(ls.min_w == 240 && ls.grow == 0, "valori di minw/grow");
    gmb_layout_configure(&m, 0, &ls, 640, 320);
    check(m.width == 240, "minw vale già alla creazione (non si aspetta il resize)");
    gmb_layout_reflow(&m, 0, 960, 480);
    check(m.width == 240, "minw applicato alla larghezza");

    /* maxw: il tetto vince sulla crescita. */
    gmb_control_t x = ctrl(10, 10, 100, 30);
    check(gmb_layout_parse("expand=h maxw=500", &ls) == 1, "parse di maxw");
    gmb_layout_configure(&x, 0, &ls, 640, 320);
    gmb_layout_reflow(&x, 0, 3000, 480);
    check(x.width == 500, "maxw limita la crescita");

    /* Limite 0 = nessun limite (default esplicito). */
    gmb_control_t z = ctrl(10, 10, 100, 30);
    check(gmb_layout_parse("expand=h maxw=0", &ls) == 1, "parse di maxw=0");
    gmb_layout_configure(&z, 0, &ls, 640, 320);
    gmb_layout_reflow(&z, 0, 740, 320);
    check(z.width == 200, "maxw=0 non limita");

    /* Reflow idempotente: nessuna modifica → nessun cambio segnalato. */
    check(gmb_layout_reflow(&z, 0, 740, 320) == 0, "reflow idempotente");

    /* Le chiavi nuove si SOMMANO alle vecchie: ancoraggio, limite e quota insieme. */
    gmb_control_t s = ctrl(20, 20, 120, 60);
    check(gmb_layout_parse("anchor=right+bottom grow=50 maxh=90", &ls) == 1, "parse combinato");
    gmb_layout_configure(&s, 0, &ls, 640, 320);
    gmb_layout_reflow(&s, 0, 960, 480);
    check(s.x == 180, "anchor=right con grow=50 → dx/2 = 160");
    check(s.height == 60, "altezza invariata senza ancoraggio verticale doppio");

    /* Un controllo senza layout resta fermo. */
    gmb_control_t f = ctrl(5, 5, 50, 10);
    gmb_layout_spec_t none;
    memset(&none, 0, sizeof none);
    none.expand = -1; none.anchor = -1; none.align_h = -1; none.align_v = -1;
    none.min_w = -1; none.min_h = -1; none.max_w = -1; none.max_h = -1; none.grow = -1;
    gmb_layout_configure(&f, 0, &none, 640, 320);
    check(gmb_layout_reflow(&f, 0, 960, 480) == 0, "nessun layout → nessun reflow");

    /* PIXEL e TEXT sono mondi separati: il reflow su un'unità diversa non tocca il rect. */
    gmb_control_t t = ctrl(1, 2, 3, 4);
    check(gmb_layout_parse("expand=both", &ls) == 1, "parse expand=both");
    gmb_layout_configure(&t, 1, &ls, 80, 24);
    check(gmb_layout_reflow(&t, 0, 200, 100) == 0, "unità diversa → nessun reflow");
    check(t.x == 1 && t.y == 2 && t.width == 3 && t.height == 4, "rect TEXT intatto");
}

/* ---- cascata nel percorso TEXT: gmb_control_style_effective ---- */

static void test_tui_cascade(void)
{
    gmb_control_style_t st;
    gmb_cell_color_t fg, bg;

    memset(&st, 0, sizeof st);
    st.role = GMB_ROLE_DANGER;
    st.role_aa = 45;

    gmb_ui_set_theme_name("midnight");
    gmb_control_style_effective(&st, false, false, false, &fg, &bg);
    check(gmb_theme_contrast_ratio(fg, bg) >= 4.4, "TUI role=danger leggibile (scuro)");
    gmb_cell_color_t dark_bg = bg;

    /* Il colore NON è congelato: cambiando tema cambia da solo (niente stantio). */
    gmb_ui_set_theme_name("light");
    gmb_control_style_effective(&st, false, false, false, &fg, &bg);
    check(gmb_theme_contrast_ratio(fg, bg) >= 4.4, "TUI role=danger leggibile (chiaro)");
    check(!(bg.r == dark_bg.r && bg.g == dark_bg.g && bg.b == dark_bg.b),
          "cambio tema → il controllo con ruolo segue il tema");

    /* L'override esplicito del programma vince sempre sul ruolo. */
    st.has_bg = true; st.bg = crgb(1, 2, 3);
    st.has_fg = true; st.fg = crgb(254, 253, 252);
    gmb_control_style_effective(&st, false, false, false, &fg, &bg);
    check(bg.r == 1 && bg.g == 2 && bg.b == 3, "bg esplicito vince sul ruolo");
    check(fg.r == 254 && fg.g == 253 && fg.b == 252, "fg esplicito vince sul ruolo");

    /* Lo stato di errore è semanticamente "danger", qualunque sia il ruolo. */
    memset(&st, 0, sizeof st);
    st.role = GMB_ROLE_PRIMARY;
    st.role_aa = 45;
    gmb_control_style_effective(&st, false, true, false, &fg, &bg);
    check(bg.r > bg.g || bg.r > bg.b, "errore su un controllo con ruolo → superficie rosso/calda");

    /* Un controllo SENZA ruolo continua a comportarsi come prima (nessuna deriva). */
    memset(&st, 0, sizeof st);
    gmb_ui_set_theme_name("turbovision");
    gmb_control_style_effective(&st, false, false, false, &fg, &bg);
    gmb_theme_t tv = gmb_theme_named("turbovision");
    check(bg.r == tv.window_bg.r && bg.g == tv.window_bg.g && bg.b == tv.window_bg.b,
          "senza ruolo: sfondo = finestra del tema");
    check(fg.r == tv.text.r && fg.g == tv.text.g && fg.b == tv.text.b,
          "senza ruolo: inchiostro = testo del tema");
}

/* ---- accessibilità: flag globali (registro → runtime) ---- */

static void test_a11y(void)
{
    /* Parse condivisi booleani. */
    check(gmb_a11y_parse_bool("1") == 1 && gmb_a11y_parse_bool("on") == 1
          && gmb_a11y_parse_bool("TRUE") == 1 && gmb_a11y_parse_bool("si") == 1,
          "parse bool: acceso");
    check(gmb_a11y_parse_bool("0") == 0 && gmb_a11y_parse_bool("off") == 0
          && gmb_a11y_parse_bool("") == 0 && gmb_a11y_parse_bool("boh") == 0,
          "parse bool: spento");
    check(gmb_a11y_parse_bool("2") == 1, "parse bool: numero > 0 = acceso");

    /* Parse della scala: clamp 0.75..3, default 1, 0/negativo/non numerico = 1. */
    check(fabs(gmb_a11y_parse_scale("") - 1.0) < 1e-9, "scala vuota = 1");
    check(fabs(gmb_a11y_parse_scale("0") - 1.0) < 1e-9, "scala 0 = default");
    check(fabs(gmb_a11y_parse_scale("0.5") - 0.75) < 1e-9, "scala minima 0.75");
    check(fabs(gmb_a11y_parse_scale("9") - 3.0) < 1e-9, "scala massima 3");
    check(fabs(gmb_a11y_parse_scale("1.25") - 1.25) < 1e-9, "scala 1.25");

    /* Il registro opzioni e' la SORGENTE: impostare li' i flag li rende attivi. */
    check(gmb_opt_set(GMB_A11Y_OPT_HIGH_CONTRAST, "1") == 0, "registro: high_contrast");
    check(gmb_opt_set(GMB_A11Y_OPT_TEXT_SCALE, "2") == 0, "registro: text_scale");
    check(gmb_opt_set(GMB_A11Y_OPT_REDUCE_MOTION, "1") == 0, "registro: reduce_motion");
    check(gmb_opt_set(GMB_A11Y_OPT_FOCUS_VISIBLE, "1") == 0, "registro: focus_always_visible");

    gmb_a11y_t a = gmb_a11y_get();
    check(a.high_contrast == 1 && a.reduce_motion == 1 && a.focus_always_visible == 1,
          "snapshot dei flag attivi");
    check(fabs(a.text_scale - 2.0) < 1e-9, "snapshot text_scale");
    check(gmb_a11y_motion() == 0, "reduce_motion spegne il movimento");
    check(gmb_a11y_scale_px(16) == 32, "text_scale=2 su 16 -> 32");
    check(gmb_a11y_scale_px(0) == 0 && gmb_a11y_scale_px(-3) == -3, "0/negativo = auto, invariato");
    check(fabs(gmb_a11y_min_text_ratio() - 7.0) < 1e-9, "high_contrast: testo AAA");
    check(fabs(gmb_a11y_min_border_ratio() - 4.5) < 1e-9, "high_contrast: bordo 4.5");

    /* AAA e' raggiungibile su uno sfondo scuro; su ogni preset il contrasto non
     * peggiora mai rispetto ad AA (puo' solo avvicinarsi al polo migliore). */
    check(gmb_theme_contrast_ratio(
              gmb_theme_ensure_contrast(crgb(60, 60, 60), crgb(0, 0, 0), 7.0), crgb(0, 0, 0)) >= 7.0,
          "AAA raggiungibile su nero");
    for (size_t i = 0; i < sizeof g_themes / sizeof g_themes[0]; i++)
    {
        gmb_theme_t t = gmb_theme_named(g_themes[i]);
        for (int role = GMB_ROLE_DEFAULT; role <= GMB_ROLE_MUTED; role++)
        {
            gmb_cell_color_t s = gmb_theme_role_color(&t, role, GMB_STATE_NORMAL, GMB_PART_SURFACE);
            gmb_cell_color_t x = gmb_theme_role_color(&t, role, GMB_STATE_NORMAL, GMB_PART_TEXT);
            if (gmb_theme_contrast_ratio(x, s) < 4.4)
            {
                printf("FAIL a11y alto contrasto %s/%s\n", g_themes[i], gmb_theme_role_name(role));
                g_fails++;
            }
        }
    }

    /* Anello di focus artificiale: senza focuscolor= esplicito e con una coppia di
     * focus identica a quella normale, il flag rende il focus distinguibile. */
    gmb_opt_set(GMB_A11Y_OPT_HIGH_CONTRAST, "0");
    gmb_ui_set_theme_name("midnight");
    gmb_control_style_t st;
    memset(&st, 0, sizeof st);
    gmb_cell_color_t nfg, nbg, ffg, fbg;
    gmb_control_style_effective(&st, false, false, false, &nfg, &nbg);
    gmb_control_style_effective(&st, true,  false, false, &ffg, &fbg);
    check(!(ffg.r == nfg.r && ffg.g == nfg.g && ffg.b == nfg.b
            && fbg.r == nbg.r && fbg.g == nbg.g && fbg.b == nbg.b),
          "focus_always_visible: il focus si distingue dal normale");

    /* Spento il flag, il focus torna esattamente alla coppia del tema. */
    gmb_opt_set(GMB_A11Y_OPT_FOCUS_VISIBLE, "0");
    gmb_control_style_effective(&st, true, false, false, &ffg, &fbg);
    check(ffg.r == nfg.r && ffg.g == nfg.g && ffg.b == nfg.b
          && fbg.r == nbg.r && fbg.g == nbg.g && fbg.b == nbg.b,
          "senza flag il focus resta quello del tema");

    /* Nessun flag residuo: tornano i valori normali (le altre verifiche non sono
     * contaminate e un programma che non imposta nulla non cambia comportamento). */
    gmb_opt_set(GMB_A11Y_OPT_TEXT_SCALE, "1");
    gmb_opt_set(GMB_A11Y_OPT_REDUCE_MOTION, "0");
    check(gmb_a11y_motion() == 1 && gmb_a11y_scale_px(10) == 10, "flag spenti = comportamento normale");
}

int main(void)
{
    test_contrast();
    test_roles();
    test_tui_cascade();
    test_layout();
    test_a11y();
    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_theme_roles_selfcheck: OK\n");
    return 0;
}
