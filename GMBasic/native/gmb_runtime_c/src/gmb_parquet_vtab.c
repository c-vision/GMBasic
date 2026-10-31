/* FILE: native/gmb_runtime_c/src/gmb_parquet_vtab.c
 * Virtual table SQLite `parquet`: legge file Parquet direttamente in SQL.
 *
 * FASE 1 (questo file): metadata Thrift, schema (piatto + gruppi), pagine, encoding
 * PLAIN / RLE / RLE_DICTIONARY, compressione UNCOMPRESSED / SNAPPY / ZSTD / GZIP,
 * tipi INT32 / INT64 / FLOAT / DOUBLE / BOOLEAN / BYTE_ARRAY(UTF8). Le colonne esposte
 * sono le FOGLIE; per lo schema piatto coincidono con le colonne logiche.
 * FASE 2 (nested list/struct/map): assemblaggio via def/rep levels (Dremel) → JSON.
 *
 * Sorgente unico (§0bis) compilato come MODULE caricabile (host C#) — stessa convenzione del
 * modulo `csv`. La registrazione statica sui backend C usa gmb_sql_register_parquet.
 */
#ifdef GMB_PARQUET_AS_EXTENSION
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#else
#include <sqlite3.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strncasecmp */
#include <stdio.h>
#include <math.h>
#include <unistd.h>   /* getpid */
#include <stdint.h>
#include <zlib.h>

#ifdef GMB_PARQUET_HAVE_ZSTD
#include <zstd.h>
#endif

/* ============================ Thrift Compact Protocol (read) ============================ */
typedef struct { const uint8_t* p; size_t n, i; int err; } thr_t;

