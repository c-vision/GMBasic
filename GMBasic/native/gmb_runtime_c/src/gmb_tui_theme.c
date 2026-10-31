/* ============================================================================
 * gmb_tui_theme.c — porting C di ClipperLib/Controls/Theme.cs (specchio esatto).
 * Preset e valori RGB allineati 1:1 a Theme.cs (Turbo default, QBasic, Midnight,
 * Nord, Dracula, Monokai, SolarizedDark, RetroGreen, Amber) + desktop_pattern.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Theme ===================== */

gmb_theme_t gmb_theme_turbo(void)
{
    gmb_theme_t t;
    /* SPECCHIO ESATTO di common/ClipperLib/Controls/Theme.cs -> Turbo(). */
    t.desktop_bg = crgb(0, 0, 170);      /* DesktopBg: blu Borland */
    t.window_bg = crgb(170, 170, 170);   /* WindowBg: grigio chiaro (finestre/controlli) */
    t.title_focus = crgb(170, 170, 170); /* TitleFocus */
    t.title_blur = crgb(85, 85, 85);     /* TitleBlur */
    t.title_fg = crgb(0, 0, 0);          /* TitleFg */
    t.border = crgb(170, 170, 170);      /* Border */
    t.ctrl_fg = crgb(0, 0, 0);           /* CtrlFg */
    t.close_fg = crgb(0, 170, 0);        /* CloseFg */
    t.text = crgb(0, 0, 0);              /* Text */
    t.dim = crgb(85, 85, 85);            /* Dim */
    t.accent = crgb(170, 170, 170);      /* Accent */
    t.active_bg = crgb(0, 170, 170);     /* ActiveBg: ciano focus/selezione */
    t.shadow = crgb(0, 0, 0);            /* Shadow */
    t.desktop_pattern = "\xE2\x96\x91"; /* DesktopPattern: '░' */
    t.head_bg = crgb(0, 0, 170); t.head_fg = crgb(255, 255, 255);
    /* Palette PER-VISTA (TV, dallo screenshot tvedit): editor blu/bianco/cornice ciano;
     * menu e dialog grigi/nero; statusline ciano/nero. (0,0,0 = deriva dai base.) */
    t.editor_bg = crgb(0, 0, 170); t.editor_fg = crgb(255, 255, 255); t.editor_frame = crgb(0, 255, 255);
    t.menu_bg = crgb(170, 170, 170); t.menu_fg = crgb(0, 0, 0);
    t.dlg_bg = crgb(170, 170, 170); t.dlg_fg = crgb(0, 0, 0);
    t.stat_bg = crgb(0, 255, 255); t.stat_fg = crgb(0, 0, 0);
    t.cal_sel_bg = crgb(0, 255, 255); t.cal_sel_fg = crgb(0, 0, 0);
    t.cal_today_bg = crgb(255, 255, 85); t.cal_today_fg = crgb(0, 0, 170);
    return t;
}

/* Colori per-vista dal tema. kind: 0=editor, 1=menu, 2=dialog, 3=status. */
void gmb_theme_kind(const gmb_theme_t* t, int kind, gmb_cell_color_t* fg, gmb_cell_color_t* bg)
{
    if (!t || !fg || !bg) return;
    gmb_cell_color_t none = { 0, 0, 0, 0 };
    gmb_cell_color_t kf = none, kb = none;
    switch (kind)
    {
        case 0: kf = t->editor_fg; kb = t->editor_bg; break;
        case 1: kf = t->menu_fg;   kb = t->menu_bg;   break;
        case 2: kf = t->dlg_fg;    kb = t->dlg_bg;    break;
        case 3: kf = t->stat_fg;   kb = t->stat_bg;   break;
    }
    *fg = (kf.r || kf.g || kf.b) ? kf : t->text;
    *bg = (kb.r || kb.g || kb.b) ? kb : t->window_bg;
}

gmb_theme_t gmb_theme_qbasic(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(0, 0, 128);
    t.window_bg = crgb(0, 0, 128);
    t.title_focus = crgb(0, 128, 128);
    t.title_blur = crgb(64, 64, 64);
    t.title_fg = crgb(255, 255, 255);
    t.border = crgb(255, 255, 255);
    t.ctrl_fg = crgb(255, 255, 255);
    t.close_fg = crgb(255, 85, 85);
    t.text = crgb(255, 255, 255);
    t.dim = crgb(128, 128, 128);
    t.accent = crgb(255, 255, 85);
    t.active_bg = crgb(0, 128, 128);
    t.shadow = crgb(0, 0, 0);
    t.desktop_pattern = " ";
    return t;
}

