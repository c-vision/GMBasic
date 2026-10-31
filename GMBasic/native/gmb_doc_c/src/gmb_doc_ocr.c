/* gmb_doc_ocr.c — OCR per documenti.
 * Livello 2 (cross-OS): Tesseract — libtesseract + libleptonica + tessdata/eng dal
 * bundle LIBS in Resources/libs/<os>/ (gmb setup LIBS), dlopen a runtime.
 * Livello 1 (macOS): Apple Vision — libllm_vision_apple via dlopen, zero modelli.
 * Stesso pattern di gmb_doc_pdf.c: le librerie si cercano in Resources/libs/<os>/
 * risalendo dalla cwd, poi in GMBASIC_* env, poi build dir.
 */
#include "gmb_doc_ocr.h"
/* gmb_paths_libs_cache_dir() (cache on-demand ~/.gmb/libs) è dichiarata in gmb_runtime.h:
 * senza questo include il compilatore la tratta come implicit declaration (errore C99) e
 * la build di gmb_vm_host si ferma qui (2026-10-28). */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#if defined(__APPLE__)
static const char* gmb_ocr_libname(void)
{
#if defined(__arm64__) || defined(__aarch64__)
    return "osx-arm64";
#else
    return "osx-x64";
#endif
}
#endif

/* ── risoluzione cartella libs per RID ── */
static int gmb_ocr_libs_dir(char* out, size_t cap)
{
    /* 1) CACHE ON-DEMAND (~/.gmb/libs/<rid>): le librerie native non stanno più nel repo.
     *    Stessa regola del resolver C# (paths.libsCacheDir, default ~/.gmb/libs). */
    {
        char cache[2048];
        gmb_paths_libs_cache_dir(cache, sizeof(cache));
        if (cache[0]) {
#if defined(_WIN32) || defined(_WIN64)
            const char* ridc = "win-x64";
#elif defined(__APPLE__)
            const char* ridc = gmb_ocr_libname();
#elif defined(__aarch64__)
            const char* ridc = "linux-arm64";
#else
            const char* ridc = "linux-x64";
#endif
            snprintf(out, cap, "%s/%s", cache, ridc);
            return 1;
        }
    }
#if defined(_WIN32) || defined(_WIN64)
    const char* rid = "win-x64";
#elif defined(__APPLE__)
    const char* rid = gmb_ocr_libname();
#elif defined(__aarch64__)
    const char* rid = "linux-arm64";
#else
    const char* rid = "linux-x64";
#endif
    if (getcwd(out, cap)) {
        for (int up = 0; up < 4; up++) {
            if (up > 0) {
                char* slash = strrchr(out, '/');
                if (!slash) break;
                *slash = '\0';
            }
            size_t n = snprintf(out, cap, "%s/Resources/libs/%s", out, rid);
            if (n < cap) return 1;
        }
    }
    return 0;
}

/* ── Livello 2: Tesseract ── */
typedef void* (*tess_create_fn)(void);
typedef int   (*tess_init_fn)(void*, const char*, const char*);
typedef void  (*tess_setimage_fn)(void*, const unsigned char*, int, int, int, int);
typedef char* (*tess_gettext_fn)(void*);
typedef void  (*tess_delete_fn)(const char*);
typedef void  (*tess_end_fn)(void*);
typedef void  (*tess_delapi_fn)(void*);