static uint8_t thr_u8(thr_t* t) { if (t->i >= t->n) { t->err = 1; return 0; } return t->p[t->i++]; }
static uint64_t thr_varint(thr_t* t)
{
    uint64_t v = 0; int s = 0;
    for (;;) {
        uint8_t b = thr_u8(t);
        if (t->err) return 0;
        v |= (uint64_t)(b & 0x7f) << s;
        if (!(b & 0x80)) break;
        s += 7;
        if (s > 63) { t->err = 1; return 0; }
    }
    return v;
}
static int64_t thr_zigzag(thr_t* t) { uint64_t u = thr_varint(t); return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
static void thr_binary(thr_t* t, const uint8_t** out, size_t* len)
{
    uint64_t l = thr_varint(t);
    if (t->err || t->i + l > t->n) { t->err = 1; *out = NULL; *len = 0; return; }
    *out = t->p + t->i; *len = (size_t)l; t->i += (size_t)l;
}
static void thr_skip(thr_t* t, int type)
{
    switch (type) {
        case 1: case 2: break;                             /* BOOL come CAMPO: valore nel nibble, 0 byte */
        case 3: thr_u8(t); break;                          /* BYTE */
        case 4: case 5: case 6: thr_zigzag(t); break;      /* I16/I32/I64 */
        case 7: { uint64_t u = thr_varint(t); (void)u; break; } /* DOUBLE: 8 byte LE (varint non corretto ma
                                                                * non usato nei metadata Parquet reali) */
        case 8: { const uint8_t* b; size_t l; thr_binary(t, &b, &l); break; } /* BINARY */
        case 9: case 10: {                                  /* LIST/SET */
            uint8_t h = thr_u8(t);
            int et = h & 0x0f; uint32_t sz = h >> 4;
            if (sz == 15) sz = (uint32_t)thr_varint(t);
            for (uint32_t k = 0; k < sz && !t->err; k++) {
                if (et == 1 || et == 2) thr_u8(t);         /* bool come ELEMENTO: 1 byte */
                else thr_skip(t, et);
            }
            break;
        }
        case 11: {                                          /* MAP */
            uint32_t sz = (uint32_t)thr_varint(t);
            if (sz) { uint8_t h = thr_u8(t); int kt = h >> 4, vt = h & 0x0f;
                for (uint32_t k = 0; k < sz && !t->err; k++) {
                    if (kt == 1 || kt == 2) thr_u8(t); else thr_skip(t, kt);
                    if (vt == 1 || vt == 2) thr_u8(t); else thr_skip(t, vt);
                } }
            break;
        }
        case 12: {                                          /* STRUCT: campi fino a STOP (0x00) */
            for (;;) {
                if (t->err) break;
                uint8_t b = thr_u8(t);
                if (b == 0) break;
                int ft = b & 0x0f;
                if (ft == 0) { t->err = 1; break; }
                thr_skip(t, ft);
            }
            break;
        }
        default: t->err = 1; break;
    }
}
/* Legge l'header di un campo struct: ritorna type (0 = STOP); field id in *fid.
 * BOOL compatta il valore nel campo (type 1/2 + valore nel nibble) secondo spec. */
static int thr_field(thr_t* t, int* fid)
{
    uint8_t b = thr_u8(t);
    if (t->err) return 0;
    if (b == 0) return 0;                 /* STOP */
    int type = b & 0x0f;
    int delta = (b & 0xf0) >> 4;
    static const int last[16] = {0};
    (void)last;
    *fid = 0;
    (void)delta;
    return type;
}
/* Implementazione con stato "last field id" per struct (necessario: delta encoding). */
typedef struct { thr_t* t; int last; } thr_struct_t;
static void thr_flist(thr_t* t, int* et, uint32_t* sz)
{
    uint8_t h = thr_u8(t);
    *et = h & 0x0f; *sz = h >> 4;
    if (*sz == 15) *sz = (uint32_t)thr_varint(t);
}

/* ============================ Modello schema / metadati ============================ */
typedef enum { PT_BOOLEAN=0, PT_INT32=1, PT_INT64=2, PT_INT96=3, PT_FLOAT=4, PT_DOUBLE=5, PT_BYTE_ARRAY=6, PT_FLBA=7 } pq_ptype;
typedef enum { RT_REQUIRED=0, RT_OPTIONAL=1, RT_REPEATED=2 } pq_rtype;
typedef enum { CT_UTF8=0, CT_NONE=-1 } pq_ctype;

typedef struct {
    int type;              /* >=0 = fisico (foglia), -1 = gruppo */
    int rep;               /* pq_rtype */
    int converted;         /* -1 nessuno, 0 UTF8 */
    char name[128];
    int parent;            /* indice del genitore, -1 radice */
    int nchildren;
    int max_def, max_rep;  /* calcolati (foglie) */
    int col_idx;           /* indice della foglia (colonnes parquet), -1 se gruppo */
} pq_schema_elem;

typedef struct {
    int64_t data_page_offset, dictionary_page_offset;
    int codec;             /* 0 UNCOMPRESSED,1 SNAPPY,2 GZIP,6 ZSTD */
    int64_t num_values;
    int64_t total_compressed_size, total_uncompressed_size;
    char path[256];
    int type;              /* pq_ptype */
} pq_colchunk;

typedef struct { pq_colchunk* cols; int ncols; } pq_rg;   /* un row-group: le sue foglie */
typedef struct {
    int64_t num_rows;
    pq_schema_elem* sch; int nsch;
    pq_rg* rgs; int nrgs;           /* TUTTI i row-group */
    int ncols;                      /* foglie (rgs[0].ncols) */
    int* leaf_elem; int nleaf;      /* indici schema delle foglie (ordine = cols) */
    char* buf; size_t buf_len;      /* contenuto file (owned) */
} pq_file;

/* ============================ Parsing FileMetaData ============================ */
/* FileMetaData (Thrift struct):
 *  1: i32 version  2: list<SchemaElement> schema  3: i64 num_rows  4: list<RowGroup> row_groups
 *  5: list<KeyValue> key_value_metadata  6: string created_by  ...
 * SchemaElement: 1 type(i32,opt) 2 type_length(i32) 3 repetition_type(i32,opt)
 *   4 name(string) 5 num_children(i32,opt) 6 converted_type(i32,opt) ...
 * RowGroup: 1 columns(list<ColumnChunk>) 2 total_byte_size 3 num_rows ...
 * ColumnChunk: 2 file_offset(i64) 3 meta_data(ColumnMetaData)
 * ColumnMetaData: 1 type(i32) 2 encodings(list) 3 path_in_schema(list<string>)
 *   4 codec(i32) 5 num_values(i64) 6 total_uncompressed_size 7 total_compressed_size
 *   9 data_page_offset(i64) 11 dictionary_page_offset(i64,opt) ... */

static void pq_parse_schema_element(thr_t* t, pq_schema_elem* e)
{
    e->type = -1; e->rep = RT_REQUIRED; e->converted = -1; e->name[0] = 0;
    e->parent = -1; e->nchildren = 0; e->max_def = 0; e->max_rep = 0; e->col_idx = -1;
    int last = 0;
    for (;;) {
        uint8_t b = thr_u8(t);
        if (t->err || b == 0) break;
        int ft = b & 0x0f, delta = (b & 0xf0) >> 4;
        int fid = (delta == 0) ? (last = (int)thr_zigzag(t)) : (last = last + delta);
        switch (fid) {
            case 1: e->type = (int)thr_zigzag(t); break;
            case 2: thr_zigzag(t); break;                        /* type_length */
            case 3: e->rep = (int)thr_zigzag(t); break;
            case 4: { const uint8_t* s; size_t l; thr_binary(t, &s, &l);
                      if (l > 127) l = 127; memcpy(e->name, s, l); e->name[l] = 0; break; }
            case 5: e->nchildren = (int)thr_zigzag(t); break;
            case 6: e->converted = (int)thr_zigzag(t); break;
            default: thr_skip(t, ft); break;
        }
    }
}
static void pq_parse_colmeta(thr_t* t, pq_colchunk* c)
{
    c->data_page_offset = 0; c->dictionary_page_offset = -1; c->codec = 0; c->num_values = 0;
    c->total_compressed_size = 0; c->total_uncompressed_size = 0; c->path[0] = 0; c->type = 0;
    int last = 0, path_first = 0;
    for (;;) {
        uint8_t b = thr_u8(t);
        if (t->err || b == 0) break;
        int ft = b & 0x0f, delta = (b & 0xf0) >> 4;
        int fid = (delta == 0) ? (last = (int)thr_zigzag(t)) : (last = last + delta);
        switch (fid) {
            case 1: c->type = (int)thr_zigzag(t); break;
            case 2: thr_skip(t, ft); break;                       /* encodings */
            case 3: {                                             /* path_in_schema list<string> */
                int et; uint32_t sz; thr_flist(t, &et, &sz);
                for (uint32_t k = 0; k < sz && !t->err; k++) {
                    const uint8_t* s; size_t l; thr_binary(t, &s, &l);
                    if (k == 0) { if (l > 255) l = 255; memcpy(c->path, s, l); c->path[l] = 0; path_first = 1; }
                }
                (void)path_first;
                break;
            }
            case 4: c->codec = (int)thr_zigzag(t); break;
            case 5: c->num_values = thr_zigzag(t); break;
            case 6: c->total_uncompressed_size = thr_zigzag(t); break;
            case 7: c->total_compressed_size = thr_zigzag(t); break;
            case 9: c->data_page_offset = thr_zigzag(t); break;
            case 11: c->dictionary_page_offset = thr_zigzag(t); break;
            default: thr_skip(t, ft); break;
        }
    }
}
static void pq_parse_meta(pq_file* f, const uint8_t* p, size_t n)
{
    thr_t t = { p, n, 0, 0 };
    int last = 0;
    int in_rg = 0;
    f->sch = NULL; f->nsch = 0; f->rgs = NULL; f->nrgs = 0; f->ncols = 0; f->num_rows = 0;
    for (;;) {
        uint8_t b = thr_u8(&t);
        if (t.err || b == 0) { if (in_rg) { in_rg = 0; continue; } break; }
        int ft = b & 0x0f, delta = (b & 0xf0) >> 4;
        int fid = (delta == 0) ? (last = (int)thr_zigzag(&t)) : (last = last + delta);
        if (fid == 1) { thr_zigzag(&t); }
        else if (fid == 2) {                                   /* schema: list<SchemaElement> */
            int et; uint32_t sz; thr_flist(&t, &et, &sz);
            f->sch = (pq_schema_elem*)calloc(sz ? sz : 1, sizeof(pq_schema_elem));
            f->nsch = (int)sz;
            for (uint32_t k = 0; k < sz && !t.err; k++) pq_parse_schema_element(&t, &f->sch[k]);
        }
        else if (fid == 3) { f->num_rows = thr_zigzag(&t); }
        else if (fid == 4) {                                   /* row_groups list<RowGroup> */
            int et; uint32_t nrg; thr_flist(&t, &et, &nrg);
            f->rgs = (pq_rg*)calloc(nrg ? nrg : 1, sizeof(pq_rg));
            f->nrgs = (int)nrg;
            for (uint32_t r = 0; r < nrg && !t.err; r++) {
                pq_rg* rg = &f->rgs[r];
                /* RowGroup struct */
                int rlast = 0;
                for (;;) {
                    uint8_t rb = thr_u8(&t);
                    if (t.err || rb == 0) break;
                    int rft = rb & 0x0f, rdelta = (rb & 0xf0) >> 4;
                    int rfid = (rdelta == 0) ? (rlast = (int)thr_zigzag(&t)) : (rlast = rlast + rdelta);
                    if (rfid == 1) {                            /* columns list<ColumnChunk> */
                        int cet; uint32_t ncc; thr_flist(&t, &cet, &ncc);
                        rg->cols = (pq_colchunk*)calloc(ncc ? ncc : 1, sizeof(pq_colchunk));
                        rg->ncols = (int)ncc;
                        for (uint32_t c = 0; c < ncc && !t.err; c++) {
                            int clast = 0;
                            for (;;) {
                                uint8_t cb = thr_u8(&t);
                                if (t.err || cb == 0) break;
                                int cft = cb & 0x0f, cdelta = (cb & 0xf0) >> 4;
                                int cfid = (cdelta == 0) ? (clast = (int)thr_zigzag(&t)) : (clast = clast + cdelta);
                                if (cfid == 3) pq_parse_colmeta(&t, &rg->cols[c]);
                                else if (cfid == 2) thr_zigzag(&t);   /* file_offset */
                                else thr_skip(&t, cft);
                            }
                        }
                        if (r == 0) f->ncols = rg->ncols;           /* numero foglie (uguale in tutti i RG) */
                    }
                    else thr_skip(&t, rft);
                }
            }
        }
        else thr_skip(&t, ft);
    }
}

/* ============================ Decompression ============================ */
/* Snappy raw block decode (formato "snappy compressed" senza framing). */
static int snappy_uncompressed_len(const uint8_t* p, size_t n, size_t* out)
{
    size_t i = 0; uint64_t v = 0; int s = 0;
    while (i < n) { uint8_t b = p[i++]; v |= (uint64_t)(b & 0x7f) << s; if (!(b & 0x80)) break; s += 7; }
    *out = (size_t)v; return 0;
}
static int snappy_decode(const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen)
{
    size_t ip = 0, op = 0;
    /* salta il varint della lunghezza non compressa */
    while (ip < inlen && (in[ip] & 0x80)) ip++;
    ip++;
    while (ip < inlen) {
        uint8_t tag = in[ip++];
        int t = tag & 3;
        if (t == 0) {                                   /* literal */
            size_t len = tag >> 2;
            if (len < 60) len += 1;
            else { int nb = (int)(len - 59); size_t v = 0; for (int k = 0; k < nb && ip < inlen; k++) v |= (size_t)in[ip++] << (8*k); len = v + 1; }
            if (op + len > outlen || ip + len > inlen) return -1;
            memcpy(out + op, in + ip, len); ip += len; op += len;
        } else {
            size_t len, off;
            if (t == 1) { len = 4 + ((tag >> 2) & 7); off = ((size_t)(tag >> 5) << 8) | in[ip++]; }
            else if (t == 2) { len = 1 + (tag >> 2); off = (size_t)in[ip] | ((size_t)in[ip+1] << 8); ip += 2; }
            else { len = 1 + (tag >> 2); off = (size_t)in[ip] | ((size_t)in[ip+1] << 8) | ((size_t)in[ip+2] << 16) | ((size_t)in[ip+3] << 24); ip += 4; }
            if (off == 0 || off > op) return -1;
            if (op + len > outlen) return -1;
            for (size_t k = 0; k < len; k++) { out[op] = out[op - off]; op++; }
        }
    }
    return (int)op;
}
/* Decomprime `src` (len) in un buffer allocato; ritorna buffer e size. codec: 0/1/2/6. */
static uint8_t* pq_decompress(int codec, const uint8_t* src, size_t len, size_t ulen, size_t* outlen)
{
    uint8_t* out;
    if (codec == 0) { out = (uint8_t*)malloc(len ? len : 1); if (out) memcpy(out, src, len); *outlen = len; return out; }
    if (codec == 1) {   /* SNAPPY */
        size_t need = ulen; if (!need) snappy_uncompressed_len(src, len, &need);
        out = (uint8_t*)malloc(need ? need : 1);
        if (!out) return NULL;
        if (snappy_decode(src, len, out, need) < 0) { free(out); return NULL; }
        *outlen = need; return out;
    }
    if (codec == 2) {   /* GZIP (zlib, formato gzip) */
        z_stream zs; memset(&zs, 0, sizeof(zs));
        if (inflateInit2(&zs, 15 + 16) != Z_OK) return NULL;
        out = (uint8_t*)malloc(ulen ? ulen : 1);
        if (!out) { inflateEnd(&zs); return NULL; }
        zs.next_in = (Bytef*)src; zs.avail_in = (uInt)len; zs.next_out = out; zs.avail_out = (uInt)ulen;
        int r = inflate(&zs, Z_FINISH);
        size_t got = ulen - zs.avail_out;
        inflateEnd(&zs);
        if (r != Z_STREAM_END && r != Z_OK) { free(out); return NULL; }
        *outlen = got; return out;
    }
#ifdef GMB_PARQUET_HAVE_ZSTD
    if (codec == 6) {   /* ZSTD */
        out = (uint8_t*)malloc(ulen ? ulen : 1);
        if (!out) return NULL;
        size_t r = ZSTD_decompress(out, ulen, src, len);
        if (ZSTD_isError(r)) { free(out); return NULL; }
        *outlen = r; return out;
    }
#endif
    return NULL;
}

/* ============================ Data page v1 decode ============================ */
/* PageHeader (Thrift): 1 type(i32: 0=DATA_PAGE,2=DICT,3=DATA_PAGE_V2) 2 uncompressed_page_size
 * 3 compressed_page_size 5 data_page_header 7 dictionary_page_header 8 data_page_header_v2 */
typedef struct {
    int type; int64_t ulen, clen;
    int32_t num_values; int encoding, def_enc, rep_enc;
    int32_t dict_num_values, dict_encoding;
    int32_t v2_num_nulls, v2_num_rows; int v2_is_compressed;
    int32_t v2_def_len, v2_rep_len;   /* DataPageHeaderV2: byte-length dei livelli */
} pq_pagehdr;

static int pq_parse_pagehdr(const uint8_t* p, size_t n, size_t* consumed, pq_pagehdr* h)
{
    thr_t t = { p, n, 0, 0 };
    int last = 0;
    memset(h, 0, sizeof(*h)); h->v2_is_compressed = 1;
    for (;;) {
        uint8_t b = thr_u8(&t);
        if (t.err || b == 0) break;
        int ft = b & 0x0f, delta = (b & 0xf0) >> 4;
        int fid = (delta == 0) ? (last = (int)thr_zigzag(&t)) : (last = last + delta);
        if (fid == 1) h->type = (int)thr_zigzag(&t);
        else if (fid == 2) h->ulen = thr_zigzag(&t);
        else if (fid == 3) h->clen = thr_zigzag(&t);
        else if (fid == 5) {                                  /* DataPageHeader */
            int dlast = 0;
            for (;;) {
                uint8_t db = thr_u8(&t); if (t.err || db == 0) break;
                int dft = db & 0x0f, dd = (db & 0xf0) >> 4;
                int dfid = (dd == 0) ? (dlast = (int)thr_zigzag(&t)) : (dlast = dlast + dd);
                if (dfid == 1) h->num_values = (int32_t)thr_zigzag(&t);
                else if (dfid == 2) h->encoding = (int)thr_zigzag(&t);
                else if (dfid == 3) h->def_enc = (int)thr_zigzag(&t);
                else if (dfid == 4) h->rep_enc = (int)thr_zigzag(&t);
                else thr_skip(&t, dft);
            }
        }
        else if (fid == 7) {                                  /* DictionaryPageHeader */
            int dlast = 0;
            for (;;) {
                uint8_t db = thr_u8(&t); if (t.err || db == 0) break;
                int dft = db & 0x0f, dd = (db & 0xf0) >> 4;
                int dfid = (dd == 0) ? (dlast = (int)thr_zigzag(&t)) : (dlast = dlast + dd);
                if (dfid == 1) h->dict_num_values = (int32_t)thr_zigzag(&t);
                else if (dfid == 2) h->dict_encoding = (int)thr_zigzag(&t);
                else thr_skip(&t, dft);
            }
        }
        else if (fid == 8) {                                  /* DataPageHeaderV2 */
            int dlast = 0;
            for (;;) {
                uint8_t db = thr_u8(&t); if (t.err || db == 0) break;
                int dft = db & 0x0f, dd = (db & 0xf0) >> 4;
                int dfid = (dd == 0) ? (dlast = (int)thr_zigzag(&t)) : (dlast = dlast + dd);
                if (dfid == 1) h->num_values = (int32_t)thr_zigzag(&t);
                else if (dfid == 2) h->v2_num_nulls = (int32_t)thr_zigzag(&t);
                else if (dfid == 3) h->v2_num_rows = (int32_t)thr_zigzag(&t);
                else if (dfid == 4) h->encoding = (int)thr_zigzag(&t);
                else if (dfid == 5) h->v2_def_len = (int32_t)thr_zigzag(&t);
                else if (dfid == 6) h->v2_rep_len = (int32_t)thr_zigzag(&t);
                else if (dfid == 7) h->v2_is_compressed = (dft == 1) ? 1 : 0;   /* packed bool nel nibble */
                else thr_skip(&t, dft);
            }
        }
        else thr_skip(&t, ft);
    }
    *consumed = t.i;
    return t.err ? -1 : 0;
}

/* ============================ RLE/bit-pack hybrid ============================ */
/* Decodifica `count` valori a larghezza `bw` bit da un buffer RLE/bit-packing (con
 * prefisso di lunghezza a 4 byte usato in RLE_DICTIONARY e nei booleani). */
typedef struct { const uint8_t* p; size_t n, i; int bw; } rle_t;
static uint32_t rle_varint(rle_t* r) { uint32_t v = 0; int s = 0; while (r->i < r->n) { uint8_t b = r->p[r->i++]; v |= (uint32_t)(b & 0x7f) << s; if (!(b & 0x80)) break; s += 7; } return v; }
static void rle_bitpack(rle_t* r, uint32_t n, uint32_t* out)
{
    int bw = r->bw;
    uint32_t mask = (bw >= 32) ? 0xffffffffu : ((1u << bw) - 1u);
    uint32_t bitpos = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t v = 0; int got = 0;
        while (got < bw) {
            size_t byte = r->i + (bitpos >> 3);
            if (byte >= r->n) { out[k] = 0; got = bw; break; }
            int avail = 8 - (int)(bitpos & 7);
            int take = (bw - got) < avail ? (bw - got) : avail;
            uint32_t chunk = (r->p[byte] >> (bitpos & 7)) & ((1u << take) - 1u);
            v |= chunk << got; got += take; bitpos += take;
        }
        out[k] = v & mask;
    }
    r->i += (bitpos + 7) / 8;
}
static void rle_decode(rle_t* r, uint32_t count, uint32_t* out)
{
    uint32_t k = 0;
    while (k < count && r->i < r->n) {
        uint32_t hdr = rle_varint(r);
        if (hdr & 1) { uint32_t groups = hdr >> 1; uint32_t n = groups * 8; if (n > count - k) n = count - k; rle_bitpack(r, n, out + k); k += n; }
        else {
            uint32_t run = hdr >> 1; int nbytes = (r->bw + 7) / 8; uint32_t v = 0;
            for (int b = 0; b < nbytes; b++) v |= (uint32_t)r->p[r->i++] << (8*b);
            for (uint32_t j = 0; j < run && k < count; j++) out[k++] = v;
        }
    }
    while (k < count) out[k++] = 0;
}

