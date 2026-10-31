#include "vb_gmb.h"
#include "vb_code.h"
#include "vb_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int type_is(const VbControl *c, const char *suffix)
{
    if (!c || !c->type) return 0;
    const char *dot = strrchr(c->type, '.');
    const char *name = dot ? dot + 1 : c->type;
    return vb_streq_ci(name, suffix);
}

static int twips_to_px(int twips)
{
    if (twips <= 0) return 0;
    return (twips + 7) / 15;
}

static int control_px(const VbControl *c, const char *key, int defval)
{
    return twips_to_px(vb_control_get_int(c, key, defval));
}

static void ident(char *out, size_t cap, const char *name, const char *fallback)
{
    const char *src = (name && *name) ? name : fallback;
    size_t n = 0;
    if (!src || !*src) src = "Ctl";
    if (isdigit((unsigned char)src[0]) && n + 1 < cap) out[n++] = '_';
    for (; *src && n + 1 < cap; src++) {
        unsigned char ch = (unsigned char)*src;
        out[n++] = (isalnum(ch) || ch == '_') ? (char)ch : '_';
    }
    out[n] = '\0';
}

static void append_escaped(VbBuf *out, const char *text)
{
    for (const char *p = text ? text : ""; *p; p++) {
        if (*p == '"') vb_buf_appendchar(out, '\'');
        else vb_buf_appendchar(out, *p);
    }
}

static char *list_pipe(const char *raw)
{
    if (!raw) return vb_strdup("");
    VbBuf b;
    vb_buf_init(&b);
    const char *p = raw;
    while (*p) {
        while (*p == '\r' || *p == '\n') p++;
        const char *start = p;
        while (*p && *p != '\r' && *p != '\n') p++;
        if (p > start) {
            if (b.len) vb_buf_appendchar(&b, '|');
            vb_buf_appendn(&b, start, (size_t)(p - start));
        }
    }
    if (!b.data) vb_buf_append(&b, "");
    return vb_buf_take(&b);
}

static void append_caption(VbBuf *out, const VbControl *c)
{
    const char *caption = vb_control_get(c, "Caption");
    if (!caption) caption = vb_control_get(c, "Text");
    char *text = vb_value_unquote(caption ? caption : "");
    append_escaped(out, text);
    free(text);
}

static int property_emitted_explicitly(const char *key)
{
    static const char *const keys[] = {
        "Name", "Left", "Top", "Width", "Height", "Visible", "Enabled", "TabIndex",
        "TabStop", "Tag", "ToolTipText", "BackColor", "ForeColor", "List", "ListIndex",
        "Caption", "Text", "Value", NULL
    };
    for (int i = 0; keys[i]; i++) if (vb_streq_ci(key, keys[i])) return 1;
    return 0;
}

static void emit_generic_properties(VbBuf *out, const VbControl *c, const char *name)
{
    for (int i = 0; i < c->nprops; i++) {
        const char *key = c->props[i].key;
        const char *raw = c->props[i].value;
        if (!key || !raw || property_emitted_explicitly(key)) continue;
        if (vb_prop_is_boolean(key)) {
            vb_buf_appendf(out, "  CONTROL SET \"%s\", \"%s\", %d, \"\"\n",
                           name, key, vb_control_get_int(c, key, 0) ? 1 : 0);
        } else if (vb_value_is_quoted(raw) || vb_prop_is_string(key)) {
            char *value = vb_value_unquote(raw);
            vb_buf_appendf(out, "  CONTROL SET \"%s\", \"%s\", 0, \"", name, key);
            append_escaped(out, value);
            vb_buf_append(out, "\"\n");
            free(value);
        } else {
            vb_buf_appendf(out, "  CONTROL SET \"%s\", \"%s\", %s, \"\"\n",
                           name, key, raw);
        }
    }
}

