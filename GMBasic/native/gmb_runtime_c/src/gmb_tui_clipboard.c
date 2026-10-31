/* ============================================================================
 * gmb_tui_clipboard.c — Clipboard.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== ClipboardUtils ===================== */

static const char* const g_image_exts[] = {
    "png", "jpg", "jpeg", "gif", "bmp", "webp", "tiff", "tif", "ico", "ppm", "pgm", "pbm", NULL
};

bool gmb_clipboard_is_image_path(const char* path)
{
    if (!path) return false;
    const char* dot = strrchr(path, '.');
    if (!dot) return false;
    char ext[16];
    snprintf(ext, sizeof(ext), "%s", dot + 1);
    for (char* p = ext; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; g_image_exts[i]; i++)
        if (strcmp(ext, g_image_exts[i]) == 0) return true;
    return false;
}


/* ---- 100% parity Clipboard.cs:28 — buffer testo in-process (headless; SDL host-only) ---- */
static char g_clip_text[4096];

void gmb_clipboard_set_text(const char* text)
{
    if (!text) { g_clip_text[0] = '\0'; return; }
    snprintf(g_clip_text, sizeof(g_clip_text), "%s", text);
}
const char* gmb_clipboard_try_get_text(void)
{
    return g_clip_text[0] ? g_clip_text : NULL;
}