/* Decodifica livelli (v1: RLE con prefisso len a 4 byte; se max==0 tutti zero). */
static uint32_t* pq_decode_levels(const uint8_t** pp, const uint8_t* end, int maxlvl, int32_t nvalues)
{
    uint32_t* out = (uint32_t*)calloc((size_t)nvalues ? (size_t)nvalues : 1, sizeof(uint32_t));
    int bw = 1; while ((1 << bw) <= maxlvl) bw++;
    if (maxlvl == 0) return out;
    if (*pp + 4 > end) return out;
    uint32_t rlen = (uint32_t)(*pp)[0] | ((uint32_t)(*pp)[1] << 8) | ((uint32_t)(*pp)[2] << 16) | ((uint32_t)(*pp)[3] << 24);
    *pp += 4;
    if (*pp + rlen > end) rlen = (uint32_t)(end - *pp);
    rle_t r = { *pp, rlen, 0, bw };
    rle_decode(&r, (uint32_t)nvalues, out);
    *pp += rlen;
    return out;
}
/* Livelli di una DataPageV2: RLE senza prefisso di lunghezza (la lunghezza è nell'header). */
static uint32_t* pq_decode_levels_raw(const uint8_t* p, size_t len, int maxlvl, int32_t nvalues)
{
    uint32_t* out = (uint32_t*)calloc((size_t)nvalues ? (size_t)nvalues : 1, sizeof(uint32_t));
    int bw = 1; while ((1 << bw) <= maxlvl) bw++;
    if (maxlvl == 0 || len == 0) return out;
    rle_t r = { p, len, 0, bw };
    rle_decode(&r, (uint32_t)nvalues, out);
    return out;
}

/* ============================ Decodifica foglie (valori + def/rep) ============================ */
typedef struct {
    int ptype;
    double* dbl;
    char** str;
    uint32_t* def;
    uint32_t* rep;
    int64_t n;
    int max_def, max_rep;
} pq_leafvals;

/* ---- JSON ---- */
typedef struct { char* p; size_t len, cap; } jbuf;
static void jb_put(jbuf* b, const char* s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n + 1) nc *= 2;
        b->p = (char*)realloc(b->p, nc); b->cap = nc;
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = 0;
}
static void jb_str(jbuf* b, const char* s) { jb_put(b, s, strlen(s)); }
static void jb_qstr(jbuf* b, const char* s)
{
    jb_str(b, "\"");
    for (const unsigned char* p = (const unsigned char*)(s ? s : ""); *p; p++) {
        switch (*p) {
            case '"': jb_str(b, "\\\""); break;
            case '\\': jb_str(b, "\\\\"); break;
            case '\n': jb_str(b, "\\n"); break;
            case '\r': jb_str(b, "\\r"); break;
            case '\t': jb_str(b, "\\t"); break;
            default: if (*p < 0x20) { char t[8]; snprintf(t, sizeof(t), "\\u%04x", *p); jb_str(b, t); } else jb_put(b, (const char*)p, 1);
        }
    }
    jb_str(b, "\"");
}
static void jb_scalar(jbuf* b, const pq_leafvals* lv, uint32_t e)
{
    if (lv->ptype == PT_BYTE_ARRAY) jb_qstr(b, lv->str[e]);
    else if (lv->ptype == PT_BOOLEAN) jb_str(b, lv->dbl[e] != 0.0 ? "true" : "false");
    else if (lv->ptype == PT_INT32 || lv->ptype == PT_INT64) { char t[32]; snprintf(t, sizeof(t), "%lld", (long long)lv->dbl[e]); jb_str(b, t); }
    else { char t[48]; snprintf(t, sizeof(t), "%.15g", lv->dbl[e]); jb_str(b, t); }
}

/* ============================ vtab ============================ */
typedef enum { PK_SCALAR = 0, PK_STRUCT = 1, PK_LIST = 2, PK_MAP = 3 } pq_kind;

typedef struct {
    char* name;
    int kind;
    int ptype;            /* SCALAR */
    int leaf;             /* SCALAR: foglia; LIST: foglia elemento; MAP: foglia key */
    int val_leaf;         /* MAP: foglia value (-1) */
    int group_def;        /* STRUCT: max_def del gruppo */
    int rep_node_def;     /* LIST/MAP: max_def del nodo REPEATED */
    int leaves[64]; int nleaf;   /* STRUCT: foglie campo */
    int64_t* rec_start;   /* LIST/MAP: inizio entry per record (nrec+1) */
    int nrec;
    char** jrows;         /* nested: JSON per record (num_rows), NULL ammesso (per il write-back) */
    int64_t jrows_n;      /* numero di slot allocati in jrows */
    int unsupported;      /* annidamento > 1 livello: errore esplicito (mai troncamento silenzioso) */
} pq_col;

typedef struct {
    sqlite3_vtab base;
    pq_file* f;
    int ncol;
    pq_leafvals* leaves; int nleaves;
    pq_col* cols;
    unsigned char* alive;  /* slot vivi (tombstone per DELETE: rowid stabile) */
    int64_t nslots;        /* numero di slot (incluse righe cancellate) */
    int* leaf_owner;       /* leaf -> indice colonna (-1) */
    char path[4096];       /* percorso file (per write-back) */
} pq_vtab;
typedef struct { sqlite3_vtab_cursor base; int64_t row; } pq_cursor;

static int pq_read_file(const char* path, char** buf, size_t* len)
{
    FILE* f = fopen(path, "rb"); long sz; char* b; size_t rd;
    if (!f) return -1;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 8) { fclose(f); return -1; }
    b = (char*)malloc((size_t)sz);
    if (!b) { fclose(f); return -1; }
    rd = fread(b, 1, (size_t)sz, f); fclose(f);
    if (rd != (size_t)sz) { free(b); return -1; }
    *buf = b; *len = (size_t)sz; return 0;
}

static void pq_tree_walk(pq_file* f, int* cursor, int parent, int def, int rep, int* leaves, int* nleaf)
{
    int i = (*cursor)++;
    pq_schema_elem* e = &f->sch[i];
    int nd = def, nr = rep;
    e->parent = parent;
    if (e->rep == RT_OPTIONAL) nd++;
    else if (e->rep == RT_REPEATED) { nd++; nr++; }
    e->max_def = nd; e->max_rep = nr; e->col_idx = -1;
    if (e->type >= 0) { e->col_idx = *nleaf; leaves[(*nleaf)++] = i; }
    else for (int c = 0; c < e->nchildren; c++) pq_tree_walk(f, cursor, i, nd, nr, leaves, nleaf);
}
static void pq_build_tree(pq_file* f)
{
    int cursor = 0, nleaf = 0;
    f->leaf_elem = (int*)calloc((size_t)(f->nsch ? f->nsch : 1), sizeof(int));
    if (f->nsch > 0) pq_tree_walk(f, &cursor, -1, 0, 0, f->leaf_elem, &nleaf);
    f->nleaf = nleaf;
}
static int pq_subtree_end(pq_file* f, int idx)
{
    int end = idx + 1;
    for (int c = 0; c < f->sch[idx].nchildren; c++) end = pq_subtree_end(f, end);
    return end;
}
/* nodo REPEATED antenato (per dr) della foglia leafidx */
static int pq_rep_node_def(pq_file* f, int leafidx)
{
    int e = f->leaf_elem[leafidx];
    while (e >= 0) { if (f->sch[e].rep == RT_REPEATED) return f->sch[e].max_def; e = f->sch[e].parent; }
    return 0;
}

/* Decodifica UNA foglia di UN row-group, APPENDENDO in lv (già allocata). */
/* Decodifica i VALORI (non i livelli) di una pagina, appendendo in lv. */
static void pq_values_decode(pq_colchunk* cc, pq_pagehdr* ph, pq_leafvals* lv,
                             const uint8_t* dp, const uint8_t* dend,
                             uint32_t* def, uint32_t* rep, int32_t nv,
                             uint8_t* dict_data, int64_t dict_len, int32_t dict_n)
{
    int max_def = lv->max_def;
    int32_t present = 0, k;
    for (k = 0; k < nv; k++) if (def[k] == (uint32_t)max_def) present++;
    if (ph->encoding == 8 || ph->encoding == 2) {   /* RLE_DICTIONARY / PLAIN_DICTIONARY */
        int bw = *dp; const uint8_t* bp = dp + 1; size_t blen = (size_t)(dend - bp);
        uint32_t* idx = (uint32_t*)calloc((size_t)(present ? present : 1), sizeof(uint32_t));
        rle_t r = { bp, blen, 0, bw };
        rle_decode(&r, (uint32_t)present, idx);
        int p = 0;
        for (k = 0; k < nv; k++) {
            lv->def[lv->n] = def[k]; lv->rep[lv->n] = rep ? rep[k] : 0;
            if (def[k] == (uint32_t)max_def) {
                uint32_t di = idx[p++];
                if (di < (uint32_t)dict_n) {
                    if (cc->type == PT_INT32) lv->dbl[lv->n] = (double)((int32_t*)dict_data)[di];
                    else if (cc->type == PT_INT64) lv->dbl[lv->n] = (double)((int64_t*)dict_data)[di];
                    else if (cc->type == PT_FLOAT) lv->dbl[lv->n] = (double)((float*)dict_data)[di];
                    else if (cc->type == PT_DOUBLE) lv->dbl[lv->n] = ((double*)dict_data)[di];
                    else if (cc->type == PT_BYTE_ARRAY) {
                        const uint8_t* q = dict_data; const uint8_t* qe = dict_data + dict_len;
                        int32_t dl = -1; int ok = 0;
                        for (uint32_t z = 0; z <= di; z++) { if (q + 4 > qe) { ok = 0; break; } memcpy(&dl, q, 4); q += 4; if (z == di) { ok = (dl >= 0 && q + dl <= qe); break; } if (dl < 0 || q + dl > qe) { ok = 0; break; } q += dl; }
                        if (ok) { char* st = (char*)malloc((size_t)dl + 1); memcpy(st, q, dl); st[dl] = 0; lv->str[lv->n] = st; }
                    }
                }
            }
            lv->n++;
        }
        free(idx);
    } else if (ph->encoding == 0 || ph->encoding == 4) {   /* PLAIN */
        const uint8_t* q = dp; int bitp = 0;
        for (k = 0; k < nv; k++) {
            lv->def[lv->n] = def[k]; lv->rep[lv->n] = rep ? rep[k] : 0;
            if (def[k] == (uint32_t)max_def) {
                if (cc->type == PT_INT32) { int32_t v; memcpy(&v, q, 4); q += 4; lv->dbl[lv->n] = (double)v; }
                else if (cc->type == PT_INT64) { int64_t v; memcpy(&v, q, 8); q += 8; lv->dbl[lv->n] = (double)v; }
                else if (cc->type == PT_FLOAT) { float v; memcpy(&v, q, 4); q += 4; lv->dbl[lv->n] = (double)v; }
                else if (cc->type == PT_DOUBLE) { double v; memcpy(&v, q, 8); q += 8; lv->dbl[lv->n] = v; }
                else if (cc->type == PT_BYTE_ARRAY) { int32_t dl; memcpy(&dl, q, 4); q += 4; char* st = (char*)malloc((size_t)dl + 1); memcpy(st, q, dl); st[dl] = 0; q += dl; lv->str[lv->n] = st; }
                else if (cc->type == PT_BOOLEAN) { lv->dbl[lv->n] = (q[bitp >> 3] >> (bitp & 7)) & 1; }
            }
            if (cc->type == PT_BOOLEAN) bitp++;
            lv->n++;
        }
    }
}

