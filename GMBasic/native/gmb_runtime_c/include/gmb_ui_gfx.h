/* gmb_ui_gfx.h — interfaccia UNICA verso la PAGINA GRAFICA (modulo UI).
 * Il core NON referenzia piu' gmb_gfx_*: chiede alla tabella, che il modulo grafico
 * installa al caricamento. In headless la tabella e' vuota => pagina grafica assente
 * (stato reale; il core usa la IConsole/ANSI). Nessuno stub. */
#ifndef GMB_UI_GFX_H
#define GMB_UI_GFX_H

typedef struct gmb_ui_gfx_iface {
    int  (*screen_on)(void);
    void (*print)(const char* text);
    void (*cls)(int color);
    void (*locate)(int col, int row);
    void (*color)(int fg, int bg);
    int  (*present_video_is_real)(void);
    void (*present_tick)(void);
    int  (*present_should_quit)(void);
    int  (*input_poll_key)(char* out, int cap);
    int  (*screen_width)(void);
    int  (*screen_height)(void);
    void (*browse_draw_px)(void* browse, void* buffer);
    void (*window_set_visible)(int on);
    void (*draw_text)(const char*, int, int, int, int);
    int  (*text_height)(void);
    int  (*text_width)(const char*);
    void (*fill_rounded_rect)(int,int,int,int,int,unsigned int);
    void (*stroke_rounded_rect)(int,int,int,int,int,unsigned int);
    void (*gradient_rect)(int,int,int,int,unsigned int,unsigned int,int);
    void (*fill_rounded_gradient)(int,int,int,int,int,unsigned int,unsigned int);
    void (*drop_shadow)(int,int,int,int,int,unsigned int,int);
    void (*fill_circle)(int,int,int,unsigned int);
    void (*line)(int,int,int,int,int,const char*);
    int  (*input_poll_mouse)(int*,int*,int*,int*,int*);
    void (*surface_autoattach)(void* buffer);
    int  (*tray_poll)(void);
    void (*winstmt_render_all)(void);
    const char* (*font_path)(void);
    int  (*font_size)(void);
    float (*render_scale)(void);
    void (*screen)(int,int,int,int);
    int  (*text_end_x)(const char*,int,int);
    void (*render_cells_at)(const int*, const unsigned int*, const unsigned int*, int, int, int, int, int, int);
    int  (*save_screen_png)(const char* path);
    int  (*restore_screen_png)(const char* path);
    void (*init)(void);
    void (*palette)(int index, int r, int g, int b);
    void (*font)(const char* path, int size);
} gmb_ui_gfx_iface_t;

extern gmb_ui_gfx_iface_t gmb_ui_gfx;
void gmb_ui_gfx_install(const gmb_ui_gfx_iface_t* iface);

