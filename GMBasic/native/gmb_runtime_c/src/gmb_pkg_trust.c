/* gmb_pkg_trust.c — keyring + firme HMAC (riferimento C del Package Manager). */
#include "gmb_pkg_trust.h"
#include "gmb_pkg_manifest.h"
#include "gmb_commonlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* HEXD = "0123456789abcdef";
static void hex32(const uint8_t d[32], char* out)
{
    for (int i = 0; i < 32; i++) { out[2 * i] = HEXD[d[i] >> 4]; out[2 * i + 1] = HEXD[d[i] & 15]; }
    out[64] = 0;
}

int gmb_pkg_hmac_hex(const char* key, const unsigned char* data, size_t len, char* out, size_t cap)
{
    if (!out || cap == 0) return 0;
    uint8_t d[32];
    gmb_hmac_sha256((const uint8_t*)(key ? key : ""), key ? strlen(key) : 0, data, len, d);
    char h[65];
    hex32(d, h);
    snprintf(out, cap, "%s", h);
    return 1;
}

int gmb_pkg_hmac_file_hex(const char* key, const char* file, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    FILE* f = fopen(file, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return 0; }
    uint8_t* buf = (uint8_t*)malloc((size_t)len ? (size_t)len : 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    uint8_t d[32];
    gmb_hmac_sha256((const uint8_t*)(key ? key : ""), key ? strlen(key) : 0, buf, rd, d);
    free(buf);
    char h[65];
    hex32(d, h);
    if (out && cap) snprintf(out, cap, "%s", h);
    return 1;
}

int gmb_pkg_key_fingerprint(const char* key, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    uint8_t d[32];
    gmb_sha256((const uint8_t*)(key ? key : ""), key ? strlen(key) : 0, d);
    char h[65];
    hex32(d, h);
    h[16] = 0;
    if (out && cap) snprintf(out, cap, "%s", h);
    return 1;
}

/* --- keyring (JSON { "<nome>": "<chiave>" }) --- */
struct kr { char** names; char** keys; int n; int cap; };

static void kr_add(struct kr* k, const char* name, const char* key)
{
    if (k->n >= k->cap) {
        k->cap = k->cap ? k->cap * 2 : 8;
        k->names = (char**)realloc(k->names, sizeof(char*) * k->cap);
        k->keys = (char**)realloc(k->keys, sizeof(char*) * k->cap);
    }
    k->names[k->n] = strdup(name ? name : "");
    k->keys[k->n] = strdup(key ? key : "");
    k->n++;
}

static void kr_free(struct kr* k)
{
    for (int i = 0; i < k->n; i++) { free(k->names[i]); free(k->keys[i]); }
    free(k->names);
    free(k->keys);
}

static void kr_load(const char* path, struct kr* k)
{
    memset(k, 0, sizeof(*k));
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return; }
    char* js = (char*)malloc((size_t)len + 1);
    if (!js) { fclose(f); return; }
    size_t rd = fread(js, 1, (size_t)len, f);
    js[rd] = 0;
    fclose(f);
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
    free(js);
    if (m) {
        int n = gmb_pkg_manifest_count(m, "");
        for (int i = 0; i < n; i++) {
            char nm[1024], kv[8192];
            if (gmb_pkg_manifest_item(m, "", i, nm, sizeof(nm), kv, sizeof(kv))) kr_add(k, nm, kv);
        }
        gmb_pkg_manifest_free(m);
    }
}

static void json_esc(const char* s, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = (s ? s : ""); *p && j + 2 < cap; p++) {
        char c = *p;
        if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = c; }
        else out[j++] = c;
    }
    out[j] = 0;
}

static int kr_write(const char* path, struct kr* k, const char* setName, const char* setKey, const char* removeName)
{
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputc('{', f);
    int first = 1;
    for (int i = 0; i < k->n; i++) {
        if (removeName && strcmp(k->names[i], removeName) == 0) continue;
        if (setName && strcmp(k->names[i], setName) == 0) continue; /* aggiornata dopo */
        char en[2048], ek[8192];
        json_esc(k->names[i], en, sizeof(en));
        json_esc(k->keys[i], ek, sizeof(ek));
        fprintf(f, "%s\"%s\":\"%s\"", first ? "" : ",", en, ek);
        first = 0;
    }
    if (setName) {
        char en[2048], ek[8192];
        json_esc(setName, en, sizeof(en));
        json_esc(setKey ? setKey : "", ek, sizeof(ek));
        fprintf(f, "%s\"%s\":\"%s\"", first ? "" : ",", en, ek);
    }
    fputc('}', f);
    fputc('\n', f);
    fclose(f);
    chmod(path, 0600); /* il keyring è un segreto: solo l'utente */
    return 1;
}

int gmb_pkg_keyring_get(const char* path, const char* name, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    struct kr k;
    kr_load(path, &k);
    int found = 0;
    for (int i = 0; i < k.n; i++)
        if (strcmp(k.names[i], name ? name : "") == 0) { if (out && cap) snprintf(out, cap, "%s", k.keys[i]); found = 1; break; }
    kr_free(&k);
    return found;
}

int gmb_pkg_keyring_count(const char* path)
{
    struct kr k;
    kr_load(path, &k);
    int n = k.n;
    kr_free(&k);
    return n;
}

int gmb_pkg_keyring_item(const char* path, int i, char* name, size_t ncap, char* key, size_t kcap)
{
    struct kr k;
    kr_load(path, &k);
    int ok = 0;
    if (i >= 0 && i < k.n) {
        if (name && ncap) snprintf(name, ncap, "%s", k.names[i]);
        if (key && kcap) snprintf(key, kcap, "%s", k.keys[i]);
        ok = 1;
    }
    kr_free(&k);
    return ok;
}

int gmb_pkg_keyring_set(const char* path, const char* name, const char* key)
{
    struct kr k;
    kr_load(path, &k);
    int ok = kr_write(path, &k, name, key, NULL);
    kr_free(&k);
    return ok;
}

int gmb_pkg_keyring_remove(const char* path, const char* name)
{
    struct kr k;
    kr_load(path, &k);
    int ok = kr_write(path, &k, NULL, NULL, name);
    kr_free(&k);
    return ok;
}