static void emit_control_properties(VbBuf *out, const VbControl *c, const char *name)
{
    const char *tag = vb_control_get(c, "Tag");
    const char *tooltip = vb_control_get(c, "ToolTipText");
    int visible = vb_control_get_int(c, "Visible", -1) != 0;
    int enabled = vb_control_get_int(c, "Enabled", -1) != 0;
    int tabIndex = vb_control_get_int(c, "TabIndex", -1);
    int tabStop = vb_control_get_int(c, "TabStop", -1) != 0;
    vb_buf_appendf(out, "  CONTROL SET \"%s\", \"name\", 0, \"%s\"\n", name, name);
    vb_buf_appendf(out, "  CONTROL SET \"%s\", \"visible\", %d, \"\"\n", name, visible);
    vb_buf_appendf(out, "  CONTROL SET \"%s\", \"enabled\", %d, \"\"\n", name, enabled);
    if (tabIndex >= 0) vb_buf_appendf(out, "  CONTROL SET \"%s\", \"tabindex\", %d, \"\"\n", name, tabIndex);
    vb_buf_appendf(out, "  CONTROL SET \"%s\", \"tabstop\", %d, \"\"\n", name, tabStop);
    if (tag && *tag) {
        char *value = vb_value_unquote(tag);
        vb_buf_appendf(out, "  CONTROL SET \"%s\", \"tag\", 0, \"", name);
        append_escaped(out, value);
        vb_buf_append(out, "\"\n");
        free(value);
    }
    if (tooltip && *tooltip) {
        char *value = vb_value_unquote(tooltip);
        vb_buf_appendf(out, "  CONTROL SET \"%s\", \"tooltip\", 0, \"", name);
        append_escaped(out, value);
        vb_buf_append(out, "\"\n");
        free(value);
    }
    {
        const char *back = vb_control_get(c, "BackColor");
        int r, g, b;
        if (back && vb_color_from_value(back, &r, &g, &b))
            vb_buf_appendf(out, "  CONTROL SET \"%s\", \"backcolor\", 0, \"#%02X%02X%02X\"\n", name, r, g, b);
    }
    {
        const char *fore = vb_control_get(c, "ForeColor");
        int r, g, b;
        if (fore && vb_color_from_value(fore, &r, &g, &b))
            vb_buf_appendf(out, "  CONTROL SET \"%s\", \"forecolor\", 0, \"#%02X%02X%02X\"\n", name, r, g, b);
    }
    if (type_is(c, "ListBox") || type_is(c, "ComboBox")) {
        int index = vb_control_get_int(c, "ListIndex", -1);
        if (index >= 0) vb_buf_appendf(out, "  CONTROL SET \"%s\", \"listindex\", %d, \"\"\n", name, index);
    }
    emit_generic_properties(out, c, name);
}

static int has_event_stub(const char *name, const char *event, char stubs[][96], int nstubs)
{
    char expected[96];
    snprintf(expected, sizeof(expected), "%s_%s", name, event);
    for (int i = 0; i < nstubs; i++)
        if (vb_streq_ci(stubs[i], expected)) return 1;
    return 0;
}

static void add_stub(char stubs[][96], int *nstubs, const char *name, const char *event)
{
    if (has_event_stub(name, event, stubs, *nstubs)) return;
    if (*nstubs >= 64) return;
    snprintf(stubs[*nstubs], 96, "%s_%s", name, event);
    (*nstubs)++;
}

