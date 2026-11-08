/* ============================================================================
 * gmb_tui_spinner.c — gmb_tui_spinner. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_px_renderer.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gmb_form.h"

/* ===================== Spinner ===================== */

static const char* spinner_presets[][12] = {
    { "|", "/", "-", "\\" },
    { "\xE2\x97\x8B", "\xE2\x97\x8F", "\xE2\x97\x8B", "\xE2\x97\x8F", "\xE2\x97\x8B", "\xE2\x97\x8F" },
    { "\xE2\x8F\xB9", "\xE2\x8F\xB8" },
    { "\xE2\x96\xA1", "\xE2\x96\xA0", "\xE2\x96\xA1", "\xE2\x96\xA0" }
};

static void spinner_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_widget_t* s = (gmb_widget_t*)u;
    gmb_cell_color_t fg, bg;
    gmb_control_style_effective(&c->style, c->focused, false, false, &fg, &bg);
    char line[64];
    gmb_spinner_render_line(s, line, (int)sizeof(line));
    gmb_screen_setstring(buf, c->x, c->y, line, fg, bg);
}
static bool spinner_click(void* u, gmb_control_t* c, int x, int y)
{
    /* Click: meta' destra = frame successivo, meta' sinistra = precedente. */
    (void)y;
    gmb_widget_t* s = (gmb_widget_t*)u;
    if (!s || s->frames_count <= 0) return false;
    int right = (x - c->x) > c->width / 2;
    int idx = (int)s->ival;
    s->ival = right ? (idx + 1) % s->frames_count
                    : (idx + s->frames_count - 1) % s->frames_count;
    return true;
}
void gmb_spinner_init(gmb_spinner_t* sp, int x, int y, int w)
{
    gmb_widget_t* s = (gmb_widget_t*)sp;
    memset(s, 0, sizeof(*s));
    gmb_control_init(&s->base, "", x, y, w, 1, spinner_draw, NULL);
    s->base.draw_px_fn = gmb_px_renderer_get(GMB_CK_SPINNER);
    s->base.handle_click_fn = spinner_click;
    s->base.ctrl_kind = GMB_CK_SPINNER;
    s->base.user = s;
    s->kind = W_SPINNER;
    gmb_spinner_set_preset(sp, "line");
}
void gmb_spinner_set_preset(gmb_spinner_t* sp, const char* name)
{
    gmb_widget_t* s = (gmb_widget_t*)sp;
    s->frames_count = 0;
    s->ival = 0;
    if (!name || !*name) return;
    /* Lista di frame esplicita "a|b|c" (usata dal BASIC: SPINNER ..., "1|2|3" / CONTROL SET preset). */
    if (strchr(name, '|'))
    {
        char tmp[512]; snprintf(tmp, sizeof(tmp), "%s", name);
        char* save = NULL;
        for (char* t = strtok_r(tmp, "|", &save); t && s->frames_count < 12; t = strtok_r(NULL, "|", &save))
        {
            while (*t == ' ') t++;
            snprintf(s->frames[s->frames_count], 16, "%s", t);
            s->frames_count++;
        }
        if (s->frames_count == 0) { snprintf(s->frames[0], 16, "%s", name); s->frames_count = 1; }
        return;
    }
    if (strcmp(name, "line") == 0)
    {
        const char* p[] = { "|", "/", "-", "\\" };
        for (int i = 0; i < 4; i++) snprintf(s->frames[i], 16, "%s", p[i]);
        s->frames_count = 4;
    }
    else if (strcmp(name, "dots") == 0)
    {
        const char* p[] = { "\xE2\x97\x8B", "\xE2\x97\x8F", "\xE2\x97\x8B", "\xE2\x97\x8F", "\xE2\x97\x8B", "\xE2\x97\x8F" };
        for (int i = 0; i < 6; i++) snprintf(s->frames[i], 16, "%s", p[i]);
        s->frames_count = 6;
    }
    else if (strcmp(name, "clock") == 0)
    {
        const char* p[] = { "\xE2\x8F\xB9", "\xE2\x8F\xB8" };
        for (int i = 0; i < 2; i++) snprintf(s->frames[i], 16, "%s", p[i]);
        s->frames_count = 2;
    }
    else
    {
        const char* p[] = { "\xE2\x96\xA1", "\xE2\x96\xA0", "\xE2\x96\xA1", "\xE2\x96\xA0" };
        for (int i = 0; i < 4; i++) snprintf(s->frames[i], 16, "%s", p[i]);
        s->frames_count = 4;
    }
}
void gmb_spinner_tick(gmb_spinner_t* sp)
{
    gmb_widget_t* s = (gmb_widget_t*)sp;
    if (s->frames_count == 0) return;
    s->ival = (int)s->ival + 1;
    if ((int)s->ival >= s->frames_count) s->ival = 0;
}
int gmb_spinner_render_line(gmb_spinner_t* s, char* out, int cap)
{
    if (s->frames_count == 0) { out[0] = '\0'; return 0; }
    int idx = (int)s->ival;
    if (idx < 0 || idx >= s->frames_count) idx = 0;
    return snprintf(out, (size_t)cap, "%s", s->frames[idx]);
}
