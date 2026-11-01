/* ============================================================================
 * gmb_hpack.c — HPACK (RFC 7541) per il server HTTP/2.
 *
 * Copre ciò che serve a un SERVER:
 *  - DECODIFICA dei blocchi di header del client: campi indicizzati (tabella statica e
 *    dinamica), letterali con/senza indicizzazione, "never indexed", aggiornamento della
 *    dimensione della tabella dinamica, stringhe con e senza Huffman;
 *  - CODIFICA delle risposte: `:status` (indice statico quando c'è), header noti con nome
 *    indicizzato, valori in chiaro (Huffman in codifica non è obbligatorio — RFC 7541 §5.2 —
 *    e non averlo evita un encoder intero).
 *
 * Tabelle: gmb_hpack_static.h (61 voci) e gmb_hpack_huffman.h (257 codici) sono GENERATE
 * dalla RFC da tools/py/gen_hpack_huffman.py.
 * ============================================================================ */
#include "gmb_hpack.h"
#include "gmb_hpack_static.h"
#include "gmb_hpack_huffman.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HPACK_MAX_ENTRIES 128
#define HPACK_ENTRY_OVERHEAD 32

typedef struct {
    char* name;
    size_t name_len;
    char* value;
    size_t value_len;
    size_t size;        /* 32 + len(name) + len(value) (RFC §4.1) */
} hp_ent_t;

struct gmb_hpack_ctx {
    hp_ent_t dyn[HPACK_MAX_ENTRIES];   /* dyn[0] = voce più recente */
    int dyn_count;
    size_t dyn_bytes;
    size_t max_size;        /* limite corrente (4096 di default) */
};

/* ── Tabella statica (indici 1..61) ─────────────────────────────────────────── */
static const gmb_hpack_static_t* hp_static(size_t idx)
{
    if (idx < 1 || idx > 61) return NULL;
    return &gmb_hpack_static[idx];
}

/* ── Huffman: tabella a 12 bit costruita una volta sola ─────────────────────── */
static int16_t g_huff12[4096];
static int g_huff_ready;

static void huff_init(void)
{
    int s, k;
    if (g_huff_ready) return;
    for (k = 0; k < 4096; k++) g_huff12[k] = -1;
    for (s = 0; s < 257; s++)
    {
        int len = gmb_hpack_huffman[s].bits;
        int base;
        if (len > 12) continue;
        base = (int)gmb_hpack_huffman[s].code << (12 - len);
        for (k = 0; k < (1 << (12 - len)); k++) g_huff12[base + k] = (int16_t)s;
    }
    g_huff_ready = 1;
}

