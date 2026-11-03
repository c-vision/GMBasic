/* ============================================================================
 * gmb_bsp.c — HTML / <%= expr %> / <% code %> → PRINT e statement BASIC.
 * Direttive `<%@ IMPORT ... %>` tradotte in IMPORT (prima erano ignorate: le pagine che
 * importano AI/JSON non compilavano). Preambolo minimo: IMPORT HTTP + STRINGS.
 * ============================================================================ */
#include "gmb_bsp.h"
#include "gmb_bsp_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Renderer VM registrato da gmb_vm_c (pagine precompilate in chunk A1). */
static gmb_bsp_vm_render_fn g_vm_renderer = 0;

void gmb_bsp_set_vm_renderer(gmb_bsp_vm_render_fn fn)
{
    g_vm_renderer = fn;
}

static char* bsp_read_file(const char* path)
{
    FILE* f;
    long n;
    char* b;
    size_t r;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 1048576L) { fclose(f); return NULL; }
    rewind(f);
    b = (char*)malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    r = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[r] = 0;
    return b;
}

static char* bsp_dup_lit(const char* s)
{
    size_t n = strlen(s);
    char* b = (char*)malloc(n + 1);
    if (!b) return NULL;
    memcpy(b, s, n + 1);
    return b;
}

static int bsp_ends_dot_bsp(const char* p)
{
    size_t n;
    const char* e;
    if (!p) return 0;
    n = strlen(p);
    if (n < 4) return 0;
    e = p + n - 4;
    return e[0] == '.'
        && (e[1] == 'b' || e[1] == 'B')
        && (e[2] == 's' || e[2] == 'S')
        && (e[3] == 'p' || e[3] == 'P');
}

/* Solo renderer VM (registrato staticamente): usato anche dalla dylib `libgmb_bsp`
 * (gmb_bsp_shared_serve) per NON rientrare nel gateway (ricorsione) e per non avere
 * alcun fallback al sottoinsieme. */
int gmb_bsp_try_serve_vm(const char* dir, const char* url_path, const char* query,
                         const char* host, char** out_html, int* out_status)
{
    const char* rel;
    char path[1024];

    if (out_html) *out_html = NULL;
    if (out_status) *out_status = 0;
    if (!dir || !*dir || !url_path) return 0;
    if (!bsp_ends_dot_bsp(url_path)) return 0;

    rel = url_path;
    while (*rel == '/') rel++;
    if (!*rel) return 0;
    if (strstr(rel, "..")) {
        if (out_status) *out_status = 403;
        if (out_html) *out_html = bsp_dup_lit("Forbidden");
        return 1;
    }

    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    if (g_vm_renderer && g_vm_renderer(path, url_path, query, host, out_html, out_status))
        return 1;
    return 0;
}

int gmb_bsp_try_serve(const char* dir, const char* url_path, const char* query,
                      const char* host, char** out_html, int* out_status)
{
    if (gmb_bsp_try_serve_vm(dir, url_path, query, host, out_html, out_status))
        return 1;

    /* Renderer BSP COMPLETO via dylib `libgmb_bsp` (VM/C). NIENTE fallback al
     * valutatore/shell statica: se il motore completo o il sidecar mancano, errore
     * ESPLICITO (mai una pagina diversa da quella compilata). */
    if (gmb_bsp_gateway_serve(dir, url_path, query, host, out_html, out_status))
        return 1;

    if (!dir || !*dir || !url_path || !bsp_ends_dot_bsp(url_path)) return 0;
    {
        const char* rel = url_path;
        char path[1024];
        while (*rel == '/') rel++;
        if (!*rel || strstr(rel, "..")) return 0;
        snprintf(path, sizeof(path), "%s/%s", dir, rel);
        char* probe = bsp_read_file(path);
        if (!probe) return 0;
        free(probe);
        if (out_status) *out_status = 500;
        if (out_html) *out_html = bsp_dup_lit(
            "<!DOCTYPE html><html><head><title>BSP non compilata</title></head><body>"
            "<h1>BSP non compilata</h1><p>Questa pagina richiede il motore BSP COMPLETO "
            "(compilatore C + VM/C) e il chunk A1 <code>&lt;pagina&gt;.bsp.chunk</code>. "
            "Esegui <code>gmb build</code>/<code>gmb run</code> sul progetto (precompila le "
            "pagine) e assicurati che <code>libgmb_bsp</code> sia nel bundle.</p></body></html>");
        fprintf(stderr, "[bsp] %s: manca il chunk A1 o libgmb_bsp (motore completo) — "
                        "esegui `gmb build`/`gmb run` e includi libgmb_bsp nel bundle\n", path);
        return 1;
    }
}