gmb_theme_t gmb_theme_midnight(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(44, 46, 50);
    t.window_bg = crgb(17, 20, 29);
    t.title_focus = crgb(29, 36, 51);
    t.title_blur = crgb(20, 24, 34);
    t.title_fg = crgb(143, 183, 255);
    t.border = crgb(58, 68, 88);
    t.ctrl_fg = crgb(150, 165, 190);
    t.close_fg = crgb(255, 107, 107);
    t.text = crgb(200, 208, 220);
    t.dim = crgb(120, 130, 150);
    t.accent = crgb(108, 182, 255);
    t.active_bg = crgb(45, 58, 85);
    t.shadow = crgb(10, 12, 16);
    t.desktop_pattern = "\xC2\xB7"; /* '·' */
    return t;
}

gmb_theme_t gmb_theme_nord(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(46, 52, 64);
    t.window_bg = crgb(36, 41, 51);
    t.title_focus = crgb(59, 66, 82);
    t.title_blur = crgb(43, 49, 60);
    t.title_fg = crgb(136, 192, 208);
    t.border = crgb(76, 86, 106);
    t.ctrl_fg = crgb(180, 190, 205);
    t.close_fg = crgb(191, 97, 106);
    t.text = crgb(216, 222, 233);
    t.dim = crgb(120, 130, 150);
    t.accent = crgb(136, 192, 208);
    t.active_bg = crgb(76, 86, 106);
    t.shadow = crgb(20, 24, 32);
    t.desktop_pattern = "\xC2\xB7"; /* '·' */
    return t;
}

/* Tema CHIARO (per GUI grafiche: finestra bianca, testo scuro, azzurro). */
gmb_theme_t gmb_theme_light(void)
{
    gmb_theme_t t;
    t.desktop_bg   = crgb(219, 234, 254);
    t.window_bg    = crgb(255, 255, 255);
    t.title_focus  = crgb(30, 58, 138);
    t.title_blur   = crgb(148, 163, 184);
    t.title_fg     = crgb(255, 255, 255);
    t.border       = crgb(148, 163, 184);
    t.ctrl_fg      = crgb(30, 41, 59);
    t.close_fg     = crgb(220, 38, 38);
    t.text         = crgb(15, 23, 42);
    t.dim          = crgb(100, 116, 139);
    t.accent       = crgb(37, 99, 235);
    t.active_bg    = crgb(191, 219, 254);
    t.shadow       = crgb(203, 213, 225);
    t.desktop_pattern = " ";
    return t;
}


/* Tema SOFT/NEUMORPH (riferimento: UI soft light, card bianche, accento ambra).
 * Pagine chiare, ombre morbide, forme a pillola, accenti caldi. */
gmb_theme_t gmb_theme_soft(void)
{
    gmb_theme_t t;
    t.desktop_bg   = crgb(233, 237, 241);   /* pagina */
    t.window_bg    = crgb(244, 246, 248);   /* superficie */
    t.title_focus  = crgb(246, 168, 0);     /* ambra */
    t.title_blur   = crgb(214, 222, 230);
    t.title_fg     = crgb(255, 255, 255);
    t.border       = crgb(227, 232, 237);
    t.ctrl_fg      = crgb(62, 74, 86);
    t.close_fg     = crgb(244, 91, 105);
    t.text         = crgb(62, 74, 86);
    t.dim          = crgb(138, 149, 161);
    t.accent       = crgb(246, 168, 0);
    t.active_bg    = crgb(255, 212, 92);
    t.shadow       = crgb(0, 0, 0);
    t.head_bg      = crgb(255, 255, 255);
    t.head_fg      = crgb(62, 74, 86);
    t.desktop_pattern = " ";
    return t;
}