/* Decodifica Huffman (RFC §5.2). Ritorna 0 = ok, -1 = dati non validi. */
static int hp_huffman_decode(const unsigned char* in, size_t in_len, char** out, size_t* out_len)
{
    size_t cap, olen = 0, i = 0;
    char* o;
    unsigned long long acc = 0;   /* bit non ancora consumati, allineati ai bit ALTI di `nbits` */
    int nbits = 0;

    huff_init();
    cap = in_len * 2 + 32;
    o = (char*)malloc(cap);
    if (!o) return -1;

    for (;;)
    {
        int progressed = 0;

        /* carica un byte se serve e se c'è (l'accumulatore 64-bit regge 40+ bit) */
        if (i < in_len && nbits <= 32)
        {
            acc = (acc << 8) | (unsigned long long)in[i++];
            nbits += 8;
        }

        for (;;)
        {
            int sym = -1, len = 0;

            if (nbits >= 12)
            {
                unsigned int peek = (unsigned int)((acc >> (nbits - 12)) & 0xFFFu);
                if (g_huff12[peek] >= 0)
                {
                    sym = g_huff12[peek];
                    len = gmb_hpack_huffman[sym].bits;
                }
            }
            if (sym < 0)
            {
                /* Codici oltre i 12 bit (simboli rari, EOS) o coda più corta di 12: scan
                 * diretto. Senza questo ramo la tabella a 12 bit non risolveva MAI i codici
                 * lunghi e il decoder andava in loop (bug visto sul primo header reale). */
                int s2;
                for (s2 = 0; s2 < 257; s2++)
                {
                    int L = gmb_hpack_huffman[s2].bits;
                    if (L > nbits) continue;
                    if (nbits >= 12 && L <= 12) continue;   /* già coperto dalla tabella */
                    if ((unsigned int)((acc >> (nbits - L)) & ((1ull << L) - 1ull)) == gmb_hpack_huffman[s2].code)
                    { sym = s2; len = L; break; }
                }
            }
            if (sym < 0) break;                    /* non c'è ancora un simbolo completo */
            if (sym == 256) { free(o); return -1; } /* EOS non ammesso nei dati */

            if (olen + 1 >= cap)
            {
                char* no;
                cap *= 2;
                no = (char*)realloc(o, cap);
                if (!no) { free(o); return -1; }
                o = no;
            }
            o[olen++] = (char)sym;
            nbits -= len;
            progressed = 1;
        }

        if (i >= in_len)
        {
            /* niente più input: se non è uscito nessun simbolo, il residuo è padding (o errore) */
            if (!progressed || nbits < 5) break;
            /* nbits >= 5 ma nessun simbolo completo: coda non valida */
            break;
        }
    }

    /* padding: al massimo 7 bit, tutti a 1 (prefisso di EOS, RFC §5.2) */
    if (nbits > 7 || (nbits > 0 && ((acc & ((1ull << nbits) - 1ull)) != ((1ull << nbits) - 1ull))))
    {
        free(o);
        return -1;
    }
    if (olen == 0) { free(o); *out = NULL; *out_len = 0; return 0; }
    *out = o;
    *out_len = olen;
    return 0;
}

/* ── Tabella dinamica ───────────────────────────────────────────────────────── */
static void hp_dyn_clear(gmb_hpack_ctx_t* h)
{
    int i;
    for (i = 0; i < h->dyn_count; i++)
    {
        free(h->dyn[i].name);
        free(h->dyn[i].value);
    }
    h->dyn_count = 0;
    h->dyn_bytes = 0;
}

static void hp_dyn_evict_to(gmb_hpack_ctx_t* h, size_t limit)
{
    while (h->dyn_count > 0 && h->dyn_bytes > limit)
    {
        hp_ent_t* e = &h->dyn[h->dyn_count - 1];
        h->dyn_bytes -= e->size;
        free(e->name);
        free(e->value);
        h->dyn_count--;
    }
}

static void hp_dyn_add(gmb_hpack_ctx_t* h, const char* name, size_t nl, const char* value, size_t vl)
{
    size_t size = HPACK_ENTRY_OVERHEAD + nl + vl;
    char* n;
    char* v;
    int i;

    if (h->dyn_count >= HPACK_MAX_ENTRIES) return;
    if (size > h->max_size) { hp_dyn_clear(h); return; }   /* voce più grande del limite: tabella vuota */

    n = (char*)malloc(nl + 1);
    v = (char*)malloc(vl + 1);
    if (!n || !v) { free(n); free(v); return; }
    memcpy(n, name, nl); n[nl] = 0;
    memcpy(v, value, vl); v[vl] = 0;

    /* shift verso il fondo e inserisci in testa */
    for (i = h->dyn_count; i > 0; i--) h->dyn[i] = h->dyn[i - 1];
    h->dyn[0].name = n;
    h->dyn[0].name_len = nl;
    h->dyn[0].value = v;
    h->dyn[0].value_len = vl;
    h->dyn[0].size = size;
    h->dyn_count++;
    h->dyn_bytes += size;
    hp_dyn_evict_to(h, h->max_size);
}