static void emit_control(VbBuf *out, const VbControl *c, int ox, int oy,
                         char stubs[][96], int *nstubs, char vars[][96], int *nvars,
                         int depth)
{
    if (!c) return;
    char name[64];
    ident(name, sizeof(name), c->name, "Ctl");
    int x = ox + control_px(c, "Left", 0);
    int y = oy + control_px(c, "Top", 0);
    int w = control_px(c, "Width", 0);
    int h = control_px(c, "Height", 0);

    if (type_is(c, "Frame") || type_is(c, "PictureBox")) {
        vb_buf_appendf(out, "' %s %s: contenitore, figli appiattiti\n", c->type, name);
        for (int i = 0; i < c->nchildren; i++)
            emit_control(out, c->children[i], x, y, stubs, nstubs, vars, nvars, depth + 1);
        return;
    }

    if (type_is(c, "CommandButton")) {
        vb_buf_appendf(out, "  BUTTON %d, %d, %d, %d, \"", x, y, w, h);
        append_caption(out, c);
        char event[96];
        snprintf(event, sizeof(event), "%s_Click", name);
        vb_buf_appendf(out, "\" ON CLICK %s\n", event);
        emit_control_properties(out, c, name);
        add_stub(stubs, nstubs, name, "Click");
        return;
    }
    if (type_is(c, "Label")) {
        vb_buf_appendf(out, "  LABEL %d, %d, %d, \"", x, y, w > 0 ? w : 80);
        append_caption(out, c);
        vb_buf_append(out, "\"\n");
        emit_control_properties(out, c, name);
        return;
    }
    if (type_is(c, "TextBox")) {
        int seen = 0;
        for (int i = 0; i < *nvars; i++) if (vb_streq(vars[i], name)) seen = 1;
        if (!seen && *nvars < 64) snprintf(vars[(*nvars)++], 96, "DIM %s AS STRING", name);
        const char *text = vb_control_get(c, "Text");
        if (text && *text) {
            char *value = vb_value_unquote(text);
            vb_buf_appendf(out, "  %s = \"", name);
            append_escaped(out, value);
            vb_buf_append(out, "\"\n");
            free(value);
        }
        vb_buf_appendf(out, "  TEXTBOX %d, %d, %d, %d INTO %s\n", x, y, w > 0 ? w : 120, h > 0 ? h : 24, name);
        emit_control_properties(out, c, name);
        add_stub(stubs, nstubs, name, "Change");
        return;
    }
    if (type_is(c, "CheckBox")) {
        int seen = 0;
        for (int i = 0; i < *nvars; i++) if (vb_streq(vars[i], name)) seen = 1;
        if (!seen && *nvars < 64) snprintf(vars[(*nvars)++], 96, "DIM %s AS INTEGER", name);
        vb_buf_appendf(out, "  %s = %d\n", name, vb_control_get_int(c, "Value", 0) ? 1 : 0);
        vb_buf_appendf(out, "  CHECKBOX %d, %d, %d, \"", x, y, w > 0 ? w : 120);
        append_caption(out, c);
        vb_buf_appendf(out, "\" INTO %s\n", name);
        emit_control_properties(out, c, name);
        add_stub(stubs, nstubs, name, "Click");
        return;
    }
    if (type_is(c, "ListBox")) {
        int seen = 0;
        for (int i = 0; i < *nvars; i++) if (vb_streq(vars[i], name)) seen = 1;
        if (!seen && *nvars < 64) snprintf(vars[(*nvars)++], 96, "DIM %s AS INTEGER", name);
        char *raw_items = vb_value_unquote(vb_control_get(c, "List"));
        char *items = list_pipe(raw_items);
        free(raw_items);
        vb_buf_appendf(out, "  %s = 0\n", name);
        vb_buf_appendf(out, "  LISTBOX %d, %d, %d, %d, \"", x, y, w > 0 ? w : 140, h > 0 ? h : 70);
        append_escaped(out, items);
        vb_buf_appendf(out, "\" INTO %s\n", name);
        emit_control_properties(out, c, name);
        free(items);
        add_stub(stubs, nstubs, name, "Click");
        return;
    }
    if (type_is(c, "ComboBox")) {
        int seen = 0;
        for (int i = 0; i < *nvars; i++) if (vb_streq(vars[i], name)) seen = 1;
        if (!seen && *nvars < 64) snprintf(vars[(*nvars)++], 96, "DIM %s AS STRING", name);
        char *raw_items = vb_value_unquote(vb_control_get(c, "List"));
        char *items = list_pipe(raw_items);
        free(raw_items);
        vb_buf_appendf(out, "  %s = \"\"\n", name);
        vb_buf_appendf(out, "  COMBOBOX %d, %d, %d, \"", x, y, w > 0 ? w : 140);
        append_escaped(out, items);
        vb_buf_appendf(out, "\" INTO %s\n", name);
        emit_control_properties(out, c, name);
        free(items);
        add_stub(stubs, nstubs, name, "Change");
        return;
    }
    if (type_is(c, "OptionButton")) {
        vb_buf_appendf(out, "' RADIO %s non esportato: GMBasic RADIO raggruppa piu' voci in un widget\n", name);
        return;
    }
    if (type_is(c, "Timer") || type_is(c, "Line") || type_is(c, "Shape") ||
        type_is(c, "Image") || type_is(c, "HScrollBar") || type_is(c, "VScrollBar") ||
        type_is(c, "Menu")) {
        vb_buf_appendf(out, "' %s %s non esportato (non supportato dal mapping)\n", c->type, name);
        return;
    }

    vb_buf_appendf(out, "' %s %s non esportato (tipo non supportato)\n", c->type ? c->type : "Control", name);
}


static VbControl *find_control(const VbForm *form, const char *name)
{
    if (!form || !name) return NULL;
    return vb_form_find_control((VbForm *)form, name);
}

static int control_is_textbox(const VbControl *c)
{
    return type_is(c, "TextBox");
}

