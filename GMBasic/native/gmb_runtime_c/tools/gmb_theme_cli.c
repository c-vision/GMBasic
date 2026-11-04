/* gmb_theme_cli.c — snapshot JSON read-only del tema GMBasic per l'IDE.
 * Uso: gmb_theme [--name vb6] [--list] [--json]
 * Non linka il compilatore: espone solo palette e stile pixel. */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <string.h>

static void rgb_json(const char* key, gmb_cell_color_t c, int last)
{
    printf("    \"%s\": \"%02X%02X%02X\"%s\n", key, c.r, c.g, c.b, last ? "" : ",");
}

static const char* names[] = {
    "turbo", "qbasic", "midnight", "nord", "light", "soft", "neon", "glass",
    "dracula", "monokai", "solarized", "retrogreen", "amber", "vb6", NULL
};

int main(int argc, char** argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("uso: gmb_theme [--list] [--name <tema>]\n"
               "  --list        elenca i temi disponibili (JSON)\n"
               "  --name NOME   emette il tema NOME in JSON (default: turbo)\n"
               "  senza argomenti: emette il tema di default in JSON\n");
        return 0;
    }
    const char* name = "turbo";
    int list = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) list = 1;
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) name = argv[++i];
    }
    if (list) {
        printf("[");
        for (int i = 0; names[i]; i++) printf("%s\"%s\"", i ? "," : "", names[i]);
        printf("]\n");
        return 0;
    }

    gmb_ui_set_theme_name(name);
    gmb_theme_t t = gmb_ui_theme();
    gmb_widget_style_t s = gmb_widget_style_current();
    printf("{\n  \"name\": \"%s\",\n  \"colors\": {\n", gmb_ui_theme_name());
    rgb_json("windowBg", t.window_bg, 0);
    rgb_json("titleFocus", t.title_focus, 0);
    rgb_json("titleFg", t.title_fg, 0);
    rgb_json("border", t.border, 0);
    rgb_json("text", t.text, 0);
    rgb_json("dim", t.dim, 0);
    rgb_json("accent", t.accent, 0);
    rgb_json("activeBg", t.active_bg, 0);
    rgb_json("shadow", t.shadow, 0);
    rgb_json("editorBg", t.editor_bg, 0);
    rgb_json("editorFg", t.editor_fg, 1);
    printf("  },\n  \"style\": {\n");
    printf("    \"shape\": %d,\n    \"radius\": %d,\n", s.shape, s.radius);
    printf("    \"fill\": \"%08X\",\n    \"fillHover\": \"%08X\",\n    \"fillPress\": \"%08X\",\n", s.fill, s.fill_hover, s.fill_press);
    printf("    \"border\": \"%08X\",\n    \"borderWidth\": %d,\n    \"text\": \"%08X\",\n", s.border, s.border_w, s.text);
    printf("    \"focusBg\": \"%08X\",\n    \"focusFg\": \"%08X\",\n    \"accent\": \"%08X\",\n", s.focus_bg, s.focus_fg, s.accent);
    printf("    \"shadowOn\": %d,\n    \"gradientOn\": %d,\n", s.shadow_on, s.gradient_on);
    printf("    \"bevel\": %d,\n    \"bevelDepth\": %d,\n", s.bevel, s.bevel_depth);
    printf("    \"bevelLight\": \"%08X\",\n    \"bevelDark\": \"%08X\",\n", s.bevel_light, s.bevel_dark);
    printf("    \"padX\": %d,\n    \"padY\": %d,\n", s.pad_x, s.pad_y);
    printf("    \"fontSize\": %d\n  }\n}\n", s.font_size);
    return 0;
}
