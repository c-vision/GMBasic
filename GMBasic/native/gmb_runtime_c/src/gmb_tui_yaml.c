/* ============================================================================
 * gmb_tui_yaml.c — parser YAML semplificato (porting YamlConfig.cs).
 * ============================================================================ */
#include "gmb_tui_yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct yaml_line {
    int indent;
    char text[GMB_YAML_MAX_SCALAR];
} yaml_line_t;

static int yaml_doc_new_node(gmb_yaml_doc_t* doc, int kind)
{
    if (doc->node_count >= GMB_YAML_MAX_NODES) return -1;
    int id = doc->node_count++;
    memset(&doc->nodes[id], 0, sizeof(doc->nodes[id]));
    doc->nodes[id].kind = kind;
    return id;
}

static int yaml_doc_str(gmb_yaml_doc_t* doc, const char* s)
{
    int len = (int)strlen(s);
    if (doc->str_count + len + 1 >= (int)sizeof(doc->strings)) return -1;
    int id = doc->str_count;
    memcpy(doc->strings + doc->str_count, s, (size_t)len + 1);
    doc->str_count += len + 1;
    return id;
}

static void yaml_unquote(char* s)
{
    int len = (int)strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\'')))
    {
        memmove(s, s + 1, (size_t)len - 2);
        s[len - 2] = '\0';
    }
}

static int yaml_node_scalar(gmb_yaml_doc_t* doc, const char* s)
{
    char buf[GMB_YAML_MAX_SCALAR];
    snprintf(buf, sizeof(buf), "%s", s);
    yaml_unquote(buf);
    int id = yaml_doc_new_node(doc, GMB_YAML_SCALAR);
    if (id < 0) return -1;
    snprintf(doc->nodes[id].scalar, sizeof(doc->nodes[id].scalar), "%s", buf);
    return id;
}

static void yaml_split_key(const char* text, char* key, int key_cap, char* rest, int rest_cap, bool* has_rest)
{
    const char* colon = strchr(text, ':');
    if (!colon)
    {
        snprintf(key, key_cap, "%s", text);
        char* t = key;
        while (*t == ' ') { memmove(t, t + 1, strlen(t)); }
        snprintf(rest, rest_cap, "%s", text);
        char* r = rest;
        while (*r == ' ') { memmove(r, r + 1, strlen(r)); }
        *has_rest = true;
        return;
    }
    int kn = (int)(colon - text);
    int k = 0;
    for (int i = 0; i < kn && k < key_cap - 1; i++)
        if (text[i] != ' ' && text[i] != '\t') key[k++] = text[i];
    key[k] = '\0';
    /* trim a destra */
    while (k > 0 && (key[k - 1] == ' ')) key[--k] = '\0';
    const char* after = colon + 1;
    while (*after == ' ' || *after == '\t') after++;
    if (*after == '\0')
    {
        *has_rest = false;
        rest[0] = '\0';
        return;
    }
    snprintf(rest, rest_cap, "%s", after);
    *has_rest = true;
}

static bool yaml_is_map_start(const char* content)
{
    const char* t = content;
    while (*t == ' ') t++;
    if (*t == '"' || *t == '\'') return false;
    const char* colon = strchr(content, ':');
    if (!colon) return false;
    /* "key: value" oppure "key:" */
    const char* p = colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    return *p == '\0' || (*p != '\0'); /* contiene ": " o termina con ':' */
}

static int yaml_parse_block(gmb_yaml_doc_t* doc, yaml_line_t* lines, int* i, int n, int indent);

