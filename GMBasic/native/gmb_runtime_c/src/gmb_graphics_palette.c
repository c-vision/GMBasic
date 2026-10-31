/* gmb_graphics_palette.c — palette NOMINATE (DEFINE/USE/REPLACE/SAVE/LOAD)
 * Parità con il motore C# (PaletteManager + PaletteDispatch grafico):
 *  - DEFINE name, arr()   → registra una palette (arr = valori 0xRRGGBB)
 *  - USE name             → rende attiva la palette (copiata in g_gfx.palette)
 *  - REPLACE name, arr()  → sostituisce i colori (se è l'attiva, applica subito)
 *  - SAVE path            → scrive la palette attiva in JSON
 *  - LOAD path            → rilegge i colori JSON nella palette attiva
 * Palette non note (es. "Nord") sono no-op: il runtime software tiene la VGA16. */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define GMB_PAL_MAX 32
#define GMB_PAL_NAME 40

typedef struct {
    char name[GMB_PAL_NAME];
    unsigned char rgb[GMB_GFX_MAX_PALETTE][3];
    int n;
} named_pal_t;

static named_pal_t g_pals[GMB_PAL_MAX];
static int g_npals = 0;
static char g_active[GMB_PAL_NAME];

static named_pal_t* pal_find(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < g_npals; i++)
        if (strcasecmp(g_pals[i].name, name) == 0) return &g_pals[i];
    return NULL;
}

static named_pal_t* pal_ensure(const char* name)
{
    named_pal_t* p = pal_find(name);
    if (p) return p;
    if (g_npals >= GMB_PAL_MAX) return NULL;
    p = &g_pals[g_npals++];
    memset(p, 0, sizeof(*p));
    snprintf(p->name, sizeof(p->name), "%s", name);
    return p;
}

static void pal_apply(named_pal_t* p)
{
    if (!p) return;
    int n = p->n < GMB_GFX_MAX_PALETTE ? p->n : GMB_GFX_MAX_PALETTE;
    for (int i = 0; i < n; i++)
    {
        g_gfx.palette[i][0] = p->rgb[i][0];
        g_gfx.palette[i][1] = p->rgb[i][1];
        g_gfx.palette[i][2] = p->rgb[i][2];
    }
    if (n > 0) g_gfx.palette_size = n;
    snprintf(g_active, sizeof(g_active), "%s", p->name);
}

static void pal_set_colors(named_pal_t* p, const double* colors, int n)
{
    if (!p) return;
    if (n > GMB_GFX_MAX_PALETTE) n = GMB_GFX_MAX_PALETTE;
    if (n < 0) n = 0;
    p->n = n;
    for (int i = 0; i < n; i++)
    {
        unsigned v = colors ? (unsigned)(colors[i]) : 0u;
        p->rgb[i][0] = (unsigned char)((v >> 16) & 0xFF);
        p->rgb[i][1] = (unsigned char)((v >> 8) & 0xFF);
        p->rgb[i][2] = (unsigned char)(v & 0xFF);
    }
}

void gmb_gfx_palette_use(const char* name)
{
    if (!name) return;
    /* "Console"/"VGA16": il runtime software parte già dalla VGA16. */
    if (strcasecmp(name, "console") == 0 || strcasecmp(name, "vga16") == 0)
    {
        snprintf(g_active, sizeof(g_active), "%s", name);
        return;
    }
    named_pal_t* p = pal_find(name);
    if (p) pal_apply(p);
}

void gmb_gfx_palette_define(const char* name, const double* colors, int n)
{
    if (!name) return;
    named_pal_t* p = pal_ensure(name);
    pal_set_colors(p, colors, n);
}

void gmb_gfx_palette_replace(const char* name, const double* colors, int n)
{
    if (!name) return;
    named_pal_t* p = pal_ensure(name);
    pal_set_colors(p, colors, n);
    if (strcasecmp(g_active, name) == 0) pal_apply(p);
}

int gmb_gfx_palette_save(const char* path)
{
    if (!path) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    const char* nm = g_active[0] ? g_active : "VGA16";
    fprintf(f, "{\"name\":\"%s\",\"size\":%d,\"colors\":[", nm, g_gfx.palette_size);
    for (int i = 0; i < g_gfx.palette_size; i++)
        fprintf(f, "%s\"%02X%02X%02X\"", i ? "," : "",
                g_gfx.palette[i][0], g_gfx.palette[i][1], g_gfx.palette[i][2]);
    fputs("]}\n", f);
    fclose(f);
    return 0;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int gmb_gfx_palette_load(const char* path)
{
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1 << 20)) { fclose(f); return -1; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    const char* c = strstr(buf, "\"colors\"");
    if (!c) c = buf;
    int idx = 0;
    while ((c = strchr(c, '"')) != NULL && idx < GMB_GFX_MAX_PALETTE)
    {
        const char* e = strchr(c + 1, '"');
        if (!e) break;
        int len = (int)(e - (c + 1));
        if (len == 6 && hexval(c[1]) >= 0)
        {
            int r = hexval(c[1]) * 16 + hexval(c[2]);
            int g = hexval(c[3]) * 16 + hexval(c[4]);
            int b = hexval(c[5]) * 16 + hexval(c[6]);
            g_gfx.palette[idx][0] = (unsigned char)r;
            g_gfx.palette[idx][1] = (unsigned char)g;
            g_gfx.palette[idx][2] = (unsigned char)b;
            idx++;
        }
        c = e + 1;
    }
    if (idx > 0) g_gfx.palette_size = idx;
    free(buf);
    return 0;
}
