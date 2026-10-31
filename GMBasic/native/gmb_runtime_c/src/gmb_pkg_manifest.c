/* gmb_pkg_manifest.c — parser del manifest di pacchetto (riferimento C del Package Manager).
 * Usa il parser JSON del runtime (gmb_json_parse / gmb_val_dict_get / gmb_val_as_list/dict).
 * Nota: liste e dizionari JSON sono entrambi GMB_TYPE_OBJECT; si distinguono dal tipo reale
 * dell'oggetto boxed (type_name "gmb_list" vs "gmb_dict") via gmb_obj_type(). */
#include "gmb_pkg_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmb_pkg_manifest {
    gmb_val_t root;
};

/* Un valore è un dizionario (oggetto JSON) se è OBJECT con tipo "gmb_dict". */
static int mf_is_dict(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return 0;
    const char* t = gmb_obj_type(v);
    return t && strstr(t, "dict") != NULL;
}

/* Un valore è una lista (array JSON) se è OBJECT e NON è un dizionario. */
static int mf_is_list(gmb_val_t v)
{
    return v.kind == GMB_TYPE_OBJECT && !mf_is_dict(v);
}

gmb_pkg_manifest_t* gmb_pkg_manifest_parse(const char* json)
{
    if (!json) return NULL;
    gmb_val_t v = gmb_json_parse(json);
    if (!mf_is_dict(v)) return NULL; /* non-oggetto (lista/scalare) → rifiuta */
    gmb_pkg_manifest_t* m = (gmb_pkg_manifest_t*)malloc(sizeof(*m));
    if (!m) return NULL;
    m->root = v;
    return m;
}

void gmb_pkg_manifest_free(gmb_pkg_manifest_t* m)
{
    if (!m) return;
    gmb_val_release(m->root);
    free(m);
}

static gmb_val_t nav_seg(gmb_val_t cur, const char* seg)
{
    if (mf_is_dict(cur))
        return gmb_dict_get(gmb_val_as_dict(cur), seg);
    if (mf_is_list(cur)) {
        char* end = NULL;
        long idx = strtol(seg, &end, 10);
        if (end && *end == 0) {
            gmb_list_t l = gmb_val_as_list(cur);
            if (idx >= 0 && idx < (long)gmb_list_count(l)) return gmb_list_get(l, idx);
        }
    }
    return gmb_val_nil();
}

static gmb_val_t nav_path(gmb_val_t root, const char* path)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path ? path : "");
    gmb_val_t cur = root;
    char* save = NULL;
    for (char* seg = strtok_r(buf, ".", &save); seg; seg = strtok_r(NULL, ".", &save)) {
        cur = nav_seg(cur, seg);
        if (cur.kind == GMB_TYPE_NIL) break;
    }
    return cur;
}

static void extract(gmb_val_t v, char* out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = 0;
    switch (v.kind) {
        case GMB_TYPE_STRING: snprintf(out, cap, "%s", v.v_string.data ? v.v_string.data : ""); break;
        case GMB_TYPE_DOUBLE: snprintf(out, cap, "%g", v.v_double); break;
        case GMB_TYPE_INT:    snprintf(out, cap, "%lld", (long long)v.v_int); break;
        case GMB_TYPE_BOOL:   snprintf(out, cap, "%d", v.v_bool ? 1 : 0); break;
        default: break; /* NIL/OBJECT/lista/dict → "" (strutture annidate via path più profondi) */
    }
}

int gmb_pkg_manifest_get(gmb_pkg_manifest_t* m, const char* path, char* out, size_t cap)
{
    if (!m) { if (out && cap) out[0] = 0; return 0; }
    gmb_val_t v = nav_path(m->root, path);
    if (v.kind == GMB_TYPE_NIL || v.kind == GMB_TYPE_OBJECT) {
        if (out && cap) out[0] = 0;
        return 0;
    }
    extract(v, out, cap);
    return 1;
}

