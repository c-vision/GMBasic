/* ============================================================================
 * gmb_dscope.c — Scope dinamico BASIC (`DYNAMIC`). Vedi gmb_dscope.h.
 * ============================================================================ */
#include "gmb_dscope.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#if defined(_MSC_VER)
#define GMB_DSCOPE_TLS __declspec(thread)
#else
#define GMB_DSCOPE_TLS __thread
#endif

#define DSCOPE_INIT_CAP 8

typedef struct {
    char*     name;
    gmb_val_t val;
} dscope_ent_t;

typedef struct {
    dscope_ent_t* ents;
    int           n;
    int           cap;
} dscope_frame_t;

static GMB_DSCOPE_TLS dscope_frame_t* g_frames = NULL;
static GMB_DSCOPE_TLS int g_n = 0;    /* frame attivi */
static GMB_DSCOPE_TLS int g_cap = 0;

static gmb_val_t ds_copy(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_STRING)
        return gmb_make_val_string(v.v_string.data ? v.v_string.data : "");
    return v;
}

static void ds_val_free(gmb_val_t* v)
{
    if (v && v->kind == GMB_TYPE_STRING) gmb_string_free(&v->v_string);
}

static void ds_frame_clear(dscope_frame_t* f)
{
    if (!f) return;
    for (int i = 0; i < f->n; i++) {
        free(f->ents[i].name);
        ds_val_free(&f->ents[i].val);
    }
    free(f->ents);
    f->ents = NULL;
    f->n = 0;
    f->cap = 0;
}

void gmb_dscope_reset(void)
{
    for (int i = 0; i < g_n; i++) ds_frame_clear(&g_frames[i]);
    free(g_frames);
    g_frames = NULL;
    g_n = 0;
    g_cap = 0;
}

/* Garantisce almeno un frame (il frame base/modulo). */
static void ds_ensure_base(void)
{
    if (g_n > 0) return;
    if (g_cap < 1) {
        g_frames = (dscope_frame_t*)calloc(DSCOPE_INIT_CAP, sizeof(dscope_frame_t));
        if (!g_frames) return;
        g_cap = DSCOPE_INIT_CAP;
    }
    g_frames[0].ents = NULL;
    g_frames[0].n = 0;
    g_frames[0].cap = 0;
    g_n = 1;
}

void gmb_dscope_push(void)
{
    if (g_n >= g_cap) {
        int nc = g_cap ? g_cap * 2 : DSCOPE_INIT_CAP;
        dscope_frame_t* nf = (dscope_frame_t*)realloc(g_frames, (size_t)nc * sizeof(dscope_frame_t));
        if (!nf) return;
        g_frames = nf;
        g_cap = nc;
    }
    g_frames[g_n].ents = NULL;
    g_frames[g_n].n = 0;
    g_frames[g_n].cap = 0;
    g_n++;
}

void gmb_dscope_pop(void)
{
    if (g_n <= 0) return;
    ds_frame_clear(&g_frames[g_n - 1]);
    g_n--;
}

int gmb_dscope_depth(void)
{
    return g_n;
}

static int ds_frame_find(dscope_frame_t* f, const char* name)
{
    if (!f || !name) return -1;
    for (int i = 0; i < f->n; i++)
        if (f->ents[i].name && strcasecmp(f->ents[i].name, name) == 0) return i;
    return -1;
}

static void ds_frame_put(dscope_frame_t* f, const char* name, gmb_val_t v)
{
    int i = ds_frame_find(f, name);
    if (i >= 0) {
        ds_val_free(&f->ents[i].val);
        f->ents[i].val = ds_copy(v);
        return;
    }
    if (f->n >= f->cap) {
        int nc = f->cap ? f->cap * 2 : DSCOPE_INIT_CAP;
        dscope_ent_t* ne = (dscope_ent_t*)realloc(f->ents, (size_t)nc * sizeof(dscope_ent_t));
        if (!ne) return;
        f->ents = ne;
        f->cap = nc;
    }
    f->ents[f->n].name = strdup(name);
    if (!f->ents[f->n].name) return;
    f->ents[f->n].val = ds_copy(v);
    f->n++;
}

void gmb_dscope_declare(const char* name, gmb_val_t v)
{
    if (!name || !name[0]) return;
    ds_ensure_base();
    if (g_n <= 0) return;
    ds_frame_put(&g_frames[g_n - 1], name, v);
}

int gmb_dscope_get(const char* name, gmb_val_t* out)
{
    if (!name) return 0;
    for (int i = g_n - 1; i >= 0; i--) {
        int idx = ds_frame_find(&g_frames[i], name);
        if (idx >= 0) {
            if (out) *out = g_frames[i].ents[idx].val;   /* prestito */
            return 1;
        }
    }
    return 0;
}

int gmb_dscope_has(const char* name)
{
    return gmb_dscope_get(name, NULL);
}

void gmb_dscope_set(const char* name, gmb_val_t v)
{
    if (!name || !name[0]) return;
    ds_ensure_base();
    for (int i = g_n - 1; i >= 0; i--) {
        if (ds_frame_find(&g_frames[i], name) >= 0) {
            ds_frame_put(&g_frames[i], name, v);
            return;
        }
    }
    if (g_n > 0) ds_frame_put(&g_frames[g_n - 1], name, v);
}

gmb_val_t gmb_dscope_get_val(const char* name)
{
    gmb_val_t v;
    if (gmb_dscope_get(name, &v)) return ds_copy(v);
    return gmb_val_nil();
}

gmb_string_t gmb_dscope_get_string(const char* name)
{
    gmb_val_t v;
    if (gmb_dscope_get(name, &v) && v.kind == GMB_TYPE_STRING)
        return gmb_string_alloc(v.v_string.data ? v.v_string.data : "");
    return gmb_string_alloc("");
}

gmb_double_t gmb_dscope_get_double(const char* name)
{
    gmb_val_t v;
    if (gmb_dscope_get(name, &v)) return gmb_val_to_double(v);
    return 0.0;
}

gmb_int_t gmb_dscope_get_int(const char* name)
{
    return (gmb_int_t)gmb_dscope_get_double(name);
}