static void pq_decode_chunk(pq_vtab* vt, pq_colchunk* cc, pq_leafvals* lv)
{
    pq_file* f = vt->f;
    int max_def = lv->max_def, max_rep = lv->max_rep;
    int64_t off = (cc->dictionary_page_offset >= 0 && cc->dictionary_page_offset < cc->data_page_offset)
                  ? cc->dictionary_page_offset : cc->data_page_offset;
    int64_t end = off + cc->total_compressed_size, pos = off;
    uint8_t* dict_data = NULL; int64_t dict_len = 0; int32_t dict_n = 0;
    if (off < 0 || end > (int64_t)f->buf_len) return;
    while (pos < end) {
        size_t consumed = 0; pq_pagehdr ph;
        if (pq_parse_pagehdr((const uint8_t*)f->buf + pos, (size_t)(end - pos), &consumed, &ph) != 0) break;
        const uint8_t* cdata = (const uint8_t*)f->buf + pos + consumed;
        size_t comp_len = (size_t)ph.clen, ulen = (size_t)ph.ulen, dlen = 0;
        int32_t nv = ph.num_values;
        if (ph.type == 2) {                                   /* DICTIONARY_PAGE */
            uint8_t* data = pq_decompress(cc->codec, cdata, comp_len, ulen, &dlen);
            if (!data) break;
            dict_n = ph.dict_num_values; dict_data = data; dict_len = (int64_t)dlen;
        } else if (ph.type == 0) {                            /* DATA_PAGE (v1) */
            uint8_t* data = pq_decompress(cc->codec, cdata, comp_len, ulen, &dlen);
            if (!data) break;
            const uint8_t* dp = data; const uint8_t* dend = data + dlen;
            uint32_t* rep = (max_rep > 0) ? pq_decode_levels(&dp, dend, max_rep, nv) : NULL;
            uint32_t* def = pq_decode_levels(&dp, dend, max_def, nv);
            pq_values_decode(cc, &ph, lv, dp, dend, def, rep, nv, dict_data, dict_len, dict_n);
            free(def); free(rep); free(data);
        } else if (ph.type == 3) {                            /* DATA_PAGE_V2 */
            int32_t rlenL = ph.v2_rep_len, dlenL = ph.v2_def_len;
            if (rlenL < 0) rlenL = 0; if (dlenL < 0) dlenL = 0;
            if ((size_t)rlenL + (size_t)dlenL > comp_len) { pos += consumed + (int64_t)comp_len; continue; }
            uint32_t* rep = (max_rep > 0 && rlenL > 0) ? pq_decode_levels_raw(cdata, (size_t)rlenL, max_rep, nv) : NULL;
            uint32_t* def = pq_decode_levels_raw(cdata + rlenL, (size_t)dlenL, max_def, nv);
            const uint8_t* vraw = cdata + rlenL + dlenL;
            size_t vraw_len = comp_len - (size_t)(rlenL + dlenL);
            uint8_t* vdata = (uint8_t*)vraw; size_t vdlen = vraw_len;
            if (ph.v2_is_compressed) {
                size_t base = (size_t)(rlenL + dlenL);
                size_t vun = (ulen > base) ? ulen - base : 0;
                vdata = pq_decompress(cc->codec, vraw, vraw_len, vun, &vdlen);
                if (!vdata) { free(def); free(rep); break; }
            }
            pq_values_decode(cc, &ph, lv, vdata, vdata + vdlen, def, rep, nv, dict_data, dict_len, dict_n);
            if (ph.v2_is_compressed && vdata) free(vdata);
            free(def); free(rep);
        }
        pos += consumed + (int64_t)comp_len;
    }
    if (dict_data) free(dict_data);
}
/* Decodifica una foglia concatenando TUTTI i row-group (schema foglie identico). */
static int pq_decode_leaf(pq_vtab* vt, int leafidx)
{
    pq_file* f = vt->f;
    pq_leafvals* lv = &vt->leaves[leafidx];
    int max_def = 0, max_rep = 0, r;
    if (leafidx < f->nleaf && f->leaf_elem) { max_def = f->sch[f->leaf_elem[leafidx]].max_def; max_rep = f->sch[f->leaf_elem[leafidx]].max_rep; }
    if (f->nrgs <= 0 || !f->rgs[0].cols) return -1;
    int64_t cap = 0;
    for (r = 0; r < f->nrgs; r++) cap += f->rgs[r].cols[leafidx].num_values;
    if (cap <= 0) cap = f->num_rows > 0 ? f->num_rows : 1;
    lv->ptype = f->rgs[0].cols[leafidx].type; lv->max_def = max_def; lv->max_rep = max_rep; lv->n = 0;
    lv->dbl = (double*)calloc((size_t)cap, sizeof(double));
    lv->str = (f->rgs[0].cols[leafidx].type == PT_BYTE_ARRAY) ? (char**)calloc((size_t)cap, sizeof(char*)) : NULL;
    lv->def = (uint32_t*)calloc((size_t)cap, sizeof(uint32_t));
    lv->rep = (uint32_t*)calloc((size_t)cap, sizeof(uint32_t));
    for (r = 0; r < f->nrgs; r++) {
        int codec = f->rgs[r].cols[leafidx].codec;
        if (!(codec == 0 || codec == 1 || codec == 2 || codec == 6)) return -(codec + 1);   /* codec non supportato */
        pq_decode_chunk(vt, &f->rgs[r].cols[leafidx], lv);
    }
    return 0;
}

/* --- colonne = figli radice; nested → JSON --- */
static void pq_build_cols(pq_vtab* vt)
{
    pq_file* f = vt->f;
    int idx = 1, ncol = 0;
    pq_col* cols = (pq_col*)calloc((size_t)(f->nsch ? f->nsch : 1), sizeof(pq_col));
    while (idx < f->nsch) {
        int end = pq_subtree_end(f, idx);
        pq_col* c = &cols[ncol];
        c->name = strdup(f->sch[idx].name);
        c->rep_node_def = -1; c->val_leaf = -1;
        if (f->sch[idx].type >= 0) {              /* foglia top-level = scalare */
            c->kind = PK_SCALAR; c->leaf = f->sch[idx].col_idx; c->ptype = f->sch[idx].type;
        } else {
            /* foglie del sottoalbero (contigue in ordine di foglia) */
            int leaves[64], nleaf = 0, any_rep = 0, deepest_rep = 0;
            for (int k = idx; k < end && nleaf < 64; k++)
                if (f->sch[k].type >= 0) { leaves[nleaf++] = f->sch[k].col_idx; if (f->sch[k].max_rep > 0) any_rep = 1; if (f->sch[k].max_rep > deepest_rep) deepest_rep = f->sch[k].max_rep; }
            c->nleaf = nleaf;
            for (int k = 0; k < nleaf; k++) c->leaves[k] = leaves[k];
            /* marker LIST/MAP (spec Parquet): converted_type o nome figlio */
            int is_map = (f->sch[idx].converted == 1), is_list = (f->sch[idx].converted == 3);
            for (int k = idx + 1; k < end; k++) { if (strcmp(f->sch[k].name, "key_value") == 0) is_map = 1; if (strcmp(f->sch[k].name, "list") == 0) is_list = 1; }
            if (!any_rep) {
                c->kind = PK_STRUCT; c->group_def = f->sch[idx].max_def;
            } else if (is_map && nleaf >= 2) {
                c->kind = PK_MAP; c->leaf = leaves[0]; c->val_leaf = leaves[1];
                c->rep_node_def = pq_rep_node_def(f, leaves[0]);
                if (nleaf > 2 || deepest_rep > 1) c->unsupported = 1;   /* map<...,struct/list> o annidato >1 livello */
            } else if (is_list) {
                c->kind = PK_LIST; c->leaf = leaves[0];
                c->rep_node_def = pq_rep_node_def(f, leaves[0]);
                if (nleaf != 1 || deepest_rep > 1) c->unsupported = 1;  /* list<struct> o list<list> */
            } else {
                /* gruppo con ripetizione ma senza marker LIST/MAP = struct contenente un campo ripetuto */
                c->kind = PK_STRUCT; c->group_def = f->sch[idx].max_def;
                c->unsupported = 1;                                     /* struct<list>: annidato >1 livello */
            }
        }
        ncol++;
        idx = end;
    }
    vt->cols = cols; vt->ncol = ncol;
    /* boundaries record per LIST/MAP */
    for (int c = 0; c < ncol; c++) {
        pq_col* pc = &cols[c];
        if (pc->kind != PK_LIST && pc->kind != PK_MAP) continue;
        pq_leafvals* lv = &vt->leaves[pc->leaf];
        int64_t cap = lv->n + 1;
        pc->rec_start = (int64_t*)calloc((size_t)cap, sizeof(int64_t));
        int nrec = 0;
        for (int64_t e = 0; e < lv->n; e++) if (lv->rep[e] == 0) pc->rec_start[nrec++] = e;
        pc->rec_start[nrec] = lv->n;
        pc->nrec = nrec;
    }
}

/* assemblaggio JSON di una colonna nested per il record r */
static void pq_json_col(jbuf* b, pq_vtab* vt, pq_col* c, int64_t r)
{
    pq_leafvals* L = vt->leaves;
    if (c->kind == PK_STRUCT) {
        int fl = c->leaves[0];
        if (fl >= 0 && r < L[fl].n && (int)L[fl].def[r] < c->group_def) { jb_str(b, "null"); return; }
        jb_str(b, "{");
        for (int k = 0; k < c->nleaf; k++) {
            pq_leafvals* lv = &L[c->leaves[k]];
            if (k) jb_str(b, ",");
            jb_qstr(b, vt->cols ? vt->f->sch[vt->f->leaf_elem[c->leaves[k]]].name : "");
            jb_str(b, ":");
            if (r < lv->n && (int)lv->def[r] == lv->max_def) jb_scalar(b, lv, (uint32_t)r);
            else jb_str(b, "null");
        }
        jb_str(b, "}");
    } else if (c->kind == PK_LIST) {
        pq_leafvals* lv = &L[c->leaf];
        int dr = c->rep_node_def, Lmax = lv->max_def;
        int64_t s = (r < c->nrec) ? c->rec_start[r] : lv->n;
        int64_t e2 = (r + 1 <= c->nrec) ? c->rec_start[r + 1] : lv->n;
        if (s >= e2) { jb_str(b, "[]"); return; }
        uint32_t d0 = lv->def[s];
        if ((int)d0 < dr - 1) { jb_str(b, "null"); return; }
        if ((int)d0 == dr - 1) { jb_str(b, "[]"); return; }
        jb_str(b, "[");
        for (int64_t e = s, first = 1; e < e2; e++) {
            if (!first) jb_str(b, ","); first = 0;
            if ((int)lv->def[e] == Lmax) jb_scalar(b, lv, (uint32_t)e);
            else jb_str(b, "null");
        }
        jb_str(b, "]");
    } else if (c->kind == PK_MAP) {
        pq_leafvals* klv = &L[c->leaf]; pq_leafvals* vlv = &L[c->val_leaf];
        int dr = c->rep_node_def;
        int64_t s = (r < c->nrec) ? c->rec_start[r] : klv->n;
        int64_t e2 = (r + 1 <= c->nrec) ? c->rec_start[r + 1] : klv->n;
        if (s >= e2) { jb_str(b, "{}"); return; }
        uint32_t d0 = klv->def[s];
        if ((int)d0 < dr - 1) { jb_str(b, "null"); return; }
        if ((int)d0 == dr - 1) { jb_str(b, "{}"); return; }
        jb_str(b, "{");
        for (int64_t e = s, first = 1; e < e2; e++) {
            if (!first) jb_str(b, ","); first = 0;
            jb_qstr(b, klv->str[e] ? klv->str[e] : "");
            jb_str(b, ":");
            if (e < vlv->n && (int)vlv->def[e] == vlv->max_def) jb_scalar(b, vlv, (uint32_t)e);
            else jb_str(b, "null");
        }
        jb_str(b, "}");
    } else jb_str(b, "null");
}