static void append_rhs(VbBuf *out, const char *rhs, const VbForm *form)
{
    const char *p = rhs;
    while (p && *p) {
        if (*p == '&') {
            vb_buf_appendchar(out, '+');
            p++;
            continue;
        }
        int matched = 0;
        for (int i = 0; form && form->root && i < form->root->nchildren; i++) {
            VbControl *c = form->root->children[i];
            if (!control_is_textbox(c) || !c->name) continue;
            size_t n = strlen(c->name);
            if (strncasecmp(p, c->name, n) == 0 && strncasecmp(p + n, ".Text", 5) == 0) {
                vb_buf_append(out, c->name);
                p += n + 5;
                matched = 1;
                break;
            }
        }
        if (!matched) {
            vb_buf_appendchar(out, *p++);
        }
    }
}

static int split_assignment(const char *line, char *left, size_t lcap, const char **right)
{
    const char *eq = strchr(line, '=');
    if (!eq || eq == line) return 0;
    size_t n = (size_t)(eq - line);
    while (n > 0 && isspace((unsigned char)line[n - 1])) n--;
    if (n == 0 || n >= lcap) return 0;
    memcpy(left, line, n);
    left[n] = '\0';
    *right = eq + 1;
    while (**right == ' ' || **right == '\t') (*right)++;
    return 1;
}

static int translate_line(VbBuf *out, const char *raw, const VbForm *form)
{
    char line[1024];
    snprintf(line, sizeof(line), "%s", raw ? raw : "");
    char *text = vb_trim(line);
    if (!*text) { vb_buf_appendchar(out, '\n'); return 1; }
    if (*text == '\'') { vb_buf_appendf(out, "  %s\n", text); return 1; }
    if (vb_starts_ci(text, "Unload ")) {
        vb_buf_append(out, "  WINDOW CLOSE\n");
        return 1;
    }
    if (vb_starts_ci(text, "MsgBox ")) {
        const char *expr = text + 7;
        while (*expr == ' ' || *expr == '(') expr++;
        size_t n = strlen(expr);
        while (n > 0 && (isspace((unsigned char)expr[n - 1]) || expr[n - 1] == ')')) n--;
        vb_buf_append(out, "  ALERTBOX 40, 40, 320, 90, ");
        VbBuf clean;
        vb_buf_init(&clean);
        vb_buf_appendn(&clean, expr, n);
        append_rhs(out, clean.data ? clean.data : "", form);
        vb_buf_append(out, "\n");
        vb_buf_free(&clean);
        return 1;
    }
    if (vb_starts_ci(text, "End") && (text[3] == '\0' || isspace((unsigned char)text[3]))) {
        vb_buf_append(out, "  END\n");
        return 1;
    }

    char left[256];
    const char *right = NULL;
    if (!split_assignment(text, left, sizeof(left), &right)) {
        vb_buf_appendf(out, "  ' TODO: %s\n", text);
        return 1;
    }
    char *dot = strrchr(left, '.');
    if (!dot) {
        vb_buf_appendf(out, "  %s = ", left);
        append_rhs(out, right, form);
        vb_buf_appendchar(out, '\n');
        return 1;
    }
    *dot = '\0';
    const char *prop = dot + 1;
    VbControl *ctl = find_control(form, left);
    if (!ctl) {
        vb_buf_appendf(out, "  ' TODO: %s.%s = %s\n", left, prop, right);
        return 1;
    }
    if (vb_streq_ci(prop, "SetFocus")) {
        vb_buf_appendf(out, "  CONTROL FOCUS \"%s\"\n", ctl->name);
        return 1;
    }
    if (vb_streq_ci(prop, "Text") && control_is_textbox(ctl)) {
        vb_buf_appendf(out, "  %s = ", ctl->name);
        append_rhs(out, right, form);
        vb_buf_appendchar(out, '\n');
        return 1;
    }
    if (vb_streq_ci(prop, "Caption") || vb_streq_ci(prop, "Text") || vb_streq_ci(prop, "ToolTip")) {
        const char *key = vb_streq_ci(prop, "Caption") ? "caption" :
                          (vb_streq_ci(prop, "ToolTip") ? "tooltip" : "text");
        vb_buf_appendf(out, "  CONTROL SET \"%s\", \"%s\", 0, ", ctl->name, key);
        append_rhs(out, right, form);
        vb_buf_appendchar(out, '\n');
        return 1;
    }
    if (vb_streq_ci(prop, "Value") || vb_streq_ci(prop, "ListIndex") ||
        vb_streq_ci(prop, "Enabled") || vb_streq_ci(prop, "Visible")) {
        const char *key = vb_streq_ci(prop, "ListIndex") ? "value" : prop;
        vb_buf_appendf(out, "  CONTROL SET \"%s\", \"%s\", ", ctl->name, key);
        if (vb_streq_ci(right, "True")) vb_buf_appendchar(out, '1');
        else if (vb_streq_ci(right, "False")) vb_buf_appendchar(out, '0');
        else append_rhs(out, right, form);
        vb_buf_append(out, ", \"\"\n");
        return 1;
    }
    vb_buf_appendf(out, "  ' TODO: %s.%s = %s\n", left, prop, right);
    return 1;
}

