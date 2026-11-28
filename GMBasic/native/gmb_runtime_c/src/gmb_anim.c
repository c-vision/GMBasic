/* ============================================================================
 * gmb_anim.c — motore di animazione tick-driven (vedi gmb_anim.h).
 *
 * Stato SOLO dati: nessun thread, nessun timer, nessuna attesa. Il frame chiama
 * gmb_anim_tick() e poi applica i valori ai controlli. La durata è in ms e
 * l'avanzamento usa il tempo di parete di gmb_ticks_ms: un'app ferma non
 * produce passi falsi, e al primo tick dopo una pausa l'animazione si chiude
 * (il valore va al target, non resta a metà).
 *
 * Perché non interpolare nei widget: il valore animato è una proprietà del
 * controllo (x/y/width/height), quindi la stessa animazione serve TUI e pixel.
 * ============================================================================ */
#include "gmb_anim.h"
#include "gmb_a11y.h"
#include "gmb_runtime.h"   /* gmb_ticks_ms */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    int used;
    int done;          /* raggiunto `to`: applicato in questo frame, liberato al prossimo */
    int handle;
    int is_tui;
    int prop;
    int easing;
    double from, to, value;
    int dur_ms, elapsed_ms;
} anim_slot;

static anim_slot g_slots[GMB_ANIM_MAX];
static long long g_last_ms = -1;

static double ease(int kind, double t)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    switch (kind) {
        case GMB_ANIM_EASE_OUT:    return 1.0 - (1.0 - t) * (1.0 - t);
        case GMB_ANIM_EASE_IN_OUT: return t < 0.5 ? 2.0 * t * t : 1.0 - 2.0 * (1.0 - t) * (1.0 - t);
        default:                   return t;
    }
}

int gmb_anim_prop_from_name(const char* name)
{
    if (!name) return -1;
    if (!strcasecmp(name, "left")   || !strcasecmp(name, "x")) return GMB_ANIM_LEFT;
    if (!strcasecmp(name, "top")    || !strcasecmp(name, "y")) return GMB_ANIM_TOP;
    if (!strcasecmp(name, "width")  || !strcasecmp(name, "w")) return GMB_ANIM_WIDTH;
    if (!strcasecmp(name, "height") || !strcasecmp(name, "h")) return GMB_ANIM_HEIGHT;
    return -1;
}

int gmb_anim_parse_spec(const char* spec, int* prop, double* to, int* ms, int* easing)
{
    if (!spec || !*spec || !prop || !to || !ms || !easing) return 0;
    char buf[128];
    snprintf(buf, sizeof buf, "%s", spec);
    char* sv = NULL;
    char* p = strtok_r(buf, ":", &sv);
    char* v = strtok_r(NULL, ":", &sv);
    char* d = strtok_r(NULL, ":", &sv);
    char* e = strtok_r(NULL, ":", &sv);
    if (!p || !v || !d) return 0;
    int pr = gmb_anim_prop_from_name(p);
    if (pr < 0) return 0;
    *prop = pr;
    *to = atof(v);
    *ms = atoi(d);
    if (*ms < 0) *ms = 0;
    *easing = GMB_ANIM_EASE_LINEAR;
    if (e && *e) {
        if (!strcasecmp(e, "out") || !strcasecmp(e, "easeout")) *easing = GMB_ANIM_EASE_OUT;
        else if (!strcasecmp(e, "inout") || !strcasecmp(e, "easeinout")) *easing = GMB_ANIM_EASE_IN_OUT;
    }
    return 1;
}

int gmb_anim_start(int handle, int is_tui, int prop, double from, double to, int ms, int easing)
{
    if (prop < 0 || prop > GMB_ANIM_HEIGHT) return -1;
    if (ms < 0) ms = 0;
    /* Stessa proprietà dello stesso controllo: la nuova animazione SOSTITUISCE
     * quella in corso (non si accodano e non si contendono il rect). */
    int idx = -1;
    for (int i = 0; i < GMB_ANIM_MAX; i++) {
        anim_slot* s = &g_slots[i];
        if (s->used && s->handle == handle && s->is_tui == (is_tui ? 1 : 0) && s->prop == prop) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < GMB_ANIM_MAX; i++) {
            if (!g_slots[i].used) { idx = i; break; }
        }
    }
    if (idx < 0) return -1;

    anim_slot* s = &g_slots[idx];
    memset(s, 0, sizeof(*s));
    s->used = 1;
    s->handle = handle;
    s->is_tui = is_tui ? 1 : 0;
    s->prop = prop;
    s->easing = easing;
    s->from = from;
    s->to = to;
    s->value = ms > 0 ? from : to;
    s->dur_ms = ms;
    s->elapsed_ms = 0;
    s->done = (ms <= 0);
    return idx;
}

int gmb_anim_advance(int dt_ms)
{
    int active = 0;
    if (dt_ms < 0) dt_ms = 0;
    for (int i = 0; i < GMB_ANIM_MAX; i++) {
        anim_slot* s = &g_slots[i];
        if (!s->used) continue;
        /* Lo slot concluso nel frame precedente è stato già applicato: via. */
        if (s->done) { memset(s, 0, sizeof(*s)); continue; }
        s->elapsed_ms += dt_ms;
        double t = s->dur_ms > 0 ? (double)s->elapsed_ms / (double)s->dur_ms : 1.0;
        if (t >= 1.0) {
            s->value = s->to;
            s->done = 1;
        } else {
            s->value = s->from + (s->to - s->from) * ease(s->easing, t);
        }
        active++;
    }
    return active;
}

void gmb_anim_tick(void)
{
    long long now = (long long)gmb_ticks_ms();
    int dt = 0;
    if (g_last_ms >= 0 && now > g_last_ms) dt = (int)(now - g_last_ms);
    g_last_ms = now;
    gmb_anim_advance(dt);
}

int gmb_anim_active(void)
{
    for (int i = 0; i < GMB_ANIM_MAX; i++) if (g_slots[i].used) return 1;
    return 0;
}

int gmb_anim_count(void)
{
    int n = 0;
    for (int i = 0; i < GMB_ANIM_MAX; i++) if (g_slots[i].used) n++;
    return n;
}

int gmb_anim_active_for(int handle, int is_tui)
{
    for (int i = 0; i < GMB_ANIM_MAX; i++) {
        const anim_slot* s = &g_slots[i];
        if (s->used && s->handle == handle && s->is_tui == (is_tui ? 1 : 0)) return 1;
    }
    return 0;
}

int gmb_anim_get(int index, int* handle, int* is_tui, int* prop, int* done, double* value)
{
    if (index < 0 || index >= GMB_ANIM_MAX || !g_slots[index].used) return 0;
    const anim_slot* s = &g_slots[index];
    if (handle) *handle = s->handle;
    if (is_tui) *is_tui = s->is_tui;
    if (prop)   *prop = s->prop;
    if (done)   *done = s->done;
    if (value)  *value = s->value;
    return 1;
}

void gmb_anim_clear(void)
{
    memset(g_slots, 0, sizeof(g_slots));
    g_last_ms = -1;
}

void gmb_anim_forget(int handle, int is_tui)
{
    for (int i = 0; i < GMB_ANIM_MAX; i++) {
        anim_slot* s = &g_slots[i];
        if (s->used && s->handle == handle && s->is_tui == (is_tui ? 1 : 0))
            memset(s, 0, sizeof(*s));
    }
}
