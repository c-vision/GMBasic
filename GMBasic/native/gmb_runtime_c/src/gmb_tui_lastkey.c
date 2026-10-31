/* ============================================================================
 * gmb_tui_lastkey.c — Globals.cs LastKey (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_lastkey.h"

static int g_last_key = 0;

int gmb_last_key_get(void) { return g_last_key; }
void gmb_last_key_set(int key) { g_last_key = key; }
