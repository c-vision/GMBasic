/* gmb_pkg_advisories.c — audit + SBOM (riferimento C del Package Manager). */
#include "gmb_pkg_advisories.h"
#include "gmb_semver.h"
#include "gmb_pkg_lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmb_pkg_advisory_hit { char* name; char* version; char* id; char* severity; char* summary; char* fixed; };
struct gmb_pkg_audit { struct gmb_pkg_advisory_hit* hits; int n; int cap; };

static void val_str(gmb_val_t v, char* out, size_t cap)
{
    out[0] = 0;
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) snprintf(out, cap, "%s", v.v_string.data);
}

static void hit_add(struct gmb_pkg_audit* a, const char* name, const char* version,
                    const char* id, const char* severity, const char* summary, const char* fixed)
{
    if (a->n >= a->cap) { a->cap = a->cap ? a->cap * 2 : 8; a->hits = (struct gmb_pkg_advisory_hit*)realloc(a->hits, sizeof(struct gmb_pkg_advisory_hit) * a->cap); }
    struct gmb_pkg_advisory_hit* h = &a->hits[a->n++];
    h->name = strdup(name ? name : ""); h->version = strdup(version ? version : "");
    h->id = strdup(id ? id : ""); h->severity = strdup(severity ? severity : "");
    h->summary = strdup(summary ? summary : ""); h->fixed = strdup(fixed ? fixed : "");
}

gmb_pkg_audit_t* gmb_pkg_audit(int npkg, const char* const* names, const char* const* versions, const char* advisories_json)
{
    gmb_pkg_audit_t* a = (gmb_pkg_audit_t*)calloc(1, sizeof(*a));
    if (!advisories_json) return a;
    gmb_val_t arr = gmb_json_parse(advisories_json);
    gmb_list_t l = gmb_val_as_list(arr);
    int na = (int)gmb_list_count(l);
    for (int ai = 0; ai < na; ai++) {
        gmb_val_t adv = gmb_list_get(l, ai);
        char apk[256], arange[256], aid[128], asev[64], asum[1024], afix[64];
        val_str(gmb_val_dict_get(adv, "package"), apk, sizeof(apk));
        val_str(gmb_val_dict_get(adv, "range"), arange, sizeof(arange));
        val_str(gmb_val_dict_get(adv, "id"), aid, sizeof(aid));
        val_str(gmb_val_dict_get(adv, "severity"), asev, sizeof(asev));
        val_str(gmb_val_dict_get(adv, "summary"), asum, sizeof(asum));
        val_str(gmb_val_dict_get(adv, "fixed"), afix, sizeof(afix));
        if (!apk[0] || !arange[0]) continue;
        for (int i = 0; i < npkg; i++) {
            if (strcasecmp(names[i], apk) != 0) continue;
            gmb_semver_t v;
            if (!gmb_semver_parse(versions[i], &v)) continue;
            if (gmb_semver_satisfies(&v, arange)) hit_add(a, names[i], versions[i], aid, asev, asum, afix);
        }
    }
    gmb_val_release(arr);
    return a;
}

int gmb_pkg_audit_count(gmb_pkg_audit_t* a) { return a ? a->n : 0; }

int gmb_pkg_audit_item(gmb_pkg_audit_t* a, int i,
                       char* name, size_t ncap, char* version, size_t vcap,
                       char* id, size_t icap, char* severity, size_t scap,
                       char* summary, size_t smcap, char* fixed, size_t fcap)
{
    if (!a || i < 0 || i >= a->n) return 0;
    struct gmb_pkg_advisory_hit* h = &a->hits[i];
    if (name) snprintf(name, ncap, "%s", h->name);
    if (version) snprintf(version, vcap, "%s", h->version);
    if (id) snprintf(id, icap, "%s", h->id);
    if (severity) snprintf(severity, scap, "%s", h->severity);
    if (summary) snprintf(summary, smcap, "%s", h->summary);
    if (fixed) snprintf(fixed, fcap, "%s", h->fixed);
    return 1;
}

void gmb_pkg_audit_free(gmb_pkg_audit_t* a)
{
    if (!a) return;
    for (int i = 0; i < a->n; i++) {
        free(a->hits[i].name); free(a->hits[i].version); free(a->hits[i].id);
        free(a->hits[i].severity); free(a->hits[i].summary); free(a->hits[i].fixed);
    }
    free(a->hits);
    free(a);
}

int gmb_pkg_sbom(int npkg, const char* const* names, const char* const* versions,
                 const char* const* install_dirs, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    size_t used = 0;
    #define APP(...) do { if (used < cap) used += (size_t)snprintf(out + used, cap - used, __VA_ARGS__); } while (0)
    APP("{\n  \"bomFormat\": \"CycloneDX\",\n  \"specVersion\": \"1.5\",\n  \"version\": 1,\n  \"metadata\": { \"tool\": \"gmb\" },\n  \"components\": [");
    for (int i = 0; i < npkg; i++) {
        char mpath[2600], sha[65] = "";
        snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dirs[i]);
        gmb_pkg_sha256_file(mpath, sha, sizeof(sha));
        APP("%s\n    {\n      \"type\": \"library\",\n      \"name\": \"%s\",\n      \"version\": \"%s\",\n      \"hashes\": [ { \"alg\": \"SHA-256\", \"content\": \"%s\" } ]\n    }",
            i ? "," : "", names[i], versions[i], sha);
    }
    APP("\n  ]\n}\n");
    #undef APP
    return 1;
}
