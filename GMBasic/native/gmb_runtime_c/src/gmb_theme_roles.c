/* ============================================================================
 * gmb_theme_roles.c — ruoli semantici del tema + contrasto WCAG.
 * Vedi include/gmb_theme_roles.h.
 *
 * Tutto PURA aritmetica sui colori: nessuno stato globale, nessuna allocazione,
 * nessuna dipendenza da SDL. Gira identico sul percorso TEXT (celle) e sul
 * percorso GRAPHICS (pixel), ed è quindi testabile senza interfaccia.
 * ============================================================================ */
#include "gmb_theme_roles.h"
#include "gmb_a11y.h"   /* soglie di contrasto imposte da `ui.high_contrast` */

#include <math.h>
#include <string.h>
#include <strings.h>

/* ===================== helper sui colori ===================== */

static uint8_t u8(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

static gmb_cell_color_t mk(int r, int g, int b)
{
    gmb_cell_color_t c;
    c.r = u8(r); c.g = u8(g); c.b = u8(b); c.is_default = false;
    return c;
}

static int has_col(gmb_cell_color_t c) { return (c.r || c.g || c.b) ? 1 : 0; }

static int same(gmb_cell_color_t a, gmb_cell_color_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

/* a*(1-t) + b*t */
gmb_cell_color_t gmb_theme_mix(gmb_cell_color_t a, gmb_cell_color_t b, double t)
{
    if (t <= 0.0) return a;
    if (t >= 1.0) return b;
    return mk((int)(a.r + ((int)b.r - (int)a.r) * t + 0.5),
              (int)(a.g + ((int)b.g - (int)a.g) * t + 0.5),
              (int)(a.b + ((int)b.b - (int)a.b) * t + 0.5));
}

/* f > 1 schiarisce, f < 1 scurisce. */
gmb_cell_color_t gmb_theme_shade(gmb_cell_color_t c, double f)
{
    return mk((int)(c.r * f + 0.5), (int)(c.g * f + 0.5), (int)(c.b * f + 0.5));
}

/* ===================== contrasto WCAG 2.1 ===================== */

static double srgb_lin(uint8_t v)
{
    double c = (double)v / 255.0;
    return (c <= 0.04045) ? (c / 12.92) : pow((c + 0.055) / 1.055, 2.4);
}

double gmb_theme_luminance(gmb_cell_color_t c)
{
    return 0.2126 * srgb_lin(c.r) + 0.7152 * srgb_lin(c.g) + 0.0722 * srgb_lin(c.b);
}

double gmb_theme_contrast_ratio(gmb_cell_color_t a, gmb_cell_color_t b)
{
    double la = gmb_theme_luminance(a);
    double lb = gmb_theme_luminance(b);
    double hi = la > lb ? la : lb;
    double lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

gmb_cell_color_t gmb_theme_text_on(gmb_cell_color_t bg)
{
    gmb_cell_color_t white = mk(255, 255, 255);
    gmb_cell_color_t black = mk(0, 0, 0);
    return (gmb_theme_contrast_ratio(white, bg) >= gmb_theme_contrast_ratio(black, bg)) ? white : black;
}

gmb_cell_color_t gmb_theme_ensure_contrast(gmb_cell_color_t fg, gmb_cell_color_t bg, double min_ratio)
{
    if (min_ratio <= 1.0) return fg;
    if (gmb_theme_contrast_ratio(fg, bg) >= min_ratio) return fg;

    /* Il target è il polo (bianco o nero) che contrasta DAVVERO di più con lo sfondo
     * (non quello dedotto dalla soglia di luminanza: uno sfondo appena sotto 0.5 ha
     * spesso il polo opposto come più leggibile). Si interpola in 10 passi e si prende
     * il PRIMO che supera la soglia: il colore cambia il minimo indispensabile e il
     * risultato è deterministico. */
    gmb_cell_color_t target = gmb_theme_text_on(bg);
    for (int i = 1; i <= 10; i++)
    {
        gmb_cell_color_t c = gmb_theme_mix(fg, target, (double)i / 10.0);
        if (gmb_theme_contrast_ratio(c, bg) >= min_ratio) return c;
    }
    return target;
}

unsigned int gmb_theme_rgba(gmb_cell_color_t c, unsigned int alpha)
{
    return ((alpha & 0xFFu) << 24) | ((unsigned int)c.r << 16) | ((unsigned int)c.g << 8) | (unsigned int)c.b;
}

/* ===================== nomi ===================== */

int gmb_theme_role_parse(const char* name)
{
    if (!name || !*name) return -1;
    if (!strcasecmp(name, "default") || !strcasecmp(name, "base") || !strcasecmp(name, "neutral")) return GMB_ROLE_DEFAULT;
    if (!strcasecmp(name, "primary") || !strcasecmp(name, "main"))    return GMB_ROLE_PRIMARY;
    if (!strcasecmp(name, "secondary") || !strcasecmp(name, "sub"))   return GMB_ROLE_SECONDARY;
    if (!strcasecmp(name, "accent"))                                   return GMB_ROLE_ACCENT;
    if (!strcasecmp(name, "success") || !strcasecmp(name, "ok") || !strcasecmp(name, "positive")) return GMB_ROLE_SUCCESS;
    if (!strcasecmp(name, "warning") || !strcasecmp(name, "warn") || !strcasecmp(name, "caution")) return GMB_ROLE_WARNING;
    if (!strcasecmp(name, "danger") || !strcasecmp(name, "error") || !strcasecmp(name, "destructive")) return GMB_ROLE_DANGER;
    if (!strcasecmp(name, "info") || !strcasecmp(name, "note"))        return GMB_ROLE_INFO;
    if (!strcasecmp(name, "muted") || !strcasecmp(name, "disabled") || !strcasecmp(name, "dim")) return GMB_ROLE_MUTED;
    return -1;
}

int gmb_theme_state_parse(const char* name)
{
    if (!name || !*name) return -1;
    if (!strcasecmp(name, "normal"))                        return GMB_STATE_NORMAL;
    if (!strcasecmp(name, "hover") || !strcasecmp(name, "over"))  return GMB_STATE_HOVER;
    if (!strcasecmp(name, "press") || !strcasecmp(name, "pressed") || !strcasecmp(name, "active")) return GMB_STATE_PRESSED;
    if (!strcasecmp(name, "disabled") || !strcasecmp(name, "off")) return GMB_STATE_DISABLED;
    if (!strcasecmp(name, "selected") || !strcasecmp(name, "sel")) return GMB_STATE_SELECTED;
    if (!strcasecmp(name, "focused") || !strcasecmp(name, "focus")) return GMB_STATE_FOCUSED;
    return -1;
}

int gmb_theme_part_parse(const char* name)
{
    if (!name || !*name) return -1;
    if (!strcasecmp(name, "surface") || !strcasecmp(name, "background") || !strcasecmp(name, "bg")) return GMB_PART_SURFACE;
    if (!strcasecmp(name, "text") || !strcasecmp(name, "foreground") || !strcasecmp(name, "fg"))   return GMB_PART_TEXT;
    if (!strcasecmp(name, "border"))                                   return GMB_PART_BORDER;
    if (!strcasecmp(name, "focus"))                                    return GMB_PART_FOCUS;
    if (!strcasecmp(name, "accent"))                                   return GMB_PART_ACCENT;
    return -1;
}

const char* gmb_theme_role_name(int role)
{
    switch (role)
    {
    case GMB_ROLE_DEFAULT:   return "default";
    case GMB_ROLE_PRIMARY:   return "primary";
    case GMB_ROLE_SECONDARY: return "secondary";
    case GMB_ROLE_ACCENT:    return "accent";
    case GMB_ROLE_SUCCESS:   return "success";
    case GMB_ROLE_WARNING:   return "warning";
    case GMB_ROLE_DANGER:    return "danger";
    case GMB_ROLE_INFO:      return "info";
    case GMB_ROLE_MUTED:     return "muted";
    default:                 return "";
    }
}

/* ===================== derivazione dei colori ===================== */

/* Il tema è scuro? La superficie di riferimento è la finestra (fallback: dialog,
 * desktop) perché è su quella che i controlli vengono disegnati. */
static int theme_is_dark(const gmb_theme_t* t)
{
    gmb_cell_color_t s = t->window_bg;
    if (!has_col(s)) s = t->dlg_bg;
    if (!has_col(s)) s = t->desktop_bg;
    return gmb_theme_luminance(s) < 0.5;
}

/* Superficie/inchiostro/tinta del ruolo allo stato NORMAL. */
static void role_base(const gmb_theme_t* t, int role,
                      gmb_cell_color_t* surface, gmb_cell_color_t* ink, gmb_cell_color_t* tint)
{
    int dark = theme_is_dark(t);
    gmb_cell_color_t accent = has_col(t->accent) ? t->accent : crgb(37, 99, 235);
    gmb_cell_color_t win    = has_col(t->window_bg) ? t->window_bg : crgb(30, 30, 30);
    /* La sentinella "palette dialog derivata" è il BG, non il FG: il NERO è un
     * inchiostro valido per un dialog (stessa convenzione di
     * gmb_control_style_from_theme). */
    int has_dlg = has_col(t->dlg_bg) ? 1 : 0;
    gmb_cell_color_t dlg_bg = has_dlg ? t->dlg_bg : win;
    gmb_cell_color_t dlg_fg = has_dlg ? t->dlg_fg : t->text;

    switch (role)
    {
    case GMB_ROLE_PRIMARY:
        *surface = accent;
        *ink = gmb_theme_text_on(accent);
        *tint = accent;
        break;
    case GMB_ROLE_SECONDARY:
        *surface = has_col(t->active_bg) ? t->active_bg : gmb_theme_mix(accent, win, 0.55);
        *ink = gmb_theme_text_on(*surface);
        *tint = accent;
        break;
    case GMB_ROLE_ACCENT:
        *surface = has_col(t->title_focus) ? t->title_focus : accent;
        /* L'inchiostro è quello leggibile sulla superficie, non il title_fg del tema:
         * un `role=accent` deve restare leggibile anche su temi chiari (ambra, ecc.). */
        *ink = gmb_theme_text_on(*surface);
        *tint = accent;
        break;
    case GMB_ROLE_SUCCESS:
        *surface = dark ? crgb(80, 200, 120) : crgb(22, 130, 70);
        *ink = gmb_theme_text_on(*surface);
        *tint = *surface;
        break;
    case GMB_ROLE_WARNING:
        *surface = dark ? crgb(255, 193, 7) : crgb(150, 105, 0);
        *ink = gmb_theme_text_on(*surface);
        *tint = *surface;
        break;
    case GMB_ROLE_DANGER:
        *surface = dark ? crgb(255, 99, 99) : crgb(190, 36, 48);
        *ink = gmb_theme_text_on(*surface);
        *tint = *surface;
        break;
    case GMB_ROLE_INFO:
        *surface = dark ? crgb(90, 160, 255) : crgb(28, 100, 200);
        *ink = gmb_theme_text_on(*surface);
        *tint = *surface;
        break;
    case GMB_ROLE_MUTED:
        *surface = dlg_bg;
        *ink = has_col(t->dim) ? t->dim : gmb_theme_mix(*surface, gmb_theme_text_on(*surface), 0.45);
        *tint = *ink;
        break;
    default: /* GMB_ROLE_DEFAULT */
        *surface = dlg_bg;
        *ink = dlg_fg;
        *tint = accent;
        break;
    }
}

gmb_cell_color_t gmb_theme_role_color(const gmb_theme_t* t, int role, int state, int part)
{
    if (!t) return crgb(0, 0, 0);
    if (role <= GMB_ROLE_NONE || role > GMB_ROLE_MUTED) role = GMB_ROLE_DEFAULT;
    if (state < GMB_STATE_NORMAL || state > GMB_STATE_FOCUSED) state = GMB_STATE_NORMAL;
    if (part < GMB_PART_SURFACE || part > GMB_PART_ACCENT) part = GMB_PART_SURFACE;

    gmb_cell_color_t surface, ink, tint;
    role_base(t, role, &surface, &ink, &tint);

    int dark = theme_is_dark(t);
    gmb_cell_color_t win = has_col(t->window_bg) ? t->window_bg : surface;

    switch (state)
    {
    case GMB_STATE_HOVER:
        surface = gmb_theme_shade(surface, dark ? 1.12 : 0.92);
        break;
    case GMB_STATE_PRESSED:
        surface = gmb_theme_shade(surface, dark ? 0.80 : 0.76);
        break;
    case GMB_STATE_DISABLED:
        surface = gmb_theme_mix(surface, win, 0.60);
        ink = has_col(t->dim) ? t->dim : gmb_theme_mix(ink, win, 0.50);
        break;
    case GMB_STATE_SELECTED:
        surface = has_col(t->active_bg) ? t->active_bg : gmb_theme_mix(surface, tint, 0.50);
        /* Preset dove selezione e accento coincidono (Turbo Vision: blu su blu):
         * senza questo scatto la selezione sarebbe invisibile. */
        if (same(surface, tint)) surface = gmb_theme_shade(surface, dark ? 1.20 : 0.80);
        ink = gmb_theme_text_on(surface);
        break;
    default:
        break;
    }

    switch (part)
    {
    /* Le soglie vengono dai flag globali di accessibilità (include/gmb_a11y.h):
     * normali = AA (4.5 testo, 3.0 bordi/focus); `ui.high_contrast` le porta ad
     * AAA (7.0 testo) e bordi 4.5. Un solo punto, vale su TEXT e PIXEL. */
    case GMB_PART_TEXT:   return gmb_theme_ensure_contrast(ink, surface, gmb_a11y_min_text_ratio());
    case GMB_PART_BORDER: return gmb_theme_ensure_contrast(gmb_theme_mix(surface, ink, 0.45), win, gmb_a11y_min_border_ratio());
    case GMB_PART_FOCUS:  return gmb_theme_ensure_contrast(tint, surface, gmb_a11y_min_border_ratio());
    case GMB_PART_ACCENT: return tint;
    default:              return surface;
    }
}

void gmb_theme_apply_role_px(gmb_widget_style_t* s, const gmb_theme_t* t, int role)
{
    if (!s || !t || role <= GMB_ROLE_NONE) return;

    gmb_cell_color_t surface = gmb_theme_role_color(t, role, GMB_STATE_NORMAL,   GMB_PART_SURFACE);
    gmb_cell_color_t hover   = gmb_theme_role_color(t, role, GMB_STATE_HOVER,    GMB_PART_SURFACE);
    gmb_cell_color_t press   = gmb_theme_role_color(t, role, GMB_STATE_PRESSED,  GMB_PART_SURFACE);
    gmb_cell_color_t disab   = gmb_theme_role_color(t, role, GMB_STATE_DISABLED, GMB_PART_SURFACE);
    gmb_cell_color_t text    = gmb_theme_role_color(t, role, GMB_STATE_NORMAL,   GMB_PART_TEXT);
    gmb_cell_color_t textOff = gmb_theme_role_color(t, role, GMB_STATE_DISABLED, GMB_PART_TEXT);
    gmb_cell_color_t border  = gmb_theme_role_color(t, role, GMB_STATE_NORMAL,   GMB_PART_BORDER);
    gmb_cell_color_t accent  = gmb_theme_role_color(t, role, GMB_STATE_NORMAL,   GMB_PART_ACCENT);
    gmb_cell_color_t focus   = gmb_theme_role_color(t, role, GMB_STATE_FOCUSED,  GMB_PART_FOCUS);

    s->fill          = gmb_theme_rgba(surface, 0xFF);
    s->fill_hover    = gmb_theme_rgba(hover,   0xFF);
    s->fill_press    = gmb_theme_rgba(press,   0xFF);
    s->fill_disabled = gmb_theme_rgba(disab,   0xFF);
    s->text          = gmb_theme_rgba(text,    0xFF);
    s->text_disabled = gmb_theme_rgba(textOff, 0xFF);
    s->border        = gmb_theme_rgba(border,  0xFF);
    s->accent        = gmb_theme_rgba(accent,  0xFF);
    s->focus_bg      = gmb_theme_rgba(focus,   0xFF);
    s->focus_fg      = gmb_theme_rgba(gmb_theme_text_on(focus), 0xFF);

    /* Il ruolo è SEMANTICO: forma, ombre, bevel e raggi già scelti restano; un
     * gradiente attivo viene solo ritinto sui colori del ruolo. */
    if (s->gradient_on) { s->grad_a = s->fill_hover; s->grad_b = s->fill_press; }
}