gmb_theme_t gmb_theme_dracula(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(54, 57, 76);
    t.window_bg = crgb(40, 42, 54);
    t.title_focus = crgb(68, 71, 90);
    t.title_blur = crgb(50, 52, 66);
    t.title_fg = crgb(189, 147, 249);
    t.border = crgb(98, 114, 164);
    t.ctrl_fg = crgb(200, 200, 220);
    t.close_fg = crgb(255, 85, 85);
    t.text = crgb(248, 248, 242);
    t.dim = crgb(139, 145, 175);
    t.accent = crgb(189, 147, 249);
    t.active_bg = crgb(98, 114, 164);
    t.shadow = crgb(25, 27, 38);
    t.desktop_pattern = "\xC2\xB7"; /* '·' */
    return t;
}

gmb_theme_t gmb_theme_monokai(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(39, 40, 34);
    t.window_bg = crgb(46, 47, 41);
    t.title_focus = crgb(62, 61, 50);
    t.title_blur = crgb(39, 40, 34);
    t.title_fg = crgb(249, 38, 114);
    t.border = crgb(166, 226, 46);
    t.ctrl_fg = crgb(102, 217, 239);
    t.close_fg = crgb(249, 38, 114);
    t.text = crgb(248, 248, 242);
    t.dim = crgb(117, 113, 94);
    t.accent = crgb(253, 151, 31);
    t.active_bg = crgb(73, 72, 62);
    t.shadow = crgb(18, 19, 16);
    t.desktop_pattern = "\xC2\xB7"; /* '·' */
    return t;
}

gmb_theme_t gmb_theme_solarized_dark(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(0, 43, 54);
    t.window_bg = crgb(7, 54, 66);
    t.title_focus = crgb(38, 139, 210);
    t.title_blur = crgb(7, 54, 66);
    t.title_fg = crgb(253, 246, 227);
    t.border = crgb(42, 161, 152);
    t.ctrl_fg = crgb(181, 137, 0);
    t.close_fg = crgb(220, 50, 47);
    t.text = crgb(131, 148, 150);
    t.dim = crgb(88, 110, 117);
    t.accent = crgb(181, 137, 0);
    t.active_bg = crgb(88, 110, 117);
    t.shadow = crgb(0, 20, 26);
    t.desktop_pattern = "\xE2\x96\x91"; /* '░' */
    return t;
}

gmb_theme_t gmb_theme_retro_green(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(0, 20, 0);
    t.window_bg = crgb(0, 30, 0);
    t.title_focus = crgb(0, 80, 0);
    t.title_blur = crgb(0, 25, 0);
    t.title_fg = crgb(51, 255, 51);
    t.border = crgb(0, 204, 0);
    t.ctrl_fg = crgb(153, 255, 153);
    t.close_fg = crgb(255, 102, 102);
    t.text = crgb(51, 255, 51);
    t.dim = crgb(0, 102, 0);
    t.accent = crgb(102, 255, 102);
    t.active_bg = crgb(0, 90, 0);
    t.shadow = crgb(0, 0, 0);
    t.desktop_pattern = "\xE2\x96\x91"; /* '░' */
    return t;
}

gmb_theme_t gmb_theme_amber(void)
{
    gmb_theme_t t;
    t.desktop_bg = crgb(20, 10, 0);
    t.window_bg = crgb(35, 18, 0);
    t.title_focus = crgb(80, 40, 0);
    t.title_blur = crgb(25, 12, 0);
    t.title_fg = crgb(255, 191, 0);
    t.border = crgb(255, 160, 0);
    t.ctrl_fg = crgb(255, 215, 0);
    t.close_fg = crgb(255, 80, 80);
    t.text = crgb(255, 176, 0);
    t.dim = crgb(128, 80, 0);
    t.accent = crgb(255, 215, 0);
    t.active_bg = crgb(90, 45, 0);
    t.shadow = crgb(0, 0, 0);
    t.desktop_pattern = "\xE2\x96\x91"; /* '░' */
    return t;
}


/* Tema NEON (light, accento viola->magenta, card bianche). */
gmb_theme_t gmb_theme_neon(void)
{
    gmb_theme_t t;
    t.desktop_bg   = crgb(243, 240, 255);
    t.window_bg    = crgb(255, 255, 255);
    t.title_focus  = crgb(139, 92, 246);
    t.title_blur   = crgb(214, 210, 230);
    t.title_fg     = crgb(255, 255, 255);
    t.border       = crgb(230, 230, 239);
    t.ctrl_fg      = crgb(43, 43, 68);
    t.close_fg     = crgb(236, 72, 153);
    t.text         = crgb(43, 43, 68);
    t.dim          = crgb(138, 138, 156);
    t.accent       = crgb(139, 92, 246);
    t.active_bg    = crgb(237, 233, 254);
    t.shadow       = crgb(0, 0, 0);
    t.head_bg      = crgb(255, 255, 255);
    t.head_fg      = crgb(43, 43, 68);
    t.desktop_pattern = " ";
    return t;
}

