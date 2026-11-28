/* ============================================================================
 * gmb_color_filter.c — filtro colore per la daltonia (vedi gmb_color_filter.h).
 *
 * Matrici 3x3 su RGB lineare-scalato (0..255), grado 1.0, da:
 *   Machado, Oliveira & Fernandes, "A Physiologically-based Model for
 *   Simulation of Color Vision Deficiency", IEEE TVCG 15(6), 2009.
 *
 * Perché matrici e non un modello LMS completo: al grado 1.0 il modello si
 * riduce a una trasformazione lineare dell'RGB sRGB (già gamma-encoded) e la
 * resa clinica è la stessa; qui conta che sia PURA, deterministica, O(1) e
 * identica sulle 4 gambe — non che simuli la fisiologia dell'occhio.
 *
 * Il clamp finale è obbligatorio: le righe non sommano a 1 (per conservare la
 * luminanza di riferimento) e un colore saturo può uscire dall'intervallo.
 * ============================================================================ */
#include "gmb_color_filter.h"
#include "gmb_runtime.h"   /* gmb_opt_get: registro opzioni condiviso */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef struct { double m[3][3]; const char* name; } cvf_matrix_t;

/* Indice = GMB_CVF_*. L'identità è esplicita: nessun ramo speciale per "none". */
static const cvf_matrix_t g_matrices[] = {
    { { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } }, "none" },
    /* deuteranopia */
    { { { 0.367322, 0.860646, -0.227968 },
        { 0.280085, 0.672501,  0.047413 },
        {-0.011820, 0.042940,  0.968881 } }, "deuteranopia" },
    /* protanopia */
    { { { 0.152286, 1.052583, -0.204868 },
        { 0.114503, 0.786281,  0.099216 },
        {-0.003882, -0.048116, 1.051998 } }, "protanopia" },
    /* tritanopia */
    { { { 1.255528, -0.076749, -0.178779 },
        {-0.078411, 0.930809,  0.147602 },
        { 0.004733, 0.691367,  0.303900 } }, "tritanopia" },
    /* achromatopsia: luminanza Rec.709 (nessun colore, contrasto intatto) */
    { { { 0.2126, 0.7152, 0.0722 },
        { 0.2126, 0.7152, 0.0722 },
        { 0.2126, 0.7152, 0.0722 } }, "achromatopsia" }
};
#define CVF_NMODES ((int)(sizeof g_matrices / sizeof g_matrices[0]))

int gmb_color_filter_parse(const char* name)
{
    if (!name || !*name) return GMB_CVF_NONE;
    /* Spento ESPLICITO: stessa semantica delle altre voci del registro. */
    if (!strcasecmp(name, "none") || !strcasecmp(name, "off") || !strcasecmp(name, "false")
        || !strcasecmp(name, "no")   || !strcasecmp(name, "spento") || !strcasecmp(name, "0"))
        return GMB_CVF_NONE;
    if (!strncasecmp(name, "deuteranopia", strlen(name)) && strlen(name) >= 3)
        return GMB_CVF_DEUTERANOPIA;    /* "deut", "deuterano" */
    if (!strncasecmp(name, "protanopia", strlen(name)) && strlen(name) >= 3)
        return GMB_CVF_PROTANOPIA;
    if (!strncasecmp(name, "tritanopia", strlen(name)) && strlen(name) >= 3)
        return GMB_CVF_TRITANOPIA;
    if (!strncasecmp(name, "achromatopsia", strlen(name)) && strlen(name) >= 3)
        return GMB_CVF_ACHROMATOPSIA;
    if (!strcasecmp(name, "gray") || !strcasecmp(name, "grey") || !strcasecmp(name, "mono")
        || !strcasecmp(name, "monochrome"))
        return GMB_CVF_ACHROMATOPSIA;
    if (!strcasecmp(name, "red") || !strcasecmp(name, "rosso"))    return GMB_CVF_PROTANOPIA;
    if (!strcasecmp(name, "green") || !strcasecmp(name, "verde"))  return GMB_CVF_DEUTERANOPIA;
    if (!strcasecmp(name, "blue") || !strcasecmp(name, "blu"))     return GMB_CVF_TRITANOPIA;
    return -1;   /* sconosciuto: il chiamante decide (qui NON si indovina) */
}

const char* gmb_color_filter_name(int mode)
{
    if (mode < 0 || mode >= CVF_NMODES) return "none";
    return g_matrices[mode].name;
}

/* ---- stato: cache del registro (come gmb_a11y_get, stesso pattern) --------- */
static int  g_mode = GMB_CVF_NONE;
static char g_seen[32] = "\x01";

