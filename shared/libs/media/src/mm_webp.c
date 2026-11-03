/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * mm_webp.c — WebP (decode/encode) via lo SHIM del PACCHETTO gmbsuite `webp`.
 *
 * NESSUNA libreria di sistema: lo shim `libgmbwebp.dylib` (libwebp STATICAMENTE linkata dentro,
 * self-contained) e' INSTALLATO col pacchetto e caricato via dlopen da
 * ~/.gmbasic/packages/webp/<ver>/lib/<platform>/. Se il pacchetto `webp` non e' installato,
 * le funzioni ritornano -1 (nessuna dipendenza implicita dal sistema). */
#include "gmbmedia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>
#include <dirent.h>

typedef unsigned char* (*mm_dec_fn)(const char*, long*, long*);
typedef int (*mm_enc_fn)(const unsigned char*, long, long, long, const char*);
typedef void (*mm_free_fn)(void*);

static void* g_h = NULL;
static int g_tried = 0;
static mm_dec_fn g_dec = NULL;
static mm_enc_fn g_enc = NULL;
static mm_free_fn g_rel = NULL;

static int mm_webp_load(void) {
    if (g_tried) return g_h != NULL;
    g_tried = 1;
    /* (1) bundle builtin: libgmbwebp.dylib in libs/webp/ o Resources/libs/<rid> (walk-up dal cwd). */
    char cwd[1024];
    if (getcwd(cwd, sizeof cwd)) {
        static const char* rel[] = {
            "libgmbwebp.dylib",
            "libs/webp/libgmbwebp.dylib", "libs/webp/lib/osx-arm64/libgmbwebp.dylib",
            "Resources/libs/osx-arm64/libgmbwebp.dylib", "Resources/libs/osx-x64/libgmbwebp.dylib",
            "Resources/libs/linux-arm64/libgmbwebp.so", "Resources/libs/linux-x64/libgmbwebp.so",
            NULL };
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", cwd);
        for (int up = 0; up < 6 && !g_h; up++) {
            for (int i = 0; rel[i] && !g_h; i++) {
                char p[2048];
                snprintf(p, sizeof p, "%s/%s", dir, rel[i]);
                g_h = dlopen(p, RTLD_LAZY);
            }
            char* sl = strrchr(dir, '/');
            if (!sl || sl == dir) break;
            *sl = 0;
        }
    }
    if (!g_h) g_h = dlopen("libgmbwebp.dylib", RTLD_LAZY);
    /* (2) fallback: pacchetto installato in ~/.gmbasic/packages/webp. */
    if (!g_h) {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            char base[1024];
            snprintf(base, sizeof(base), "%s/.gmbasic/packages/webp", pw->pw_dir);
            DIR* d = opendir(base);
            if (d) {
                static const char* relpaths[] = {
                    "lib/osx-arm64/libgmbwebp.dylib", "lib/osx-x64/libgmbwebp.dylib",
                    "lib/linux-x64/libgmbwebp.so", "lib/linux-arm64/libgmbwebp.so", NULL };
                struct dirent* e;
                while (!g_h && (e = readdir(d))) {
                    if (e->d_name[0] == '.') continue;
                    for (int i = 0; relpaths[i] && !g_h; i++) {
                        char p[2048];
                        snprintf(p, sizeof(p), "%s/%s/%s", base, e->d_name, relpaths[i]);
                        g_h = dlopen(p, RTLD_LAZY);
                    }
                }
                closedir(d);
            }
        }
    }
    if (!g_h) return 0;
    g_dec = (mm_dec_fn)dlsym(g_h, "gmbwebp_decode_rgba");
    g_enc = (mm_enc_fn)dlsym(g_h, "gmbwebp_encode_rgba");
    g_rel = (mm_free_fn)dlsym(g_h, "gmbwebp_free");
    if (!g_dec || !g_enc) { dlclose(g_h); g_h = NULL; return 0; }
    return 1;
}

int mm_webp_read(const char* path, mm_image* out) {
    if (!path || !out || !mm_webp_load()) return -1;
    long w = 0, hh = 0;
    unsigned char* rgba = g_dec(path, &w, &hh);
    if (!rgba || w < 1 || hh < 1) return -1;
    out->data = (unsigned char*)malloc((size_t)w * (size_t)hh * 4);
    if (!out->data) { if (g_rel) g_rel(rgba); return -1; }
    memcpy(out->data, rgba, (size_t)w * (size_t)hh * 4);
    if (g_rel) g_rel(rgba);
    out->width = (int)w; out->height = (int)hh; out->channels = 4;
    return 0;
}

int mm_webp_write(const char* path, const mm_image* img, int quality) {
    if (!path || !img || !img->data || img->width < 1 || img->height < 1 || !mm_webp_load()) return -1;
    int w = img->width, hh = img->height, ch = img->channels;
    unsigned char* rgba = (unsigned char*)malloc((size_t)w * (size_t)hh * 4);
    if (!rgba) return -1;
    for (long i = 0; i < (long)w * hh; i++) {
        const unsigned char* p = &img->data[(size_t)i * ch];
        rgba[i * 4] = p[0];
        rgba[i * 4 + 1] = ch >= 3 ? p[1] : p[0];
        rgba[i * 4 + 2] = ch >= 3 ? p[2] : p[0];
        rgba[i * 4 + 3] = ch == 4 ? p[3] : 255;
    }
    int rc = g_enc(rgba, w, hh, quality, path);
    free(rgba);
    return rc;
}