/* Tema GLASS / DASHBOARD (navy traslucido, testo chiaro, accento ciano). */
gmb_theme_t gmb_theme_glass(void)
{
    gmb_theme_t t;
    t.desktop_bg   = crgb(14, 34, 64);
    t.window_bg    = crgb(20, 51, 95);
    t.title_focus  = crgb(79, 195, 247);
    t.title_blur   = crgb(44, 78, 130);
    t.title_fg     = crgb(232, 241, 255);
    t.border       = crgb(44, 78, 130);
    t.ctrl_fg      = crgb(232, 241, 255);
    t.close_fg     = crgb(255, 107, 129);
    t.text         = crgb(232, 241, 255);
    t.dim          = crgb(147, 169, 199);
    t.accent       = crgb(79, 195, 247);
    t.active_bg    = crgb(27, 75, 143);
    t.shadow       = crgb(0, 0, 0);
    t.head_bg      = crgb(17, 48, 92);
    t.head_fg      = crgb(232, 241, 255);
    t.desktop_pattern = " ";
    return t;
}

gmb_theme_t gmb_theme_vb6(void)
{
    gmb_theme_t t;
    /* Visual Basic 6 / Windows 98 classic control palette. */
    t.desktop_bg = crgb(0, 128, 128);
    t.window_bg = crgb(192, 192, 192);
    t.title_focus = crgb(0, 0, 128);
    t.title_blur = crgb(128, 128, 128);
    t.title_fg = crgb(255, 255, 255);
    t.border = crgb(128, 128, 128);
    t.ctrl_fg = crgb(0, 0, 0);
    t.close_fg = crgb(0, 0, 0);
    t.text = crgb(0, 0, 0);
    t.dim = crgb(128, 128, 128);
    t.accent = crgb(0, 0, 128);
    t.active_bg = crgb(0, 0, 128);
    t.shadow = crgb(0, 0, 0);
    t.desktop_pattern = " ";
    t.head_bg = crgb(0, 0, 128); t.head_fg = crgb(255, 255, 255);
    t.editor_bg = crgb(255, 255, 255); t.editor_fg = crgb(0, 0, 0); t.editor_frame = crgb(128, 128, 128);
    t.menu_bg = crgb(192, 192, 192); t.menu_fg = crgb(0, 0, 0);
    t.dlg_bg = crgb(192, 192, 192); t.dlg_fg = crgb(0, 0, 0);
    t.stat_bg = crgb(192, 192, 192); t.stat_fg = crgb(0, 0, 0);
    t.cal_sel_bg = crgb(0, 0, 128); t.cal_sel_fg = crgb(255, 255, 255);
    t.cal_today_bg = crgb(255, 255, 128); t.cal_today_fg = crgb(0, 0, 0);
    return t;
}

gmb_theme_t gmb_theme_named(const char* name)
{
    char n[48];
    int j = 0;
    const char* s = name ? name : "";
    for (; *s && j < (int)sizeof(n) - 1; s++)
    {
        char c = *s;
        if (c == '-' || c == '_' || c == ' ') continue;   /* Theme.cs: Replace(-,_,space) */
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        n[j++] = c;
    }
    n[j] = 0;
    if (strcmp(n, "midnight") == 0) return gmb_theme_midnight();
    if (strcmp(n, "nord") == 0) return gmb_theme_nord();
    if (strcmp(n, "dracula") == 0) return gmb_theme_dracula();
    if (strcmp(n, "qbasic") == 0 || strcmp(n, "dos") == 0) return gmb_theme_qbasic();
    if (strcmp(n, "monokai") == 0) return gmb_theme_monokai();
    if (strcmp(n, "solarized") == 0 || strcmp(n, "solarizeddark") == 0) return gmb_theme_solarized_dark();
    if (strcmp(n, "retrogreen") == 0 || strcmp(n, "green") == 0 || strcmp(n, "matrix") == 0) return gmb_theme_retro_green();
    if (strcmp(n, "amber") == 0) return gmb_theme_amber();
    if (strcmp(n, "vb6") == 0 || strcmp(n, "visualbasic") == 0) return gmb_theme_vb6();
    if (strcmp(n, "turbo") == 0) return gmb_theme_turbo();
    if (strcmp(n, "light") == 0 || strcmp(n, "win") == 0 || strcmp(n, "windows") == 0) return gmb_theme_light();
    if (strcmp(n, "soft") == 0 || strcmp(n, "neumorph") == 0 || strcmp(n, "neumorphic") == 0) return gmb_theme_soft();
    if (strcmp(n, "neon") == 0 || strcmp(n, "violet") == 0) return gmb_theme_neon();
    if (strcmp(n, "glass") == 0 || strcmp(n, "dashboard") == 0 || strcmp(n, "navy") == 0) return gmb_theme_glass();
    return gmb_theme_turbo();   /* Theme.cs: _ => Turbo() */
}