/* Cambio di modo → il frame va rifatto (nessun controllo cambiato: il colore sì).
 * Hook opzionale registrato dal layer UI; se nessuno lo registra vale NULL. */
static void (*g_on_change)(void) = NULL;

void gmb_color_filter_on_change(void (*fn)(void)) { g_on_change = fn; }

static void cvf_changed(void)
{
    if (g_on_change) g_on_change();
    /* Anche la pagina grafica è da ripresentare: il prossimo present la ricompone. */
    { extern void gmb_form_invalidate_all(void); gmb_form_invalidate_all(); }
}

static void cvf_refresh(void)
{
    char v[32];
    v[0] = '\0';
    (void)gmb_opt_get(GMB_COLOR_FILTER_OPT, v, (int)sizeof v);
    if (!strcmp(v, g_seen)) return;
    snprintf(g_seen, sizeof g_seen, "%s", v);
    int m = gmb_color_filter_parse(v);
    if (m < 0) m = GMB_CVF_NONE;   /* valore sconosciuto = spento, mai un filtro "a caso" */
    if (m != g_mode) { g_mode = m; cvf_changed(); }
}

int gmb_color_filter_mode(void) { cvf_refresh(); return g_mode; }
int gmb_color_filter_active(void) { cvf_refresh(); return g_mode != GMB_CVF_NONE; }

void gmb_color_filter_set(int mode)
{
    /* mode < 0 = torna a leggere il registro (il registro resta l'unica verità). */
    if (mode < 0) { g_seen[0] = '\x01'; cvf_refresh(); }
    else {
        if (mode >= CVF_NMODES) mode = GMB_CVF_NONE;
        /* Il valore forzato vale FINCHÉ l'opzione del registro non cambia: si registra
         * quello visto adesso, così una scritta successiva l'opzione lo scavalca. */
        char v[32];
        v[0] = '\0';
        (void)gmb_opt_get(GMB_COLOR_FILTER_OPT, v, (int)sizeof v);
        snprintf(g_seen, sizeof g_seen, "%s", v);
        g_mode = mode;
    }
    cvf_changed();
}

/* ---- trasformazione pura -------------------------------------------------- */
static uint8_t clamp255(double x)
{
    if (x <= 0.0) return 0;
    if (x >= 255.0) return 255;
    return (uint8_t)(x + 0.5);
}

gmb_cell_color_t gmb_color_filter_cell(gmb_cell_color_t c, int mode)
{
    if (mode <= GMB_CVF_NONE || mode >= CVF_NMODES) return c;
    if (c.is_default) return c;   /* colore del terminale: non è un RGB nostro */
    const double (*m)[3] = g_matrices[mode].m;
    gmb_cell_color_t o;
    o.r = clamp255(m[0][0] * c.r + m[0][1] * c.g + m[0][2] * c.b);
    o.g = clamp255(m[1][0] * c.r + m[1][1] * c.g + m[1][2] * c.b);
    o.b = clamp255(m[2][0] * c.r + m[2][1] * c.g + m[2][2] * c.b);
    o.is_default = 0;
    return o;
}

uint32_t gmb_color_filter_rgba(uint32_t rgb, int mode)
{
    if (mode <= GMB_CVF_NONE || mode >= CVF_NMODES) return rgb;
    /* I colori del percorso grafico sono 0xAARRGGBB: l'alpha (0x40 = superficie
     * traslucida dei temi "glass") NON è colore e va CONSERVATO, altrimenti il
     * filtro renderebbe trasparenti/opache le superfici. Si trasforma solo RGB. */
    uint32_t keep = rgb & 0xFF000000u;
    gmb_cell_color_t c;
    c.r = (uint8_t)((rgb >> 16) & 0xFF);
    c.g = (uint8_t)((rgb >> 8) & 0xFF);
    c.b = (uint8_t)(rgb & 0xFF);
    c.is_default = 0;
    gmb_cell_color_t o = gmb_color_filter_cell(c, mode);
    return keep | ((uint32_t)o.r << 16) | ((uint32_t)o.g << 8) | (uint32_t)o.b;
}

uint32_t gmb_color_filter_u32(unsigned int rgb, int mode)
{
    return gmb_color_filter_rgba((uint32_t)(rgb & 0xFFFFFFu), mode);
}

gmb_cell_color_t gmb_color_filter_cell_now(gmb_cell_color_t c)
{
    cvf_refresh();
    return gmb_color_filter_cell(c, g_mode);
}

uint32_t gmb_color_filter_rgba_now(uint32_t rgb)
{
    cvf_refresh();
    return gmb_color_filter_rgba(rgb, g_mode);
}