/* ============================ Writer Parquet (schema generale: piatto + nested) ============
 * Serializza le foglie in un file Parquet valido: DATA_PAGE v1 con livelli (rep poi def) RLE/
 * bit-packed e valori PLAIN, compressione UNCOMPRESSED, un row-group, footer FileMetaData Thrift
 * Compact. Lo SCHEMA è quello originale (f->sch): i write cambiano solo i dati. Per le colonne
 * annidate i valori per-record (JSON esposto all'utente) vengono ri-shreddati nei def/rep delle
 * foglie. Scrittura atomica (tmp + rename). */
typedef struct { unsigned char* p; size_t len, cap; } wbuf;
static void wb_need(wbuf* b, size_t n) { if (b->len + n > b->cap) { size_t nc = b->cap ? b->cap * 2 : 256; while (nc < b->len + n) nc *= 2; b->p = (unsigned char*)realloc(b->p, nc); b->cap = nc; } }
static void wb_raw(wbuf* b, const void* s, size_t n) { wb_need(b, n); memcpy(b->p + b->len, s, n); b->len += n; }
static void wb_u8(wbuf* b, unsigned v) { unsigned char c = (unsigned char)v; wb_raw(b, &c, 1); }
static void wb_varint(wbuf* b, uint64_t v) { for (;;) { unsigned char c = (unsigned char)(v & 0x7f); v >>= 7; if (v) c |= 0x80; wb_u8(b, c); if (!v) break; } }
static void wb_zz(wbuf* b, int64_t v) { wb_varint(b, ((uint64_t)v << 1) ^ (uint64_t)(v >> 63)); }
static void wb_field(wbuf* b, int fid, int type, int* last) { int d = fid - *last; if (d > 0 && d <= 15) wb_u8(b, (unsigned)((d << 4) | type)); else { wb_u8(b, (unsigned)type); wb_zz(b, fid); } *last = fid; }
static void wb_stop(wbuf* b) { wb_u8(b, 0); }
static void wb_bin(wbuf* b, const char* s, size_t n) { wb_varint(b, n); wb_raw(b, s, n); }
static void wb_listhdr(wbuf* b, int et, size_t n) { if (n < 15) wb_u8(b, (unsigned)((n << 4) | et)); else { wb_u8(b, (unsigned)(0xf0 | et)); wb_varint(b, n); } }

/* livelli (bit-packed, bw bit) con prefisso len 4 byte LE. */
static void wb_levels(wbuf* b, const uint32_t* vals, int64_t n, int bw)
{
    wbuf s; memset(&s, 0, sizeof(s));
    if (bw > 0 && n > 0) {
        int64_t ngroups = (n + 7) / 8;
        wb_varint(&s, (uint64_t)((ngroups << 1) | 1));
        int nbytes = bw;   /* 8 valori × bw bit = bw byte per gruppo */
        for (int64_t g = 0; g < ngroups; g++) {
            uint32_t bytev = 0;
            for (int k = 0; k < 8; k++) { int64_t i = g * 8 + k; if (i < n) bytev |= (vals[i] & ((bw >= 32) ? 0xffffffffu : ((1u << bw) - 1u))) << (k * bw); }
            for (int b = 0; b < nbytes; b++) wb_u8(&s, (bytev >> (8 * b)) & 0xff);
        }
    }
    { uint32_t l = (uint32_t)s.len; wb_u8(b, l & 0xff); wb_u8(b, (l >> 8) & 0xff); wb_u8(b, (l >> 16) & 0xff); wb_u8(b, (l >> 24) & 0xff); }
    wb_raw(b, s.p, s.len); free(s.p);
}

/* Serializza una foglia (con i suoi def/rep) in una pagina DATA_PAGE v1. */
static void pq_ser_leaf(pq_leafvals* lv, wbuf* pages, int64_t* page_off, int64_t* total_sz)
{
    int64_t n = lv->n, i;
    wbuf body; memset(&body, 0, sizeof(body));
    int bwrep = 0, bwdef = 0;
    while ((1 << bwrep) <= lv->max_rep) bwrep++;
    while ((1 << bwdef) <= lv->max_def) bwdef++;
    if (lv->max_rep > 0) wb_levels(&body, lv->rep, n, bwrep);
    if (lv->max_def > 0) wb_levels(&body, lv->def, n, bwdef);
    /* valori PLAIN (presenti: def==max_def) */
    if (lv->ptype == PT_BOOLEAN) {
        unsigned char cur = 0; int bit = 0;
        for (i = 0; i < n; i++) { if ((int)lv->def[i] != lv->max_def) continue; if (lv->dbl[i] != 0.0) cur |= (unsigned char)(1u << bit); if (++bit == 8) { wb_u8(&body, cur); cur = 0; bit = 0; } }
        if (bit) wb_u8(&body, cur);
    } else {
        for (i = 0; i < n; i++) {
            if ((int)lv->def[i] != lv->max_def) continue;
            if (lv->ptype == PT_INT32) { int32_t v = (int32_t)lv->dbl[i]; wb_raw(&body, &v, 4); }
            else if (lv->ptype == PT_INT64) { int64_t v = (int64_t)lv->dbl[i]; wb_raw(&body, &v, 8); }
            else if (lv->ptype == PT_FLOAT) { float v = (float)lv->dbl[i]; wb_raw(&body, &v, 4); }
            else if (lv->ptype == PT_DOUBLE) { double v = lv->dbl[i]; wb_raw(&body, &v, 8); }
            else if (lv->ptype == PT_BYTE_ARRAY) { const char* s = lv->str[i] ? lv->str[i] : ""; uint32_t sl = (uint32_t)strlen(s); unsigned char lb[4] = { (unsigned char)(sl&0xff),(unsigned char)((sl>>8)&0xff),(unsigned char)((sl>>16)&0xff),(unsigned char)((sl>>24)&0xff) }; wb_raw(&body, lb, 4); wb_raw(&body, s, sl); }
        }
    }
    wbuf ph; memset(&ph, 0, sizeof(ph)); int last = 0;
    wb_field(&ph, 1, 5, &last); wb_zz(&ph, 0);
    wb_field(&ph, 2, 5, &last); wb_zz(&ph, (int64_t)body.len);
    wb_field(&ph, 3, 5, &last); wb_zz(&ph, (int64_t)body.len);
    wb_field(&ph, 5, 12, &last);
    { int dl = 0;
      wb_field(&ph, 1, 5, &dl); wb_zz(&ph, n);
      wb_field(&ph, 2, 5, &dl); wb_zz(&ph, 0);
      wb_field(&ph, 3, 5, &dl); wb_zz(&ph, 3);
      wb_field(&ph, 4, 5, &dl); wb_zz(&ph, 3);
      wb_stop(&ph);
    }
    wb_stop(&ph);
    *page_off = (int64_t)pages->len;
    wb_raw(pages, ph.p, ph.len);
    wb_raw(pages, body.p, body.len);
    *total_sz = (int64_t)(ph.len + body.len);
    free(ph.p); free(body.p);
}

/* ---------- JSON minimale ---------- */
enum { JV_NULL, JV_BOOL, JV_NUM, JV_STR, JV_ARR, JV_OBJ };
typedef struct jv { int t; double num; int b; char* s; struct jv** it; char** key; int n, cap; } jv;
static void jv_free(jv* v) { if (!v) return; free(v->s); for (int i = 0; i < v->n; i++) { free(v->key ? v->key[i] : NULL); jv_free(v->it[i]); } free(v->it); free(v->key); free(v); }
static void jskip(const char** p) { while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++; }
static char* jstr(const char** p)
{
    wbuf s; memset(&s, 0, sizeof(s));
    if (**p != '"') return NULL;
    (*p)++;
    while (**p && **p != '"') {
        if (**p == '\\') { (*p)++; char c = **p; char o = (c=='n')?'\n':(c=='t')?'\t':(c=='r')?'\r':c; wb_raw(&s, &o, 1); (*p)++; }
        else { wb_raw(&s, *p, 1); (*p)++; }
    }
    if (**p == '"') (*p)++;
    wb_u8(&s, 0);
    return (char*)s.p;
}
static jv* jparse(const char** p)
{
    jskip(p);
    jv* v = (jv*)calloc(1, sizeof(jv));
    if (**p == 'n') { v->t = JV_NULL; *p += 4; return v; }
    if (**p == 't') { v->t = JV_BOOL; v->b = 1; *p += 4; return v; }
    if (**p == 'f') { v->t = JV_BOOL; v->b = 0; *p += 5; return v; }
    if (**p == '"') { v->t = JV_STR; v->s = jstr(p); return v; }
    if (**p == '[') { (*p)++; v->t = JV_ARR; for (;;) { jskip(p); if (**p == ']') { (*p)++; break; } if (**p == 0) break; if (v->n >= v->cap) { v->cap = v->cap ? v->cap * 2 : 4; v->it = (jv**)realloc(v->it, (size_t)v->cap * sizeof(jv*)); } v->it[v->n++] = jparse(p); jskip(p); if (**p == ',') { (*p)++; continue; } if (**p == ']') { (*p)++; break; } break; } return v; }
    if (**p == '{') { (*p)++; v->t = JV_OBJ; for (;;) { jskip(p); if (**p == '}') { (*p)++; break; } if (**p == 0) break; char* k = jstr(p); jskip(p); if (**p == ':') (*p)++; if (v->n >= v->cap) { v->cap = v->cap ? v->cap * 2 : 4; v->it = (jv**)realloc(v->it, (size_t)v->cap * sizeof(jv*)); v->key = (char**)realloc(v->key, (size_t)v->cap * sizeof(char*)); } v->key[v->n] = k; v->it[v->n++] = jparse(p); jskip(p); if (**p == ',') { (*p)++; continue; } if (**p == '}') { (*p)++; break; } break; } return v; }
    { char* e; v->t = JV_NUM; v->num = strtod(*p, &e); if (e == *p) (*p)++; else *p = e; return v; }
}
static jv* jobj_get(jv* o, const char* key) { if (!o || o->t != JV_OBJ) return NULL; for (int i = 0; i < o->n; i++) if (o->key[i] && strcmp(o->key[i], key) == 0) return o->it[i]; return NULL; }

