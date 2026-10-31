/* ============================================================================
 * gmb_tui_flex.h — motore Flexbox (specchio ClipperLib/Clipper FlexLayout.cs).
 * Solo geometria, nessuna dipendenza da terminale/OS.
 * ============================================================================ */
#ifndef GMB_TUI_FLEX_H
#define GMB_TUI_FLEX_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GMB_FLEX_ROW = 0, GMB_FLEX_COLUMN
};
enum {
    GMB_FLEX_NOWRAP = 0, GMB_FLEX_WRAP
};
enum {
    GMB_JUSTIFY_START = 0, GMB_JUSTIFY_END, GMB_JUSTIFY_CENTER,
    GMB_JUSTIFY_SPACE_BETWEEN, GMB_JUSTIFY_SPACE_AROUND
};
enum {
    GMB_ALIGN_ITEMS_START = 0, GMB_ALIGN_ITEMS_END, GMB_ALIGN_ITEMS_CENTER, GMB_ALIGN_ITEMS_STRETCH
};
enum {
    GMB_ALIGN_CONTENT_START = 0, GMB_ALIGN_CONTENT_END, GMB_ALIGN_CONTENT_CENTER,
    GMB_ALIGN_CONTENT_STRETCH, GMB_ALIGN_CONTENT_SPACE_BETWEEN, GMB_ALIGN_CONTENT_SPACE_AROUND
};

typedef struct gmb_flex_item {
    float grow;
    float shrink;
    bool has_basis;
    int basis;
    int min;
    int max;
    int size_w, size_h;
    gmb_rect_t rect;
} gmb_flex_item_t;

void gmb_flex_item_init(gmb_flex_item_t* it);
void gmb_flex_item_set_size(gmb_flex_item_t* it, int w, int h, float grow);

/* Generate: calcola un rect per ogni item nell'ordine di input.
   Ritorna il numero di rect scritti in out (== items_count). */
int gmb_flex_generate(gmb_rect_t container, gmb_flex_item_t* items, int items_count,
                      int dir, int wrap, int justify, int alignItems, int alignContent);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_FLEX_H */
