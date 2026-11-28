/* gmb_graphics_design.c — P7 blocco 5: `DESIGN`, immagini definite da righe ASCII.
 *
 * Riferimento (Engine1999, docs pubblici): `DESIGN` definisce un'immagine scrivendola come
 * righe di caratteri, una riga per riga di pixel, e `@` la richiama. Il runtime C non aveva
 * né il token né la definizione da stringa: questa è la parte che manca al MOTORE, cioè la
 * conversione righe ASCII → sprite (l'innesto nel linguaggio resta alla superficie BASIC).
 *
 * Contratto (verificato da tests/gmb_gfx_design_selfcheck.c):
 *  - le righe sono separate da '\n' (un '\r' finale è ignorato, così il testo arriva anche
 *    da file Windows); una '\n' finale non aggiunge una riga vuota;
 *  - tutte le righe devono avere la STESSA larghezza, almeno 1 colonna e 1 riga, al massimo
 *    256 × 256 e 4096 pixel: oltre si rifiuta (errore esplicito, niente sprite a metà);
 *  - ogni carattere è un indice di PALETTE in base 36: '0'..'9' → 0..9, 'A'..'Z'/'a'..'z'
 *    → 10..35; '.' e ' ' sono TRASPARENTI (il pixel non c'è);
 *  - i colori si risolvono con la palette corrente del runtime (palette_rgb), quindi lo
 *    sprite è una texture normale: disegno, collisione e gruppi funzionano come per
 *    `gmb_gfx_sprite_def`;
 *  - 0 = fatto; -1 = errore, con il motivo in `gmb_gfx_design_error()`.
 */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GMB_DESIGN_MAX_DIM   256
#define GMB_DESIGN_MAX_PIXELS 4096

static char g_design_err[160];

static void design_error(const char* msg)
{
    snprintf(g_design_err, sizeof(g_design_err), "%s", msg);
}

const char* gmb_gfx_design_error(void) { return g_design_err; }

/* Carattere → indice di palette, -1 se trasparente, -2 se non valido. */
static int design_index(char c)
{
    if (c == '.' || c == ' ') return -1;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    return -2;
}

int gmb_gfx_design(int id, const char* rows)
{
    int w = -1, h = 0, npx, row = 0;
    const char* s;
    int* px;

    design_error("");
    if (!rows) { design_error("righe nulle"); return -1; }

    /* prima passata: larghezza della prima riga e numero di righe */
    for (s = rows;;)
    {
        const char* e = s;
        int len;
        while (*e && *e != '\n') e++;
        len = (int)(e - s);
        if (len > 0 && s[len - 1] == '\r') len--;
        if (len == 0 && *e == 0) break;   /* '\n' finale: non è una riga in più */
        if (w < 0)
        {
            if (len == 0)
            {
                if (!*e) break;      /* stringa vuota: nessuna riga */
                s = e + 1;           /* righe vuote iniziali: rifiutate sotto (len 0) */
                design_error("la prima riga è vuota");
                return -1;
            }
            w = len;
        }
        else if (len != w)
        {
            design_error("righe di larghezza diversa");
            return -1;
        }
        h++;
        if (!*e) break;
        s = e + 1;
    }
    if (w <= 0 || h <= 0) { design_error("nessuna riga"); return -1; }
    if (w > GMB_DESIGN_MAX_DIM || h > GMB_DESIGN_MAX_DIM)
    {
        design_error("immagine troppo grande (max 256×256)");
        return -1;
    }
    if (w * h > GMB_DESIGN_MAX_PIXELS)
    {
        design_error("troppi pixel (max 4096)");
        return -1;
    }

    npx = w * h;
    px = (int*)malloc((size_t)npx * sizeof(int));
    if (!px) { design_error("memoria esaurita"); return -1; }

    for (s = rows; row < h; row++)
    {
        int col;
        for (col = 0; col < w; col++)
        {
            int idx = design_index(s[col]);
            if (idx == -2)
            {
                design_error("carattere non valido: usa 0-9, A-Z o '.' per trasparente");
                free(px);
                return -1;
            }
            px[row * w + col] = (idx < 0) ? 0 : (int)palette_rgb(idx);
        }
        while (*s && *s != '\n') s++;
        if (*s == '\n') s++;
    }

    if (gmb_gfx_sprite_def(id, w, h, px) < 0)
    {
        free(px);
        design_error("sprite non definibile (id o dimensioni non valide)");
        return -1;
    }
    free(px);
    return 0;
}
