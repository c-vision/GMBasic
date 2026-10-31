/* gmb_pkg_lock.c — lockfile + integrità (sha256 per-file) del Package Manager (riferimento C). */
#include "gmb_pkg_lock.h"
#include "gmb_pkg_manifest.h"
#include "gmb_commonlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

struct gmb_pkg_lock_entry {
    char* name;
    char* version;
    char* ct;
    char* backend;
    char* platform;
    char** files;
    char** hashes;
    int nhash;
    int cap;
};

struct gmb_pkg_lock {
    struct gmb_pkg_lock_entry* items;
    int count;
    int cap;
};

static char* dupstr(const char* s) { return s ? strdup(s) : strdup(""); }

static void entry_add_hash(struct gmb_pkg_lock_entry* e, const char* file, const char* hash)
{
    if (e->nhash >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8;
        e->files = (char**)realloc(e->files, sizeof(char*) * e->cap);
        e->hashes = (char**)realloc(e->hashes, sizeof(char*) * e->cap);
    }
    e->files[e->nhash] = dupstr(file);
    e->hashes[e->nhash] = dupstr(hash);
    e->nhash++;
}

gmb_pkg_lock_t* gmb_pkg_lock_read(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    gmb_pkg_lock_t* l = (gmb_pkg_lock_t*)calloc(1, sizeof(*l));
    char line[65536];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (n == 0 || line[0] == '#') continue;
        char* at = strchr(line, '@');
        if (!at) continue;
        char* bar = strchr(at + 1, '|');
        if (!bar) continue;
        *at = 0;
        *bar = 0;
        char* name = line;
        char* version = at + 1;
        char rest[65536];
        snprintf(rest, sizeof(rest), "%s", bar + 1);
        char* p[4];
        int np = 0;
        char* sv = NULL;
        for (char* t = strtok_r(rest, "|", &sv); t && np < 4; t = strtok_r(NULL, "|", &sv)) p[np++] = t;

        if (l->count >= l->cap) {
            l->cap = l->cap ? l->cap * 2 : 8;
            l->items = (struct gmb_pkg_lock_entry*)realloc(l->items, sizeof(struct gmb_pkg_lock_entry) * l->cap);
        }
        struct gmb_pkg_lock_entry* e = &l->items[l->count++];
        memset(e, 0, sizeof(*e));
        e->name = dupstr(name);
        e->version = dupstr(version);
        e->ct = dupstr(np > 0 ? p[0] : "");
        e->backend = dupstr(np > 1 ? p[1] : "");
        e->platform = dupstr(np > 2 ? p[2] : "");
        const char* hs = np > 3 ? p[3] : "";
        if (hs && *hs && strcmp(hs, "unknown") != 0 && strchr(hs, ':')) {
            char hb[65536];
            snprintf(hb, sizeof(hb), "%s", hs);
            char* sv2 = NULL;
            for (char* t = strtok_r(hb, ",", &sv2); t; t = strtok_r(NULL, ",", &sv2)) {
                char* c = strchr(t, ':');
                if (!c) continue;
                *c = 0;
                entry_add_hash(e, t, c + 1);
            }
        }
    }
    fclose(f);
    if (l->count == 0) { free(l); return NULL; }
    return l;
}

void gmb_pkg_lock_free(gmb_pkg_lock_t* l)
{
    if (!l) return;
    for (int i = 0; i < l->count; i++) {
        struct gmb_pkg_lock_entry* e = &l->items[i];
        free(e->name); free(e->version); free(e->ct); free(e->backend); free(e->platform);
        for (int j = 0; j < e->nhash; j++) { free(e->files[j]); free(e->hashes[j]); }
        free(e->files); free(e->hashes);
    }
    free(l->items);
    free(l);
}

int gmb_pkg_lock_count(gmb_pkg_lock_t* l) { return l ? l->count : 0; }

int gmb_pkg_lock_entry(gmb_pkg_lock_t* l, int i,
                       char* name, size_t ncap, char* version, size_t vcap,
                       char* ct, size_t ccap, char* backend, size_t bcap,
                       char* platform, size_t pcap)
{
    if (!l || i < 0 || i >= l->count) return 0;
    struct gmb_pkg_lock_entry* e = &l->items[i];
    if (name) snprintf(name, ncap, "%s", e->name);
    if (version) snprintf(version, vcap, "%s", e->version);
    if (ct) snprintf(ct, ccap, "%s", e->ct);
    if (backend) snprintf(backend, bcap, "%s", e->backend);
    if (platform) snprintf(platform, pcap, "%s", e->platform);
    return 1;
}

int gmb_pkg_lock_hash_count(gmb_pkg_lock_t* l, int i)
{
    return (l && i >= 0 && i < l->count) ? l->items[i].nhash : 0;
}

