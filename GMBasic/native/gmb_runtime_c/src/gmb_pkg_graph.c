/* gmb_pkg_graph.c — grafo dipendenze + peer (riferimento C del Package Manager). */
#include "gmb_pkg_graph.h"
#include "gmb_pkg_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmb_pkg_tree { char** parents; char** children; int n; int cap; };

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (l < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)l + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)l, f);
    b[rd] = 0;
    fclose(f);
    return b;
}

gmb_pkg_tree_t* gmb_pkg_dep_tree(int n, const char* const* names, const char* const* install_dirs)
{
    gmb_pkg_tree_t* t = (gmb_pkg_tree_t*)calloc(1, sizeof(*t));
    for (int i = 0; i < n; i++) {
        char mpath[2600];
        snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dirs[i]);
        char* js = read_file(mpath);
        char children[8192] = "";
        size_t cl = 0;
        if (js) {
            gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
            free(js);
            if (m) {
                int nd = gmb_pkg_manifest_count(m, "dependencies");
                for (int k = 0; k < nd; k++) { char kk[512], vv[128]; if (gmb_pkg_manifest_item(m, "dependencies", k, kk, sizeof(kk), vv, sizeof(vv)) && kk[0]) cl += (size_t)snprintf(children + cl, sizeof(children) - cl, "%s%s", cl ? "," : "", kk); }
                int fc = gmb_pkg_manifest_count(m, "features.default");
                for (int k = 0; k < fc; k++) { char kk[256], vv[256]; if (gmb_pkg_manifest_item(m, "features.default", k, kk, sizeof(kk), vv, sizeof(vv))) { const char* dep = gmb_pkg_manifest_is_list(m, "features.default") ? vv : kk; if (dep[0]) cl += (size_t)snprintf(children + cl, sizeof(children) - cl, "%s%s", cl ? "," : "", dep); } }
                gmb_pkg_manifest_free(m);
            }
        }
        if (t->n >= t->cap) { t->cap = t->cap ? t->cap * 2 : 16; t->parents = (char**)realloc(t->parents, sizeof(char*) * t->cap); t->children = (char**)realloc(t->children, sizeof(char*) * t->cap); }
        t->parents[t->n] = strdup(names[i]);
        t->children[t->n] = strdup(children);
        t->n++;
    }
    return t;
}

int gmb_pkg_tree_count(gmb_pkg_tree_t* t) { return t ? t->n : 0; }
int gmb_pkg_tree_row(gmb_pkg_tree_t* t, int i, char* parent, size_t pcap, char* children_csv, size_t ccap)
{
    if (!t || i < 0 || i >= t->n) return 0;
    if (parent) snprintf(parent, pcap, "%s", t->parents[i]);
    if (children_csv) snprintf(children_csv, ccap, "%s", t->children[i]);
    return 1;
}
void gmb_pkg_tree_free(gmb_pkg_tree_t* t)
{
    if (!t) return;
    for (int i = 0; i < t->n; i++) { free(t->parents[i]); free(t->children[i]); }
    free(t->parents); free(t->children); free(t);
}

static int name_present(int n, const char* const* names, const char* s)
{
    for (int i = 0; i < n; i++) if (strcasecmp(names[i], s) == 0) return 1;
    return 0;
}

int gmb_pkg_check_peers(int n, const char* const* names, const char* const* install_dirs, char* out_csv, size_t cap)
{
    if (out_csv && cap) out_csv[0] = 0;
    size_t used = 0;
    int count = 0;
    for (int i = 0; i < n; i++) {
        char mpath[2600];
        snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dirs[i]);
        char* js = read_file(mpath);
        if (!js) continue;
        gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
        free(js);
        if (!m) continue;
        int np = gmb_pkg_manifest_count(m, "peerDependencies");
        for (int k = 0; k < np; k++) {
            char pk[512], pv[128];
            if (!gmb_pkg_manifest_item(m, "peerDependencies", k, pk, sizeof(pk), pv, sizeof(pv)) || !pk[0]) continue;
            if (name_present(n, names, pk)) continue;
            count++;
            if (used < cap) used += (size_t)snprintf(out_csv + used, cap - used, "%s%s richiede il peer '%s' %s (non presente nel progetto).", used ? "\n" : "", names[i], pk, pv);
        }
        gmb_pkg_manifest_free(m);
    }
    return count;
}