#define GMB_GFX_SCREEN_ON()          (gmb_ui_gfx.screen_on ? gmb_ui_gfx.screen_on() : 0)
#define GMB_GFX_PRINT(s)             do { if (gmb_ui_gfx.print) gmb_ui_gfx.print(s); } while (0)
#define GMB_GFX_CLS(c)               do { if (gmb_ui_gfx.cls) gmb_ui_gfx.cls(c); } while (0)
#define GMB_GFX_LOCATE(x,y)          do { if (gmb_ui_gfx.locate) gmb_ui_gfx.locate(x,y); } while (0)
#define GMB_GFX_COLOR(f,b)           do { if (gmb_ui_gfx.color) gmb_ui_gfx.color(f,b); } while (0)
#define GMB_GFX_VIDEO_REAL()         (gmb_ui_gfx.present_video_is_real ? gmb_ui_gfx.present_video_is_real() : 0)
#define GMB_GFX_PRESENT_TICK()       do { if (gmb_ui_gfx.present_tick) gmb_ui_gfx.present_tick(); } while (0)
#define GMB_GFX_SHOULD_QUIT()        (gmb_ui_gfx.present_should_quit ? gmb_ui_gfx.present_should_quit() : 0)
#define GMB_UI_MOUSE_POLL(a,b,c,d,e) (gmb_ui_gfx.input_poll_mouse ? gmb_ui_gfx.input_poll_mouse(a,b,c,d,e) : 0)
#define GMB_GFX_POLL_KEY(o,n)        (gmb_ui_gfx.input_poll_key ? gmb_ui_gfx.input_poll_key(o,n) : 0)
#define GMB_GFX_SCREEN_W()   (gmb_ui_gfx.screen_width ? gmb_ui_gfx.screen_width() : 0)
#define GMB_GFX_DRAW_TEXT(t,x,y,a,c) do { if (gmb_ui_gfx.draw_text) gmb_ui_gfx.draw_text(t,x,y,a,c); } while (0)
#define GMB_GFX_TEXT_H() (gmb_ui_gfx.text_height ? gmb_ui_gfx.text_height() : 0)
#define GMB_GFX_TEXT_W(t) (gmb_ui_gfx.text_width ? gmb_ui_gfx.text_width(t) : 0)
#define GMB_GFX_FILL_RRECT(x,y,w,h,r,c) do { if (gmb_ui_gfx.fill_rounded_rect) gmb_ui_gfx.fill_rounded_rect(x,y,w,h,r,c); } while (0)
#define GMB_GFX_STROKE_RRECT(x,y,w,h,r,c) do { if (gmb_ui_gfx.stroke_rounded_rect) gmb_ui_gfx.stroke_rounded_rect(x,y,w,h,r,c); } while (0)
#define GMB_GFX_SURFACE_AUTOATTACH(b) do { if (gmb_ui_gfx.surface_autoattach) gmb_ui_gfx.surface_autoattach(b); } while (0)
#define GMB_UI_TRAY_POLL() (gmb_ui_gfx.tray_poll ? gmb_ui_gfx.tray_poll() : 0)
#define GMB_GFX_DROP_SHADOW(x,y,w,h,o,c,r) do { if (gmb_ui_gfx.drop_shadow) gmb_ui_gfx.drop_shadow(x,y,w,h,o,c,r); } while (0)
#define GMB_GFX_GRADIENT_RECT(x,y,w,h,c1,c2,v) do { if (gmb_ui_gfx.gradient_rect) gmb_ui_gfx.gradient_rect(x,y,w,h,c1,c2,v); } while (0)
#define GMB_GFX_FONT_PATH() (gmb_ui_gfx.font_path ? gmb_ui_gfx.font_path() : "")
#define GMB_GFX_FONT_SIZE() (gmb_ui_gfx.font_size ? gmb_ui_gfx.font_size() : 0)
#define GMB_GFX_RENDER_SCALE() (gmb_ui_gfx.render_scale ? gmb_ui_gfx.render_scale() : 1.0f)
#define GMB_GFX_SCREEN(m,w,h,b) do { if (gmb_ui_gfx.screen) gmb_ui_gfx.screen(m,w,h,b); } while (0)
#define GMB_GFX_RENDER_CELLS_AT(c,f,b,w,h,cw,ch,ox,oy) do { if (gmb_ui_gfx.render_cells_at) gmb_ui_gfx.render_cells_at(c,f,b,w,h,cw,ch,ox,oy); } while (0)
#define GMB_GFX_TEXT_END_X(t,x,y) (gmb_ui_gfx.text_end_x ? gmb_ui_gfx.text_end_x(t,x,y) : 0)
#define GMB_GFX_FILL_RGRADIENT(x,y,w,h,r,c1,c2) do { if (gmb_ui_gfx.fill_rounded_gradient) gmb_ui_gfx.fill_rounded_gradient(x,y,w,h,r,c1,c2); } while (0)
#define GMB_GFX_WINSTM_RENDER_ALL() do { if (gmb_ui_gfx.winstmt_render_all) gmb_ui_gfx.winstmt_render_all(); } while (0)
#define GMB_GFX_LINE(x1,y1,x2,y2,c,s) do { if (gmb_ui_gfx.line) gmb_ui_gfx.line(x1,y1,x2,y2,c,s); } while (0)
#define GMB_GFX_FILL_CIRCLE(cx,cy,d,c) do { if (gmb_ui_gfx.fill_circle) gmb_ui_gfx.fill_circle(cx,cy,d,c); } while (0)
#define GMB_GFX_WINDOW_SET_VISIBLE(v) do { if (gmb_ui_gfx.window_set_visible) gmb_ui_gfx.window_set_visible(v); } while (0)
#define GMB_GFX_SCREEN_H()   (gmb_ui_gfx.screen_height ? gmb_ui_gfx.screen_height() : 0)
#define GMB_GFX_SAVE_PNG(p)          (gmb_ui_gfx.save_screen_png ? gmb_ui_gfx.save_screen_png(p) : 0)
#define GMB_GFX_RESTORE_PNG(p)       (gmb_ui_gfx.restore_screen_png ? gmb_ui_gfx.restore_screen_png(p) : 0)
#define GMB_GFX_INIT()               do { if (gmb_ui_gfx.init) gmb_ui_gfx.init(); } while (0)
#define GMB_GFX_PALETTE(i,r,g,b)     do { if (gmb_ui_gfx.palette) gmb_ui_gfx.palette(i,r,g,b); } while (0)
#define GMB_GFX_FONT(p,s)            do { if (gmb_ui_gfx.font) gmb_ui_gfx.font(p,s); } while (0)

#endif /* GMB_UI_GFX_H */