int gmb_pkg_lock_hash(gmb_pkg_lock_t* l, int i, int j, char* file, size_t fcap, char* hash, size_t hcap)
{
    if (!l || i < 0 || i >= l->count) return 0;
    struct gmb_pkg_lock_entry* e = &l->items[i];
    if (j < 0 || j >= e->nhash) return 0;
    if (file) snprintf(file, fcap, "%s", e->files[j]);
    if (hash) snprintf(hash, hcap, "%s", e->hashes[j]);
    return 1;
}

int gmb_pkg_lock_write(const char* path, const char* body)
{
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputs("# gmb-package.lock \xE2\x80\x94 generato da GMBlib PackageManager v2\n", f);
    fputs("# package@version|contentType|backend|platform|sha256(file:hash,...)\n", f);
    if (body) fputs(body, f);
    if (body && *body && body[strlen(body) - 1] != '\n') fputc('\n', f);
    fclose(f);
    return 1;
}

/* mkdir -p locale. */
static void lock_mkdir_p(const char* path)
{
    char tmp[2600];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

/* (testo di un lock) == (body nel formato canonico)? |confronto righe utili (non-#, non-vuote): ordinato*/
static int lock_body_equal(const char* existing, const char* body)
{
    char eb[1 << 16], bb[1 << 13];
    size_t ei = 0, bi = 0;
    int of = 0;
    for (const char* p = existing; *p;)
    {
        const char* e = p; while (*e && *e != '\n') e++;
        size_t len = (size_t)(e - p);
        if (len && *p != '#')
        {
            if (ei + len + 1 >= sizeof(eb)) { of = 1; break; }
            memcpy(eb + ei, p, len); ei += len; eb[ei++] = '\n';
        }
        p = *e ? e + 1 : e;
    }
    for (const char* p = body ? body : ""; *p && !of;)
    {
        const char* e = p; while (*e && *e != '\n') e++;
        size_t len = (size_t)(e - p);
        if (len && *p != '#')
        {
            if (bi + len + 1 >= sizeof(bb)) { of = 1; break; }
            memcpy(bb + bi, p, len); bi += len; bb[bi++] = '\n';
        }
        p = *e ? e + 1 : e;
    }
    if (of) return 0;
    if (ei != bi) return 0;
    if (ei == 0) return 1;
    return memcmp(eb, bb, ei) == 0;
}

int gmb_pkg_lock_write_with_history(const char* project_dir, const char* lock_path, const char* body)
{
    if (!lock_path) return 0;
    if (project_dir && project_dir[0])
    {
        char existing[1 << 16];
        existing[0] = 0;
        FILE* f = fopen(lock_path, "rb");
        if (f) { size_t n = fread(existing, 1, sizeof(existing) - 1, f); fclose(f); existing[n] = 0; }
        if (existing[0] && !lock_body_equal(existing, body))
        {
            char hist[2800];
            snprintf(hist, sizeof(hist), "%s/.gmbasic/lock-history", project_dir);
            lock_mkdir_p(hist);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            struct tm g;
            gmtime_r(&ts.tv_sec, &g);
            char hp[2900];
            snprintf(hp, sizeof(hp), "%s/%04d%02d%02d-%02d%02d%02d-%03d.lock", hist,
                     g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec,
                     (int)(ts.tv_nsec / 1000000));
            FILE* o = fopen(hp, "wb");
            if (o) { fputs(existing, o); fclose(o); }
        }
    }
    return gmb_pkg_lock_write(lock_path, body);
}

static void hex32(const uint8_t d[32], char* out) /* out: 65 byte */
{
    static const char* H = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[2 * i] = H[d[i] >> 4]; out[2 * i + 1] = H[d[i] & 15]; }
    out[64] = 0;
}

int gmb_pkg_sha256_file(const char* path, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    FILE* f = fopen(path, "rb");
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
    gmb_sha256(buf, rd, d);
    free(buf);
    char hex[65];
    hex32(d, hex);
    if (out && cap) snprintf(out, cap, "%s", hex);
    return 1;
}

/* --- raccolta file relativi di un pacchetto installato --- */
static int has_suffix(const char* s, const char* suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && strcmp(s + a - b, suf) == 0; }