gmb_theme_t gmb_theme_default(void) { return gmb_theme_turbo(); }

/* ===================== Widget style (pixel) ===================== */

static gmb_theme_t g_ui_theme;
static int g_ui_theme_set = 0;
static char g_ui_theme_name[48] = "turbo";

static unsigned int ps_rgba(gmb_cell_color_t c, unsigned int a)
{
    return (a << 24) | ((unsigned int)c.r << 16) | ((unsigned int)c.g << 8) | (unsigned int)c.b;
}

static int theme_is_classic(const char* n)
{
    return strcasecmp(n, "turbo") == 0 || strcasecmp(n, "qbasic") == 0
        || strcasecmp(n, "dos") == 0 || strcasecmp(n, "retrogreen") == 0
        || strcasecmp(n, "green") == 0 || strcasecmp(n, "matrix") == 0
        || strcasecmp(n, "amber") == 0 || strcasecmp(n, "vb6") == 0
        || strcasecmp(n, "visualbasic") == 0;
}

void gmb_ui_set_theme(const gmb_theme_t* t)
{
    if (t) { g_ui_theme = *t; g_ui_theme_set = 1; }
}

void gmb_ui_set_theme_name(const char* name)
{
    char n[48]; int j = 0; const char* s = name ? name : "";
    for (; *s && j < (int)sizeof(n) - 1; s++) {
        char c = *s;
        if (c == '-' || c == '_' || c == ' ') continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        n[j++] = c;
    }
    n[j] = 0;
    g_ui_theme = gmb_theme_named(name);
    g_ui_theme_set = 1;
    snprintf(g_ui_theme_name, sizeof(g_ui_theme_name), "%s", n[0] ? n : "turbo");
}

gmb_theme_t gmb_ui_theme(void)
{
    if (!g_ui_theme_set) { g_ui_theme = gmb_theme_default(); g_ui_theme_set = 1; }
    return g_ui_theme;
}

const char* gmb_ui_theme_name(void) { return g_ui_theme_name; }
int gmb_ui_theme_is_set(void) { return g_ui_theme_set; }