static int yaml_parse_map(gmb_yaml_doc_t* doc, yaml_line_t* lines, int* i, int n, int indent)
{
    int id = yaml_doc_new_node(doc, GMB_YAML_MAP);
    if (id < 0) return -1;
    gmb_yaml_node_t* node = &doc->nodes[id];
    while (*i < n && lines[*i].indent == indent && !(strlen(lines[*i].text) >= 2 && strncmp(lines[*i].text, "- ", 2) == 0))
    {
        char key[GMB_YAML_MAX_SCALAR], rest[GMB_YAML_MAX_SCALAR];
        bool has_rest;
        yaml_split_key(lines[*i].text, key, (int)sizeof(key), rest, (int)sizeof(rest), &has_rest);
        if (!has_rest)
        {
            (*i)++;
            if (*i < n && lines[*i].indent > indent)
            {
                int child = yaml_parse_block(doc, lines, i, n, lines[*i].indent);
                if (child < 0) return -1;
                if (node->map_count < GMB_YAML_MAX_NODES)
                {
                    node->map_keys[node->map_count] = yaml_doc_str(doc, key);
                    node->map_vals[node->map_count] = child;
                    node->map_count++;
                }
            }
            else
            {
                if (node->map_count < GMB_YAML_MAX_NODES)
                {
                    node->map_keys[node->map_count] = yaml_doc_str(doc, key);
                    node->map_vals[node->map_count] = yaml_doc_new_node(doc, GMB_YAML_MAP);
                    node->map_count++;
                }
            }
        }
        else
        {
            int child = yaml_node_scalar(doc, rest);
            if (node->map_count < GMB_YAML_MAX_NODES)
            {
                node->map_keys[node->map_count] = yaml_doc_str(doc, key);
                node->map_vals[node->map_count] = child;
                node->map_count++;
            }
            (*i)++;
        }
    }
    return id;
}

static int yaml_parse_list(gmb_yaml_doc_t* doc, yaml_line_t* lines, int* i, int n, int indent)
{
    int id = yaml_doc_new_node(doc, GMB_YAML_LIST);
    if (id < 0) return -1;
    gmb_yaml_node_t* node = &doc->nodes[id];
    while (*i < n && lines[*i].indent == indent && strlen(lines[*i].text) >= 2 && strncmp(lines[*i].text, "- ", 2) == 0)
    {
        const char* content = lines[*i].text + 2;
        while (*content == ' ') content++;
        if (yaml_is_map_start(content))
        {
            char saved[GMB_YAML_MAX_SCALAR];
            snprintf(saved, sizeof(saved), "%s", content);
            lines[*i].indent = indent + 2;
            snprintf(lines[*i].text, sizeof(lines[*i].text), "%s", saved);
            int child = yaml_parse_map(doc, lines, i, n, indent + 2);
            if (child < 0) return -1;
            if (node->list_count < GMB_YAML_MAX_NODES) node->list_items[node->list_count++] = child;
        }
        else
        {
            int child = yaml_node_scalar(doc, content);
            if (node->list_count < GMB_YAML_MAX_NODES) node->list_items[node->list_count++] = child;
            (*i)++;
        }
    }
    return id;
}

static int yaml_parse_block(gmb_yaml_doc_t* doc, yaml_line_t* lines, int* i, int n, int indent)
{
    if (strlen(lines[*i].text) >= 2 && strncmp(lines[*i].text, "- ", 2) == 0)
        return yaml_parse_list(doc, lines, i, n, indent);
    return yaml_parse_map(doc, lines, i, n, indent);
}

int gmb_yaml_parse(gmb_yaml_doc_t* doc, const char* yaml)
{
    memset(doc, 0, sizeof(*doc));
    if (!yaml) return -1;
    yaml_line_t lines[GMB_YAML_MAX_NODES];
    int n = 0;
    char copy[8192];
    snprintf(copy, sizeof(copy), "%s", yaml);
    char* save = NULL;
    for (char* raw = strtok_r(copy, "\n", &save); raw && n < GMB_YAML_MAX_NODES; raw = strtok_r(NULL, "\n", &save))
    {
        /* trim \r */
        int rl = (int)strlen(raw);
        while (rl > 0 && (raw[rl - 1] == '\r' || raw[rl - 1] == '\n')) raw[--rl] = '\0';
        char* t = raw;
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '\0') continue;
        if (*t == '#') continue;
        lines[n].indent = (int)(t - raw);
        snprintf(lines[n].text, sizeof(lines[n].text), "%s", t);
        n++;
    }
    if (n == 0)
    {
        doc->root = yaml_doc_new_node(doc, GMB_YAML_MAP);
        return doc->root >= 0 ? 0 : -1;
    }
    int i = 0;
    doc->root = yaml_parse_block(doc, lines, &i, n, 0);
    return doc->root >= 0 ? 0 : -1;
}

int gmb_yaml_node_get(gmb_yaml_doc_t* doc, int node, const char* key)
{
    if (node < 0 || node >= doc->node_count) return -1;
    gmb_yaml_node_t* nd = &doc->nodes[node];
    if (nd->kind != GMB_YAML_MAP) return -1;
    for (int k = 0; k < nd->map_count; k++)
    {
        const char* kk = doc->strings + nd->map_keys[k];
        if (strcmp(kk, key) == 0) return nd->map_vals[k];
    }
    return -1;
}

