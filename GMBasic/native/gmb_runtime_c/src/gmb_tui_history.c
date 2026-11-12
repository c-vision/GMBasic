/* ============================================================================
 * gmb_tui_history.c — LineEditor.cs BasicHistory (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_history.h"
#include <stdio.h>
#include <string.h>
#include "gmb_compat_win.h"   /* strcasestr (solo su Windows) */

void gmb_basic_history_init(gmb_basic_history_t* h, int capacity)
{
    memset(h, 0, sizeof(*h));
    memset(h, 0, sizeof(*h));
    h->capacity = capacity;
    h->cursor = -1;
}

int gmb_basic_history_count(const gmb_basic_history_t* h) { return h->count; }
bool gmb_basic_history_is_empty(const gmb_basic_history_t* h) { return h->count == 0; }

void gmb_basic_history_add(gmb_basic_history_t* h, const char* entry)
{
    if (!entry || !*entry) return;
    if (h->count >= GMB_HISTORY_MAX) return;
    snprintf(h->entries[h->count], GMB_HISTORY_ENTRY, "%s", entry);
    h->count++;
    if (h->capacity > 0 && h->count > h->capacity)
    {
        for (int i = 0; i < h->count - 1; i++)
            snprintf(h->entries[i], GMB_HISTORY_ENTRY, "%s", h->entries[i + 1]);
        h->count--;
    }
    h->cursor = -1;
}

int gmb_basic_history_previous(gmb_basic_history_t* h, char* out, int cap)
{
    if (h->count == 0) return 0;
    int last = h->count - 1;
    if (h->cursor < 0)
    {
        h->cursor = last;
        snprintf(out, cap, "%s", h->entries[last]);
        return (int)strlen(out);
    }
    int idx = h->cursor;
    if (idx > 0)
    {
        int next = idx - 1;
        h->cursor = next;
        snprintf(out, cap, "%s", h->entries[next]);
        return (int)strlen(out);
    }
    snprintf(out, cap, "%s", h->entries[idx]);
    return (int)strlen(out);
}

int gmb_basic_history_next(gmb_basic_history_t* h, char* out, int cap)
{
    if (h->count == 0 || h->cursor < 0) return 0;
    int idx = h->cursor;
    int last = h->count - 1;
    if (idx < last)
    {
        int next = idx + 1;
        h->cursor = next;
        snprintf(out, cap, "%s", h->entries[next]);
        return (int)strlen(out);
    }
    h->cursor = -1;
    return 0;
}

int gmb_basic_history_search(const gmb_basic_history_t* h, const char* prefix, char out[][GMB_HISTORY_ENTRY], int cap)
{
    if (!prefix || !*prefix) return 0;
    int n = 0;
    for (int i = h->count - 1; i >= 0 && n < cap; i--)
        if (strcasestr(h->entries[i], prefix))
            snprintf(out[n++], GMB_HISTORY_ENTRY, "%s", h->entries[i]);
    return n;
}

void gmb_basic_history_clear(gmb_basic_history_t* h)
{
    h->count = 0;
    h->cursor = -1;
}