int gmb_pkg_manifest_get_num(gmb_pkg_manifest_t* m, const char* path, double* out)
{
    if (!m) { if (out) *out = 0; return 0; }
    /* Si legge il VALORE, non la sua resa testuale: `get` formatta i numeri con "%g" (6 cifre
     * significative), quindi una dimensione come 27744844 diventava 2.77448e+07 = 27744800 —
     * e il motore di download rifiutava l'asset ("più byte del dichiarato"). Bug reale, trovato
     * pubblicando i cervelli da 27 MB e 607 MB. */
    gmb_val_t v = nav_path(m->root, path);
    switch (v.kind) {
        case GMB_TYPE_DOUBLE: if (out) *out = v.v_double; return 1;
        case GMB_TYPE_INT:    if (out) *out = (double)v.v_int; return 1;
        case GMB_TYPE_STRING: if (out) *out = v.v_string.data ? atof(v.v_string.data) : 0; return 1;
        default:              if (out) *out = 0; return 0;
    }
}

int gmb_pkg_manifest_count(gmb_pkg_manifest_t* m, const char* path)
{
    if (!m) return -1;
    gmb_val_t v = nav_path(m->root, path);
    if (mf_is_list(v)) return (int)gmb_list_count(gmb_val_as_list(v));
    if (mf_is_dict(v)) return (int)gmb_dict_count(gmb_val_as_dict(v));
    return -1;
}

int gmb_pkg_manifest_item(gmb_pkg_manifest_t* m, const char* path, int i,
                          char* key, size_t kcap, char* val, size_t vcap)
{
    if (!m) return 0;
    gmb_val_t v = nav_path(m->root, path);
    if (mf_is_list(v)) {
        gmb_list_t l = gmb_val_as_list(v);
        if (i < 0 || i >= (int)gmb_list_count(l)) return 0;
        if (key && kcap) snprintf(key, kcap, "%d", i);
        extract(gmb_list_get(l, i), val, vcap);
        return 1;
    }
    if (mf_is_dict(v)) {
        gmb_dict_t d = gmb_val_as_dict(v);
        if (i < 0 || i >= (int)d.count) return 0;
        if (key && kcap) snprintf(key, kcap, "%s", d.items[i].key ? d.items[i].key : "");
        extract(d.items[i].value, val, vcap);
        return 1;
    }
    return 0;
}

int gmb_pkg_manifest_has(gmb_pkg_manifest_t* m, const char* path)
{
    return m ? nav_path(m->root, path).kind != GMB_TYPE_NIL : 0;
}

int gmb_pkg_manifest_is_list(gmb_pkg_manifest_t* m, const char* path)
{
    return m ? mf_is_list(nav_path(m->root, path)) : 0;
}

/* --- edit del manifest su disco (gmb add/remove): usa il JSON del runtime --- */

int gmb_pkg_manifest_set_dep(const char* manifest_path, const char* section, const char* name, const char* spec)
{
    if (!manifest_path || !section || !name || !name[0]) return 0;
    gmb_val_t root = gmb_json_load_file(manifest_path);
    if (root.kind != GMB_TYPE_OBJECT) return 0;
    gmb_val_t sec = gmb_val_dict_get(root, section);
    if (sec.kind != GMB_TYPE_OBJECT)
    {
        gmb_dict_t nd = gmb_dict_new();
        sec = gmb_make_val_dict_take(&nd);   /* il box adotta lo storage di nd (nd resta svuotato) */
        gmb_val_dict_set(root, section, sec);
    }
    gmb_val_dict_set(sec, name, gmb_make_val_string(spec && spec[0] ? spec : "*"));
    gmb_json_save_val(root, manifest_path);
    return 1;
}

int gmb_pkg_manifest_remove_dep(const char* manifest_path, const char* name)
{
    if (!manifest_path || !name || !name[0]) return 0;
    gmb_val_t root = gmb_json_load_file(manifest_path);
    if (root.kind != GMB_TYPE_OBJECT) return 0;
    const char* sections[2] = { "dependencies", "devDependencies" };
    for (int s = 0; s < 2; s++)
    {
        gmb_val_t sec = gmb_val_dict_get(root, sections[s]);
        if (sec.kind == GMB_TYPE_OBJECT) gmb_val_dict_remove(sec, name);
    }
    gmb_json_save_val(root, manifest_path);
    return 1;
}