const char* gmb_yaml_scalar(gmb_yaml_doc_t* doc, int node)
{
    if (node < 0 || node >= doc->node_count) return "";
    if (doc->nodes[node].kind != GMB_YAML_SCALAR) return "";
    return doc->nodes[node].scalar;
}

int gmb_yaml_as_int(gmb_yaml_doc_t* doc, int node)
{
    const char* s = gmb_yaml_scalar(doc, node);
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end && end != s) return (int)v;
    double d = strtod(s, &end);
    if (end && end != s) return (int)d;
    return 0;
}

double gmb_yaml_as_double(gmb_yaml_doc_t* doc, int node)
{
    const char* s = gmb_yaml_scalar(doc, node);
    char* end = NULL;
    double d = strtod(s, &end);
    if (end && end != s) return d;
    return 0.0;
}

bool gmb_yaml_as_bool(gmb_yaml_doc_t* doc, int node)
{
    const char* s = gmb_yaml_scalar(doc, node);
    return strcmp(s, "true") == 0 || strcmp(s, "True") == 0 || strcmp(s, "TRUE") == 0;
}

/* ===================== Config model ===================== */

static int yaml_parse_border(const char* s)
{
    char t[32];
    snprintf(t, sizeof(t), "%s", s ? s : "");
    for (char* p = t; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    if (strcmp(t, "none") == 0) return GMB_BORDER_NONE;
    if (strcmp(t, "single") == 0) return GMB_BORDER_SINGLE;
    if (strcmp(t, "double") == 0) return GMB_BORDER_DOUBLE;
    return GMB_BORDER_ROUNDED;
}

static void yaml_field_extras(gmb_yaml_doc_t* doc, int node, gmb_field_config_t* f)
{
    int v;
    if ((v = gmb_yaml_node_get(doc, node, "max_len")) >= 0) f->max_len = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "min_len")) >= 0) f->min_len = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "case")) >= 0) snprintf(f->case_, sizeof(f->case_), "%s", gmb_yaml_scalar(doc, v));
    if ((v = gmb_yaml_node_get(doc, node, "allowed")) >= 0) snprintf(f->allowed, sizeof(f->allowed), "%s", gmb_yaml_scalar(doc, v));
    if ((v = gmb_yaml_node_get(doc, node, "decimals")) >= 0) f->decimals = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "min")) >= 0) f->min = gmb_yaml_as_double(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "max")) >= 0) f->max = gmb_yaml_as_double(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "format")) >= 0) snprintf(f->format, sizeof(f->format), "%s", gmb_yaml_scalar(doc, v));
    if ((v = gmb_yaml_node_get(doc, node, "century")) >= 0) f->century = gmb_yaml_as_bool(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "mask")) >= 0) snprintf(f->mask, sizeof(f->mask), "%s", gmb_yaml_scalar(doc, v));
    if ((v = gmb_yaml_node_get(doc, node, "width")) >= 0) f->width = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "height")) >= 0) f->height = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "frozen")) >= 0) f->frozen = gmb_yaml_as_int(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "mouse_scroll")) >= 0) f->mouse_scroll = gmb_yaml_as_bool(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "multi_select")) >= 0) f->multi_select = gmb_yaml_as_bool(doc, v);
    if ((v = gmb_yaml_node_get(doc, node, "searchable")) >= 0) f->searchable = gmb_yaml_as_bool(doc, v);

    /* columns */
    if ((v = gmb_yaml_node_get(doc, node, "columns")) >= 0 && doc->nodes[v].kind == GMB_YAML_LIST)
    {
        gmb_yaml_node_t* cl = &doc->nodes[v];
        for (int k = 0; k < cl->list_count && f->columns_count < 8; k++)
        {
            int cn = cl->list_items[k];
            gmb_grid_column_config_t* col = &f->columns[f->columns_count++];
            int ck;
            if ((ck = gmb_yaml_node_get(doc, cn, "header")) >= 0) snprintf(col->header, sizeof(col->header), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, cn, "width")) >= 0) col->width = gmb_yaml_as_int(doc, ck);
            if ((ck = gmb_yaml_node_get(doc, cn, "align")) >= 0) snprintf(col->align, sizeof(col->align), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, cn, "format")) >= 0) snprintf(col->format, sizeof(col->format), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, cn, "editable")) >= 0) col->editable = gmb_yaml_as_bool(doc, ck);
        }
    }
    /* items */
    if ((v = gmb_yaml_node_get(doc, node, "items")) >= 0 && doc->nodes[v].kind == GMB_YAML_LIST)
    {
        gmb_yaml_node_t* il = &doc->nodes[v];
        for (int k = 0; k < il->list_count && f->items_count < 16; k++)
            snprintf(f->items[f->items_count++], sizeof(f->items[0]), "%s", gmb_yaml_scalar(doc, il->list_items[k]));
    }
}