static void* gmb_tess_load(tess_create_fn* create, tess_init_fn* init, tess_setimage_fn* setimg,
                           tess_gettext_fn* gettext, tess_delete_fn* del, tess_end_fn* end, tess_delapi_fn* delapi)
{
    char dir[2048];
    if (!gmb_ocr_libs_dir(dir, sizeof(dir))) return NULL;
    char lib[2200];
#if defined(_WIN32) || defined(_WIN64)
    snprintf(lib, sizeof(lib), "%s/libtesseract.dll", dir);
#else
    snprintf(lib, sizeof(lib), "%s/libtesseract.%s", dir, "dylib");
#if !defined(__APPLE__)
    snprintf(lib, sizeof(lib), "%s/libtesseract.so", dir);
#endif
#endif
    void* dl = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!dl) return NULL;
    dlerror();
    *create   = (tess_create_fn)dlsym(dl, "TessBaseAPICreate");
    *init     = (tess_init_fn)dlsym(dl, "TessBaseAPIInit3");
    *setimg   = (tess_setimage_fn)dlsym(dl, "TessBaseAPISetImage");
    *gettext  = (tess_gettext_fn)dlsym(dl, "TessBaseAPIGetUTF8Text");
    *del      = (tess_delete_fn)dlsym(dl, "TessDeleteText");
    *end      = (tess_end_fn)dlsym(dl, "TessBaseAPIEnd");
    *delapi   = (tess_delapi_fn)dlsym(dl, "TessBaseAPIDelete");
    if (!*create || !*init || !*setimg || !*gettext || !*del || !*end || !*delapi) {
        dlclose(dl);
        return NULL;
    }
    return dl;
}

char* gmb_doc_ocr_rgba(const uint8_t* rgba, int w, int h)
{
    tess_create_fn create; tess_init_fn init; tess_setimage_fn setimg;
    tess_gettext_fn gettext; tess_delete_fn del; tess_end_fn end; tess_delapi_fn delapi;
    void* dl = gmb_tess_load(&create, &init, &setimg, &gettext, &del, &end, &delapi);
    if (!dl || !rgba || w <= 0 || h <= 0) { if (dl) dlclose(dl); return NULL; }
    char dir[2048];
    if (!gmb_ocr_libs_dir(dir, sizeof(dir))) { dlclose(dl); return NULL; }
    char datapath[2200];
    snprintf(datapath, sizeof(datapath), "%s/tessdata", dir);

    void* api = create();
    if (!api || init(api, datapath, "eng") != 0) { if (api) delapi(api); dlclose(dl); return NULL; }
    setimg(api, rgba, w, h, 4, w * 4);
    char* text = gettext(api);
    char* out = NULL;
    if (text && text[0]) out = strdup(text);
    if (text) del(text);
    end(api);
    delapi(api);
    dlclose(dl);
    return out;
}

/* ── Livello 1: Apple Vision (macOS) ── */
typedef const char* (*vision_ocr_fn)(const char* image_path);
typedef void (*vision_free_fn)(char* s);

static void* gmb_vision_open(void)
{
#if !defined(__APPLE__)
    return NULL;
#else
    const char* env = getenv("GMBASIC_VISION_LIB");
    if (env && env[0])
    {
        void* h = dlopen(env, RTLD_NOW | RTLD_LOCAL);
        if (h) return h;
    }
    char cwd[2048];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    char cand[4096];
    const char* name = gmb_ocr_libname();
    for (int up = 0; up < 4; up++)
    {
        if (up > 0)
        {
            char* slash = strrchr(cwd, '/');
            if (!slash) break;
            *slash = '\0';
        }
        snprintf(cand, sizeof(cand), "%s/Resources/libs/%s/libllm_vision_apple.dylib", cwd, name);
        void* h = dlopen(cand, RTLD_NOW | RTLD_LOCAL);
        if (h) return h;
    }
    return NULL;
#endif
}

char* gmb_doc_ocr_image(const char* png_path)
{
    if (!png_path || !png_path[0]) return NULL;
    void* dl = gmb_vision_open();
    if (!dl) return NULL;
    dlerror();
    vision_ocr_fn ocr = (vision_ocr_fn)dlsym(dl, "llm_vision_apple_recognize_text");
    vision_free_fn freestr = (vision_free_fn)dlsym(dl, "llm_vision_apple_free_string");
    if (!ocr || !freestr) { dlclose(dl); return NULL; }
    const char* r = ocr(png_path);
    if (!r || !r[0]) { dlclose(dl); return NULL; }
    char* out = strdup(r);
    freestr((char*)r);
    dlclose(dl);
    return out;
}