static void emit_handler(VbBuf *out, const VbHandler *handler, const VbForm *form)
{
    vb_buf_appendf(out, "SUB %s()\n", handler->name ? handler->name : "Handler");
    if (handler->body && *handler->body) {
        char *copy = vb_strdup(handler->body);
        char *save = NULL;
        for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            translate_line(out, line, form);
        }
        free(copy);
    }
    vb_buf_append(out, "END SUB\n");
}

char *vb_gmb_form_export(const VbForm *form)
{
    if (!form) return NULL;
    char form_name[64];
    ident(form_name, sizeof(form_name), vb_form_name(form), "Form1");

    VbBuf out;
    vb_buf_init(&out);
    vb_buf_appendf(&out, "' Generato da GMBasicIDE da %s\n", form->path ? form->path : form_name);
    vb_buf_append(&out, "' Layout: twips -> pixel (15 twips/pixel). Handler semplici tradotti; TODO espliciti.\n");
    vb_buf_append(&out, "IMPORT WINDOWS\n\n");

    char stubs[64][96];
    char vars[64][96];
    int nstubs = 0, nvars = 0;

    VbBuf body;
    vb_buf_init(&body);
    int width = twips_to_px(vb_form_client_width(form));
    int height = twips_to_px(vb_form_client_height(form));
    if (width < 320) width = 320;
    if (height < 240) height = 240;
    vb_buf_appendf(&body, "SUB %s_Show()\n", form_name);
    vb_buf_appendf(&body, "  SCREEN 12, %d, %d, 32\n", width, height);
    vb_buf_append(&body, "  WINDOW THEME \"vb6\"\n");
    vb_buf_append(&body, "  WINDOW SHOW\n");
    if (form->root) {
        for (int i = 0; i < form->root->nchildren; i++)
            emit_control(&body, form->root->children[i], 0, 0, stubs, &nstubs, vars, &nvars, 0);
    }
    VbHandlerList handlers;
    vb_handler_list_init(&handlers);
    vb_code_handlers(form->code ? form->code : "", &handlers);
    for (int i = 0; i < handlers.count; i++) {
        char expected[96];
        snprintf(expected, sizeof(expected), "%s_Load", form_name);
        if (vb_streq_ci(handlers.items[i].name, expected) ||
            vb_streq_ci(handlers.items[i].name, "Form_Load")) {
            vb_buf_appendf(&body, "  %s\n", handlers.items[i].name);
            break;
        }
    }
    vb_buf_append(&body, "  DO WHILE WINDOWCLOSED() = 0\n");
    vb_buf_append(&body, "    WINDOW PUMP 16\n");
    vb_buf_append(&body, "  LOOP\n");
    vb_buf_append(&body, "END SUB\n");

    for (int i = 0; i < nvars; i++) vb_buf_appendf(&out, "%s\n", vars[i]);
    if (nvars) vb_buf_appendchar(&out, '\n');
    vb_buf_append(&out, body.data ? body.data : "");
    int emitted[64] = {0};
    for (int i = 0; i < handlers.count; i++) {
        vb_buf_appendchar(&out, '\n');
        emit_handler(&out, &handlers.items[i], form);
        for (int j = 0; j < nstubs; j++) {
            if (vb_streq_ci(handlers.items[i].name, stubs[j])) emitted[j] = 1;
        }
    }
    for (int i = 0; i < nstubs; i++) {
        if (!emitted[i]) vb_buf_appendf(&out, "\nSUB %s()\nEND SUB\n", stubs[i]);
    }
    vb_handler_list_free(&handlers);
    vb_buf_free(&body);

    char *form_caption = vb_value_unquote(vb_form_caption(form));
    vb_buf_appendf(&out, "\n' Form: %s\n", form_caption);
    free(form_caption);
    return vb_buf_take(&out);
}
