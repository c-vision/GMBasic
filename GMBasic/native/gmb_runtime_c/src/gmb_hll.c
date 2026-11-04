/* ============================================================================
 * gmb_hll.c — HyperLogLog: implementazione unica del runtime condiviso (§0bis).
 * Vedi gmb_hll.h per il contratto di hashing e il formato di serializzazione.
 * ============================================================================ */
#include "gmb_hll.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct gmb_hll {
    int p;               /* precisione: m = 1<<p */
    int q;               /* q = 64 - p (bit del rho) */
    int m;               /* numero registri */
    uint8_t* regs;       /* m registri, valori 0..q+1 */
    int64_t nnz;         /* registri non-zero (istogramma C[0] implicito) */
};

/* ---- xxHash64 (seed 0) ----------------------------------------------------
 * Algoritmo canonico (Yann Collet, public domain). Deve restare IDENTICO in
 * CommonLib/Collections/HyperLogLog.cs (stessi primi, stessa sequenza). */
static uint64_t xxh_read64(const void* p)
{
    uint64_t v;
    memcpy(&v, p, sizeof v);
    return v;   /* little-endian su tutte le piattaforme target */
}
static uint32_t xxh_read32(const void* p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}
static uint64_t xxh_rotl64(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }

#define XXH_P1 11400714785074694791ULL
#define XXH_P2 14029467366897019727ULL
#define XXH_P3  1609587929392839161ULL
#define XXH_P4  9650029242287828579ULL
#define XXH_P5  2870177450012600261ULL

static uint64_t xxh_round(uint64_t acc, uint64_t input)
{
    acc += input * XXH_P2;
    acc = xxh_rotl64(acc, 31);
    acc *= XXH_P1;
    return acc;
}
static uint64_t xxh_merge(uint64_t acc, uint64_t val)
{
    val = xxh_round(0, val);
    acc ^= val;
    acc = acc * XXH_P1 + XXH_P4;
    return acc;
}

uint64_t gmb_hll_hash_bytes(const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    const uint8_t* end = p + len;
    uint64_t h;

    if (len >= 32)
    {
        uint64_t v1 = XXH_P1 + XXH_P2;
        uint64_t v2 = XXH_P2;
        uint64_t v3 = 0;
        uint64_t v4 = (uint64_t)0 - XXH_P1;
        const uint8_t* limit = end - 32;
        do
        {
            v1 = xxh_round(v1, xxh_read64(p)); p += 8;
            v2 = xxh_round(v2, xxh_read64(p)); p += 8;
            v3 = xxh_round(v3, xxh_read64(p)); p += 8;
            v4 = xxh_round(v4, xxh_read64(p)); p += 8;
        } while (p <= limit);
        h = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) + xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);
        h = xxh_merge(h, v1);
        h = xxh_merge(h, v2);
        h = xxh_merge(h, v3);
        h = xxh_merge(h, v4);
    }
    else
    {
        h = XXH_P5;
    }
    h += (uint64_t)len;

    while (p + 8 <= end)
    {
        uint64_t k1 = xxh_round(0, xxh_read64(p));
        h ^= k1;
        h = xxh_rotl64(h, 27) * XXH_P1 + XXH_P4;
        p += 8;
    }
    if (p + 4 <= end)
    {
        h ^= (uint64_t)xxh_read32(p) * XXH_P1;
        h = xxh_rotl64(h, 23) * XXH_P2 + XXH_P3;
        p += 4;
    }
    while (p < end)
    {
        h ^= (uint64_t)(*p) * XXH_P5;
        h = xxh_rotl64(h, 11) * XXH_P1;
        p++;
    }
    h ^= h >> 33;
    h *= XXH_P2;
    h ^= h >> 29;
    h *= XXH_P3;
    h ^= h >> 32;
    return h;
}

/* ---- Payload canonico ---------------------------------------------------- */
static uint64_t hll_hash_tagged(const void* tag_and_data, size_t len)
{
    return gmb_hll_hash_bytes(tag_and_data, len);
}