/* push di una entry in una foglia annidata */
static void leaf_push(pq_leafvals* lv, uint32_t def, uint32_t rep, int hasval, double num, const char* str)
{
    int64_t i = lv->n;
    lv->dbl = (double*)realloc(lv->dbl, (size_t)(i + 1) * sizeof(double));
    lv->def = (uint32_t*)realloc(lv->def, (size_t)(i + 1) * sizeof(uint32_t));
    lv->rep = (uint32_t*)realloc(lv->rep, (size_t)(i + 1) * sizeof(uint32_t));
    if (lv->ptype == PT_BYTE_ARRAY) lv->str = (char**)realloc(lv->str, (size_t)(i + 1) * sizeof(char*));
    lv->def[i] = def; lv->rep[i] = rep; lv->dbl[i] = 0;
    if (lv->ptype == PT_BYTE_ARRAY) lv->str[i] = NULL;
    if (hasval) { if (lv->ptype == PT_BYTE_ARRAY) { if (lv->str) lv->str[i] = strdup(str ? str : ""); } else lv->dbl[i] = num; }
    lv->n++;
}
static int jv_num(jv* v, double* out) { if (!v) return 0; if (v->t == JV_NUM) { *out = v->num; return 1; } if (v->t == JV_BOOL) { *out = v->b ? 1 : 0; return 1; } return 0; }
static void leaf_clear(pq_leafvals* lv) { if (lv->str) { for (int64_t k = 0; k < lv->n; k++) free(lv->str[k]); free(lv->str); lv->str = NULL; } free(lv->dbl); free(lv->def); free(lv->rep); lv->dbl = NULL; lv->def = NULL; lv->rep = NULL; lv->n = 0; }


/* ricalcola i confini record (rep==0) dopo uno shred. */
static void pq_recompute_recstart(pq_col* pc, pq_leafvals* lv)
{
    free(pc->rec_start);
    pc->rec_start = (int64_t*)calloc((size_t)(lv->n + 1), sizeof(int64_t));
    int nrec = 0;
    for (int64_t e = 0; e < lv->n; e++) if (lv->rep[e] == 0) pc->rec_start[nrec++] = e;
    pc->rec_start[nrec] = lv->n;
    pc->nrec = nrec;
}

/* rigenera le foglie di una colonna nested dai jrows (JSON per record) */
static void pq_shred(pq_vtab* vt, pq_col* pc, pq_leafvals* T)
{
    pq_leafvals* L = T;
    int64_t nrow = vt->nslots;
    if (pc->kind == PK_SCALAR) return;
    if (pc->kind == PK_STRUCT) {
        for (int k = 0; k < pc->nleaf; k++) leaf_clear(&L[pc->leaves[k]]);
        for (int64_t r = 0; r < nrow; r++) {
            if (!vt->alive[r]) continue;
            const char* s = pc->jrows ? pc->jrows[r] : NULL; jv* v = NULL; const char* p;
            if (s) { p = s; v = jparse(&p); }
            int isnull = (!v || v->t == JV_NULL);
            for (int k = 0; k < pc->nleaf; k++) {
                pq_leafvals* lv = &L[pc->leaves[k]];
                if (isnull) { leaf_push(lv, (uint32_t)(pc->group_def > 0 ? pc->group_def - 1 : 0), 0, 0, 0, NULL); continue; }
                const char* fname = vt->f->sch[vt->f->leaf_elem[pc->leaves[k]]].name;
                jv* fv = jobj_get(v, fname);
                if (fv && fv->t != JV_NULL) { double d = 0; leaf_push(lv, (uint32_t)lv->max_def, 0, 1, jv_num(fv, &d) ? d : 0, fv->t == JV_STR ? fv->s : NULL); }
                else leaf_push(lv, (uint32_t)pc->group_def, 0, 0, 0, NULL);
            }
            jv_free(v);
        }
    } else if (pc->kind == PK_LIST) {
        pq_leafvals* lv = &L[pc->leaf]; int dr = pc->rep_node_def; int Lmax = lv->max_def;
        leaf_clear(lv);
        for (int64_t r = 0; r < nrow; r++) {
            if (!vt->alive[r]) continue;
            const char* s = pc->jrows ? pc->jrows[r] : NULL; jv* v = NULL; const char* p;
            if (s) { p = s; v = jparse(&p); }
            if (!v || v->t == JV_NULL) { leaf_push(lv, (uint32_t)(dr > 1 ? dr - 2 : 0), 0, 0, 0, NULL); jv_free(v); continue; }
            if (v->t != JV_ARR || v->n == 0) { leaf_push(lv, (uint32_t)(dr > 0 ? dr - 1 : 0), 0, 0, 0, NULL); jv_free(v); continue; }
            for (int i = 0; i < v->n; i++) {
                jv* e = v->it[i]; double d = 0;
                if (e && e->t != JV_NULL) leaf_push(lv, (uint32_t)Lmax, (uint32_t)(i ? 1 : 0), 1, jv_num(e, &d) ? d : 0, e->t == JV_STR ? e->s : NULL);
                else leaf_push(lv, (uint32_t)(Lmax > 0 ? Lmax - 1 : 0), (uint32_t)(i ? 1 : 0), 0, 0, NULL);
            }
            jv_free(v);
        }
        pq_recompute_recstart(pc, lv);
    } else if (pc->kind == PK_MAP) {
        pq_leafvals* k = &L[pc->leaf]; pq_leafvals* val = &L[pc->val_leaf]; int dr = pc->rep_node_def;
        leaf_clear(k); leaf_clear(val);
        for (int64_t r = 0; r < nrow; r++) {
            if (!vt->alive[r]) continue;
            const char* s = pc->jrows ? pc->jrows[r] : NULL; jv* v = NULL; const char* p;
            if (s) { p = s; v = jparse(&p); }
            if (!v || v->t == JV_NULL) { leaf_push(k, (uint32_t)(dr > 1 ? dr - 2 : 0), 0, 0, 0, NULL); leaf_push(val, (uint32_t)(dr > 1 ? dr - 2 : 0), 0, 0, 0, NULL); jv_free(v); continue; }
            if (v->t != JV_OBJ || v->n == 0) { leaf_push(k, (uint32_t)(dr > 0 ? dr - 1 : 0), 0, 0, 0, NULL); leaf_push(val, (uint32_t)(dr > 0 ? dr - 1 : 0), 0, 0, 0, NULL); jv_free(v); continue; }
            for (int i = 0; i < v->n; i++) {
                uint32_t rep = (uint32_t)(i ? 1 : 0);
                leaf_push(k, (uint32_t)k->max_def, rep, 1, 0, v->key[i] ? v->key[i] : "");
                jv* vv = v->it[i]; double d = 0;
                if (vv && vv->t != JV_NULL) leaf_push(val, (uint32_t)val->max_def, rep, 1, jv_num(vv, &d) ? d : 0, vv->t == JV_STR ? vv->s : NULL);
                else leaf_push(val, (uint32_t)(val->max_def > 0 ? val->max_def - 1 : 0), rep, 0, 0, NULL);
            }
            jv_free(v);
        }
        pq_recompute_recstart(pc, k);
    }
}

static void leaf_copy_filtered(const pq_leafvals* src, const unsigned char* alive, int64_t nslots, pq_leafvals* dst)
{
    int64_t i;
    memset(dst, 0, sizeof(*dst));
    dst->ptype = src->ptype; dst->max_def = src->max_def; dst->max_rep = src->max_rep;
    for (i = 0; i < nslots && i < src->n; i++) {
        if (!alive[i]) continue;
        leaf_push(dst, src->def[i], src->rep[i], (int)src->def[i] == src->max_def,
                  src->dbl ? src->dbl[i] : 0, (src->ptype == PT_BYTE_ARRAY && src->str) ? src->str[i] : NULL);
    }
}
static void pq_write_file(pq_vtab* vt)
{
    wbuf pages; memset(&pages, 0, sizeof(pages));
    int nleaves = vt->nleaves;
    int64_t* offs = (int64_t*)calloc((size_t)nleaves, sizeof(int64_t));
    int64_t* sizes = (int64_t*)calloc((size_t)nleaves, sizeof(int64_t));
    pq_leafvals* T = (pq_leafvals*)calloc((size_t)(nleaves ? nleaves : 1), sizeof(pq_leafvals));   /* leaf filtrate per la scrittura */
    int l;
    /* Le leaf temporanee T devono ereditare ptype/max_def/max_rep: senza, per le colonne
     * ANNIDATE restano 0 (=> tipo BOOLEAN e NESSUN livello), producendo un file corrotto
     * che il reader in-sessione maschera leggendo i jrows in memoria. */
    for (l = 0; l < nleaves; l++) { T[l].ptype = vt->leaves[l].ptype; T[l].max_def = vt->leaves[l].max_def; T[l].max_rep = vt->leaves[l].max_rep; }
    for (l = 0; l < nleaves; l++) {
        int c = vt->leaf_owner ? vt->leaf_owner[l] : -1;
        if (c >= 0 && vt->cols[c].kind == PK_SCALAR) leaf_copy_filtered(&vt->leaves[l], vt->alive, vt->nslots, &T[l]);
    }
    for (int c = 0; c < vt->ncol; c++) if (vt->cols[c].kind != PK_SCALAR) pq_shred(vt, &vt->cols[c], T);
    wb_bin(&pages, "PAR1", 4);
    for (l = 0; l < nleaves; l++) pq_ser_leaf(&T[l], &pages, &offs[l], &sizes[l]);
    wbuf md; memset(&md, 0, sizeof(md)); int L = 0;
    wb_field(&md, 1, 5, &L); wb_zz(&md, 1);
    wb_field(&md, 2, 9, &L); wb_listhdr(&md, 12, (size_t)vt->f->nsch);
    for (int i = 0; i < vt->f->nsch; i++) {
        pq_schema_elem* e = &vt->f->sch[i];
        int l = 0;
        if (e->type >= 0) { wb_field(&md, 1, 5, &l); wb_zz(&md, e->type); }
        if (i > 0) { wb_field(&md, 3, 5, &l); wb_zz(&md, e->rep); }
        wb_field(&md, 4, 8, &l); wb_bin(&md, e->name, strlen(e->name));
        if (e->nchildren > 0) { wb_field(&md, 5, 5, &l); wb_zz(&md, e->nchildren); }
        if (e->converted >= 0) { wb_field(&md, 6, 5, &l); wb_zz(&md, e->converted); }
        wb_stop(&md);
    }
    { int64_t live = 0; for (int64_t r = 0; r < vt->nslots; r++) if (vt->alive[r]) live++; wb_field(&md, 3, 6, &L); wb_zz(&md, live); }
    wb_field(&md, 4, 9, &L); wb_listhdr(&md, 12, 1);
    { int rl = 0;
      wb_field(&md, 1, 9, &rl); wb_listhdr(&md, 12, (size_t)nleaves);
      for (int l = 0; l < nleaves; l++) {
        int cl = 0;
        wb_field(&md, 2, 6, &cl); wb_zz(&md, offs[l]);
        wb_field(&md, 3, 12, &cl); { int ml = 0;
          wb_field(&md, 1, 5, &ml); wb_zz(&md, T[l].ptype);
          wb_field(&md, 2, 9, &ml); wb_listhdr(&md, 5, 3); wb_zz(&md, 0); wb_zz(&md, 3); wb_zz(&md, 4);
          wb_field(&md, 3, 9, &ml);
          {   /* path_in_schema COMPLETO (root..leaf), non solo il nome della foglia:
                 per le foglie annidate (struct/list/map) pyarrow richiede il percorso intero. */
              const char* path[64]; int np = 0;
              for (int e = vt->f->leaf_elem[l]; e > 0 && np < 64; e = vt->f->sch[e].parent) path[np++] = vt->f->sch[e].name;
              wb_listhdr(&md, 8, (size_t)np);
              for (int k = np - 1; k >= 0; k--) wb_bin(&md, path[k], strlen(path[k]));
          }
          wb_field(&md, 4, 5, &ml); wb_zz(&md, 0);
          wb_field(&md, 5, 6, &ml); wb_zz(&md, T[l].n);   /* num_values = entry della leaf SCRITTA (non del reader) */
          wb_field(&md, 6, 6, &ml); wb_zz(&md, sizes[l]);
          wb_field(&md, 7, 6, &ml); wb_zz(&md, sizes[l]);
          wb_field(&md, 9, 6, &ml); wb_zz(&md, offs[l]);
          wb_stop(&md); }
        wb_stop(&md);
      }
      wb_field(&md, 2, 6, &rl); wb_zz(&md, 0);
      { int64_t live = 0; for (int64_t r = 0; r < vt->nslots; r++) if (vt->alive[r]) live++; wb_field(&md, 3, 6, &rl); wb_zz(&md, live); }
      wb_stop(&md); }
    wb_field(&md, 6, 8, &L); wb_bin(&md, "gmbasic", 7);
    wb_stop(&md);

    char tmp[4096]; snprintf(tmp, sizeof(tmp), "%s.tmp%ld", vt->path, (long)getpid());
    FILE* f = fopen(tmp, "wb");
    if (f) {
        fwrite(pages.p, 1, pages.len, f);
        uint32_t ml = (uint32_t)md.len;
        fwrite(md.p, 1, md.len, f);
        unsigned char tail[8]; tail[0]=ml&0xff; tail[1]=(ml>>8)&0xff; tail[2]=(ml>>16)&0xff; tail[3]=(ml>>24)&0xff; memcpy(tail+4,"PAR1",4);
        fwrite(tail, 1, 8, f);
        fclose(f);
        rename(tmp, vt->path);
    } else remove(tmp);
    for (l = 0; l < nleaves; l++) { free(T[l].dbl); free(T[l].def); free(T[l].rep); if (T[l].str) { for (int64_t k = 0; k < T[l].n; k++) free(T[l].str[k]); free(T[l].str); } }
    free(T);
    free(pages.p); free(md.p); free(offs); free(sizes);
}

