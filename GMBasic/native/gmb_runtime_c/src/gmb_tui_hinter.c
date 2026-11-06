/* ============================================================================
 * gmb_tui_hinter.c — LineEditor.cs DefaultHinter (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_hinter.h"
#include <stdio.h>
#include <string.h>

void gmb_default_hinter_init(gmb_default_hinter_t* h, int min_chars, gmb_basic_history_t* history)
{
    memset(h, 0, sizeof(*h));
    h->min_chars = min_chars;
    h->history = history;
}

int gmb_default_hinter_hint(gmb_default_hinter_t* h, const char* text, char* out, int cap)
{
    if (!text || !*text || (int)strlen(text) < h->min_chars || !h->history) return 0;
    char matches[GMB_HISTORY_MAX][GMB_HISTORY_ENTRY];
    int n = gmb_basic_history_search(h->history, text, matches, GMB_HISTORY_MAX);
    if (n == 0) return 0;
    if ((int)strlen(matches[0]) <= (int)strlen(text)) return 0;
    snprintf(out, cap, "%s", matches[0] + strlen(text));
    return (int)strlen(out);
}
