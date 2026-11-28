/* gmb_graphics_layers.c — P7: layer 2D indicizzati con palette propria, priorita' e
 * composizione (famiglia grafica; contratto in include/gmb_graphics_layers.h).
 *
 * Proprieta' della composizione (deciso il 2026-11-28): le pagine di g_gfx sono l'unico
 * target. Questo modulo non disegna lo schermo da solo, ci scrive con
 * `gmb_layers_blit_page()` — lo stesso rapporto che c'e' tra un canvas e la pagina. Per il
 * resto la composizione e' una funzione PURA su buffer, verificabile senza schermo e SDL.
 *
 * Scelte dichiarate:
 *  - la palette si applica per layer, non per frame: cambiare la palette di un layer non
 *    tocca gli altri (e' la differenza con la palette VGA16 globale di g_gfx);
 *  - la composizione e' a frame intero; il "ridisegno dei soli layer modified" del
 *    riferimento qui si traduce in "non comporre se nessun layer e' modificato"
 *    (`gmb_layers_composite_dirty`), perche' le primitive 2D scrivono indici, non rect
 *    sporchi. Un tracciamento per rettangolo costerebbe una tabella per layer senza
 *    cambiare il risultato: quando servira' la misura, si aggiunge qui e non nei chiamanti;
 *  - nessuna allocazione nella composizione: i buffer si riservano alla creazione e il blit
 *    scrive direttamente nei pixel della pagina.
 */
#include "gmb_graphics_layers.h"
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"   /* save_png_rgb24: un solo codificatore PNG */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int in_use;
    int kind;
    int w, h;
    int opaque;
    int priority;
    int seq;              /* ordine di creazione: spareggio stabile a parita' di priority */
    int dirty;
    unsigned char* idx;   /* w*h indici, riga 0 in alto */
    uint32_t pal[GMB_LAYERS_PALETTE];
} gmb_layer_t;

static gmb_layer_t g_layers[GMB_LAYERS_MAX];
static int g_seq = 0;
static uint32_t g_backdrop = 0;

static gmb_layer_t* lay(int id)
{
    if (id < 1 || id > GMB_LAYERS_MAX) return NULL;
    gmb_layer_t* L = &g_layers[id - 1];
    return L->in_use ? L : NULL;
}

static void lay_default_palette(gmb_layer_t* L)
{
    for (int i = 0; i < GMB_LAYERS_PALETTE; i++)
    {
        uint32_t v = (uint32_t)i & 0xFF;
        L->pal[i] = (v << 16) | (v << 8) | v;
    }
}

int gmb_layers_reset(void)
{
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
    {
        free(g_layers[i].idx);
        memset(&g_layers[i], 0, sizeof(g_layers[i]));
    }
    g_seq = 0;
    g_backdrop = 0;
    return 0;
}

int gmb_layers_create(int kind, int w, int h, int opaque)
{
    if (kind < 0 || kind >= GMB_LAYER_KINDS) return -1;
    if (w <= 0 || h <= 0) return -1;
    /* Limite di sicurezza: un layer piu' grande di 16k×16k non e' un layer 2D, e' un errore
     * di chiamata. Senza il tetto una dimensione assurda entra in overflow nel prodotto. */
    if (w > 16384 || h > 16384) return -1;

    int slot = -1;
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (!g_layers[i].in_use) { slot = i; break; }
    if (slot < 0) return -1;

    size_t n = (size_t)w * (size_t)h;
    unsigned char* idx = (unsigned char*)calloc(n, 1);
    if (!idx) return -1;

    gmb_layer_t* L = &g_layers[slot];
    memset(L, 0, sizeof(*L));
    L->in_use = 1;
    L->kind = kind;
    L->w = w;
    L->h = h;
    L->opaque = opaque ? 1 : 0;
    L->priority = kind;
    L->seq = ++g_seq;
    L->dirty = 1;
    L->idx = idx;
    lay_default_palette(L);
    return slot + 1;
}

int gmb_layers_free(int id)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    free(L->idx);
    memset(L, 0, sizeof(*L));
    return 0;
}

int gmb_layers_count(void)
{
    int n = 0;
    for (int i = 0; i < GMB_LAYERS_MAX; i++) if (g_layers[i].in_use) n++;
    return n;
}

int gmb_layers_kind(int id)   { gmb_layer_t* L = lay(id); return L ? L->kind : -1; }
int gmb_layers_width(int id)  { gmb_layer_t* L = lay(id); return L ? L->w : -1; }
int gmb_layers_height(int id) { gmb_layer_t* L = lay(id); return L ? L->h : -1; }

int gmb_layers_set_palette(int id, const uint32_t* rgb, int n)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (!rgb || n <= 0) return -1;
    if (n > GMB_LAYERS_PALETTE) n = GMB_LAYERS_PALETTE;
    for (int i = 0; i < n; i++) L->pal[i] = rgb[i] & 0xFFFFFFu;
    L->dirty = 1;
    return n;
}

int gmb_layers_set_palette_entry(int id, int index, uint32_t rgb)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (index < 0 || index >= GMB_LAYERS_PALETTE) return -1;
    L->pal[index] = rgb & 0xFFFFFFu;
    L->dirty = 1;
    return 0;
}

