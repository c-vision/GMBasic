/* gmb_memstats.c — contabilità della memoria viva per TIPO (diagnostica, opzione
 * `debug.memory`). Vedi gmb_memstats.h per il contratto e i limiti dichiarati.
 *
 * Tabella hash aperta (ptr -> byte), cresce a fattore di carico 0.7. Quando la
 * contabilità è spenta nessuna funzione fa lavoro: le chiamate sono protette dal flag
 * `g_on` nei punti di alloc/free del runtime.
 */
#include "gmb_memstats.h"
#include "gmb_runtime.h"   /* gmb_opt_* + gmb_opt_attach: registro opzioni */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GMB_MEM_SLOTS_MIN 4096

typedef struct {
    void*  ptr;
    size_t bytes;
    int    kind;
} mem_slot_t;

static mem_slot_t* g_slots = NULL;
static size_t g_nslots = 0;       /* capacità (potenza di 2) */
static size_t g_nused = 0;        /* slot occupati */
static int    g_on = -1;          /* -1 = non ancora letto */

static size_t g_live_blocks[GMB_MEM_KIND_COUNT];
static size_t g_live_bytes[GMB_MEM_KIND_COUNT];
static size_t g_peak_blocks[GMB_MEM_KIND_COUNT];
static size_t g_peak_bytes[GMB_MEM_KIND_COUNT];
static int    g_report_registered = 0;

static const char* kind_name(int kind)
{
    switch (kind) {
    case GMB_MEM_STRING: return "string";
    case GMB_MEM_LIST:   return "list";
    case GMB_MEM_DICT:   return "dict";
    default:             return "other";
    }
}

static size_t hash_ptr(const void* p)
{
    unsigned long long v = (unsigned long long)(size_t)p;
    v ^= v >> 33; v *= 0xFF51AFD7ED558CCDULL;
    v ^= v >> 33; v *= 0xC4CEB9FE1A85EC53ULL;
    v ^= v >> 33;
    return (size_t)v;
}

static void slots_alloc(size_t n)
{
    free(g_slots);
    g_slots = (mem_slot_t*)calloc(n, sizeof(mem_slot_t));
    g_nslots = g_slots ? n : 0;
    g_nused = 0;
}

static mem_slot_t* slot_find(void* p)
{
    if (!g_slots || !p) return NULL;
    size_t mask = g_nslots - 1;
    size_t i = hash_ptr(p) & mask;
    for (size_t probe = 0; probe < g_nslots; probe++) {
        mem_slot_t* s = &g_slots[i];
        if (!s->ptr) return NULL;
        if (s->ptr == p) return s;
        i = (i + 1) & mask;
    }
    return NULL;
}

/* Inserisce in uno slot libero (la tabella non è MAI piena: si cresce prima). */
static mem_slot_t* slot_insert(void* p)
{
    if (!g_slots) return NULL;
    size_t mask = g_nslots - 1;
    size_t i = hash_ptr(p) & mask;
    for (size_t probe = 0; probe < g_nslots; probe++) {
        mem_slot_t* s = &g_slots[i];
        if (!s->ptr || s->ptr == p) return s;
        i = (i + 1) & mask;
    }
    return NULL;
}

static void maybe_grow(void)
{
    if (!g_slots) { slots_alloc(GMB_MEM_SLOTS_MIN); return; }
    if (g_nused + 1 < (g_nslots * 7) / 10) return;
    size_t ncap = g_nslots * 2;
    mem_slot_t* old = g_slots;
    size_t on = g_nslots;
    g_slots = (mem_slot_t*)calloc(ncap, sizeof(mem_slot_t));
    if (!g_slots) { g_slots = old; return; }
    g_nslots = ncap;
    g_nused = 0;
    for (size_t i = 0; i < on; i++) {
        if (!old[i].ptr) continue;
        mem_slot_t* s = slot_insert(old[i].ptr);
        if (s) { s->ptr = old[i].ptr; s->bytes = old[i].bytes; s->kind = old[i].kind; g_nused++; }
    }
    free(old);
}

static void bump_peak(int kind)
{
    if (g_live_blocks[kind] > g_peak_blocks[kind]) g_peak_blocks[kind] = g_live_blocks[kind];
    if (g_live_bytes[kind] > g_peak_bytes[kind]) g_peak_bytes[kind] = g_live_bytes[kind];
}

static void mem_on_set(const char* value);

int gmb_mem_on(void)
{
    if (g_on >= 0) return g_on;
    char v[64];
    v[0] = 0;
    if (gmb_opt_get("debug.memory", v, sizeof v) != 0) v[0] = 0;
    g_on = (v[0] && strcmp(v, "0") && strcasecmp(v, "off") && strcasecmp(v, "false") && strcasecmp(v, "no")) ? 1 : 0;
    if (g_on && !g_report_registered) {
        g_report_registered = 1;
        atexit(gmb_mem_report);
    }
    /* Il programma può ACCENDERE la voce dopo le prime allocazioni (ordine dei statement):
     * il setter aggiorna il flag, così la contabilità parte da `AI SET OPTION` in poi. */
    gmb_opt_attach("debug.memory", mem_on_set);
    return g_on;
}

static void mem_on_set(const char* value)
{
    int on = (value && value[0] && strcmp(value, "0")
              && strcasecmp(value, "off") && strcasecmp(value, "false") && strcasecmp(value, "no")) ? 1 : 0;
    g_on = on;
    if (on && !g_report_registered) {
        g_report_registered = 1;
        atexit(gmb_mem_report);
    }
}

void gmb_mem_track(void* p, size_t bytes, int kind)
{
    if (!p || kind < 0 || kind >= GMB_MEM_KIND_COUNT) return;
    maybe_grow();
    mem_slot_t* s = slot_insert(p);
    if (!s) return;
    if (!s->ptr) g_nused++;
    s->ptr = p;
    s->bytes = bytes;
    s->kind = kind;
    g_live_blocks[kind]++;
    g_live_bytes[kind] += bytes;
    bump_peak(kind);
}

void gmb_mem_resize(void* oldp, void* newp, size_t new_bytes, int kind)
{
    if (oldp == newp) {
        mem_slot_t* s = slot_find(oldp);
        if (s) {
            g_live_bytes[s->kind] -= s->bytes;
            s->bytes = new_bytes;
            g_live_bytes[s->kind] += new_bytes;
            bump_peak(s->kind);
        }
        return;
    }
    gmb_mem_untrack(oldp);
    gmb_mem_track(newp, new_bytes, kind);
}

void gmb_mem_untrack(void* p)
{
    if (!p || !g_slots) return;
    mem_slot_t* s = slot_find(p);
    if (!s) return;                       /* non tracciato: nessun effetto */
    int k = s->kind;
    if (g_live_blocks[k] > 0) g_live_blocks[k]--;
    g_live_bytes[k] = (g_live_bytes[k] >= s->bytes) ? g_live_bytes[k] - s->bytes : 0;
    s->ptr = NULL;
    s->bytes = 0;
    s->kind = 0;
    if (g_nused > 0) g_nused--;
}

void gmb_mem_report(void)
{
    if (!gmb_mem_on()) return;
    for (int k = 0; k < GMB_MEM_KIND_COUNT; k++) {
        fprintf(stderr, "[MEM] %-6s vivi %zu blocchi / %zu B (picco %zu / %zu B)\n",
                kind_name(k), g_live_blocks[k], g_live_bytes[k], g_peak_blocks[k], g_peak_bytes[k]);
    }
    fflush(stderr);
}