uint64_t gmb_hll_hash_val(gmb_val_t v)
{
    uint8_t buf[8 + 4];
    switch (v.kind)
    {
        case GMB_TYPE_NIL:
            buf[0] = 0x00;
            return hll_hash_tagged(buf, 1);
        case GMB_TYPE_BOOL:
            buf[0] = 0x04;
            buf[1] = v.v_bool ? 1 : 0;
            return hll_hash_tagged(buf, 2);
        case GMB_TYPE_INT:
        case GMB_TYPE_DOUBLE:
        {
            double d = (v.kind == GMB_TYPE_INT) ? (double)v.v_int : v.v_double;
            /* Numerico INTERO canonico (|v| ≤ 2^53): stesso hash per 1, 1.0, INTEGER 1. */
            if (d == floor(d) && fabs(d) <= 9007199254740992.0)
            {
                int64_t iv = (int64_t)d;
                buf[0] = 0x01;
                memcpy(buf + 1, &iv, 8);
                return hll_hash_tagged(buf, 9);
            }
            buf[0] = 0x02;
            memcpy(buf + 1, &d, 8);
            return hll_hash_tagged(buf, 9);
        }
        case GMB_TYPE_STRING:
        {
            /* 0x03 + uint32 len + byte (tag separato per non confondere "1" con 1). */
            uint32_t n = (uint32_t)(v.v_string.length > 0 ? v.v_string.length : 0);
            if (n == 0 && v.v_string.data) n = (uint32_t)strlen(v.v_string.data);
            {
                size_t total = 1 + 4 + (size_t)n;
                uint8_t* tmp = (uint8_t*)malloc(total);
                uint64_t h;
                if (!tmp) return 0;
                tmp[0] = 0x03;
                memcpy(tmp + 1, &n, 4);
                if (n && v.v_string.data) memcpy(tmp + 5, v.v_string.data, n);
                h = hll_hash_tagged(tmp, total);
                free(tmp);
                return h;
            }
        }
        default:
        {
            /* Altro: JSON canonico (tag 0x05 + len + byte). */
            gmb_string_t js = gmb_val_to_json(v);
            uint32_t n = (uint32_t)(js.length > 0 ? js.length : (js.data ? strlen(js.data) : 0));
            size_t total = 1 + 4 + (size_t)n;
            uint8_t* tmp = (uint8_t*)malloc(total);
            uint64_t h;
            if (!tmp) { gmb_string_free(&js); return 0; }
            tmp[0] = 0x05;
            memcpy(tmp + 1, &n, 4);
            if (n && js.data) memcpy(tmp + 5, js.data, n);
            h = hll_hash_tagged(tmp, total);
            free(tmp);
            gmb_string_free(&js);
            return h;
        }
    }
}

/* ---- Sketch -------------------------------------------------------------- */
gmb_hll_t* gmb_hll_new(int p)
{
    gmb_hll_t* h;
    if (p < GMB_HLL_MIN_PRECISION) p = GMB_HLL_MIN_PRECISION;
    if (p > GMB_HLL_MAX_PRECISION) p = GMB_HLL_MAX_PRECISION;
    h = (gmb_hll_t*)calloc(1, sizeof *h);
    if (!h) return NULL;
    h->p = p;
    h->q = 64 - p;
    h->m = 1 << p;
    h->regs = (uint8_t*)calloc((size_t)h->m, 1);
    if (!h->regs) { free(h); return NULL; }
    h->nnz = 0;
    return h;
}

void gmb_hll_free(gmb_hll_t* h)
{
    if (!h) return;
    free(h->regs);
    free(h);
}

void gmb_hll_clear(gmb_hll_t* h)
{
    if (!h || !h->regs) return;
    memset(h->regs, 0, (size_t)h->m);
    h->nnz = 0;
}

int gmb_hll_precision(const gmb_hll_t* h) { return h ? h->p : 0; }

void gmb_hll_add_hash(gmb_hll_t* h, uint64_t hash)
{
    uint64_t idx;
    int rho;
    if (!h || !h->regs) return;
    idx = hash >> (64 - h->p);
    /* rho = posizione del primo 1 nei q bit bassi, +1 (1..q+1). I bit rimanenti
     * (dopo il taglio) si ottengono shiftando a sinistra di p. */
    {
        uint64_t w = hash << h->p;   /* bit rimanenti nei 64-p bit alti */
        int r = 1;
        if (w == 0)
        {
            r = h->q + 1;
        }
        else
        {
            while ((w & (1ULL << 63)) == 0) { r++; w <<= 1; }
        }
        rho = r;
    }
    if (rho > h->q + 1) rho = h->q + 1;
    {
        uint8_t cur = h->regs[idx];
        if ((uint8_t)rho > cur)
        {
            if (cur == 0) h->nnz++;
            h->regs[idx] = (uint8_t)rho;
        }
    }
}

