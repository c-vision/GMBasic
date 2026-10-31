/* ============================================================================
 * gmb_tui_tab.c — Tab.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

static void tab_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    /* C# Draw: per ogni tab " {ApplyCase(name)} "; colore active = Effective(selected:true). */
    gmb_tab_t* t = (gmb_tab_t*)u;
    gmb_cell_color_t fg, bg, afg, abg;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &afg, &abg);
    int cx = c->x;
    for (int i = 0; i < t->tabs_count; i++)
    {
        char raw[80], cased[160];
        snprintf(raw, sizeof(raw), " %s ", t->tabs[i]);
        gmb_control_apply_case(c, raw, cased, (int)sizeof(cased));
        int tl = (int)strlen(cased);
        if (cx + tl > c->x + c->width) break;
        int active = (i == t->selected);
        gmb_screen_setstring(buf, cx, c->y, cased, active ? afg : fg, active ? abg : bg);
        cx += tl;
    }
}

static bool tab_key(void* u, gmb_control_t* c, int key)
{
    gmb_tab_t* t = (gmb_tab_t*)u;
    if (!c->enabled || t->tabs_count == 0) return false;
    switch (key)
    {
        case GMB_K_LEFT:
            if (t->selected > 0) t->selected--;
            return true;
        case GMB_K_RIGHT:
            if (t->selected < t->tabs_count - 1) t->selected++;
            return true;
        case GMB_K_HOME: t->selected = 0; return true;
        case GMB_K_END: t->selected = t->tabs_count - 1; return true;
    }
    return false;
}

static bool tab_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_tab_t* t = (gmb_tab_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int cx = c->x;
    for (int i = 0; i < t->tabs_count; i++)
    {
        char tb[80];
        snprintf(tb, sizeof(tb), " %s ", t->tabs[i]);
        int tl = (int)strlen(tb);
        if (cx + tl > c->x + c->width) break;
        if (x >= cx && x < cx + tl) { t->selected = i; return true; }
        cx += tl;
    }
    return true;
}

static void tab_draw_px(void* u, gmb_control_t* c)
{
    gmb_tab_t* t = (gmb_tab_t*)u;
    const char* _it[16]; for (int i = 0; i < t->tabs_count && i < 16; i++) _it[i] = t->tabs[i];
    gmb_px_draw_ex(GMB_PX_TABS, c->x, c->y, c->width, c->height, NULL, _it, t->tabs_count, t->selected, 0, 0, NULL, 0, NULL, 0);
}
void gmb_tab_init(gmb_tab_t* t, int x, int y, int w)
{
    gmb_control_init(&t->ctrl, "", x, y, w, 1, tab_draw, tab_key);
    t->ctrl.draw_px_fn = tab_draw_px;
    t->ctrl.ctrl_kind = GMB_CK_TAB;
    t->ctrl.handle_click_fn = tab_click;
    t->ctrl.user = t;
}

void gmb_tab_set_tabs(gmb_tab_t* t, const char* const* tabs, int count)
{
    t->tabs_count = count < 16 ? count : 16;
    for (int i = 0; i < t->tabs_count && tabs; i++)
        snprintf(t->tabs[i], sizeof(t->tabs[i]), "%s", tabs[i] ? tabs[i] : "");
}

int gmb_tab_render_line(gmb_tab_t* t, char* out, int cap)
{
    /* C# RenderLine: concatena " name " finché non supera Width, poi PadRight(Width). */
    int o = 0;
    for (int i = 0; i < t->tabs_count; i++)
    {
        char tb[80];
        snprintf(tb, sizeof(tb), " %s ", t->tabs[i]);
        int tl = (int)strlen(tb);
        if (o + tl > t->ctrl.width) break;
        if (o + tl > cap - 1) break;
        memcpy(out + o, tb, (size_t)tl);
        o += tl;
    }
    for (; o < t->ctrl.width && o < cap - 1; o++) out[o] = ' ';
    out[o] = '\0';
    return o;
}