/* Voce per indice globale: 1..61 statica, 62.. dinamica. Ritorna 0 = trovata. */
static int hp_lookup(gmb_hpack_ctx_t* h, size_t idx, const char** name, size_t* nl,
                     const char** value, size_t* vl)
{
    const gmb_hpack_static_t* s;
    int d;
    if (idx == 0) return -1;
    if (idx <= 61)
    {
        s = hp_static(idx);
        if (!s) return -1;
        *name = s->name;
        *nl = strlen(s->name);
        *value = s->value;
        *vl = strlen(s->value);
        return 0;
    }
    d = (int)(idx - 62);
    if (d < 0 || d >= h->dyn_count) return -1;
    *name = h->dyn[d].name;
    *nl = h->dyn[d].name_len;
    *value = h->dyn[d].value;
    *vl = h->dyn[d].value_len;
    return 0;
}

/* ── Primitive: intero e stringa ────────────────────────────────────────────── */
static int hp_int(const unsigned char* d, size_t len, size_t* pos, int prefix, unsigned long* out)
{
    unsigned long v;
    unsigned char b;
    if (*pos >= len) return -1;
    b = d[(*pos)++];
    v = b & ((1u << prefix) - 1u);
    if (v < (unsigned long)((1u << prefix) - 1u)) { *out = v; return 0; }
    {
        int shift = 0;
        for (;;)
        {
            if (*pos >= len) return -1;
            b = d[(*pos)++];
            v += (unsigned long)(b & 0x7F) << shift;
            shift += 7;
            if (!(b & 0x80)) break;
            if (shift > 28) return -1;
        }
    }
    *out = v;
    return 0;
}

static int hp_string(const unsigned char* d, size_t len, size_t* pos, char** out, size_t* out_len)
{
    int huff;
    unsigned long n;
    if (*pos >= len) return -1;
    huff = (d[*pos] & 0x80) != 0;
    if (hp_int(d, len, pos, 7, &n) != 0) return -1;
    if (*pos + n > len) return -1;
    if (huff)
    {
        int rc = hp_huffman_decode(d + *pos, (size_t)n, out, out_len);
        *pos += (size_t)n;
        return rc;
    }
    *out = (char*)malloc((size_t)n + 1);
    if (!*out) return -1;
    memcpy(*out, d + *pos, (size_t)n);
    (*out)[n] = 0;
    *out_len = (size_t)n;
    *pos += (size_t)n;
    return 0;
}

/* ── API ────────────────────────────────────────────────────────────────────── */
gmb_hpack_ctx_t* gmb_hpack_new(void)
{
    gmb_hpack_ctx_t* h = (gmb_hpack_ctx_t*)calloc(1, sizeof(*h));
    if (h) h->max_size = 4096;
    return h;
}

void gmb_hpack_free(gmb_hpack_ctx_t* h)
{
    if (!h) return;
    hp_dyn_clear(h);
    free(h);
}

void gmb_hpack_set_max_size(gmb_hpack_ctx_t* h, size_t max)
{
    if (!h) return;
    h->max_size = max;
    hp_dyn_evict_to(h, max);
}