void gmb_hll_add_val(gmb_hll_t* h, gmb_val_t v)
{
    gmb_hll_add_hash(h, gmb_hll_hash_val(v));
}

double gmb_hll_count(const gmb_hll_t* h)
{
    double sum = 0.0;
    double alpha, raw, est;
    int zeros = 0;
    int i;
    if (!h || !h->regs) return 0.0;
    if (h->nnz == 0) return 0.0;
    for (i = 0; i < h->m; i++)
    {
        int r = h->regs[i];
        if (r == 0) { zeros++; sum += 1.0; }
        else sum += 1.0 / (double)(1ULL << r);
    }
    if (sum <= 0.0) return 0.0;
    alpha = 0.7213 / (1.0 + 1.079 / (double)h->m);
    raw = alpha * (double)h->m * (double)h->m / sum;
    est = raw;
    if (raw <= 2.5 * (double)h->m && zeros > 0)
    {
        /* Small range: linear counting (esatto in pratica fino a ~2.5m). */
        est = (double)h->m * log((double)h->m / (double)zeros);
    }
    else
    {
        /* Large range: correzione per le collisioni dell'hash a 64 bit. */
        const double two64 = 18446744073709551616.0;
        if (est > two64 / 30.0) est = -two64 * log(1.0 - est / two64);
    }
    if (est < 0.0) est = 0.0;
    return floor(est + 0.5);   /* cardinalità intera: stampa identica su tutte le gambe */
}

double gmb_hll_error(const gmb_hll_t* h)
{
    if (!h) return 0.0;
    return 1.04 / sqrt((double)h->m);
}

int gmb_hll_merge(gmb_hll_t* dst, const gmb_hll_t* src)
{
    int i;
    if (!dst || !src || dst->p != src->p) return -1;
    for (i = 0; i < dst->m; i++)
    {
        if (src->regs[i] > dst->regs[i])
        {
            if (dst->regs[i] == 0) dst->nnz++;
            dst->regs[i] = src->regs[i];
        }
    }
    return 0;
}

int gmb_hll_serialize(const gmb_hll_t* h, char* out, size_t cap)
{
    size_t need;
    if (!h || !out) return -1;
    need = (size_t)GMB_HLL_SERIAL_HEADER_LEN + (size_t)h->m;
    if (cap < need) return -1;
    memcpy(out, GMB_HLL_SERIAL_MAGIC, GMB_HLL_SERIAL_MAGIC_LEN);
    out[GMB_HLL_SERIAL_MAGIC_LEN] = (char)h->p;
    out[GMB_HLL_SERIAL_MAGIC_LEN + 1] = (char)GMB_HLL_HASH_ID;
    memcpy(out + GMB_HLL_SERIAL_HEADER_LEN, h->regs, (size_t)h->m);
    return (int)need;
}

gmb_hll_t* gmb_hll_deserialize(const char* data, size_t len)
{
    int p;
    gmb_hll_t* h;
    int m;
    if (!data || len < (size_t)GMB_HLL_SERIAL_HEADER_LEN) return NULL;
    if (memcmp(data, GMB_HLL_SERIAL_MAGIC, GMB_HLL_SERIAL_MAGIC_LEN) != 0) return NULL;
    p = (uint8_t)data[GMB_HLL_SERIAL_MAGIC_LEN];
    if ((uint8_t)data[GMB_HLL_SERIAL_MAGIC_LEN + 1] != (uint8_t)GMB_HLL_HASH_ID) return NULL;
    if (p < GMB_HLL_MIN_PRECISION || p > GMB_HLL_MAX_PRECISION) return NULL;
    m = 1 << p;
    if (len < (size_t)GMB_HLL_SERIAL_HEADER_LEN + (size_t)m) return NULL;
    h = gmb_hll_new(p);
    if (!h) return NULL;
    memcpy(h->regs, data + GMB_HLL_SERIAL_HEADER_LEN, (size_t)m);
    h->nnz = 0;
    for (int i = 0; i < m; i++) if (h->regs[i]) h->nnz++;
    return h;
}
