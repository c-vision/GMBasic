/* gmb_screen_config.c — parità CommonLib/Screens/ScreenConfig.cs:509 (palette + GetDefault + apply)
 * Port completo, nessun bridge C↔C#.
 */
#include "gmb_screen_config.h"
#include "gmb_graphics.h"
#include "gmb_runtime.h"
#include "gmb_a11y.h"   /* ui.text_scale sul font configurato a startup */
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define PAL16(v) (v)
#define RGB(r,g,b) (r),(g),(b)

/* ---- palette base (ScreenConfig.cs:313-365) ---- */

void gmb_screen_palette_vga16(unsigned char p[16][3])
{
    static const unsigned char d[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
        {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    memcpy(p, d, sizeof(d));
}

void gmb_screen_palette_vga256(unsigned char p[256][3])
{
    unsigned char v[16][3];
    gmb_screen_palette_vga16(v);
    memcpy(p, v, sizeof(v));
    int idx = 16;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
            { p[idx][0] = (unsigned char)(r * 51); p[idx][1] = (unsigned char)(g * 51); p[idx][2] = (unsigned char)(b * 51); idx++; }
    for (int i = 0; i < 24; i++)
    { unsigned char s = (unsigned char)(8 + (int)(i * 10.3)); p[232 + i][0] = s; p[232 + i][1] = s; p[232 + i][2] = s; }
}

void gmb_screen_palette_c64(unsigned char p[16][3])
{
    static const unsigned char d[16][3] = {
        {0,0,0},{255,255,255},{136,0,0},{170,255,238},{204,68,204},{0,204,85},{0,0,170},{238,238,119},
        {221,136,85},{102,68,0},{255,119,119},{51,51,51},{119,119,119},{170,255,102},{0,136,255},{187,187,187}
    };
    memcpy(p, d, sizeof(d));
}

void gmb_screen_palette_c64_crt(unsigned char p[16][3])
{
    static const unsigned char d[16][3] = {
        {0,0,0},{255,255,255},{104,55,43},{112,164,178},{111,61,134},{88,141,67},{53,40,121},{184,199,111},
        {111,79,37},{67,57,0},{154,103,89},{68,68,68},{108,108,108},{154,210,132},{108,94,181},{149,149,149}
    };
    memcpy(p, d, sizeof(d));
}

void gmb_screen_palette_c64_classic(unsigned char p[16][3])
{
    static const unsigned char d[16][3] = {
        {0,0,0},{255,255,255},{139,63,53},{122,192,182},{142,70,180},{108,165,80},{62,53,150},{206,215,123},
        {144,95,47},{101,78,30},{183,109,99},{86,86,86},{128,128,128},{159,213,131},{140,130,230},{172,172,172}
    };
    memcpy(p, d, sizeof(d));
}

/* ---- 21 palette named (ScreenConfig.cs:157-307) ---- */

static const struct { const char* name; unsigned char c[16][3]; } k_named[] = {
    {"Console", {
        {0,0,0},{0,0,170},{0,80,255},{100,180,255},{0,150,150},{0,180,0},{100,255,0},{150,150,0},
        {255,255,0},{255,140,0},{200,0,0},{255,50,80},{200,0,200},{120,0,200},{180,180,180},{255,255,255}
    }},
    {"Solarized", {
        {0,43,54},{7,54,66},{88,110,117},{101,123,131},{131,148,150},{147,161,161},{238,232,213},{253,246,227},
        {181,137,0},{203,75,22},{220,50,47},{211,54,130},{108,113,196},{38,139,210},{42,161,152},{133,153,0}
    }},
    {"Dracula", {
        {40,42,54},{68,71,90},{80,85,110},{98,114,164},{248,248,242},{139,233,253},{80,250,123},{255,184,108},
        {255,121,198},{189,147,249},{255,85,85},{241,250,140},{255,120,120},{200,170,255},{150,255,200},{240,240,230}
    }},
    {"Nord", {
        {46,52,64},{59,66,82},{67,76,94},{76,86,106},{216,222,233},{229,233,240},{236,239,244},
        {143,188,187},{136,192,208},{129,161,193},{94,129,172},{191,97,106},{208,135,112},{235,203,139},{163,190,140},{180,142,173}
    }},
    {"Tokyo Night", {
        {26,27,38},{169,177,214},{65,72,104},{247,118,142},{158,206,106},{224,175,104},{122,162,247},{187,154,247},
        {125,207,255},{192,202,245},{255,158,100},{157,124,216},{26,188,156},{130,200,220},{180,190,240},{220,160,200}
    }},
    {"Gruvbox Dark", {
        {29,32,33},{40,40,40},{50,48,47},{60,56,54},{251,241,199},{235,219,178},{213,196,161},{189,174,147},
        {204,36,29},{152,151,26},{215,153,33},{69,133,136},{177,98,134},{104,157,106},{214,93,14},{146,131,116}
    }},
    {"Catppuccin Mocha", {
        {30,30,46},{24,24,37},{17,17,27},{49,50,68},{69,71,90},{88,91,112},{108,112,134},{198,208,245},
        {165,173,203},{137,180,250},{180,190,254},{166,227,161},{249,226,175},{250,179,135},{243,139,168},{203,166,247}
    }},
    {"One Half Dark", {
        {40,44,52},{92,99,112},{224,108,117},{224,108,117},{152,195,121},{152,195,121},{209,154,102},{209,154,102},
        {97,175,239},{97,175,239},{198,120,221},{198,120,221},{86,182,194},{86,182,194},{220,223,228},{220,223,228}
    }},
    {"Ayu Dark", {
        {15,16,20},{22,24,29},{67,71,85},{32,35,42},{203,204,198},{251,124,119},{147,197,206},{255,177,76},
        {209,205,111},{166,209,137},{230,158,186},{189,132,215},{255,77,100},{242,154,54},{115,119,130},{255,179,91}
    }},
    {"Gotham", {
        {8,18,28},{15,30,45},{22,40,58},{82,98,120},{130,147,162},{215,223,230},{245,247,250},
        {79,142,200},{49,162,174},{63,180,88},{192,181,43},{238,140,42},{221,81,69},{213,108,150},{173,131,183},{27,48,68}
    }},
    {"Tomorrow Night", {
        {29,31,33},{40,42,46},{55,59,65},{150,152,150},{204,102,102},{222,147,95},{240,200,110},{181,189,104},
        {138,190,183},{129,162,190},{178,148,187},{46,52,56},{197,200,198},{134,136,134},{255,198,109},{242,119,122}
    }},
    {"Nightfox", {
        {14,17,23},{10,12,17},{30,35,46},{68,76,97},{192,197,206},{134,140,151},{168,83,96},{210,129,70},
        {218,175,94},{102,180,96},{76,158,176},{96,138,213},{149,122,195},{193,104,140},{72,162,148},{96,175,207}
    }},
    {"Everforest Dark", {
        {43,47,39},{52,57,49},{61,66,56},{73,79,65},{211,198,170},{159,149,130},{163,101,91},{201,134,82},
        {200,165,95},{141,158,100},{121,162,134},{128,153,136},{180,144,132},{190,128,100},{126,121,106},{76,82,65}
    }},
    {"GitHub Dark", {
        {13,17,23},{8,12,18},{33,38,45},{110,119,129},{230,237,243},{160,170,180},{248,81,73},{212,142,57},
        {211,164,61},{63,185,80},{57,164,195},{88,166,255},{163,113,247},{219,111,170},{255,255,255},{47,129,247}
    }},
    {"Kanagawa Wave", {
        {30,31,40},{213,209,195},{200,195,180},{196,84,84},{104,158,93},{207,163,84},{100,148,188},{144,122,169},
        {86,165,186},{228,191,120},{204,141,143},{232,158,113},{138,140,150},{193,195,203},{97,109,150},{125,114,156}
    }},
    {"Rose Pine", {
        {25,23,36},{31,29,46},{38,35,58},{110,106,134},{144,140,170},{224,222,244},{235,111,146},
        {246,193,119},{235,188,186},{38,191,166},{86,148,159},{196,167,231},{33,32,46},{64,61,82},{82,79,103},{186,180,199}
    }},
    {"Nightfly", {
        {23,30,46},{33,44,63},{41,53,73},{112,132,164},{194,204,222},{224,108,117},{222,149,89},{229,209,57},
        {153,205,102},{118,201,214},{130,170,230},{192,128,230},{240,145,165},{244,248,255},{146,230,245},{162,215,113}
    }},
    {"Cobalt2", {
        {0,18,46},{0,12,34},{34,54,81},{100,124,160},{255,246,226},{255,80,61},{255,160,61},{255,200,60},
        {63,255,127},{0,220,255},{80,160,255},{190,120,255},{220,128,255},{255,255,255},{255,183,0},{255,100,100}
    }},
    {"Jellybeans", {
        {18,20,19},{15,16,15},{34,36,33},{104,112,121},{232,226,205},{207,106,76},{204,147,85},{209,165,65},
        {142,165,93},{108,153,160},{94,124,174},{169,117,200},{218,137,174},{248,248,240},{184,210,120},{145,195,200}
    }},
    {"Iceberg Dark", {
        {20,22,31},{26,29,40},{32,36,50},{95,102,122},{196,200,214},{172,177,193},{162,105,112},{195,138,97},
        {194,169,104},{118,158,108},{109,151,155},{102,138,169},{151,131,169},{175,125,130},{35,39,54},{43,47,63}
    }},
    {"Srcery", {
        {24,24,29},{30,30,36},{40,40,47},{117,117,120},{220,215,200},{186,55,42},{225,140,40},{233,211,63},
        {95,159,67},{80,172,170},{71,128,191},{137,109,173},{185,86,140},{245,245,245},{160,160,164},{109,159,222}
    }},
};

int gmb_screen_palette_named(const char* name, unsigned char out[16][3])
{
    if (!name) return 0;
    for (size_t i = 0; i < sizeof(k_named) / sizeof(k_named[0]); i++)
        if (strcasecmp(name, k_named[i].name) == 0) { memcpy(out, k_named[i].c, sizeof(k_named[i].c)); return 1; }
    return 0;
}

int gmb_screen_palette_named_list(char out[][32], int cap)
{
    int n = (int)(sizeof(k_named) / sizeof(k_named[0]));
    if (n > cap) n = cap;
    for (int i = 0; i < n; i++) snprintf(out[i], 32, "%s", k_named[i].name);
    return n;
}

/* ---- GetDefault(ScreenMode) (ScreenConfig.cs:374-455) ---- */

gmb_screen_config_t gmb_screen_config_get_default(gmb_screen_mode_t mode)
{
    gmb_screen_config_t c;
    memset(&c, 0, sizeof(c));
    c.mode = mode;
    c.font_hinting = GMB_FH_NORMAL;
    c.use_logical_size_scaling = true;
    c.use_native_resolution_rendering = false;
    c.window_mode = GMB_WM_WINDOWED;
    switch (mode) {
        case GMB_SM_TEXT:
            c.char_grid_width = 100; c.char_grid_height = 40;
            gmb_screen_palette_named("Console", (void*)c.resolved_palette); c.palette_size = 16;
            c.palette_name = "Console"; c.default_fg_color = 15; c.default_bg_color = 0;
            c.font_path = "Fira Code"; c.font_size = 14;
            c.font_hinting = GMB_FH_NORMAL; c.use_native_resolution_rendering = true;
            break;
        case GMB_SM_C64:
            c.char_grid_width = 80; c.char_grid_height = 50;
            gmb_screen_palette_c64_classic((void*)c.resolved_palette); c.palette_size = 16;
            c.default_fg_color = 14; c.default_bg_color = 6;
            c.font_path = "Sixtyfour.ttf"; c.font_size = 16;   /* C64: Sixtyfour (OFL), non piu' C64 Pro Mono */
            c.font_hinting = GMB_FH_MONOSPACE; c.use_logical_size_scaling = false;
            c.use_native_resolution_rendering = false;
            break;
        case GMB_SM_CGA:
            c.logical_width = 320; c.logical_height = 200;
            gmb_screen_palette_vga16((void*)c.resolved_palette); c.palette_size = 16;
            c.palette_name = "VGA16"; c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        case GMB_SM_EGA:
            c.logical_width = 640; c.logical_height = 350;
            gmb_screen_palette_vga16((void*)c.resolved_palette); c.palette_size = 16;
            c.palette_name = "VGA16"; c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        case GMB_SM_VGA:
            c.logical_width = 640; c.logical_height = 480;
            c.char_grid_width = 80; c.char_grid_height = 30;
            gmb_screen_palette_vga16((void*)c.resolved_palette); c.palette_size = 16;
            c.palette_name = "VGA16"; c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        case GMB_SM_SVGA:
            c.logical_width = 320; c.logical_height = 200;
            gmb_screen_palette_vga256((void*)c.resolved_palette); c.palette_size = 256;
            c.palette_name = "VGA256"; c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        case GMB_SM_XGA:
            c.logical_width = 640; c.logical_height = 480;
            gmb_screen_palette_vga256((void*)c.resolved_palette); c.palette_size = 256;
            c.palette_name = "VGA256"; c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        case GMB_SM_CRT:
            c.logical_width = 320; c.logical_height = 200;
            c.char_grid_width = 40; c.char_grid_height = 25;
            gmb_screen_palette_c64_crt((void*)c.resolved_palette); c.palette_size = 16;
            c.default_fg_color = 15; c.default_bg_color = 0;
            break;
        default:
            break;
    }
    return c;
}

/* ---- apply: porta la config nello stato runtime (grafica + console) ---- */
void gmb_screen_config_apply(const gmb_screen_config_t* cfg)
{
    if (!cfg) return;
    /* palette risolta */
    if (cfg->palette_size > 0) {
        gmb_gfx_init();
        for (int i = 0; i < cfg->palette_size && i < GMB_SCFG_MAX_PALETTE; i++)
            gmb_gfx_palette(i, cfg->resolved_palette[i][0], cfg->resolved_palette[i][1], cfg->resolved_palette[i][2]);
    }
    /* colori default */
    if (cfg->default_fg_color >= 0 || cfg->default_bg_color >= 0)
        gmb_gfx_color(cfg->default_fg_color, cfg->default_bg_color);
    /* dimensione console a caratteri o grafica */
    if (cfg->char_grid_width > 0 && cfg->char_grid_height > 0) {
        gmb_console_set_buffer_size(cfg->char_grid_width, cfg->char_grid_height);
        gmb_console_set_window_size(cfg->char_grid_width, cfg->char_grid_height);
    } else if (cfg->logical_width > 0 && cfg->logical_height > 0) {
        gmb_gfx_screen(-1, cfg->logical_width, cfg->logical_height, 0);
    }
    /* font */
    if (cfg->font_path && cfg->font_size > 0)
        gmb_gfx_font(cfg->font_path, gmb_a11y_scale_px(cfg->font_size));
}
