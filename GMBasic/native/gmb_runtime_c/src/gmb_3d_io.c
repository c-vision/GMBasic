/* gmb_3d_io.c — mondi a settori: schema `gmb-3d` v1 e caricamento (P2).
 *
 * Decisioni già chiuse che questo file applica:
 *  - un solo schema con `kind` obbligatorio (specifica §12.1): qui si accetta
 *    `kind:"sectors"` e si rifiuta tutto il resto con un errore che lo dice;
 *  - la validazione delle invarianti è FATALE nel load e nomina l'elemento
 *    (`sectors[3]: soffitto <= pavimento`, F-ERRORS; specifica §12.2);
 *  - R11: il file si legge su un thread esterno e il task parca; fuori da un
 *    task (tool, oracolo — T3) la lettura è sincrona. Parsing e validazione
 *    girano SEMPRE nel chiamante, dopo il resume, e il load è atomico: il
 *    mondo nuovo si installa solo se tutto è valido.
 *
 * Forma del documento (le chiavi non elencate sono ignorate):
 *
 *   { "format": "gmb-3d", "version": 1, "kind": "sectors",
 *     "environment": { "fog": 0, "fogColor": "#101418" },
 *     "textures": [ { "w": 32, "h": 32, "a": "#204060", "b": "#e0c020",
 *                     "checker": 8 } ],
 *     "sectors": [ { "floor": 0, "ceil": 64, "floorTex": 0, "ceilTex": 0,
 *                    "flags": 0,
 *                    "walls": [ { "x0": 0, "y0": 0, "x1": 128, "y1": 0,
 *                                 "to": -1, "tex": 0,
 *                                 "bottom": 0, "top": 64 } ] } ] }
 *
 * Le texture sono generate (due colori a scacchiera): il formato non incorpora
 * immagini (F-TEXTURES).
 */
#include "gmb_3d_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmb_c99gpm.h"    /* GMB_THREAD_*, extjob, gmb_cur, gmb_yield_ticks */
#include "gmb_runtime.h"   /* parser JSON + dict/list condivisi (§0bis) */

#define IO_MAX_SECTORS 4096
#define IO_MAX_WALLS   512      /* pareti per settore */
#define IO_EPS         1.0e-6

static volatile unsigned long g_parks = 0;
static char g_err[192];

unsigned long gmb_3d_load_park_count(void)
{
    return g_parks;
}

static int fail(int rc, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_err, sizeof(g_err), fmt, ap);
    va_end(ap);
    gmb_3d_set_error(g_err);
    return rc;
}

/* --- lettura del file: sincrona in T3, su thread esterno dentro un task --- */

typedef struct io_read {
    const char* path;
    char* buf;
    long len;
    volatile int done;
} io_read_t;

static void* io_read_main(void* p)
{
    io_read_t* j = (io_read_t*)p;
    gmb_c99gpm_extjob_begin();
    FILE* f = fopen(j->path, "rb");
    if (f)
    {
        long len = -1;
        if (fseek(f, 0, SEEK_END) == 0) { len = ftell(f); fseek(f, 0, SEEK_SET); }
        if (len > 0)
        {
            char* b = (char*)malloc((size_t)len + 1);
            if (b && fread(b, 1, (size_t)len, f) == (size_t)len) { b[len] = 0; j->buf = b; j->len = len; }
            else free(b);
        }
        fclose(f);
    }
    __sync_synchronize();
    j->done = 1;
    gmb_c99gpm_extjob_end();
    return NULL;
}

static char* read_bytes(const char* path, long* len_out)
{
    io_read_t j;
    memset(&j, 0, sizeof(j));
    j.path = path;

    if (gmb_cur() == NULL)   /* T3: nessuno scheduler, lettura sincrona */
    {
        io_read_main(&j);
    }
    else
    {
        gmb_thread_t th;
        if (!GMB_THREAD_CREATE(&th, io_read_main, &j))
        {
            io_read_main(&j);   /* thread non creato: stesso lavoro, senza park */
        }
        else
        {
            __sync_fetch_and_add(&g_parks, 1);
            /* Park: il worker torna a eseguire gli altri task (R10/R11),
             * mai spin e mai gmb_sleep_ms. */
            while (!j.done) gmb_yield_ticks(1);
            GMB_THREAD_JOIN(th);
        }
    }
    *len_out = j.len;
    return j.buf;
}

