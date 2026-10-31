/* gmb_pkg_contentaddr.c — cache content-addressable (riferimento C del Package Manager). */
#include "gmb_pkg_contentaddr.h"
#include "gmb_pkg_lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static void mkdir_p(const char* path)
{
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static int copy_file(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in) return 0;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    fclose(out);
    return ok;
}

int gmb_pkg_content_intern(const char* contentRoot, const char* filePath, char* sha_out, size_t cap)
{
    if (sha_out && cap) sha_out[0] = 0;
    char sha[65];
    if (!gmb_pkg_sha256_file(filePath, sha, sizeof(sha))) return 0;

    char blobDir[2048], blob[2200];
    snprintf(blobDir, sizeof(blobDir), "%s/%c%c", contentRoot, sha[0], sha[1]);
    snprintf(blob, sizeof(blob), "%s/%s", blobDir, sha);
    mkdir_p(contentRoot);
    mkdir_p(blobDir);

    if (access(blob, F_OK) != 0) {
        if (rename(filePath, blob) != 0 && !copy_file(filePath, blob)) return 0;
    }
    unlink(filePath); /* se già presente o rimasto */
    if (link(blob, filePath) != 0)
        if (symlink(blob, filePath) != 0)
            if (!copy_file(blob, filePath)) return 0;

    if (sha_out && cap) snprintf(sha_out, cap, "%s", sha);
    return 1;
}

static int intern_walk(const char* contentRoot, const char* dir, int* n)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { intern_walk(contentRoot, full, n); continue; }
        if (S_ISREG(st.st_mode) && gmb_pkg_content_intern(contentRoot, full, NULL, 0)) (*n)++;
    }
    closedir(d);
    return 1;
}

int gmb_pkg_content_intern_dir(const char* contentRoot, const char* dir)
{
    int n = 0;
    intern_walk(contentRoot, dir, &n);
    return n;
}

/* --- GC: rimuove i blob il cui sha non è referenziato da nessun file della cache --- */
struct slist { char** v; int n; int cap; };

static void sl_add(struct slist* s, const char* x)
{
    if (s->n >= s->cap) { s->cap = s->cap ? s->cap * 2 : 64; s->v = (char**)realloc(s->v, sizeof(char*) * s->cap); }
    s->v[s->n++] = strdup(x);
}
static int sl_has(struct slist* s, const char* x)
{
    for (int i = 0; i < s->n; i++) if (strcmp(s->v[i], x) == 0) return 1;
    return 0;
}

static int live_walk(const char* cacheRoot, const char* dir, struct slist* live, const char* skip)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        if (skip && strcmp(full, skip) == 0) continue;
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { live_walk(cacheRoot, full, live, skip); continue; }
        if (S_ISREG(st.st_mode)) { char h[65]; if (gmb_pkg_sha256_file(full, h, sizeof(h))) sl_add(live, h); }
    }
    closedir(d);
    return 1;
}

static int gc_walk(const char* contentRoot, struct slist* live)
{
    int removed = 0;
    /* contentRoot ha struttura <prefix>/<sha> */
    DIR* d = opendir(contentRoot);
    if (!d) return 0;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[2048];
        snprintf(sub, sizeof(sub), "%s/%s", contentRoot, de->d_name);
        if (strlen(de->d_name) == 2) { /* cartella di prefisso */
            DIR* d2 = opendir(sub);
            if (!d2) continue;
            struct dirent* e2;
            while ((e2 = readdir(d2))) {
                if (e2->d_name[0] == '.') continue;
                if (!sl_has(live, e2->d_name)) {
                    char blob[4096];
                    snprintf(blob, sizeof(blob), "%s/%s", sub, e2->d_name);
                    if (unlink(blob) == 0) removed++;
                }
            }
            closedir(d2);
        }
    }
    closedir(d);
    return removed;
}

int gmb_pkg_content_gc(const char* contentRoot, const char* cacheRoot)
{
    if (access(contentRoot, F_OK) != 0) return 0;
    struct slist live = {NULL, 0, 0};
    live_walk(cacheRoot, cacheRoot, &live, contentRoot);
    int removed = gc_walk(contentRoot, &live);
    for (int i = 0; i < live.n; i++) free(live.v[i]);
    free(live.v);
    return removed;
}