uint32_t gmb_layers_palette_entry(int id, int index)
{
    gmb_layer_t* L = lay(id);
    if (!L) return 0xFFFFFFFFu;
    if (index < 0 || index >= GMB_LAYERS_PALETTE) return 0xFFFFFFFFu;
    return L->pal[index];
}

int gmb_layers_clear(int id, int index)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (index < 0 || index >= GMB_LAYERS_PALETTE) return -1;
    memset(L->idx, index, (size_t)L->w * (size_t)L->h);
    L->dirty = 1;
    return 0;
}

int gmb_layers_set(int id, int x, int y, int index)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (x < 0 || y < 0 || x >= L->w || y >= L->h) return -1;
    if (index < 0 || index >= GMB_LAYERS_PALETTE) return -1;
    unsigned char* p = &L->idx[(size_t)y * (size_t)L->w + (size_t)x];
    if (*p == (unsigned char)index) return 0;
    *p = (unsigned char)index;
    L->dirty = 1;
    return 0;
}

int gmb_layers_get(int id, int x, int y)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (x < 0 || y < 0 || x >= L->w || y >= L->h) return -1;
    return (int)L->idx[(size_t)y * (size_t)L->w + (size_t)x];
}

int gmb_layers_set_priority(int id, int priority)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    if (priority == L->priority) return 0;
    L->priority = priority;
    /* Cambiare l'ordine cambia il frame: e' una modifica anche del layer sotto. */
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (g_layers[i].in_use) g_layers[i].dirty = 1;
    return 0;
}

int gmb_layers_priority(int id)
{
    gmb_layer_t* L = lay(id);
    return L ? L->priority : -1;
}

int gmb_layers_set_backdrop(uint32_t rgb)
{
    g_backdrop = rgb & 0xFFFFFFu;
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (g_layers[i].in_use) g_layers[i].dirty = 1;
    return 0;
}

uint32_t gmb_layers_backdrop(void) { return g_backdrop; }

int gmb_layers_touch(int id)
{
    gmb_layer_t* L = lay(id);
    if (!L) return -1;
    L->dirty = 1;
    return 0;
}

int gmb_layers_touched(void)
{
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (g_layers[i].in_use && g_layers[i].dirty) return 1;
    return 0;
}

/* Ordine di disegno: priorita' crescente, a pari priorita' creazione (seq) crescente.
 * Insertion sort su un vettore di puntatori (max 8): nessuna allocazione, nessuna qsort. */
static int draw_order(const gmb_layer_t** out)
{
    int n = 0;
    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (g_layers[i].in_use) out[n++] = &g_layers[i];

    for (int i = 1; i < n; i++)
    {
        const gmb_layer_t* k = out[i];
        int j = i - 1;
        while (j >= 0 && (out[j]->priority > k->priority ||
                          (out[j]->priority == k->priority && out[j]->seq > k->seq)))
        {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = k;
    }
    return n;
}

static void compose_into(uint32_t* out, int w, int h)
{
    const gmb_layer_t* order[GMB_LAYERS_MAX];
    int n = draw_order(order);

    size_t total = (size_t)w * (size_t)h;
    for (size_t i = 0; i < total; i++) out[i] = g_backdrop;

    for (int li = 0; li < n; li++)
    {
        const gmb_layer_t* L = order[li];
        int cw = L->w < w ? L->w : w;
        int ch = L->h < h ? L->h : h;
        for (int y = 0; y < ch; y++)
        {
            const unsigned char* src = &L->idx[(size_t)y * (size_t)L->w];
            uint32_t* dst = &out[(size_t)y * (size_t)w];
            for (int x = 0; x < cw; x++)
            {
                unsigned char k = src[x];
                if (k == 0 && !L->opaque) continue;   /* indice 0 trasparente */
                dst[x] = L->pal[k];
            }
        }
    }

    for (int i = 0; i < GMB_LAYERS_MAX; i++)
        if (g_layers[i].in_use) g_layers[i].dirty = 0;
}

int gmb_layers_composite(uint32_t* out, int w, int h)
{
    if (!out || w <= 0 || h <= 0) return -1;
    compose_into(out, w, h);
    return 1;
}

int gmb_layers_composite_dirty(uint32_t* out, int w, int h)
{
    if (!out || w <= 0 || h <= 0) return -1;
    if (!gmb_layers_touched()) return 0;
    compose_into(out, w, h);
    return 1;
}

/* Unico ponte: compone nella pagina ATTIVA (canvas se c'e', altrimenti schermo) e usa il
 * dirty, cosi' un frame senza modifiche non tocca la pagina. Nessuna allocazione: compone
 * direttamente nei pixel della pagina. -1 se non c'e' una pagina con pixel (SCREEN mai
 * aperto), 0 se non c'e' niente da comporre, 1 se ha composto. */
int gmb_layers_blit_page(void)
{
    gmb_gfx_page_t* p = active_page();
    if (!p || !p->pixels || p->w <= 0 || p->h <= 0) return -1;
    if (!gmb_layers_touched()) return 0;
    compose_into(p->pixels, p->w, p->h);
    return 1;
}

int gmb_layers_screenshot(const char* path, int w, int h)
{
    if (!path || w <= 0 || h <= 0) return -1;
    uint32_t* buf = (uint32_t*)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    if (!buf) return -1;
    compose_into(buf, w, h);
    int rc = save_png_rgb24(path, buf, w, h);
    free(buf);
    return rc == 0 ? 0 : -1;
}
