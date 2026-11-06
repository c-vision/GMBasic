/* ============================================================================
 * gmb_tui_form.c — Form.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void formw_draw(void* u, gmb_control_t* c, gmb_screen_buffer_t* buf)
{
    gmb_form_widget_t* f = (gmb_form_widget_t*)u;
    gmb_cell_color_t fg, bg, ef, eb;
    gmb_control_style_effective(&c->style, c->focused, c->has_error, false, &fg, &bg);
    gmb_control_style_effective(&c->style, false, false, true, &ef, &eb);
    /* Grafica (way 3): ogni campo come rettangolo + testo. Stesso controllo. */
    gmb_screen_sink_t* sk = gmb_screen_get_sink(buf);
    if (sk && sk->is_gfx)
    {
        for (int i = 0; i < f->fields_count && i < c->height; i++)
        {
            char line[256];
            gmb_form_widget_render_line(f, i, line, (int)sizeof(line));
            gmb_cell_color_t lf = f->fields[i].editing ? ef : fg;
            gmb_cell_color_t lb = f->fields[i].editing ? eb : bg;
            gmb_surface_rounded_rect(buf, c->x, c->y + i, c->width, 1, 2, lf, lb, 1);
            gmb_gfx_draw_text(line, sk->origin_x + c->x * sk->cell_w,
                              sk->origin_y + (c->y + i) * sk->cell_h + sk->cell_h / 4, 0,
                              (int)(0x40000000u | ((unsigned)lf.r << 16) | ((unsigned)lf.g << 8) | lf.b));
        }
        return;
    }
    for (int i = 0; i < f->fields_count && i < c->height; i++)
    {
        char line[256];
        gmb_form_widget_render_line(f, i, line, (int)sizeof(line));
        if (f->fields[i].editing) gmb_screen_setstring(buf, c->x, c->y + i, line, ef, eb);
        else gmb_screen_setstring(buf, c->x, c->y + i, line, fg, bg);
    }
}
static bool formw_key(void* u, gmb_control_t* c, int key)
{
    gmb_form_widget_t* f = (gmb_form_widget_t*)u;
    if (!c->enabled) return false;
    if (f->fields_count == 0) return false;
    gmb_form_field_w_t* field = &f->fields[f->selected_index];
    if (field->editing)
    {
        switch (key)
        {
            case GMB_K_ENTER: case GMB_K_ESC:
                field->editing = false;
                return true;
            case GMB_K_BS:
                {
                    int vl = (int)strlen(field->value);
                    if (vl > 0) field->value[vl - 1] = '\0';
                    return true;
                }
            default:
                if (key >= 32 && key <= 126)
                {
                    int vl = (int)strlen(field->value);
                    if (vl < 511) { field->value[vl] = (char)key; field->value[vl + 1] = '\0'; }
                    return true;
                }
                break;
        }
    }
    else
    {
        switch (key)
        {
            case GMB_K_ENTER:
                field->editing = true;
                return true;
            case GMB_K_UP:
                if (f->selected_index > 0) f->selected_index--;
                return true;
            case GMB_K_DOWN:
                if (f->selected_index < f->fields_count - 1) f->selected_index++;
                return true;
        }
    }
    return false;
}
static bool formw_click(void* u, gmb_control_t* c, int x, int y)
{
    gmb_form_widget_t* f = (gmb_form_widget_t*)u;
    if (!c->enabled) return false;
    gmb_rect_t r = gmb_control_rect(c);
    if (!rect_contains(r, x, y)) return false;
    c->focused = true;
    int idx = y - c->y;
    if (idx >= 0 && idx < f->fields_count)
    {
        if (f->fields[f->selected_index].editing) f->fields[f->selected_index].editing = false;
        f->selected_index = idx;
        f->fields[idx].editing = true;
    }
    return true;
}
void gmb_form_widget_init(gmb_form_widget_t* f, int x, int y, int w, int h)
{
    memset(f, 0, sizeof(*f));
    gmb_control_init(&f->ctrl, "", x, y, w, h, formw_draw, formw_key);
    f->ctrl.handle_click_fn = formw_click;
    f->ctrl.user = f;
}
void gmb_form_widget_add_field(gmb_form_widget_t* f, const char* label, const char* value)
{
    if (f->fields_count >= 16) return;
    gmb_form_field_w_t* field = &f->fields[f->fields_count++];
    snprintf(field->label, sizeof(field->label), "%s", label ? label : "");
    snprintf(field->value, sizeof(field->value), "%s", value ? value : "");
}
const char* gmb_form_widget_get_value(gmb_form_widget_t* f, int idx)
{
    if (idx >= 0 && idx < f->fields_count) return f->fields[idx].value;
    return NULL;
}
int gmb_form_widget_render_line(gmb_form_widget_t* f, int idx, char* out, int cap)
{
    if (idx < 0 || idx >= f->fields_count)
    {
        for (int i = 0; i < f->ctrl.width && i < cap - 1; i++) out[i] = ' ';
        out[f->ctrl.width < cap - 1 ? f->ctrl.width : cap - 1] = '\0';
        return f->ctrl.width;
    }
    const char* marker = idx == f->selected_index ? ">" : " ";
    const char* edit_mark = f->fields[idx].editing ? "_" : " ";
    int o = snprintf(out, cap, "%s %s: %s%s", marker, f->fields[idx].label, f->fields[idx].value, edit_mark);
    if (o > f->ctrl.width) o = f->ctrl.width;
    else for (int i = o; i < f->ctrl.width && i < cap - 1; i++) out[o++] = ' ';
    out[o] = '\0';
    return o;
}