int gmb_hpack_decode(gmb_hpack_ctx_t* h, const unsigned char* d, size_t len,
                     gmb_hpack_pair_fn on_pair, void* user)
{
    size_t pos = 0;
    if (!h || !d) return -1;
    while (pos < len)
    {
        unsigned char b = d[pos];

        if (b & 0x80)
        {
            /* 6.1 campo indicizzato: 1xxxxxxx */
            unsigned long idx = 0;
            const char *name, *value;
            size_t nl, vl;
            if (hp_int(d, len, &pos, 7, &idx) != 0) return -1;
            if (hp_lookup(h, (size_t)idx, &name, &nl, &value, &vl) != 0) return -1;
            on_pair(user, name, nl, value, vl);
        }
        else if ((b & 0xC0) == 0x40)
        {
            /* 6.2.1 letterale con indicizzazione incrementale: 01xxxxxx */
            unsigned long idx = 0;
            const char* name = NULL;
            size_t nl = 0;
            char* vname = NULL;
            char* vval = NULL;
            size_t vl = 0;
            if (hp_int(d, len, &pos, 6, &idx) != 0) return -1;
            if (idx == 0)
            {
                if (hp_string(d, len, &pos, &vname, &nl) != 0) return -1;
                name = vname;
            }
            else
            {
                const char* tn; const char* tv; size_t tnl, tvl;
                if (hp_lookup(h, (size_t)idx, &tn, &tnl, &tv, &tvl) != 0) { free(vname); return -1; }
                vname = (char*)malloc(tnl + 1);
                if (!vname) return -1;
                memcpy(vname, tn, tnl); vname[tnl] = 0;
                name = vname; nl = tnl;
            }
            if (hp_string(d, len, &pos, &vval, &vl) != 0) { free(vname); return -1; }
            on_pair(user, name, nl, vval, vl);
            hp_dyn_add(h, name, nl, vval, vl);
            free(vname);
            free(vval);
        }
        else if ((b & 0xF0) == 0x00)
        {
            /* 6.2.2 letterale SENZA indicizzazione: 0000xxxx */
            unsigned long idx = 0;
            char* vname = NULL;
            const char* name;
            char* vval = NULL;
            size_t nl, vl;
            if (hp_int(d, len, &pos, 4, &idx) != 0) return -1;
            if (idx == 0)
            {
                if (hp_string(d, len, &pos, &vname, &nl) != 0) return -1;
                name = vname;
            }
            else
            {
                const char* tn; const char* tv; size_t tnl, tvl;
                if (hp_lookup(h, (size_t)idx, &tn, &tnl, &tv, &tvl) != 0) return -1;
                name = tn; nl = tnl;
            }
            if (hp_string(d, len, &pos, &vval, &vl) != 0) { free(vname); return -1; }
            on_pair(user, name, nl, vval, vl);
            free(vname);
            free(vval);
        }
        else if ((b & 0xF0) == 0x10)
        {
            /* 6.2.3 letterale MAI indicizzato: 0001xxxx */
            unsigned long idx = 0;
            char* vname = NULL;
            const char* name;
            char* vval = NULL;
            size_t nl, vl;
            if (hp_int(d, len, &pos, 4, &idx) != 0) return -1;
            if (idx == 0)
            {
                if (hp_string(d, len, &pos, &vname, &nl) != 0) return -1;
                name = vname;
            }
            else
            {
                const char* tn; const char* tv; size_t tnl, tvl;
                if (hp_lookup(h, (size_t)idx, &tn, &tnl, &tv, &tvl) != 0) return -1;
                name = tn; nl = tnl;
            }
            if (hp_string(d, len, &pos, &vval, &vl) != 0) { free(vname); return -1; }
            on_pair(user, name, nl, vval, vl);
            free(vname);
            free(vval);
        }
        else
        {
            /* 6.3 aggiornamento dimensione tabella dinamica: 001xxxxx */
            unsigned long sz = 0;
            if (hp_int(d, len, &pos, 5, &sz) != 0) return -1;
            h->max_size = (size_t)sz;
            hp_dyn_evict_to(h, h->max_size);
        }
    }
    return 0;
}