/* --- lettura dei valori JSON --------------------------------------------- */

static double jnum(gmb_val_t list, int idx, const char* key, double dflt)
{
    gmb_val_t v = gmb_json_index_member_val(list, (gmb_int_t)idx, key);
    if (v.kind == GMB_TYPE_NIL) return dflt;
    return gmb_val_to_double(v);
}

static unsigned int hex_color(const char* s, unsigned int dflt)
{
    if (!s) return dflt;
    while (*s == '#') s++;
    if (!*s) return dflt;
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 16);
    return (end && end != s) ? (unsigned int)(v & 0xFFFFFFu) : dflt;
}

/* --- costruzione del mondo ----------------------------------------------- */

static int build_textures(gmb_val_t root, gmb_3d_world_t* w)
{
    gmb_val_t list = gmb_val_dict_get(root, "textures");
    gmb_int_t n = gmb_val_length(list);
    w->ntex = (int)n;
    w->tex = NULL;
    if (n <= 0) { w->sw.tex = NULL; w->sw.ntex = 0; w->sw.tex_sky = -1; return 0; }

    w->tex = (gm_tex*)calloc((size_t)n, sizeof(gm_tex));
    if (!w->tex) return fail(GMB_3D_E_NOMEM, "textures: memoria esaurita");
    for (int i = 0; i < (int)n; i++)
    {
        int tw = (int)jnum(list, i, "w", 32);
        int th = (int)jnum(list, i, "h", 32);
        int ck = (int)jnum(list, i, "checker", 8);
        gmb_val_t va = gmb_json_index_member_val(list, i, "a");
        gmb_val_t vb = gmb_json_index_member_val(list, i, "b");
        unsigned int ca = hex_color(va.kind == GMB_TYPE_STRING ? va.v_string.data : NULL, 0x204060u);
        unsigned int cb = hex_color(vb.kind == GMB_TYPE_STRING ? vb.v_string.data : NULL, 0xE0C020u);
        if (tw <= 0 || th <= 0 || tw > 1024 || th > 1024 || ck <= 0)
            return fail(GMB_3D_E_SCHEMA, "textures[%d]: dimensioni o scacchiera non valide", i);
        if (gm_tex_alloc(&w->tex[i], tw, th) != GM_OK)
            return fail(GMB_3D_E_NOMEM, "textures[%d]: memoria esaurita", i);
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++)
            {
                unsigned int c = (((x / ck) + (y / ck)) & 1) ? cb : ca;
                gm_tex_set(&w->tex[i], x, y, (unsigned char)(c >> 16), (unsigned char)(c >> 8),
                           (unsigned char)c, 255);
            }
    }
    w->sw.tex = w->tex;
    w->sw.ntex = w->ntex;
    w->sw.tex_sky = -1;
    return 0;
}

