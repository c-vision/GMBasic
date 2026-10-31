/* ============================================================================
 * gmb_tui_yaml.h — parser YAML semplificato + config form
 * (specchio ClipperLib/Clipper YamlConfig.cs). Puro, TTY-free.
 * ============================================================================ */
#ifndef GMB_TUI_YAML_H
#define GMB_TUI_YAML_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { GMB_YAML_SCALAR = 0, GMB_YAML_MAP, GMB_YAML_LIST };
enum { GMB_BORDER_NONE = 0, GMB_BORDER_SINGLE, GMB_BORDER_DOUBLE, GMB_BORDER_ROUNDED };

#define GMB_YAML_MAX_NODES 256
#define GMB_YAML_MAX_SCALAR 128

typedef struct gmb_yaml_node {
    int kind;
    char scalar[GMB_YAML_MAX_SCALAR];
    int map_count;
    int map_keys[GMB_YAML_MAX_NODES];      /* indici stringhe chiave nel pool */
    int map_vals[GMB_YAML_MAX_NODES];
    int list_count;
    int list_items[GMB_YAML_MAX_NODES];
} gmb_yaml_node_t;

/* Pool di stringhe per chiavi/scalari del documento */
typedef struct gmb_yaml_doc {
    gmb_yaml_node_t nodes[GMB_YAML_MAX_NODES];
    int node_count;
    char strings[4096];
    int str_count;
    int root;
} gmb_yaml_doc_t;

int gmb_yaml_parse(gmb_yaml_doc_t* doc, const char* yaml);
int gmb_yaml_node_get(gmb_yaml_doc_t* doc, int node, const char* key);
const char* gmb_yaml_scalar(gmb_yaml_doc_t* doc, int node);
int gmb_yaml_as_int(gmb_yaml_doc_t* doc, int node);
double gmb_yaml_as_double(gmb_yaml_doc_t* doc, int node);
bool gmb_yaml_as_bool(gmb_yaml_doc_t* doc, int node);

/* ---- Config model ---- */
typedef struct gmb_window_config {
    int x, y, width, height;
    int border;
    bool shadow;
} gmb_window_config_t;

typedef struct gmb_mouse_config {
    bool enabled, click_focus, drag_windows, scroll_in_grid;
} gmb_mouse_config_t;

typedef struct gmb_grid_column_config {
    char header[64];
    int width;
    char align[16];
    char format[32];
    bool editable;
} gmb_grid_column_config_t;

typedef struct gmb_field_config {
    char type[16];
    char name[64], label[64];
    int x, y;
    int max_len, min_len;
    char case_[8], allowed[64];
    int decimals;
    double min, max;
    char format[32];
    bool century;
    char mask[64];
    int width, height, frozen;
    bool mouse_scroll;
    gmb_grid_column_config_t columns[8];
    int columns_count;
    bool multi_select, searchable;
    char items[16][64];
    int items_count;
} gmb_field_config_t;

typedef struct gmb_form_config {
    char title[64];
    gmb_window_config_t window;
    gmb_field_config_t fields[16];
    int field_count;
    gmb_mouse_config_t mouse;
    bool has_mouse;
} gmb_form_config_t;

int gmb_yaml_form_config_from_node(gmb_yaml_doc_t* doc, int node, gmb_form_config_t* cfg);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_YAML_H */