int gmb_yaml_form_config_from_node(gmb_yaml_doc_t* doc, int node, gmb_form_config_t* cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->window.border = GMB_BORDER_ROUNDED;
    cfg->mouse.enabled = true;
    cfg->mouse.click_focus = true;
    cfg->mouse.scroll_in_grid = true;
    int v;
    if ((v = gmb_yaml_node_get(doc, node, "title")) >= 0)
        snprintf(cfg->title, sizeof(cfg->title), "%s", gmb_yaml_scalar(doc, v));
    if ((v = gmb_yaml_node_get(doc, node, "window")) >= 0)
    {
        int w;
        if ((w = gmb_yaml_node_get(doc, v, "x")) >= 0) cfg->window.x = gmb_yaml_as_int(doc, w);
        if ((w = gmb_yaml_node_get(doc, v, "y")) >= 0) cfg->window.y = gmb_yaml_as_int(doc, w);
        if ((w = gmb_yaml_node_get(doc, v, "width")) >= 0) cfg->window.width = gmb_yaml_as_int(doc, w);
        if ((w = gmb_yaml_node_get(doc, v, "height")) >= 0) cfg->window.height = gmb_yaml_as_int(doc, w);
        if ((w = gmb_yaml_node_get(doc, v, "border")) >= 0) cfg->window.border = yaml_parse_border(gmb_yaml_scalar(doc, w));
        if ((w = gmb_yaml_node_get(doc, v, "shadow")) >= 0) cfg->window.shadow = gmb_yaml_as_bool(doc, w);
    }
    if ((v = gmb_yaml_node_get(doc, node, "fields")) >= 0 && doc->nodes[v].kind == GMB_YAML_LIST)
    {
        gmb_yaml_node_t* fl = &doc->nodes[v];
        for (int k = 0; k < fl->list_count && cfg->field_count < 16; k++)
        {
            int fn = fl->list_items[k];
            gmb_field_config_t* f = &cfg->fields[cfg->field_count];
            int ck;
            if ((ck = gmb_yaml_node_get(doc, fn, "type")) >= 0) snprintf(f->type, sizeof(f->type), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, fn, "name")) >= 0) snprintf(f->name, sizeof(f->name), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, fn, "label")) >= 0) snprintf(f->label, sizeof(f->label), "%s", gmb_yaml_scalar(doc, ck));
            if ((ck = gmb_yaml_node_get(doc, fn, "x")) >= 0) f->x = gmb_yaml_as_int(doc, ck);
            if ((ck = gmb_yaml_node_get(doc, fn, "y")) >= 0) f->y = gmb_yaml_as_int(doc, ck);
            yaml_field_extras(doc, fn, f);
            cfg->field_count++;
        }
    }
    if ((v = gmb_yaml_node_get(doc, node, "mouse")) >= 0)
    {
        cfg->has_mouse = true;
        int m;
        if ((m = gmb_yaml_node_get(doc, v, "enabled")) >= 0) cfg->mouse.enabled = gmb_yaml_as_bool(doc, m);
        if ((m = gmb_yaml_node_get(doc, v, "click_focus")) >= 0) cfg->mouse.click_focus = gmb_yaml_as_bool(doc, m);
        if ((m = gmb_yaml_node_get(doc, v, "drag_windows")) >= 0) cfg->mouse.drag_windows = gmb_yaml_as_bool(doc, m);
        if ((m = gmb_yaml_node_get(doc, v, "scroll_in_grid")) >= 0) cfg->mouse.scroll_in_grid = gmb_yaml_as_bool(doc, m);
    }
    return 0;
}