static int build_sectors(gmb_val_t root, gmb_3d_world_t* w)
{
    gmb_val_t list = gmb_val_dict_get(root, "sectors");
    gmb_int_t n = gmb_val_length(list);
    if (n <= 0) return fail(GMB_3D_E_SCHEMA, "sectors: manca o è vuoto (kind:\"sectors\")");
    if (n > IO_MAX_SECTORS) return fail(GMB_3D_E_SCHEMA, "sectors: più di %d settori", IO_MAX_SECTORS);

    w->sw.sectors = (gm_sector*)calloc((size_t)n, sizeof(gm_sector));
    if (!w->sw.sectors) return fail(GMB_3D_E_NOMEM, "sectors: memoria esaurita");
    w->sw.nsectors = (int)n;

    int wcap = 0;
    for (int i = 0; i < (int)n; i++)
    {
        gm_sector* s = &w->sw.sectors[i];
        s->floor_z = jnum(list, i, "floor", 0.0);
        s->ceil_z = jnum(list, i, "ceil", 128.0);
        s->tex_floor = (int)jnum(list, i, "floorTex", -1);
        s->tex_ceil = (int)jnum(list, i, "ceilTex", -1);
        s->flags = (int)jnum(list, i, "flags", 0);

        gmb_val_t walls = gmb_json_index_member_val(list, (gmb_int_t)i, "walls");
        gmb_int_t nw = gmb_val_length(walls);
        if (nw < 3) return fail(GMB_3D_E_INVARIANT, "sectors[%d]: %d pareti (minimo 3, settore non chiuso)", i, (int)nw);
        if (nw > IO_MAX_WALLS) return fail(GMB_3D_E_INVARIANT, "sectors[%d]: più di %d pareti", i, IO_MAX_WALLS);

        s->walls = (int*)malloc((size_t)nw * sizeof(int));
        if (!s->walls) return fail(GMB_3D_E_NOMEM, "sectors[%d]: memoria esaurita", i);
        s->nwalls = (int)nw;

        for (int k = 0; k < (int)nw; k++)
        {
            gm_sector_wall* wall = (gm_sector_wall*)realloc(w->sw.walls,
                                   (size_t)(wcap + 1) * sizeof(gm_sector_wall));
            if (!wall) return fail(GMB_3D_E_NOMEM, "sectors[%d].walls[%d]: memoria esaurita", i, k);
            w->sw.walls = wall;
            wall += wcap;
            wcap++;
            w->sw.nwalls = wcap;
            s->walls[k] = wcap - 1;

            wall->x0 = jnum(walls, k, "x0", 0.0);
            wall->y0 = jnum(walls, k, "y0", 0.0);
            wall->x1 = jnum(walls, k, "x1", 0.0);
            wall->y1 = jnum(walls, k, "y1", 0.0);
            wall->to = (int)jnum(walls, k, "to", -1.0);
            wall->tex = (int)jnum(walls, k, "tex", -1.0);   /* assente = senza texture */
            wall->bottom = jnum(walls, k, "bottom", s->floor_z);
            wall->top = jnum(walls, k, "top", s->ceil_z);

            if (wall->to < -1 || wall->to >= (int)n || wall->to == i)
                return fail(GMB_3D_E_INVARIANT, "sectors[%d].walls[%d]: to=%d non è un altro settore", i, k, wall->to);
            if (wall->tex < -1 || wall->tex >= w->ntex)
                return fail(GMB_3D_E_SCHEMA, "sectors[%d].walls[%d]: tex=%d non esiste", i, k, wall->tex);
            if (wall->x0 == wall->x1 && wall->y0 == wall->y1)
                return fail(GMB_3D_E_INVARIANT, "sectors[%d].walls[%d]: parete di lunghezza zero", i, k);
        }
    }
    return 0;
}

/* W-INV-FLOOR, W-INV-HEIGHT, W-INV-CCW: si controllano dopo la costruzione,
 * perché un portale può puntare a un settore che non è ancora stato letto. */
static int check_invariants(gmb_3d_world_t* w)
{
    for (int i = 0; i < w->sw.nsectors; i++)
    {
        const gm_sector* s = &w->sw.sectors[i];
        if (s->ceil_z <= s->floor_z + IO_EPS)
            return fail(GMB_3D_E_INVARIANT, "sectors[%d]: soffitto (%.1f) <= pavimento (%.1f)",
                        i, (double)s->ceil_z, (double)s->floor_z);
        for (int k = 0; k < s->nwalls; k++)
        {
            const gm_sector_wall* a = &w->sw.walls[s->walls[k]];
            if (a->to < 0) continue;
            const gm_sector* n = &w->sw.sectors[a->to];
            if (a->top <= a->bottom + IO_EPS)
                return fail(GMB_3D_E_INVARIANT, "sectors[%d].walls[%d]: portale senza apertura (top<=bottom)", i, k);
            if (a->top > n->ceil_z + IO_EPS || a->bottom < n->floor_z - IO_EPS)
                return fail(GMB_3D_E_INVARIANT,
                            "sectors[%d].walls[%d]: apertura %.1f..%.1f fuori dal settore %d (%.1f..%.1f)",
                            i, k, (double)a->bottom, (double)a->top, a->to,
                            (double)n->floor_z, (double)n->ceil_z);
            /* Il portale va elencato da entrambi i lati, estremi uguali. */
            int found = 0;
            for (int j = 0; j < n->nwalls && !found; j++)
            {
                const gm_sector_wall* b = &w->sw.walls[n->walls[j]];
                if (b->to != i) continue;
                found = (b->x0 == a->x1 && b->y0 == a->y1 && b->x1 == a->x0 && b->y1 == a->y0);
            }
            if (!found)
                return fail(GMB_3D_E_INVARIANT,
                            "sectors[%d].walls[%d]: portale verso %d non elencato dall'altro lato", i, k, a->to);
        }
    }
    return 0;
}

