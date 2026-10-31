/* ============================================================================
 * gmb_tui_fileicons.c — gmb_tui_fileicons. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== FileIcons ===================== */

static gmb_icon_info_t icon_for(const char* name, bool is_dir, bool is_executable)
{
    gmb_icon_info_t info;
    memset(&info, 0, sizeof(info));
    info.has_color = true;
    if (is_dir) { snprintf(info.icon, sizeof(info.icon), "\xE2\x96\xB8"); info.color = GMB_CELLCOLOR_CYAN; return info; }
    if (!name) { snprintf(info.icon, sizeof(info.icon), "?"); info.color = GMB_CELLCOLOR_GREY; return info; }
    if (is_executable) { snprintf(info.icon, sizeof(info.icon), "\xE2\x9A\x99"); info.color = GMB_CELLCOLOR_GREEN; return info; }
    const char* dot = strrchr(name, '.');
    const char* ext = dot ? dot + 1 : "";
    char e[16];
    snprintf(e, sizeof(e), "%s", ext);
    for (char* p = e; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    if (strcmp(e, "c") == 0 || strcmp(e, "h") == 0 || strcmp(e, "cpp") == 0) { snprintf(info.icon, sizeof(info.icon), "\xC2\xB5"); info.color = GMB_CELLCOLOR_BLUE; }
    else if (strcmp(e, "cs") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x9A\x99"); info.color = GMB_CELLCOLOR_DARKMAGENTA; }
    else if (strcmp(e, "py") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x90\x9F"); info.color = GMB_CELLCOLOR_YELLOW; }
    else if (strcmp(e, "md") == 0 || strcmp(e, "txt") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x98\xB9"); info.color = GMB_CELLCOLOR_GREY; }
    else if (strcmp(e, "json") == 0 || strcmp(e, "yaml") == 0 || strcmp(e, "yml") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x9A\x99"); info.color = GMB_CELLCOLOR_DARKYELLOW; }
    else if (strcmp(e, "png") == 0 || strcmp(e, "jpg") == 0 || strcmp(e, "gif") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x96\xA5"); info.color = GMB_CELLCOLOR_MAGENTA; }
    else if (strcmp(e, "zip") == 0 || strcmp(e, "tar") == 0 || strcmp(e, "gz") == 0) { snprintf(info.icon, sizeof(info.icon), "\xE2\x9E\xA5"); info.color = GMB_CELLCOLOR_DARKRED; }
    else { snprintf(info.icon, sizeof(info.icon), "\xE2\x9A\xA1"); info.color = GMB_CELLCOLOR_GREY; }
    return info;
}
gmb_icon_info_t gmb_file_theme_icon_for_path(const char* path)
{
    if (!path) { gmb_icon_info_t i; memset(&i, 0, sizeof(i)); snprintf(i.icon, sizeof(i.icon), "?"); return i; }
    const char* slash = strrchr(path, '/');
    const char* name = slash ? slash + 1 : path;
    return icon_for(name, false, false);
}
gmb_icon_info_t gmb_file_theme_icon_for(const char* name, bool is_dir, bool is_symlink, bool is_executable)
{
    (void)is_symlink;
    return icon_for(name, is_dir, is_executable);
}
