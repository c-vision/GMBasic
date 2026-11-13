/* ============================================================================
 * gmb_chunk.c — Loader del CompiledChunk serializzato (formato A1)
 * File: native/gmb_vm_c/gmb_chunk.c
 * ============================================================================ */

#include "gmb_chunk.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---- Reader big-endian ---- */
typedef struct {
    const uint8_t* p;
    const uint8_t* end;
    int err;
} reader_t;

static uint8_t rd_u8(reader_t* r)
{
    if (r->p >= r->end) { r->err = 1; return 0; }
    return *r->p++;
}

static uint32_t rd_u32(reader_t* r)
{
    uint8_t a = rd_u8(r), b = rd_u8(r), c = rd_u8(r), d = rd_u8(r);
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

static double rd_f64(reader_t* r)
{
    uint32_t hi = rd_u32(r);
    uint32_t lo = rd_u32(r);
    uint64_t bits = ((uint64_t)hi << 32) | lo;
    double v;
    memcpy(&v, &bits, 8);
    return v;
}

static char* rd_str(reader_t* r)
{
    uint32_t len = rd_u32(r);
    if (r->err || r->p + len > r->end) { r->err = 1; return NULL; }
    char* s = (char*)malloc(len + 1);
    if (!s) { r->err = 1; return NULL; }
    memcpy(s, r->p, len);
    s[len] = '\0';
    r->p += len;
    return s;
}

/* ---- Deserializzazione value (boxed) ---- */
static gmb_chunk_dict_t* rd_dict(reader_t* r)
{
    uint32_t n = rd_u32(r);
    gmb_chunk_dict_t* d = (gmb_chunk_dict_t*)calloc(1, sizeof(gmb_chunk_dict_t));
    if (!d) { r->err = 1; return NULL; }
    d->count = n;
    if (n == 0) return d;
    d->entries = (gmb_chunk_dict_entry_t*)calloc(n, sizeof(gmb_chunk_dict_entry_t));
    if (!d->entries) { free(d); r->err = 1; return NULL; }
    for (uint32_t i = 0; i < n; i++)
    {
        uint8_t ktag = rd_u8(r);
        gmb_chunk_dict_entry_t* e = &d->entries[i];
        if (ktag == 5) { e->key_is_int = 1; e->key_int = (int32_t)rd_u32(r); }
        else { e->key_str = rd_str(r); }
        /* valore */
        uint8_t vtag = rd_u8(r);
        e->val_type = vtag;
        switch (vtag)
        {
            case GMB_CONST_NIL: break;
            case GMB_CONST_DOUBLE: e->val_double = rd_f64(r); break;
            case GMB_CONST_STRING: e->val_str = rd_str(r); break;
            case GMB_CONST_INT: e->val_double = (double)(int32_t)rd_u32(r); break;
            case GMB_CONST_DICT: e->val_dict = rd_dict(r); break;
            case GMB_CONST_LIST: e->val_list = (gmb_chunk_list_t*)rd_dict(r); break; /* lista: stesso layout, items=entries */
            case GMB_CONST_DATATYPE: e->val_dt_cat = rd_u8(r); break;
            default: r->err = 1; return d;
        }
        if (r->err) return d;
    }
    return d;
}

static gmb_value_t rd_value(reader_t* r)
{
    gmb_value_t v = { 0 };
    uint8_t tag = rd_u8(r);
    v.type = tag;
    switch (tag)
    {
        case GMB_CONST_NIL: break;
        case GMB_CONST_DOUBLE: v.d = rd_f64(r); break;
        case GMB_CONST_STRING: v.s = rd_str(r); break;
        case GMB_CONST_INT: v.d = (double)(int32_t)rd_u32(r); break;
        case GMB_CONST_DICT: v.dict = rd_dict(r); break;
        case GMB_CONST_LIST: v.list = (gmb_chunk_list_t*)rd_dict(r); break;
        case GMB_CONST_DATATYPE: v.dt_cat = rd_u8(r); break;
        default: r->err = 1; break;
    }
    return v;
}

/* ---- Loader ---- */
gmb_chunk_t* gmb_chunk_load(const uint8_t* data, size_t len, bool load_lines)
{
    if (!data || len < 5 + 5 * 4) return NULL;
    reader_t r = { data, data + len, 0 };

    /* MAGIC + VERSION (1 storico, 2 descrittore FFI, 3 firme callback per-slot; tutte
     * leggibili, vedi gmb_chunk.h) */
    for (int i = 0; i < 4; i++)
        if (rd_u8(&r) != (uint8_t)GMB_CHUNK_MAGIC[i]) return NULL;
    uint8_t version = rd_u8(&r);
    if (version < 1 || version > GMB_CHUNK_VERSION) return NULL;

    gmb_chunk_t* c = (gmb_chunk_t*)calloc(1, sizeof(gmb_chunk_t));
    if (!c) return NULL;
    c->version = version;

    c->entry_point = (int32_t)rd_u32(&r);
    c->global_count = rd_u32(&r);
    uint32_t data_count = rd_u32(&r);
    c->code_len = rd_u32(&r);
    c->consts_len = rd_u32(&r);
    if (r.err) goto fail;

    /* CODE */
    c->code = (uint8_t*)malloc(c->code_len ? c->code_len : 1);
    if (!c->code) goto fail;
    if (c->code_len && r.p + c->code_len > r.end) { r.err = 1; }
    if (!r.err) { memcpy(c->code, r.p, c->code_len); r.p += c->code_len; }

    /* LINES */
    if (!r.err)
    {
        if (load_lines && c->code_len > 0)
        {
            c->lines = (uint32_t*)malloc(c->code_len * sizeof(uint32_t));
            if (!c->lines) goto fail;
            for (uint32_t i = 0; i < c->code_len; i++) c->lines[i] = rd_u32(&r);
        }
        else
        {
            for (uint32_t i = 0; i < c->code_len; i++) rd_u32(&r);
        }
    }
    if (r.err) goto fail;

    /* CONSTANTS */
    if (c->consts_len > 0)
    {
        c->constants = (gmb_value_t*)calloc(c->consts_len, sizeof(gmb_value_t));
        if (!c->constants) goto fail;
        for (uint32_t i = 0; i < c->consts_len; i++) c->constants[i] = rd_value(&r);
    }
    if (r.err) goto fail;

    /* DATA */
    if (data_count > 0)
    {
        c->data = (gmb_value_t*)calloc(data_count, sizeof(gmb_value_t));
        if (!c->data) goto fail;
        for (uint32_t i = 0; i < data_count; i++) c->data[i] = rd_value(&r);
        c->data_len = data_count;
    }
    if (r.err) goto fail;

    /* PROCS: u32 count → (string name, u32 addr) */
    c->proc_count = rd_u32(&r);
    if (c->proc_count > 0)
    {
        c->proc_names = (char**)calloc(c->proc_count, sizeof(char*));
        c->proc_addrs = (uint32_t*)calloc(c->proc_count, sizeof(uint32_t));
        if (!c->proc_names || !c->proc_addrs) goto fail;
        for (uint32_t i = 0; i < c->proc_count; i++)
        {
            c->proc_names[i] = rd_str(&r);
            c->proc_addrs[i] = rd_u32(&r);
        }
    }
    if (r.err) goto fail;

    /* ARITYS: u32 count → (u32 addr, u32 arity) */
    c->arity_count = rd_u32(&r);
    if (c->arity_count > 0)
    {
        c->arity_addrs = (uint32_t*)calloc(c->arity_count, sizeof(uint32_t));
        c->arity_vals = (uint32_t*)calloc(c->arity_count, sizeof(uint32_t));
        if (!c->arity_addrs || !c->arity_vals) goto fail;
        for (uint32_t i = 0; i < c->arity_count; i++)
        {
            c->arity_addrs[i] = rd_u32(&r);
            c->arity_vals[i] = rd_u32(&r);
        }
    }
    if (r.err) goto fail;

    /* GLOBAL NAMES (opzionale: chunk vecchi finiscono qui) */
    if (!r.err && r.p < r.end)
    {
        c->global_name_count = rd_u32(&r);
        if (!r.err && c->global_name_count > 0)
        {
            c->global_names = (char**)calloc(c->global_name_count, sizeof(char*));
            if (!c->global_names) goto fail;
            for (uint32_t i = 0; i < c->global_name_count; i++)
                c->global_names[i] = rd_str(&r);
        }
    }
    if (r.err) goto fail;

    /* LOCAL COUNT + TRY REGIONS (coda opzionale A1, specchio ChunkA1Serializer:
     * u32 LocalCount, poi u32 tryCount e per voce tryIp/finallyIp/endIp/hasCatch).
     * Chunk vecchi finiscono prima: ogni lettura corta è tollerata. */
    if (!r.err && r.p < r.end)
    {
        c->local_count = rd_u32(&r);
        if (!r.err && r.p < r.end)
        {
            uint32_t try_count = rd_u32(&r);
            if (!r.err && try_count > 0)
            {
                c->try_ips = (uint32_t*)calloc(try_count, sizeof(uint32_t));
                c->try_finally_ips = (uint32_t*)calloc(try_count, sizeof(uint32_t));
                c->try_end_ips = (uint32_t*)calloc(try_count, sizeof(uint32_t));
                c->try_has_catch = (uint8_t*)calloc(try_count, sizeof(uint8_t));
                if (!c->try_ips || !c->try_finally_ips || !c->try_end_ips || !c->try_has_catch) goto fail;
                c->try_region_count = try_count;
                for (uint32_t i = 0; i < try_count && !r.err; i++)
                {
                    c->try_ips[i] = rd_u32(&r);
                    c->try_finally_ips[i] = rd_u32(&r);
                    c->try_end_ips[i] = rd_u32(&r);
                    c->try_has_catch[i] = rd_u32(&r) != 0;
                }
            }
        }
        /* PROC LOCAL NAMES (coda opzionale S_eval): u32 count, per voce u32 addr,
         * u32 nameCount, nameCount stringhe ("" = slot anonimo → NULL). Chunk vecchi
         * finiscono prima: ogni lettura corta è tollerata. */
        if (!r.err && r.p < r.end)
        {
            uint32_t plc = rd_u32(&r);
            if (!r.err && plc > 0)
            {
                c->ploc_addrs = (uint32_t*)calloc(plc, sizeof(uint32_t));
                c->ploc_names = (char***)calloc(plc, sizeof(char**));
                c->ploc_counts = (uint32_t*)calloc(plc, sizeof(uint32_t));
                if (!c->ploc_addrs || !c->ploc_names || !c->ploc_counts) goto fail;
                c->ploc_count = plc;
                for (uint32_t i = 0; i < plc && !r.err; i++)
                {
                    c->ploc_addrs[i] = rd_u32(&r);
                    uint32_t nc = rd_u32(&r);
                    c->ploc_counts[i] = nc;
                    if (!r.err && nc > 0)
                    {
                        c->ploc_names[i] = (char**)calloc(nc, sizeof(char*));
                        if (!c->ploc_names[i]) goto fail;
                        for (uint32_t j = 0; j < nc && !r.err; j++)
                        {
                            char* s = rd_str(&r);
                            /* "" = slot anonimo (specchio C#: null → "") */
                            if (s && s[0] == '\0') { free(s); s = NULL; }
                            c->ploc_names[i][j] = s;
                        }
                    }
                }
            }
        }
        /* STATEMENT RANGES (coda FINALE opzionale): u32 count + (u32 start, u32 end) per voce.
         * Chunk vecchi finiscono prima: lettura corta tollerata. */
        if (!r.err && r.p < r.end)
        {
            uint32_t ssc = rd_u32(&r);
            if (!r.err && ssc > 0 && ssc <= (uint32_t)(r.end - r.p) / 8)
            {
                c->stmt_starts = (uint32_t*)calloc(ssc, sizeof(uint32_t));
                c->stmt_ends = (uint32_t*)calloc(ssc, sizeof(uint32_t));
                if (!c->stmt_starts || !c->stmt_ends) goto fail;
                c->stmt_range_count = ssc;
                for (uint32_t i = 0; i < ssc && !r.err; i++)
                {
                    c->stmt_starts[i] = rd_u32(&r);
                    c->stmt_ends[i] = rd_u32(&r);
                }
            }
        }
        /* LOCAL NUMERIC (coda FINALE opzionale): u32 count, per voce u32 addr, u32 count,
         * count byte (0/1), DOPO le statement ranges. Lettura corta tollerata. */
        if (!r.err && r.p < r.end)
        {
            uint32_t n = rd_u32(&r);
            if (!r.err && n > 0 && n <= (uint32_t)(r.end - r.p) / 8)
            {
                c->plnum_addrs = (uint32_t*)calloc(n, sizeof(uint32_t));
                c->plnum_flags = (uint8_t**)calloc(n, sizeof(uint8_t*));
                c->plnum_counts = (uint32_t*)calloc(n, sizeof(uint32_t));
                if (!c->plnum_addrs || !c->plnum_flags || !c->plnum_counts) goto fail;
                c->plnum_count = n;
                for (uint32_t i = 0; i < n && !r.err; i++)
                {
                    c->plnum_addrs[i] = rd_u32(&r);
                    uint32_t cnt = rd_u32(&r);
                    if (!r.err && cnt > 0 && cnt <= (uint32_t)(r.end - r.p))
                    {
                        c->plnum_flags[i] = (uint8_t*)calloc(cnt, 1);
                        if (!c->plnum_flags[i]) goto fail;
                        c->plnum_counts[i] = cnt;
                        for (uint32_t j = 0; j < cnt && !r.err; j++) c->plnum_flags[i][j] = rd_u8(&r);
                    }
                }
            }
        }
        /* USES TRAY (coda FINALE opzionale): 1 byte 0/1. */
        if (!r.err && r.p < r.end) c->uses_tray = rd_u8(&r) != 0 ? 1 : 0;
        /* PROC PARAM DEFAULTS (coda FINALE opzionale A1): per procedura i default LETTERALI dei
         * parametri OPTIONAL. Servono a `gmb_vm_call_proc_sync_vals` per completare una chiamata
         * fatta da una macro (EVAL/&) sul task corrente: il chunk non porta l'AST e il default
         * altrimenti restava 0. Formato speculare a ChunkA1Serializer (u32 big-endian, stringa
         * = u32 len + byte, default = u8 kind {0=nessuno,1=f64,2=stringa}). */
        if (!r.err && r.p < r.end)
        {
            uint32_t pd_count = rd_u32(&r);
            if (!r.err && pd_count > 0 && pd_count <= 65536)
            {
                c->pdef_addrs = (uint32_t*)calloc(pd_count, sizeof(uint32_t));
                c->pdef_counts = (uint32_t*)calloc(pd_count, sizeof(uint32_t));
                c->pdef_kinds = (uint8_t**)calloc(pd_count, sizeof(uint8_t*));
                c->pdef_nums = (double**)calloc(pd_count, sizeof(double*));
                c->pdef_strs = (char***)calloc(pd_count, sizeof(char**));
                if (!c->pdef_addrs || !c->pdef_counts || !c->pdef_kinds || !c->pdef_nums || !c->pdef_strs)
                    goto fail;
                c->pdef_count = pd_count;
                for (uint32_t i = 0; i < pd_count && !r.err; i++)
                {
                    c->pdef_addrs[i] = rd_u32(&r);
                    uint32_t n = rd_u32(&r);
                    if (r.err || n > 64) { r.err = 1; break; }
                    c->pdef_counts[i] = n;
                    if (n == 0) continue;
                    c->pdef_kinds[i] = (uint8_t*)calloc(n, sizeof(uint8_t));
                    c->pdef_nums[i] = (double*)calloc(n, sizeof(double));
                    c->pdef_strs[i] = (char**)calloc(n, sizeof(char*));
                    if (!c->pdef_kinds[i] || !c->pdef_nums[i] || !c->pdef_strs[i]) goto fail;
                    for (uint32_t j = 0; j < n && !r.err; j++)
                    {
                        uint8_t kind = rd_u8(&r);
                        c->pdef_kinds[i][j] = kind;
                        if (kind == 1) c->pdef_nums[i][j] = rd_f64(&r);
                        else if (kind == 2) c->pdef_strs[i][j] = rd_str(&r);
                    }
                }
                if (r.err) { c->pdef_count = 0; goto fail; }
            }
        }
    }
    if (r.err) goto fail;

    c->refs = 1;             /* referenza del chiamante; la cache BSP ne aggiunge una sua */
    return c;

fail:
    gmb_chunk_free(c);
    return NULL;
}

static void free_entry(gmb_chunk_dict_entry_t* e)
{
    if (e->key_str) free(e->key_str);
    if (e->val_str) free(e->val_str);
    if (e->val_dict) { gmb_chunk_dict_t* d = e->val_dict; for (uint32_t i = 0; i < d->count; i++) free_entry(&d->entries[i]); free(d->entries); free(d); }
    if (e->val_list) { gmb_chunk_list_t* l = e->val_list; for (uint32_t i = 0; i < l->count; i++) free_entry(&l->items[i]); free(l->items); free(l); }
}

gmb_chunk_t* gmb_chunk_retain(gmb_chunk_t* c)
{
    if (c) c->refs++;
    return c;
}

void gmb_chunk_free(gmb_chunk_t* c)
{
    if (!c) return;
    if (c->refs > 1) { c->refs--; return; }   /* ancora in uso da un render concorrente */
    free(c->code);
    free(c->stmt_starts);
    free(c->stmt_ends);
    if (c->pdef_count)
    {
        for (uint32_t i = 0; i < c->pdef_count; i++)
        {
            free(c->pdef_kinds ? c->pdef_kinds[i] : NULL);
            free(c->pdef_nums ? c->pdef_nums[i] : NULL);
            if (c->pdef_strs)
            {
                for (uint32_t j = 0; j < c->pdef_counts[i]; j++) free(c->pdef_strs[i] ? c->pdef_strs[i][j] : NULL);
                free(c->pdef_strs[i]);
            }
        }
    }
    free(c->pdef_addrs); free(c->pdef_counts); free(c->pdef_kinds); free(c->pdef_nums); free(c->pdef_strs);
    if (c->plnum_flags)
    {
        for (uint32_t i = 0; i < c->plnum_count; i++) free(c->plnum_flags[i]);
        free(c->plnum_flags);
    }
    free(c->plnum_addrs);
    free(c->plnum_counts);
    free(c->lines);
    if (c->constants)
    {
        for (uint32_t i = 0; i < c->consts_len; i++)
        {
            gmb_value_t* v = &c->constants[i];
            if (v->s) free(v->s);
            if (v->dict) { for (uint32_t k = 0; k < v->dict->count; k++) free_entry(&v->dict->entries[k]); free(v->dict->entries); free(v->dict); }
            if (v->list) { for (uint32_t k = 0; k < v->list->count; k++) free_entry(&v->list->items[k]); free(v->list->items); free(v->list); }
        }
        free(c->constants);
    }
    if (c->data)
    {
        for (uint32_t i = 0; i < c->data_len; i++)
        {
            gmb_value_t* v = &c->data[i];
            if (v->s) free(v->s);
            if (v->dict) { for (uint32_t k = 0; k < v->dict->count; k++) free_entry(&v->dict->entries[k]); free(v->dict->entries); free(v->dict); }
        }
        free(c->data);
    }
    for (uint32_t i = 0; i < c->proc_count; i++) free(c->proc_names[i]);
    free(c->proc_names);
    free(c->proc_addrs);
    free(c->arity_addrs);
    free(c->arity_vals);
    if (c->global_names)
    {
        for (uint32_t i = 0; i < c->global_name_count; i++) free(c->global_names[i]);
        free(c->global_names);
    }
    free(c->try_ips);
    free(c->try_finally_ips);
    free(c->try_end_ips);
    free(c->try_has_catch);
    if (c->ploc_names)
    {
        for (uint32_t i = 0; i < c->ploc_count; i++)
        {
            if (c->ploc_names[i])
            {
                for (uint32_t j = 0; j < c->ploc_counts[i]; j++) free(c->ploc_names[i][j]);
                free(c->ploc_names[i]);
            }
        }
        free(c->ploc_names);
    }
    free(c->ploc_addrs);
    free(c->ploc_counts);
    for (uint32_t i = 0; i < c->_strings_count; i++) free(c->_strings[i]);
    free(c->_strings);
    free(c);
}

const gmb_value_t* gmb_chunk_const(const gmb_chunk_t* chunk, uint32_t idx)
{
    if (!chunk || idx >= chunk->consts_len) return NULL;
    return &chunk->constants[idx];
}

int gmb_chunk_try_region(const gmb_chunk_t* chunk, int try_ip, uint32_t* finally_ip, uint32_t* end_ip, int* has_catch)
{    if (!chunk || try_ip < 0) return 0;
    for (uint32_t i = 0; i < chunk->try_region_count; i++)
    {
        if ((int)chunk->try_ips[i] == try_ip)
        {
            if (finally_ip) *finally_ip = chunk->try_finally_ips[i];
            if (end_ip) *end_ip = chunk->try_end_ips[i];
            if (has_catch) *has_catch = chunk->try_has_catch[i] ? 1 : 0;
            return 1;
        }
    }
    return 0;
}

gmb_value_t gmb_field_default(uint8_t dt_cat)
{
    gmb_value_t v = { 0 };
    if (dt_cat == GMB_DT_STR)
    {
        v.type = GMB_CONST_STRING;
        v.s = strdup("");
    }
    else if (dt_cat == GMB_DT_NUM)
    {
        v.type = GMB_CONST_DOUBLE;
        v.d = 0.0;
    }
    else
    {
        v.type = GMB_CONST_STRING;
        v.s = strdup("");
    }
    return v;
}

/* S_eval: risolve uno slot locale per nome nella procedura che contiene ip.
 * Confronto case-insensitive (i nomi sono già normalizzati dai writer, senza
 * suffisso tipo; il retry con suffisso lo fa il chiamante gmb_expr lookup). */
int gmb_chunk_local_slot(const gmb_chunk_t* chunk, int ip, const char* name)
{
    if (!chunk || !name || !name[0] || ip < 0 || chunk->ploc_count == 0) return -1;
    int best = -1;
    for (uint32_t i = 0; i < chunk->ploc_count; i++) {
        int a = (int)chunk->ploc_addrs[i];
        if (a <= ip && (best < 0 || a > (int)chunk->ploc_addrs[best])) best = (int)i;
    }
    if (best < 0) return -1;
    /* Ricerca dal PIU' PROFONDO (ultimo slot): come il compilatore (ResolveLocal), vince la
     * dichiarazione piu' recente — un contatore FOR o una DIM di blocco che mascherano una
     * locale di procedura stanno su slot piu' alti (first-match li nasconderebbe). */
    for (int j = (int)chunk->ploc_counts[best] - 1; j >= 0; j--) {
        const char* n = chunk->ploc_names[best] ? chunk->ploc_names[best][j] : NULL;
        if (n && n[0] && strcasecmp(n, name) == 0) return j;
    }
    return -1;
}