static int parse_document(const char* text, gmb_3d_world_t** out)
{
    gmb_val_t root = gmb_json_parse(text);
    if (root.kind != GMB_TYPE_OBJECT)
    {
        gmb_val_release(root);
        return fail(GMB_3D_E_SCHEMA, "documento JSON non valido (oggetto atteso)");
    }

    gmb_val_t fv = gmb_val_dict_get(root, "format");
    const char* format = fv.kind == GMB_TYPE_STRING ? fv.v_string.data : "";
    if (!format || strcmp(format, "gmb-3d") != 0)
    {
        gmb_val_release(root);
        return fail(GMB_3D_E_SCHEMA, "format: atteso \"gmb-3d\"");
    }
    double version = gmb_val_to_double(gmb_val_dict_get(root, "version"));
    if ((int)version != 1)   /* F-MIGRATE: una versione futura si rifiuta, non si indovina */
    {
        gmb_val_release(root);
        return fail(GMB_3D_E_SCHEMA, "version: %d non supportata (attesa 1)", (int)version);
    }

    gmb_val_t kv = gmb_val_dict_get(root, "kind");
    const char* kind = kv.kind == GMB_TYPE_STRING ? kv.v_string.data : "";
    if (!kind || strcmp(kind, "sectors") != 0)
    {
        gmb_val_release(root);
        return fail(GMB_3D_E_SCHEMA, "kind: WORLD vuole \"sectors\", trovato \"%s\"",
                    kind ? kind : "");
    }

    gmb_3d_world_t* w = (gmb_3d_world_t*)calloc(1, sizeof(gmb_3d_world_t));
    if (!w) { gmb_val_release(root); return fail(GMB_3D_E_NOMEM, "memoria esaurita"); }

    int rc = build_textures(root, w);
    if (rc == 0) rc = build_sectors(root, w);
    if (rc == 0) rc = check_invariants(w);

    w->sw.fog = 0.0;
    w->sw.fog_rgb = 0x101418u;
    gmb_val_t env = gmb_val_dict_get(root, "environment");
    if (rc == 0 && env.kind == GMB_TYPE_OBJECT)   /* `environment` è un oggetto: fog e colore */
    {
        w->sw.fog = gmb_val_to_double(gmb_val_dict_get(env, "fog"));
        gmb_val_t fc = gmb_val_dict_get(env, "fogColor");
        w->sw.fog_rgb = hex_color(fc.kind == GMB_TYPE_STRING ? fc.v_string.data : NULL, 0x101418u);
    }

    gmb_val_release(root);   /* il mondo copia solo numeri: nessun puntatore al documento */
    if (rc != 0) { gmb_3d_world_free(w); return rc; }
    *out = w;
    gmb_3d_set_error("");
    return 0;
}

/* --- API ----------------------------------------------------------------- */

int gmb_3d_world_load(const char* path, gmb_3d_world_t** out)
{
    if (!path || !*path || !out)
    {
        gmb_3d_set_error("world load: argomenti non validi");
        return GMB_3D_E_ARG;
    }
    *out = NULL;
    long len = 0;
    char* text = read_bytes(path, &len);
    if (!text) return fail(GMB_3D_E_IO, "world load: %s non leggibile", path);
    int rc = parse_document(text, out);
    free(text);
    return rc;
}

void gmb_3d_world_free(gmb_3d_world_t* w)
{
    if (!w) return;
    for (int i = 0; i < w->sw.nsectors; i++) free(w->sw.sectors[i].walls);
    free(w->sw.sectors);
    free(w->sw.walls);
    for (int i = 0; i < w->ntex; i++) gm_tex_free(&w->tex[i]);
    free(w->tex);
    free(w);
}