gmb_widget_style_t gmb_widget_style_from_theme(const gmb_theme_t* t)
{
    gmb_widget_style_t s;
    memset(&s, 0, sizeof(s));
    int classic = theme_is_classic(g_ui_theme_name);
    int soft = (strcmp(g_ui_theme_name, "soft") == 0 || strcmp(g_ui_theme_name, "neumorph") == 0 || strcmp(g_ui_theme_name, "neumorphic") == 0);
    int neon = (strcmp(g_ui_theme_name, "neon") == 0 || strcmp(g_ui_theme_name, "violet") == 0);
    int glass = (strcmp(g_ui_theme_name, "glass") == 0 || strcmp(g_ui_theme_name, "dashboard") == 0 || strcmp(g_ui_theme_name, "navy") == 0);
    s.shape = classic ? 0 : 1;
    s.radius = classic ? 0 : 6;
    s.fill = ps_rgba(t->window_bg, 0xFF);
    s.fill_hover = ps_rgba(t->active_bg, 0xFF);
    s.fill_press = ps_rgba(t->title_focus, 0xFF);
    s.fill_disabled = ps_rgba(t->dim, 0xFF);
    s.border = ps_rgba(t->border, 0xFF);
    s.border_w = 1;
    s.text = ps_rgba(t->text, 0xFF);
    s.text_disabled = ps_rgba(t->dim, 0xFF);
    s.shadow = ps_rgba(t->shadow, 0xC0);
    s.focus_bg = ps_rgba(t->accent, 0xFF);       /* focus: sfondo accento */
    s.focus_fg = ps_rgba(t->window_bg, 0xFF);    /* focus: inchiostro = sfondo finestra (inversione) */
    s.accent = ps_rgba(t->accent, 0xFF);
    s.today_bg = (t->cal_today_bg.r || t->cal_today_bg.g || t->cal_today_bg.b) ? ps_rgba(t->cal_today_bg, 0xFF) : ps_rgba(t->accent, 0xFF);
    s.today_fg = (t->cal_today_fg.r || t->cal_today_fg.g || t->cal_today_fg.b) ? ps_rgba(t->cal_today_fg, 0xFF) : ps_rgba(t->window_bg, 0xFF);
    s.sel_bg = (t->cal_sel_bg.r || t->cal_sel_bg.g || t->cal_sel_bg.b) ? ps_rgba(t->cal_sel_bg, 0xFF) : ps_rgba(t->title_focus, 0xFF);
    s.sel_fg = (t->cal_sel_fg.r || t->cal_sel_fg.g || t->cal_sel_fg.b) ? ps_rgba(t->cal_sel_fg, 0xFF) : ps_rgba(t->title_fg, 0xFF);
    s.head_bg = (t->head_bg.r || t->head_bg.g || t->head_bg.b) ? ps_rgba(t->head_bg, 0xFF) : ps_rgba(t->window_bg, 0xFF);
    s.head_fg = (t->head_fg.r || t->head_fg.g || t->head_fg.b) ? ps_rgba(t->head_fg, 0xFF) : ps_rgba(t->text, 0xFF);
    s.row_h = (soft || neon || glass) ? 26 : 0; s.pad = soft || neon || glass ? 8 : 6;
    s.shadow_on = classic ? 0 : 1;
    if (strcmp(g_ui_theme_name, "vb6") == 0 || strcmp(g_ui_theme_name, "visualbasic") == 0) {
        s.shape = 0; s.radius = 0; s.shadow_on = 0; s.gradient_on = 0;
        s.bevel = 1; s.bevel_depth = 1;
        s.fill = ps_rgba(crgb(192,192,192), 0xFF);
        s.fill_hover = ps_rgba(crgb(212,208,200), 0xFF);
        s.fill_press = ps_rgba(crgb(168,168,168), 0xFF);
        s.border = ps_rgba(crgb(128,128,128), 0xFF);
        s.bevel_light = ps_rgba(crgb(255,255,255), 0xFF);
        s.bevel_dark = ps_rgba(crgb(128,128,128), 0xFF);
        s.text = ps_rgba(crgb(0,0,0), 0xFF);
        s.focus_bg = ps_rgba(crgb(0,0,128), 0xFF);
        s.focus_fg = ps_rgba(crgb(255,255,255), 0xFF);
        s.accent = ps_rgba(crgb(0,0,128), 0xFF);
        s.title_bg = ps_rgba(crgb(0,0,128), 0xFF);
        s.title_fg = ps_rgba(crgb(255,255,255), 0xFF);
        s.pad_x = 6; s.pad_y = 4; s.font_size = 0;
    }
    s.shadow_dx = 2; s.shadow_dy = 2; s.shadow_soft = 0;
    s.skin = soft ? 0 : (neon ? 1 : 2);
    if (neon) {   /* NEON: card bianche, ombre soft, accento viola->magenta */
        s.shape = 1; s.radius = 16;
        s.shadow_on = 1; s.shadow_soft = 1; s.shadow_dx = 0; s.shadow_dy = 6;
        s.shadow = ps_rgba(t->shadow, 0x1C);
        s.border_w = 1; s.pad_x = 18; s.pad_y = 10; s.row_h = 26;
        s.fill = ps_rgba(crgb(255,255,255), 0xFF);
        s.fill_hover = ps_rgba(crgb(248,247,255), 0xFF);
        s.fill_press = ps_rgba(t->active_bg, 0xFF);
        s.accent = ps_rgba(crgb(139,92,246), 0xFF);
        s.grad_a = ps_rgba(crgb(168,85,247), 0xFF);
        s.grad_b = ps_rgba(crgb(236,72,153), 0xFF);
        s.gradient_on = 1;
    }
    if (glass) {  /* GLASS: pannelli navy traslucidi, bordo chiaro, accento ciano */
        s.shape = 1; s.radius = 16;
        s.shadow_on = 1; s.shadow_soft = 1; s.shadow_dx = 0; s.shadow_dy = 8;
        s.shadow = ps_rgba(crgb(0,0,0), 0x40);
        s.border_w = 1; s.pad_x = 18; s.pad_y = 10; s.row_h = 26;
        s.fill = ps_rgba(crgb(30, 64, 116), 0xFF);
        s.fill_hover = ps_rgba(crgb(38, 80, 140), 0xFF);
        s.fill_press = ps_rgba(t->active_bg, 0xFF);
        s.border = ps_rgba(crgb(96, 140, 200), 0x66);
        s.accent = ps_rgba(crgb(79,195,247), 0xFF);
        s.grad_a = ps_rgba(crgb(79,195,247), 0xFF);
        s.grad_b = ps_rgba(crgb(41, 121, 255), 0xFF);
        s.gradient_on = 1;
        s.glow_on = 1; s.glow = ps_rgba(crgb(79,195,247), 0x22);
    }

    if (soft) {   /* SOFT: pillole, ombre morbide, padding ampio */
        s.shape = 1; s.radius = 14;
        s.shadow_on = 1; s.shadow_soft = 1; s.shadow_dx = 0; s.shadow_dy = 4;
        s.shadow = ps_rgba(t->shadow, 0x22);
        s.border_w = 1;
        s.pad_x = 16; s.pad_y = 10;
        s.row_h = 26;
        s.fill = ps_rgba(crgb(255,255,255), 0xFF);
        s.fill_hover = ps_rgba(crgb(248,250,252), 0xFF);
        s.fill_press = ps_rgba(t->active_bg, 0xFF);
        s.border = ps_rgba(t->border, 0xFF);
    }
    s.font_size = 0;
    s.pad_x = 10; s.pad_y = 6;
    s.bevel = 0; s.bevel_depth = 1;
    s.bevel_light = ps_rgba(t->text, 0x55);      /* highlight 3D */
    s.bevel_dark = ps_rgba(t->shadow, 0x99);      /* ombra 3D */
    s.emboss = 0;
    s.gradient_on = 0;
    s.grad_top = s.fill; s.grad_bottom = ps_rgba(t->title_blur, 0xFF);
    s.glow_on = 0; s.glow = ps_rgba(t->accent, 0xFF);
    s.title_bg = ps_rgba(t->title_focus, 0xFF);
    s.title_fg = ps_rgba(t->title_fg, 0xFF);
    s.body = ps_rgba(t->window_bg, 0xFF);
    if (strcmp(g_ui_theme_name, "vb6") == 0 || strcmp(g_ui_theme_name, "visualbasic") == 0) {
        s.shape = 0; s.radius = 0; s.shadow_on = 0; s.gradient_on = 0; s.glow_on = 0;
        s.bevel = 1; s.bevel_depth = 1;
        s.fill = ps_rgba(crgb(192,192,192), 0xFF);
        s.fill_hover = ps_rgba(crgb(212,208,200), 0xFF);
        s.fill_press = ps_rgba(crgb(168,168,168), 0xFF);
        s.border = ps_rgba(crgb(128,128,128), 0xFF);
        s.bevel_light = ps_rgba(crgb(255,255,255), 0xFF);
        s.bevel_dark = ps_rgba(crgb(128,128,128), 0xFF);
        s.text = ps_rgba(crgb(0,0,0), 0xFF);
        s.focus_bg = ps_rgba(crgb(0,0,128), 0xFF);
        s.focus_fg = ps_rgba(crgb(255,255,255), 0xFF);
        s.accent = ps_rgba(crgb(0,0,128), 0xFF);
        s.title_bg = ps_rgba(crgb(0,0,128), 0xFF);
        s.title_fg = ps_rgba(crgb(255,255,255), 0xFF);
        s.pad_x = 6; s.pad_y = 4;
    }
    return s;
}

gmb_widget_style_t gmb_widget_style_current(void)
{
    gmb_theme_t t = gmb_ui_theme();
    return gmb_widget_style_from_theme(&t);
}