static void collect_rel(const char* installDir, const char* rel, char*** out, int* n, int* cap)
{
    char relbuf[4096];
    snprintf(relbuf, sizeof(relbuf), "%s", rel ? rel : "");
    size_t rl = strlen(relbuf);
    while (rl > 0 && (relbuf[rl - 1] == '/' || relbuf[rl - 1] == '\\')) relbuf[--rl] = 0;
    rel = relbuf;
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", installDir, rel);
    struct stat st;
    if (stat(full, &st) != 0) return;
    if (S_ISREG(st.st_mode)) {
        if (*n >= *cap) { *cap = *cap ? *cap * 2 : 32; *out = (char**)realloc(*out, sizeof(char*) * (*cap)); }
        (*out)[(*n)++] = dupstr(rel);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(full);
        if (!d) return;
        struct dirent* de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char sub[4096];
            snprintf(sub, sizeof(sub), "%s/%s", rel, de->d_name);
            collect_rel(installDir, sub, out, n, cap);
        }
        closedir(d);
    }
}

static int cmp_str(const void* a, const void* b) { return strcmp(*(const char**)a, *(const char**)b); }

/* Estrae i valori di un oggetto/lista al path e li aggiunge come rel (nomi o valori). */
static void collect_manifest_map(gmb_pkg_manifest_t* m, const char* path, const char* installDir, char*** out, int* n, int* cap)
{
    int c = gmb_pkg_manifest_count(m, path);
    for (int i = 0; i < c; i++) {
        char k[1024], v[8192];
        if (!gmb_pkg_manifest_item(m, path, i, k, sizeof(k), v, sizeof(v))) continue;
        collect_rel(installDir, v, out, n, cap); /* sia array (valori) sia mappa (valori = path) */
    }
}

int gmb_pkg_pkg_hash_files(const char* installDir, const char* manifestJson, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(manifestJson);
    if (!m) return 0;
    char** files = NULL;
    int n = 0, fcap = 0;
    collect_rel(installDir, "gmb-package.json", &files, &n, &fcap);
    collect_manifest_map(m, "exports", installDir, &files, &n, &fcap);
    collect_manifest_map(m, "samples", installDir, &files, &n, &fcap);
    {   /* binaries.{backend}.{platform} e v1 binaries.{platform} */
        int nb = gmb_pkg_manifest_count(m, "binaries");
        for (int i = 0; i < nb; i++) {
            char bk[1024], bv[8192];
            if (!gmb_pkg_manifest_item(m, "binaries", i, bk, sizeof(bk), bv, sizeof(bv))) continue;
            if (bv[0]) collect_rel(installDir, bv, &files, &n, &fcap);
            else {
                char path[1200];
                snprintf(path, sizeof(path), "binaries.%s", bk);
                collect_manifest_map(m, path, installDir, &files, &n, &fcap);
            }
        }
    }
    collect_manifest_map(m, "ffi.headers", installDir, &files, &n, &fcap);
    collect_manifest_map(m, "ffi.libs", installDir, &files, &n, &fcap);

    /* dedup + ordina */
    qsort(files, n, sizeof(char*), cmp_str);
    size_t used = 0;
    char prev[4096]; prev[0] = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(prev, files[i]) == 0) continue;
        snprintf(prev, sizeof(prev), "%s", files[i]);
        char full[8192], hash[65];
        snprintf(full, sizeof(full), "%s/%s", installDir, files[i]);
        if (!gmb_pkg_sha256_file(full, hash, sizeof(hash))) continue;
        if (used >= cap) break;
        int w = snprintf(out + used, cap - used, "%s%s:%s", used ? "," : "", files[i], hash);
        if (w > 0) used += (size_t)w;
    }
    for (int i = 0; i < n; i++) free(files[i]);
    free(files);
    gmb_pkg_manifest_free(m);
    return used > 0;
}

static void safe_dir(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = name; *p && j + 1 < cap; p++) out[j++] = (*p == '/' || *p == '\\') ? '_' : *p;
    out[j] = 0;
}

int gmb_pkg_lock_verify(gmb_pkg_lock_t* l, const char* cacheRoot, char* out, size_t cap)
{
    if (out && cap) out[0] = 0;
    if (!l) return 0;
    size_t used = 0;
    int problems = 0;
    for (int i = 0; i < l->count; i++) {
        struct gmb_pkg_lock_entry* e = &l->items[i];
        if (e->nhash == 0) continue;
        char sdir[1024];
        safe_dir(e->name, sdir, sizeof(sdir));
        for (int j = 0; j < e->nhash; j++) {
            char full[8192], actual[65];
            snprintf(full, sizeof(full), "%s/%s/%s/%s", cacheRoot, sdir, e->version, e->files[j]);
            if (!gmb_pkg_sha256_file(full, actual, sizeof(actual))) {
                problems++;
                used += snprintf(out + used, used < cap ? cap - used : 0, "%s: mancante: %s\n", e->name, e->files[j]);
            } else if (strcmp(actual, e->hashes[j]) != 0) {
                problems++;
                used += snprintf(out + used, used < cap ? cap - used : 0, "%s: hash diverso: %s\n", e->name, e->files[j]);
            }
        }
    }
    return problems;
}
