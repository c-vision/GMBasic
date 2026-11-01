/*
 * gmb_signal.c — bus dei segnali (namespace SIGNAL). Vedi gmb_signal.h.
 *
 * Una sola implementazione (runtime condiviso, §0bis): la usano C99Native e la VM/C.
 * Nessuna duplicazione del filtro nei backend: il match vive qui.
 */
#include "gmb_signal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define GMB_SIGNAL_MAX 64

typedef struct {
    int used;
    char id[40];
    char types[256];
    char asset[64];
    char market[64];
    char symbols[512];
    char timeframe[16];
    gmb_signal_fn4 fn4;
    gmb_signal_fn5 fn5;
} gsignal_sub;

static gsignal_sub g_sig[GMB_SIGNAL_MAX];

static void copy_trunc(char *dst, size_t cap, const char *src)
{
    size_t n;
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* Uguaglianza case-insensitive; NULL o "" = filtro assente (tutto ammesso). */
static int eq_or_any(const char *filter, const char *value)
{
    size_t i;
    if (!filter || !filter[0]) return 1;
    if (!value) value = "";
    for (i = 0; filter[i] || value[i]; i++) {
        unsigned char a = (unsigned char)filter[i], b = (unsigned char)value[i];
        if (tolower(a) != tolower(b)) return 0;
    }
    return 1;
}

/* Il valore è una voce del CSV (separatore ',', spazi ai bordi ignorati)? */
static int csv_has(const char *csv, const char *value)
{
    const char *p = csv;
    if (!csv || !csv[0]) return 1;
    if (!value) value = "";
    while (*p) {
        const char *start, *end, *q;
        size_t len;
        while (*p == ' ' || *p == '\t') p++;
        start = p;
        while (*p && *p != ',') p++;
        end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        len = (size_t)(end - start);
        q = value;
        {
            size_t k = 0;
            int same = 1;
            while (k < len && q[k]) {
                if (tolower((unsigned char)start[k]) != tolower((unsigned char)q[k])) { same = 0; break; }
                k++;
            }
            if (same && k == len && q[k] == 0) return 1;
        }
        if (*p == ',') p++;
    }
    return 0;
}

static gsignal_sub *find_sub(const char *id)
{
    int i;
    if (!id || !id[0]) return NULL;
    for (i = 0; i < GMB_SIGNAL_MAX; i++)
        if (g_sig[i].used && strcmp(g_sig[i].id, id) == 0) return &g_sig[i];
    return NULL;
}

void gmb_signal_subscribe(const char *id, const char *types, const char *asset, const char *market,
                          const char *symbols, const char *timeframe)
{
    gsignal_sub *s = find_sub(id);
    int i;
    if (!id || !id[0]) return;
    if (!s) {
        for (i = 0; i < GMB_SIGNAL_MAX; i++) if (!g_sig[i].used) { s = &g_sig[i]; break; }
        if (!s) return;                       /* slot esauriti: sottoscrizione non registrata */
        memset(s, 0, sizeof(*s));
        s->used = 1;
    }
    copy_trunc(s->id, sizeof(s->id), id);
    copy_trunc(s->types, sizeof(s->types), types);
    copy_trunc(s->asset, sizeof(s->asset), asset);
    copy_trunc(s->market, sizeof(s->market), market);
    copy_trunc(s->symbols, sizeof(s->symbols), symbols);
    copy_trunc(s->timeframe, sizeof(s->timeframe), timeframe);
}

void gmb_signal_on(const char *id, gmb_signal_fn4 fn4, gmb_signal_fn5 fn5)
{
    gsignal_sub *s = find_sub(id);
    if (!s) return;
    s->fn4 = fn4;
    s->fn5 = fn5;
}

int gmb_signal_emit(const char *type, const char *symbol, double value, const char *asset,
                    const char *market, const char *timeframe)
{
    int i, hits = 0;
    if (!type) type = "";
    if (!symbol) symbol = "";
    for (i = 0; i < GMB_SIGNAL_MAX; i++) {
        gsignal_sub *s = &g_sig[i];
        if (!s->used || (!s->fn4 && !s->fn5)) continue;
        if (!csv_has(s->types, type)) continue;
        if (!csv_has(s->symbols, symbol)) continue;
        if (!eq_or_any(s->asset, asset)) continue;
        if (!eq_or_any(s->market, market)) continue;
        if (!eq_or_any(s->timeframe, timeframe)) continue;
        if (s->fn5) s->fn5(s->id, type, symbol, value, timeframe ? timeframe : "");
        else s->fn4(s->id, type, symbol, value);
        hits++;
    }
    return hits;
}

void gmb_signal_unsubscribe(const char *id)
{
    int i;
    if (!id || !id[0]) { memset(g_sig, 0, sizeof(g_sig)); return; }
    for (i = 0; i < GMB_SIGNAL_MAX; i++)
        if (g_sig[i].used && strcmp(g_sig[i].id, id) == 0) { memset(&g_sig[i], 0, sizeof(g_sig[i])); return; }
}

void gmb_signal_ping(void)
{
    /* Bus locale: nessun keepalive da inviare (parità con gmb_msg_ping su bus locale). */
}

int gmb_signal_count(void)
{
    int i, n = 0;
    for (i = 0; i < GMB_SIGNAL_MAX; i++) if (g_sig[i].used) n++;
    return n;
}
