/* ============================================================================
 * gmb_tui_keys.c — Macros.cs Keys (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_keys.h"
#include <stdio.h>
#include <string.h>

int gmb_keys_key_name(const char* name)
{
    if (!name) return 0;
    char n[32];
    snprintf(n, sizeof(n), "%s", name);
    for (char* p = n; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    if (strcmp(n, "enter") == 0 || strcmp(n, "return") == 0) return GMB_K_ENTER;
    if (strcmp(n, "esc") == 0) return GMB_K_ESC;
    if (strcmp(n, "tab") == 0) return GMB_K_TAB;
    if (strcmp(n, "btab") == 0 || strcmp(n, "backtab") == 0) return GMB_K_BTAB;
    if (strcmp(n, "up") == 0) return GMB_K_UP;
    if (strcmp(n, "down") == 0) return GMB_K_DOWN;
    if (strcmp(n, "left") == 0) return GMB_K_LEFT;
    if (strcmp(n, "right") == 0) return GMB_K_RIGHT;
    if (strcmp(n, "home") == 0) return GMB_K_HOME;
    if (strcmp(n, "end") == 0) return GMB_K_END;
    if (strcmp(n, "pgup") == 0) return GMB_K_PGUP;
    if (strcmp(n, "pgdn") == 0) return GMB_K_PGDN;
    if (strcmp(n, "bs") == 0 || strcmp(n, "backspace") == 0) return GMB_K_BS;
    if (strcmp(n, "del") == 0 || strcmp(n, "delete") == 0) return GMB_K_DEL;
    if (strcmp(n, "ins") == 0) return GMB_K_INS;
    return 0;
}

bool gmb_keys_matches_char(int last_key, char c) { return last_key == (int)c; }
bool gmb_keys_matches_name(int last_key, const char* name) { return last_key == gmb_keys_key_name(name); }
