/* ============================================================================
 * gmb_tui_keymap.c — gmb_tui_keymap. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== KeyMap ===================== */

void gmb_key_map_init(gmb_key_map_t* k) { memset(k, 0, sizeof(*k)); }
void gmb_key_map_bind(gmb_key_map_t* k, const char* chord, int action)
{
    memset(k, 0, sizeof(*k));
    if (!chord || k->count >= 16) return;
    char c[32];
    snprintf(c, sizeof(c), "%s", chord);
    for (char* p = c; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; i < k->count; i++)
        if (strcmp(k->chords[i], c) == 0) { k->actions[i] = action; return; }
    snprintf(k->chords[k->count], 32, "%s", c);
    k->actions[k->count] = action;
    k->count++;
}
int gmb_key_map_resolve(gmb_key_map_t* k, const char* chord)
{
    if (!chord) return 0;
    char c[32];
    snprintf(c, sizeof(c), "%s", chord);
    for (char* p = c; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; i < k->count; i++)
        if (strcmp(k->chords[i], c) == 0) return k->actions[i];
    return 0;
}
bool gmb_key_map_contains(gmb_key_map_t* k, const char* chord) { return gmb_key_map_resolve(k, chord) != 0; }
int gmb_key_map_count(gmb_key_map_t* k) { return k->count; }
bool gmb_key_map_is_empty(gmb_key_map_t* k) { return k->count == 0; }
void gmb_key_map_clear(gmb_key_map_t* k) { k->count = 0; }