/* xUpdate: INSERT/UPDATE/DELETE a SLOT con tombstone (rowid stabile durante le scan multi-riga). */
static int pq_update(sqlite3_vtab* p, int argc, sqlite3_value** argv, sqlite3_int64* pRowid)
{
    pq_vtab* vt = (pq_vtab*)p;
    int c;
    if (argc == 1) {                                    /* DELETE → tombstone */
        sqlite3_int64 rid = sqlite3_value_int64(argv[0]);
        if (rid < 1 || rid > vt->nslots) return SQLITE_ERROR;
        vt->alive[rid - 1] = 0;
        *pRowid = rid;
    } else if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {   /* INSERT → nuovo slot in coda */
        int64_t s = vt->nslots;
        for (c = 0; c < vt->ncol; c++) {
            pq_col* pc = &vt->cols[c];
            int hasval = (c + 2 < argc && sqlite3_value_type(argv[c+2]) != SQLITE_NULL);
            const unsigned char* t = hasval ? sqlite3_value_text(argv[c+2]) : NULL;
            if (pc->kind == PK_SCALAR) {
                pq_leafvals* lv = &vt->leaves[pc->leaf];
                lv->dbl = (double*)realloc(lv->dbl, (size_t)(s + 1) * sizeof(double));
                lv->def = (uint32_t*)realloc(lv->def, (size_t)(s + 1) * sizeof(uint32_t));
                lv->rep = (uint32_t*)realloc(lv->rep, (size_t)(s + 1) * sizeof(uint32_t));
                if (lv->ptype == PT_BYTE_ARRAY) lv->str = (char**)realloc(lv->str, (size_t)(s + 1) * sizeof(char*));
                lv->def[s] = hasval ? (uint32_t)lv->max_def : 0; lv->rep[s] = 0; lv->dbl[s] = 0; if (lv->str) lv->str[s] = NULL;
                if (hasval) { if (lv->ptype == PT_BYTE_ARRAY) { if (lv->str) lv->str[s] = strdup(t ? (const char*)t : ""); } else lv->dbl[s] = sqlite3_value_double(argv[c+2]); }
                lv->n = s + 1;
            } else if (pc->jrows) {
                pc->jrows = (char**)realloc(pc->jrows, (size_t)(s + 1) * sizeof(char*));
                pc->jrows[s] = hasval ? strdup((const char*)t) : NULL;
                pc->jrows_n = s + 1;
            }
        }
        vt->alive = (unsigned char*)realloc(vt->alive, (size_t)(s + 1));
        vt->alive[s] = 1;
        vt->nslots = s + 1;
        *pRowid = s + 1;
    } else {                                            /* UPDATE */
        sqlite3_int64 rid = sqlite3_value_int64(argv[0]);
        if (rid < 1 || rid > vt->nslots || !vt->alive[rid-1]) return SQLITE_ERROR;
        int64_t i = rid - 1;
        for (c = 0; c < vt->ncol; c++) {
            pq_col* pc = &vt->cols[c];
            int hasval = (c + 2 < argc && sqlite3_value_type(argv[c+2]) != SQLITE_NULL);
            const unsigned char* t = hasval ? sqlite3_value_text(argv[c+2]) : NULL;
            if (pc->kind == PK_SCALAR) {
                pq_leafvals* lv = &vt->leaves[pc->leaf];
                if (hasval) { lv->def[i] = (uint32_t)lv->max_def; if (lv->ptype == PT_BYTE_ARRAY) { free(lv->str ? lv->str[i] : NULL); if (lv->str) lv->str[i] = strdup(t ? (const char*)t : ""); } else lv->dbl[i] = sqlite3_value_double(argv[c+2]); }
                else { lv->def[i] = 0; free(lv->str ? lv->str[i] : NULL); if (lv->str) lv->str[i] = NULL; }
            } else if (pc->jrows) { free(pc->jrows[i]); pc->jrows[i] = hasval ? strdup((const char*)t) : NULL; }
        }
        *pRowid = rid;
    }
    pq_write_file(vt);
    return SQLITE_OK;
}

static int pq_connect(sqlite3* db, void* aux, int argc, const char* const* argv, sqlite3_vtab** pp, char** pzErr);

/* xCreate: crea un NUOVO file Parquet dal `schema=` (name1 TYPE1, name2 TYPE2, ...). Se il file
 * esiste già si comporta come xConnect (lo schema è nel file). */
static int pq_xcreate(sqlite3* db, void* aux, int argc, const char* const* argv, sqlite3_vtab** pp, char** pzErr)
{
    const char* fname = NULL; const char* schema = NULL;
    (void)aux;
    for (int i = 3; i < argc; i++) {
        if (i == 3 && strchr(argv[i], '=') == NULL) fname = argv[i];
        else if (strncmp(argv[i], "name=", 5) == 0) fname = argv[i] + 5;
        else if (strncmp(argv[i], "schema=", 7) == 0) schema = argv[i] + 7;
    }
    if (!fname) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: manca il file"); return SQLITE_ERROR; }
    {
        char path[4096]; size_t fl = strlen(fname);
        if (fl >= 2 && (fname[0] == '\'' || fname[0] == '"') && fname[fl-1] == fname[0]) { memcpy(path, fname+1, fl-2); path[fl-2] = 0; }
        else { strncpy(path, fname, sizeof(path)-1); path[sizeof(path)-1] = 0; }
        FILE* exist = fopen(path, "rb");
        if (exist) { fclose(exist); return pq_connect(db, aux, argc, argv, pp, pzErr); }
        if (!schema || !*schema) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: file inesistente e manca schema=..."); return SQLITE_ERROR; }
        {
            /* parse "name TYPE, name TYPE, ..." */
            char names[64][128]; int ptypes[64]; int ncol = 0;
            const char* p = schema;
            while (*p && ncol < 64) {
                while (*p == ' ' || *p == ',' || *p == '\'') p++;
                if (!*p) break;
                int ni = 0;
                while (*p && *p != ' ' && *p != ',' && ni < 127) names[ncol][ni++] = *p++;
                names[ncol][ni] = 0;
                while (*p == ' ') p++;
                int ti = 0; char tb[32];
                while (*p && *p != ',' && ti < 31) tb[ti++] = *p++;
                tb[ti] = 0;
                for (int q = 0; q < ti; q++) { if (tb[q] == '\'' || tb[q] == ' ') tb[q] = 0; }
                if (strncasecmp(tb, "INT32", 5) == 0) ptypes[ncol] = PT_INT32;
                else if (strncasecmp(tb, "INT", 3) == 0 || strncasecmp(tb, "LONG", 4) == 0) ptypes[ncol] = PT_INT64;
                else if (strncasecmp(tb, "FLOAT", 5) == 0) ptypes[ncol] = PT_FLOAT;
                else if (strncasecmp(tb, "DOUBLE", 6) == 0 || strncasecmp(tb, "REAL", 4) == 0) ptypes[ncol] = PT_DOUBLE;
                else if (strncasecmp(tb, "BOOL", 4) == 0) ptypes[ncol] = PT_BOOLEAN;
                else ptypes[ncol] = PT_BYTE_ARRAY;
                ncol++;
            }
            if (ncol == 0) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: schema vuoto"); return SQLITE_ERROR; }
            /* scrive un file vuoto con lo schema */
            wbuf pages; memset(&pages, 0, sizeof(pages));
            int64_t offs[64], sizes[64];
            wb_bin(&pages, "PAR1", 4);
            for (int c = 0; c < ncol; c++) {
                pq_leafvals lv; memset(&lv, 0, sizeof(lv));
                lv.ptype = ptypes[c]; lv.max_def = 1; lv.max_rep = 0; lv.n = 0;
                int64_t o, sz; pq_ser_leaf(&lv, &pages, &o, &sz); offs[c] = o; sizes[c] = sz;
            }
            wbuf md; memset(&md, 0, sizeof(md)); int L = 0;
            wb_field(&md, 1, 5, &L); wb_zz(&md, 1);
            wb_field(&md, 2, 9, &L); wb_listhdr(&md, 12, (size_t)(ncol + 1));
            { int l = 0; wb_field(&md, 4, 8, &l); wb_bin(&md, "schema", 6); wb_field(&md, 5, 5, &l); wb_zz(&md, ncol); wb_stop(&md); }
            for (int c = 0; c < ncol; c++) {
                int l = 0;
                wb_field(&md, 1, 5, &l); wb_zz(&md, ptypes[c]);
                wb_field(&md, 3, 5, &l); wb_zz(&md, RT_OPTIONAL);
                wb_field(&md, 4, 8, &l); wb_bin(&md, names[c], strlen(names[c]));
                if (ptypes[c] == PT_BYTE_ARRAY) { wb_field(&md, 6, 5, &l); wb_zz(&md, 0); }
                wb_stop(&md);
            }
            wb_field(&md, 3, 6, &L); wb_zz(&md, 0);
            wb_field(&md, 4, 9, &L); wb_listhdr(&md, 12, 1);
            { int rl = 0;
              wb_field(&md, 1, 9, &rl); wb_listhdr(&md, 12, (size_t)ncol);
              for (int c = 0; c < ncol; c++) { int cl = 0;
                wb_field(&md, 2, 6, &cl); wb_zz(&md, offs[c]);
                wb_field(&md, 3, 12, &cl); { int ml = 0;
                  wb_field(&md, 1, 5, &ml); wb_zz(&md, ptypes[c]);
                  wb_field(&md, 2, 9, &ml); wb_listhdr(&md, 5, 3); wb_zz(&md, 0); wb_zz(&md, 3); wb_zz(&md, 4);
                  wb_field(&md, 3, 9, &ml); wb_listhdr(&md, 8, 1); wb_bin(&md, names[c], strlen(names[c]));
                  wb_field(&md, 4, 5, &ml); wb_zz(&md, 0);
                  wb_field(&md, 5, 6, &ml); wb_zz(&md, 0);
                  wb_field(&md, 6, 6, &ml); wb_zz(&md, sizes[c]);
                  wb_field(&md, 7, 6, &ml); wb_zz(&md, sizes[c]);
                  wb_field(&md, 9, 6, &ml); wb_zz(&md, offs[c]);
                  wb_stop(&md); }
                wb_stop(&md); }
              wb_field(&md, 2, 6, &rl); wb_zz(&md, 0);
              wb_field(&md, 3, 6, &rl); wb_zz(&md, 0);
              wb_stop(&md); }
            wb_field(&md, 6, 8, &L); wb_bin(&md, "gmbasic", 7);
            wb_stop(&md);
            FILE* f = fopen(path, "wb");
            if (!f) { free(pages.p); free(md.p); if (pzErr) *pzErr = sqlite3_mprintf("parquet: impossibile creare '%s'", path); return SQLITE_ERROR; }
            fwrite(pages.p, 1, pages.len, f);
            uint32_t ml = (uint32_t)md.len;
            fwrite(md.p, 1, md.len, f);
            unsigned char tail[8]; tail[0]=ml&0xff; tail[1]=(ml>>8)&0xff; tail[2]=(ml>>16)&0xff; tail[3]=(ml>>24)&0xff; memcpy(tail+4,"PAR1",4);
            fwrite(tail, 1, 8, f); fclose(f);
            free(pages.p); free(md.p);
        }
    }
    return pq_connect(db, aux, argc, argv, pp, pzErr);
}