/* ── Codifica della risposta ────────────────────────────────────────────────── */
static int hp_put_int(unsigned char* out, size_t cap, size_t* pos, int prefix, unsigned long value, unsigned char flags)
{
    unsigned long max = (1ul << prefix) - 1ul;
    if (*pos >= cap) return -1;
    if (value < max)
    {
        out[(*pos)++] = (unsigned char)(flags | value);
        return 0;
    }
    out[(*pos)++] = (unsigned char)(flags | max);
    value -= max;
    while (value >= 0x80)
    {
        if (*pos >= cap) return -1;
        out[(*pos)++] = (unsigned char)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    if (*pos >= cap) return -1;
    out[(*pos)++] = (unsigned char)value;
    return 0;
}

static int hp_put_str(unsigned char* out, size_t cap, size_t* pos, const char* s, size_t len)
{
    if (hp_put_int(out, cap, pos, 7, (unsigned long)len, 0x00) != 0) return -1;
    if (*pos + len > cap) return -1;
    memcpy(out + *pos, s, len);
    *pos += len;
    return 0;
}

/* Indice statico per (nome[, valore]). -1 = non c'è. */
static int hp_static_index(const char* name, const char* value)
{
    int i;
    for (i = 1; i <= 61; i++)
    {
        if (strcasecmp(gmb_hpack_static[i].name, name) != 0) continue;
        if (!value) return i;                                  /* solo nome */
        if (strcmp(gmb_hpack_static[i].value, value) == 0) return i;
    }
    return -1;
}

/* Header che il framing di HTTP/2 gestisce da sé: NON vanno emessi. */
static int hp_skip_header(const char* name, size_t nl)
{
    /* content-length NON va saltato: in HTTP/2 è lecito e serve (soprattutto agli HEAD,
     * dove non c'è corpo da cui dedurlo). */
    static const char* skip[] = { "connection", "transfer-encoding",
                                  "keep-alive", "upgrade", "date" };
    size_t i;
    for (i = 0; i < sizeof(skip) / sizeof(skip[0]); i++)
        if (strlen(skip[i]) == nl && strncasecmp(name, skip[i], nl) == 0) return 1;
    return 0;
}

int gmb_hpack_encode_response(int status, const char* headers_text,
                              unsigned char* out, size_t cap)
{
    size_t pos = 0;
    char num[16];
    int slug;
    int status_idx;

    /* :status */
    snprintf(num, sizeof num, "%d", status);
    status_idx = hp_static_index(":status", num);
    if (status_idx > 0)
    {
        if (hp_put_int(out, cap, &pos, 7, (unsigned long)status_idx, 0x80) != 0) return -1;
    }
    else
    {
        slug = hp_static_index(":status", NULL);
        if (hp_put_int(out, cap, &pos, 4, (unsigned long)slug, 0x00) != 0) return -1;
        if (hp_put_str(out, cap, &pos, num, strlen(num)) != 0) return -1;
    }

    /* header aggiuntivi: righe "Nome: Valore\n" */
    if (headers_text)
    {
        const char* line = headers_text;
        while (line && *line)
        {
            const char* nl = strchr(line, '\n');
            size_t ll = nl ? (size_t)(nl - line) : strlen(line);
            /* le righe interne possono finire con \r (forma HTTP/1.1): un CR dentro il valore
             * di un header HTTP/2 rende l'header INVALIDO e il client lo scarta in silenzio
             * (era il caso di Content-Type/ETag: risposta senza header). */
            while (ll > 0 && (line[ll - 1] == '\r' || line[ll - 1] == ' ')) ll--;
            if (ll > 2)
            {
                const char* colon = memchr(line, ':', ll);
                if (colon && colon > line)
                {
                    size_t nlen = (size_t)(colon - line);
                    const char* v = colon + 1;
                    size_t vlen;
                    char name[64];
                    int idx;
                    while (v < line + ll && (*v == ' ' || *v == '\t')) v++;
                    vlen = (size_t)(line + ll - v);
                    if (nlen < sizeof(name))
                    {
                        size_t k;
                        for (k = 0; k < nlen; k++) name[k] = (char)tolower((unsigned char)line[k]);
                        name[nlen] = 0;
                        if (!hp_skip_header(name, nlen))
                        {
                            idx = hp_static_index(name, NULL);
                            if (idx > 0)
                            {
                                /* nome indicizzato (0000 <idx>) */
                                if (hp_put_int(out, cap, &pos, 4, (unsigned long)idx, 0x00) != 0) return -1;
                            }
                            else
                            {
                                /* nome letterale (0000 0000 + stringa) */
                                if (hp_put_int(out, cap, &pos, 4, 0, 0x00) != 0) return -1;
                                if (hp_put_str(out, cap, &pos, name, nlen) != 0) return -1;
                            }
                            if (hp_put_str(out, cap, &pos, v, vlen) != 0) return -1;
                        }
                    }
                }
            }
            if (!nl) break;
            line = nl + 1;
        }
    }
    return (int)pos;
}
