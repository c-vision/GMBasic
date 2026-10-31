/* gmb_pkg_registry.c — client del registry HTTP (riferimento C del Package Manager). */
#include "gmb_pkg_registry.h"
#include "gmb_pkg_manifest.h"
#include "gmb_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gmb_pkg_registry_detail {
    char** versions;
    int n;
    int cap;
};

/* Barra di avanzamento ON/OFF (impostata dall'host solo se la console è un TTY). */
static int g_progress = 0;
void gmb_pkg_registry_set_progress(int on) { g_progress = on; }

/* Richiesta autenticata (POST JSON / GET) al registry: scrive il corpo risposta in out (cap),
   lo status in out_status. Ritorna 1 se 2xx, 0 altrimenti. Delega a gmb_net_http_request. */
static int http_json(const char* url, const char* method, const char* json_body,
                     const char* bearer, char* out, size_t cap, int* out_status)
{
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof(opts));
    const char* hn[1] = { "Content-Type" };
    const char* hv[1] = { "application/json" };
    opts.method = method;
    if (json_body && *json_body)
    {
        opts.body = json_body;
        opts.body_len = strlen(json_body);
        opts.header_names = hn; opts.header_values = hv; opts.header_count = 1;
    }
    opts.bearer_token = (bearer && bearer[0]) ? bearer : NULL;
    int status = 0; char* body = NULL; size_t blen = 0; char* err = NULL;
    int rc = gmb_net_http_request(url, &opts, &status, &body, &blen, &err, NULL);
    if (out_status) *out_status = status;
    if (out && cap) { out[0] = 0; if (body) snprintf(out, cap, "%s", body); }
    if (body) free(body);
    if (err) free(err);
    return rc == 0 && status >= 200 && status < 300;
}

int gmb_pkg_registry_post(const char* url, const char* json_body, const char* bearer,
                          char* out, size_t cap, int* out_status)
{
    return http_json(url, "POST", json_body, bearer, out, cap, out_status);
}

int gmb_pkg_registry_get_auth(const char* url, const char* bearer,
                              char* out, size_t cap, int* out_status)
{
    return http_json(url, "GET", NULL, bearer, out, cap, out_status);
}

int gmb_pkg_registry_get(const char* url, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    char* body = gmb_net_http_get(url);
    if (!body) return 0;
    if (out && cap) snprintf(out, cap, "%s", body);
    free(body);
    return 1;
}

gmb_pkg_registry_detail_t* gmb_pkg_registry_detail_from_json(const char* json)
{
    if (!json) return NULL;
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(json);
    if (!m) return NULL;
    gmb_pkg_registry_detail_t* d = (gmb_pkg_registry_detail_t*)calloc(1, sizeof(*d));
    if (!d) { gmb_pkg_manifest_free(m); return NULL; }
    /* versionList (array) oppure versions (oggetto: chiavi) */
    const char* path = "versionList";
    int n = gmb_pkg_manifest_count(m, path);
    int use_keys = 0;
    if (n <= 0) { path = "versions"; n = gmb_pkg_manifest_count(m, path); use_keys = 1; }
    for (int i = 0; i < n; i++) {
        char k[256], v[256];
        if (!gmb_pkg_manifest_item(m, path, i, k, sizeof(k), v, sizeof(v))) continue;
        const char* ver = use_keys ? k : v; /* versions: chiavi; versionList: valori */
        if (!ver[0]) continue;
        if (d->n >= d->cap) { d->cap = d->cap ? d->cap * 2 : 16; d->versions = (char**)realloc(d->versions, sizeof(char*) * d->cap); }
        d->versions[d->n++] = strdup(ver);
    }
    gmb_pkg_manifest_free(m);
    return d;
}

gmb_pkg_registry_detail_t* gmb_pkg_registry_detail(const char* base_url, const char* name)
{
    char url[2048];
    snprintf(url, sizeof(url), "%s/api/packages/%s", base_url, name);
    char* body = gmb_net_http_get(url);
    if (!body) return NULL;
    gmb_pkg_registry_detail_t* d = gmb_pkg_registry_detail_from_json(body);
    free(body);
    return d;
}

int gmb_pkg_registry_detail_count(gmb_pkg_registry_detail_t* d) { return d ? d->n : 0; }

int gmb_pkg_registry_detail_version(gmb_pkg_registry_detail_t* d, int i, char* out, size_t cap)
{
    if (!d || i < 0 || i >= d->n) return 0;
    if (out && cap) snprintf(out, cap, "%s", d->versions[i]);
    return 1;
}

void gmb_pkg_registry_detail_free(gmb_pkg_registry_detail_t* d)
{
    if (!d) return;
    for (int i = 0; i < d->n; i++) free(d->versions[i]);
    free(d->versions);
    free(d);
}

int gmb_pkg_registry_tag(const char* base_url, const char* name, const char* tag, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    char url[2048];
    snprintf(url, sizeof(url), "%s/api/packages/%s/tags", base_url, name);
    char* body = gmb_net_http_get(url);
    if (!body) return 0;
    int ok = 0;
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(body);
    if (m) {
        char v[256];
        if (gmb_pkg_manifest_get(m, tag, v, sizeof(v))) { if (out && cap) snprintf(out, cap, "%s", v); ok = v[0] != 0; }
        gmb_pkg_manifest_free(m);
    }
    free(body);
    return ok;
}

int gmb_pkg_registry_download(const char* base_url, const char* name, const char* version, const char* qs, const char* dest_path)
{
    char url[2048];
    snprintf(url, sizeof(url), "%s/download/%s/%s%s", base_url, name, version, qs ? qs : "");
    /* Streaming (libcurl) con barra su stderr se abilitata; nessuna callback gestita. */
    return gmb_net_http_download(url, dest_path, g_progress) == 0;
}
