/* ============================================================================
 * gmb_a11y.c — flag globali di accessibilità (vedi include/gmb_a11y.h).
 *
 * La SORGENTE è il registro opzioni condiviso (`gmb_opts.c`): qui non si tiene
 * uno stato separato che possa divergere, si tiene solo l'ULTIMO valore visto,
 * per non rifare quattro letture del registro a ogni cella disegnata.
 *
 * Perché la rilettura è a confronto di stringhe e non un "sono pronto" una
 * volta sola: le voci possono essere impostate PRIMA che questo modulo sia
 * chiamato (gerarchia dei settings all'avvio, `AI SET OPTION` all'inizio del
 * programma) e DOPO (a runtime). Confrontare i valori rende l'ordine
 * irrilevante e non richiede callback registrate: il registro è l'unica
 * verità, qui c'è solo la sua forma tipizzata.
 * ============================================================================ */
#include "gmb_a11y.h"
#include "gmb_runtime.h"   /* gmb_opt_get: registro opzioni condiviso */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static gmb_a11y_t g_a11y;
/* Ultimi valori testuali letti: se il registro non è cambiato non si rielabora. */
static char g_seen_hc[32] = "\x01";
static char g_seen_ts[32] = "\x01";
static char g_seen_rm[32] = "\x01";
static char g_seen_fv[32] = "\x01";

int gmb_a11y_parse_bool(const char* value)
{
    if (!value || !*value) return 0;
    if (!strcasecmp(value, "0") || !strcasecmp(value, "off") || !strcasecmp(value, "false")
        || !strcasecmp(value, "no") || !strcasecmp(value, "none") || !strcasecmp(value, "spento"))
        return 0;
    if (!strcasecmp(value, "1") || !strcasecmp(value, "on") || !strcasecmp(value, "true")
        || !strcasecmp(value, "yes") || !strcasecmp(value, "si") || !strcasecmp(value, "attivo"))
        return 1;
    return atoi(value) > 0 ? 1 : 0;   /* "2", "10" = acceso; spazzatura = spento */
}

double gmb_a11y_parse_scale(const char* value)
{
    if (!value || !*value) return 1.0;
    double s = atof(value);
    if (!(s > 0.0)) return 1.0;        /* vuoto, 0, negativo o non numerico (NaN) = default */
    if (s < 0.75) s = 0.75;
    if (s > 3.0)  s = 3.0;
    return s;
}

/* Legge una voce del registro in `out`; voce assente = stringa vuota (= default).
 * Ritorna 1 se il valore testuale è DIVERSO da quello già visto. */
static int a11y_read_one(const char* name, char* out, size_t cap, char* seen, size_t seencap)
{
    out[0] = '\0';
    (void)gmb_opt_get(name, out, (int)cap);
    if (!strcmp(out, seen)) return 0;
    snprintf(seen, seencap, "%s", out);
    return 1;
}

gmb_a11y_t gmb_a11y_get(void)
{
    char v[32];

    if (a11y_read_one(GMB_A11Y_OPT_HIGH_CONTRAST, v, sizeof v, g_seen_hc, sizeof g_seen_hc))
        g_a11y.high_contrast = gmb_a11y_parse_bool(v);
    if (a11y_read_one(GMB_A11Y_OPT_TEXT_SCALE, v, sizeof v, g_seen_ts, sizeof g_seen_ts))
        g_a11y.text_scale = gmb_a11y_parse_scale(v);
    if (a11y_read_one(GMB_A11Y_OPT_REDUCE_MOTION, v, sizeof v, g_seen_rm, sizeof g_seen_rm))
        g_a11y.reduce_motion = gmb_a11y_parse_bool(v);
    if (a11y_read_one(GMB_A11Y_OPT_FOCUS_VISIBLE, v, sizeof v, g_seen_fv, sizeof g_seen_fv))
        g_a11y.focus_always_visible = gmb_a11y_parse_bool(v);

    return g_a11y;
}

int gmb_a11y_motion(void) { return gmb_a11y_get().reduce_motion ? 0 : 1; }

int gmb_a11y_scale_px(int base)
{
    double s;
    int r;
    if (base <= 0) return base;        /* "automatico/ereditato": 0 non è una dimensione */
    s = gmb_a11y_get().text_scale;
    if (s == 1.0) return base;
    r = (int)((double)base * s + 0.5);
    return r < 1 ? 1 : r;
}

double gmb_a11y_min_text_ratio(void)   { return gmb_a11y_get().high_contrast ? 7.0 : 4.5; }
double gmb_a11y_min_border_ratio(void) { return gmb_a11y_get().high_contrast ? 4.5 : 3.0; }