static int pq_connect(sqlite3* db, void* aux, int argc, const char* const* argv, sqlite3_vtab** pp, char** pzErr)
{
    const char* fname = NULL;
    (void)aux;
    if (argc >= 4 && strchr(argv[3], '=') == NULL) fname = argv[3];
    for (int i = 3; i < argc; i++) if (strncmp(argv[i], "name=", 5) == 0) fname = argv[i] + 5;
    if (!fname) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: manca il file"); return SQLITE_ERROR; }
    {
        char fb[4096]; size_t fl = strlen(fname);
        if (fl >= 2 && (fname[0] == '\'' || fname[0] == '"') && fname[fl-1] == fname[0]) { memcpy(fb, fname+1, fl-2); fb[fl-2]=0; fname = fb; }
        pq_vtab* vt = (pq_vtab*)calloc(1, sizeof(pq_vtab));
        char* buf; size_t len;
        if (!vt) return SQLITE_NOMEM;
        { size_t pl = strlen(fname); if (pl > sizeof(vt->path)-1) pl = sizeof(vt->path)-1; memcpy(vt->path, fname, pl); vt->path[pl] = 0; }
        if (pq_read_file(fname, &buf, &len) != 0) { free(vt); if (pzErr) *pzErr = sqlite3_mprintf("parquet: impossibile leggere '%s'", fname); return SQLITE_ERROR; }
        if (len < 12 || memcmp(buf + len - 4, "PAR1", 4) != 0) { free(buf); free(vt); if (pzErr) *pzErr = sqlite3_mprintf("parquet: non è un file Parquet (magic)"); return SQLITE_ERROR; }
        uint32_t meta_len; memcpy(&meta_len, buf + len - 8, 4);
        uint32_t meta_off = (uint32_t)(len - 8 - meta_len);
        pq_file* f = (pq_file*)calloc(1, sizeof(pq_file));
        f->buf = buf; f->buf_len = len;
        pq_parse_meta(f, (const uint8_t*)buf + meta_off, meta_len);
        pq_build_tree(f);
        vt->f = f;
        vt->nleaves = f->ncols;
        vt->leaves = (pq_leafvals*)calloc((size_t)(f->ncols ? f->ncols : 1), sizeof(pq_leafvals));
        for (int c = 0; c < f->ncols; c++) {
            int dr = pq_decode_leaf(vt, c);
            if (dr) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: compressione non supportata (codec %d) nella colonna %d; supportate UNCOMPRESSED/SNAPPY/GZIP/ZSTD", -dr - 1, c); return SQLITE_ERROR; }
        }
        pq_build_cols(vt);
        for (int c = 0; c < vt->ncol; c++) if (vt->cols[c].unsupported) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: colonna '%s' annidata oltre 1 livello non supportata (list/struct/map di scalari)", vt->cols[c].name); return SQLITE_ERROR; }
        /* slot/tombstone + owner delle foglie */
        vt->nslots = f->num_rows;
        vt->alive = (unsigned char*)calloc((size_t)(vt->nslots ? vt->nslots : 1), 1);
        for (int64_t r = 0; r < vt->nslots; r++) vt->alive[r] = 1;
        vt->leaf_owner = (int*)malloc((size_t)(f->ncols ? f->ncols : 1) * sizeof(int));
        for (int i = 0; i < f->ncols; i++) vt->leaf_owner[i] = -1;
        for (int c = 0; c < vt->ncol; c++) {
            pq_col* pc = &vt->cols[c];
            if (pc->kind == PK_SCALAR) vt->leaf_owner[pc->leaf] = c;
            else { for (int k = 0; k < pc->nleaf; k++) vt->leaf_owner[pc->leaves[k]] = c; if (pc->kind == PK_LIST) vt->leaf_owner[pc->leaf] = c; if (pc->kind == PK_MAP) { vt->leaf_owner[pc->leaf] = c; vt->leaf_owner[pc->val_leaf] = c; } }
        }
        /* materializza il JSON per-record delle colonne annidate (per il write-back) */
        for (int c = 0; c < vt->ncol; c++) {
            if (vt->cols[c].kind == PK_SCALAR) continue;
            vt->cols[c].jrows = (char**)calloc((size_t)(vt->nslots ? vt->nslots : 1), sizeof(char*));
            vt->cols[c].jrows_n = vt->nslots;
            for (int64_t r = 0; r < vt->nslots; r++) {
                jbuf b; memset(&b, 0, sizeof(b));
                pq_json_col(&b, vt, &vt->cols[c], r);
                vt->cols[c].jrows[r] = b.p ? b.p : strdup("null");
            }
        }
        {
            char sql[8192]; size_t used; int c;
            snprintf(sql, sizeof(sql), "CREATE TABLE x(");
            used = strlen(sql);
            for (c = 0; c < vt->ncol; c++) {
                if (c) { if (used < sizeof(sql)-1) sql[used++] = ','; }
                if (used < sizeof(sql)-1) sql[used++] = '"';
                for (const char* q = vt->cols[c].name; *q && used < sizeof(sql)-4; q++) { if (*q=='"') { sql[used++]='"'; sql[used++]='"'; } else sql[used++]=*q; }
                sql[used]=0;
                const char* t = (vt->cols[c].kind != PK_SCALAR) ? "\" TEXT" : (vt->cols[c].ptype == PT_BYTE_ARRAY ? "\" TEXT" : "\" NUMERIC");
                size_t tl = strlen(t);
                if (used + tl < sizeof(sql)) { memcpy(sql+used, t, tl); used += tl; }
            }
            if (used < sizeof(sql)-1) sql[used++]=')';
            sql[used]=0;
            if (sqlite3_declare_vtab(db, sql) != SQLITE_OK) { if (pzErr) *pzErr = sqlite3_mprintf("parquet: declare_vtab fallito"); return SQLITE_ERROR; }
        }
        *pp = &vt->base;
        return SQLITE_OK;
    }
}
static int pq_disconnect(sqlite3_vtab* p)
{
    pq_vtab* vt = (pq_vtab*)p;
    if (vt->cols) { for (int c = 0; c < vt->ncol; c++) { free(vt->cols[c].name); free(vt->cols[c].rec_start); if (vt->cols[c].jrows) { for (int64_t r = 0; r < vt->cols[c].jrows_n; r++) free(vt->cols[c].jrows[r]); free(vt->cols[c].jrows); } } free(vt->cols); }
    if (vt->leaves) {
        for (int c = 0; c < vt->nleaves; c++) {
            free(vt->leaves[c].dbl); free(vt->leaves[c].def); free(vt->leaves[c].rep);
            if (vt->leaves[c].str) { for (int64_t k = 0; k < vt->leaves[c].n; k++) free(vt->leaves[c].str[k]); free(vt->leaves[c].str); }
        }
        free(vt->leaves);
    }
    if (vt->f) { if (vt->f->rgs) { for (int _r = 0; _r < vt->f->nrgs; _r++) free(vt->f->rgs[_r].cols); free(vt->f->rgs); } free(vt->f->buf); free(vt->f->sch); free(vt->f->leaf_elem); free(vt->f); }
    free(vt->alive); free(vt->leaf_owner);
    free(vt);
    return SQLITE_OK;
}
static int pq_open(sqlite3_vtab* p, sqlite3_vtab_cursor** pp) { pq_cursor* c=(pq_cursor*)calloc(1,sizeof(pq_cursor)); (void)p; if(!c)return SQLITE_NOMEM; *pp=&c->base; return SQLITE_OK; }
static int pq_close(sqlite3_vtab_cursor* c){ free(c); return SQLITE_OK; }
static void pq_advance(pq_cursor* c)
{
    pq_vtab* v = (pq_vtab*)c->base.pVtab;
    while (c->row < v->nslots && !v->alive[c->row]) c->row++;
}
static int pq_filter(sqlite3_vtab_cursor* c,int a,const char*b,int d,sqlite3_value**e){ (void)a;(void)b;(void)d;(void)e; ((pq_cursor*)c)->row=0; pq_advance((pq_cursor*)c); return SQLITE_OK; }
static int pq_next(sqlite3_vtab_cursor* c){ ((pq_cursor*)c)->row++; pq_advance((pq_cursor*)c); return SQLITE_OK; }
static int pq_eof(sqlite3_vtab_cursor* c){ return ((pq_cursor*)c)->row >= ((pq_vtab*)c->pVtab)->nslots; }
static int pq_column(sqlite3_vtab_cursor* c, sqlite3_context* ctx, int col)
{
    pq_vtab* v=(pq_vtab*)c->pVtab; int64_t r=((pq_cursor*)c)->row;
    if (col<0||col>=v->ncol) { sqlite3_result_null(ctx); return SQLITE_OK; }
    pq_col* pc=&v->cols[col];
    if (pc->kind == PK_SCALAR) {
        pq_leafvals* lv=&v->leaves[pc->leaf];
        if (r>=lv->n || (int)lv->def[r] != lv->max_def) { sqlite3_result_null(ctx); return SQLITE_OK; }
        if (pc->ptype==PT_BYTE_ARRAY) sqlite3_result_text(ctx, lv->str[r]?lv->str[r]:"", -1, SQLITE_TRANSIENT);
        else if (pc->ptype==PT_BOOLEAN) sqlite3_result_int(ctx, lv->dbl[r]!=0.0?1:0);
        else if (pc->ptype==PT_INT32||pc->ptype==PT_INT64) sqlite3_result_int64(ctx,(sqlite3_int64)lv->dbl[r]);
        else sqlite3_result_double(ctx, lv->dbl[r]);
        return SQLITE_OK;
    }
    if (pc->jrows && r < pc->jrows_n && pc->jrows[r]) { sqlite3_result_text(ctx, pc->jrows[r], -1, SQLITE_TRANSIENT); return SQLITE_OK; }
    { jbuf b; memset(&b,0,sizeof(b)); pq_json_col(&b, v, pc, r); sqlite3_result_text(ctx, b.p?b.p:"null", -1, SQLITE_TRANSIENT); free(b.p); }
    return SQLITE_OK;
}
static int pq_rowid(sqlite3_vtab_cursor* c, sqlite3_int64* id){ *id=((pq_cursor*)c)->row+1; return SQLITE_OK; }
static int pq_bestindex(sqlite3_vtab* p, sqlite3_index_info* info){ (void)p; info->estimatedCost=10000.0; return SQLITE_OK; }
static sqlite3_module pq_module = { 0, pq_xcreate, pq_connect, pq_bestindex, pq_disconnect, pq_disconnect, pq_open, pq_close, pq_filter, pq_next, pq_eof, pq_column, pq_rowid, pq_update, 0,0,0,0,0,0,0,0,0 };

#ifdef GMB_PARQUET_AS_EXTENSION
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_gmbparquetext_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;
    return sqlite3_create_module(db, "parquet", &pq_module, 0);
}
#else
int gmb_sql_register_parquet(sqlite3* db) { return sqlite3_create_module(db, "parquet", &pq_module, 0); }
#endif
