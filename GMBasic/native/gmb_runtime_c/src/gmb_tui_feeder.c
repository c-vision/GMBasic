/* ============================================================================
 * gmb_tui_feeder.c — gmb_tui_feeder. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void gmb_feeder_init(gmb_feeder_t* f, const char* dir)
{
    snprintf(f->directory, sizeof(f->directory), "%s", dir && dir[0] ? dir : ".");
}
int gmb_feeder_filter(const char* const* items, int n, const char* text, char out[][256], int cap)
{
    if (!text || !*text)
    {
        int m = n < cap ? n : cap;
        for (int i = 0; i < m && items; i++) snprintf(out[i], 256, "%s", items[i] ? items[i] : "");
        return m;
    }
    int o = 0;
    char lower[256];
    snprintf(lower, sizeof(lower), "%s", text);
    for (char* p = lower; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; i < n && o < cap && items; i++)
    {
        if (!items[i]) continue;
        char li[256];
        snprintf(li, sizeof(li), "%s", items[i]);
        for (char* p = li; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (strstr(li, lower)) snprintf(out[o++], 256, "%s", items[i]);
    }
    return o;
}
