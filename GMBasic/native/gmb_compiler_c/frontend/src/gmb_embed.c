/* gmb_embed.c — EMBEDFILE: lettura compile-time del file da incorporare.
 * Vedi gmb_embed.h per il contratto. Nessuna dipendenza dal runtime: il frontend C
 * resta autonomo (AGENTS: i due compilatori sono indipendenti).
 */
#include "gmb_embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_base[1024] = {0};

void gmb_embed_set_base_dir(const char* dir)
{
    g_base[0] = 0;
    if (!dir || !*dir) return;
    size_t n = strlen(dir);
    if (n >= sizeof(g_base)) n = sizeof(g_base) - 1;
    memcpy(g_base, dir, n);
    g_base[n] = 0;
    /* togli la '/' finale (tranne la radice) */
    while (n > 1 && g_base[n - 1] == '/') g_base[--n] = 0;
}

void gmb_embed_set_base_from_path(const char* file)
{
    if (!file || !*file) { g_base[0] = 0; return; }
    const char* slash = strrchr(file, '/');
    if (!slash) { g_base[0] = 0; return; }   /* nessuna cartella: CWD */
    size_t n = (size_t)(slash - file);
    if (n == 0) n = 1;                        /* radice "/" */
    char dir[1024];
    if (n >= sizeof(dir)) n = sizeof(dir) - 1;
    memcpy(dir, file, n);
    dir[n] = 0;
    gmb_embed_set_base_dir(dir);
}

/* File intero in un buffer malloc (NUL in coda). NULL se non leggibile. */
static char* slurp(const char* path, long* out_len)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = sz > 0 ? fread(buf, 1, (size_t)sz, f) : 0;
    fclose(f);
    buf[got] = 0;
    if (out_len) *out_len = (long)got;
    return buf;
}

/* EMBEDFILE porta il contenuto in una STRINGA: la stringa del linguaggio è TESTO (LEN conta
 * i caratteri, non i byte, su tutte le gambe). Un payload binario non sarebbe rappresentabile
 * senza corromperlo, quindi lo si RIFIUTA esplicitamente invece di troncarlo al primo NUL.
 * Ritorna NULL se valido, altrimenti un messaggio malloc. */
static char* utf8_check(const char* data, long len)
{
    long i = 0;
    while (i < len) {
        unsigned char b = (unsigned char)data[i];
        if (b == 0) {
            char* m = (char*)malloc(220);
            if (m) snprintf(m, 220, "EMBEDFILE: il file contiene un byte NUL (offset %ld): non e' testo "
                                    "UTF-8. EMBEDFILE incorpora TESTO; per i payload binari serve una "
                                    "forma a array di byte (non ancora disponibile)", i);
            return m;
        }
        long extra;
        unsigned int cp;
        if (b < 0x80) { i++; continue; }
        else if ((b & 0xE0) == 0xC0) { extra = 1; cp = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { extra = 2; cp = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0) { extra = 3; cp = b & 0x07; }
        else { extra = -1; cp = 0; }
        if (extra < 0 || i + extra >= len) {
            char* m = (char*)malloc(200);
            if (m) snprintf(m, 200, "EMBEDFILE: sequenza UTF-8 non valida al byte %ld: il file non e' "
                                    "testo UTF-8 valido", i);
            return m;
        }
        for (long k = 1; k <= extra; k++) {
            unsigned char c = (unsigned char)data[i + k];
            if ((c & 0xC0) != 0x80) {
                char* m = (char*)malloc(200);
                if (m) snprintf(m, 200, "EMBEDFILE: sequenza UTF-8 non valida al byte %ld: il file non e' "
                                        "testo UTF-8 valido", i);
                return m;
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)
            || cp > 0x10FFFF) {
            char* m = (char*)malloc(200);
            if (m) snprintf(m, 200, "EMBEDFILE: sequenza UTF-8 non canonica al byte %ld", i);
            return m;
        }
        i += extra + 1;
    }
    return NULL;
}

char* gmb_embed_load(const char* path, long* out_len, char** error)
{
    if (out_len) *out_len = 0;
    if (error) *error = NULL;
    if (!path || !*path) {
        if (error) *error = strdup("EMBEDFILE: percorso vuoto");
        return NULL;
    }
    long len = 0;
    char* buf = NULL;
    int is_abs = (path[0] == '/' || (path[0] && path[1] == ':'));
    if (!is_abs && g_base[0]) {
        size_t n = strlen(g_base) + strlen(path) + 2;
        char* cand = (char*)malloc(n);
        if (!cand) { if (error) *error = strdup("EMBEDFILE: memoria esaurita"); return NULL; }
        snprintf(cand, n, "%s/%s", g_base, path);
        buf = slurp(cand, &len);
        free(cand);
    }
    if (!buf) buf = slurp(path, &len);
    if (!buf) {
        if (error) {
            size_t n = strlen(path) + 160;
            char* m = (char*)malloc(n);
            if (m) {
                if (g_base[0])
                    snprintf(m, n, "EMBEDFILE: file non leggibile: '%s' (cercato anche in '%s')", path, g_base);
                else
                    snprintf(m, n, "EMBEDFILE: file non leggibile: '%s'", path);
            }
            *error = m;
        }
        return NULL;
    }
    if (out_len) *out_len = len;
    {
        char* uerr = utf8_check(buf, len);
        if (uerr) {
            free(buf);
            if (error) *error = uerr; else free(uerr);
            if (out_len) *out_len = 0;
            return NULL;
        }
    }
    return buf;
}