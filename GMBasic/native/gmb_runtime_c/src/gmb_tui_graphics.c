/* ============================================================================
 * gmb_tui_graphics.c — gmb_tui_graphics. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"

/* base64 canonica (gmb_encoding.c / gmb_commonlib.h) — forward decl per evitare
   di includere gmb_runtime.h che collide con gmb_tui_widgets.h su gmb_event_handler. */
char* gmb_base64_encode(const uint8_t* data, size_t len);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


int gmb_graphics_detect(void)
{
    const char* env = getenv("MAXXLIB_GRAPHICS");
    if (env)
    {
        char e[32];
        snprintf(e, sizeof(e), "%s", env);
        for (char* p = e; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (strcmp(e, "kitty") == 0) return GMB_GFX_KITTY;
        if (strcmp(e, "sixel") == 0) return GMB_GFX_SIXEL;
        return GMB_GFX_NONE;
    }
    const char* term = getenv("TERM");
    if (term)
    {
        char t[64];
        snprintf(t, sizeof(t), "%s", term);
        for (char* p = t; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (strstr(t, "kitty") || strstr(t, "wezterm") || strstr(t, "ghostty")) return GMB_GFX_KITTY;
        if (strstr(t, "iterm") || strstr(t, "xterm")) return GMB_GFX_SIXEL;
    }
    return GMB_GFX_NONE;
}
int gmb_graphics_write_image(int proto, const unsigned char* rgba, int w, int h, int col, int row, int id, char* out, int cap)
{
    if (proto == GMB_GFX_NONE || !out || cap <= 0) return 0;
    if (proto == GMB_GFX_KITTY)
    {
        int o = 0;
        const char* apc = "\x1b_G";
        const char* st = "\x1b\\";
        char hdr[128];
        int hdr_len = snprintf(hdr, sizeof(hdr), "%sf=100,s=%d,v=%d,a=f,x=%d,y=%d,i=%d;", apc, w, h, col, row, id);
        for (int i = 0; i < hdr_len && o < cap - 1; i++) out[o++] = hdr[i];
        if (rgba)
        {
            char* b64 = gmb_base64_encode(rgba, (size_t)(w * h * 4));
            if (b64)
            {
                for (char* p = b64; *p && o < cap - 1; p++) out[o++] = *p;
                free(b64);
            }
        }
        for (const char* p = st; *p && o < cap - 1; p++) out[o++] = *p;
        out[o] = '\0';
        return o;
    }
    return 0;
}
int gmb_graphics_delete_image(int id, char* out, int cap)
{
    return snprintf(out, cap, "\x1b_Ga=d,i=%d\x1b\\", id);
}


