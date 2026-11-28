/* ============================================================================
 * gmb_graphics.h — Sottosistema grafico software (CommonGraphics → runtime C).
 *
 * Porta della libreria CommonGraphics (SDK C#) nel runtime condiviso
 * native/gmb_runtime_c. Nessuna dipendenza da SDL: rendering software su
 * framebuffer con pagine multiple, palette (VGA16/VGA256), primitive, testo e
 * texture (PNG via zlib). Usato dai backend VM/C (community) e C99 (commerciale).
 *
 * Modello di riferimento (semantica): GraphicsManager di CommonGraphics + servizio
 * SdlGraphicsService (vedi common/CommonGraphics). Coordinate in pixel, origine
 * alto-sinistra. I colori sono indici di palette; il flag 0x40000000 distingue un
 * RGB diretto (0x40000000|rrggbb) da un indice (come ToColorIndex del C#).
 * ============================================================================ */
#ifndef GMB_GRAPHICS_H
#define GMB_GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_GFX_RGB_FLAG 0x40000000

/* ---- Ciclo di vita ---- */
void gmb_gfx_init(void);
extern float g_gfx_scale;
float gmb_gfx_render_scale(void);
void  gmb_gfx_set_render_scale(float sc);
void  gmb_gfx_resize_screen(int w, int h);
void gmb_gfx_shutdown(void);

/* 2.6(r): null-mode grafico (env GMB_GFX_NULL=1) — parita' NullGraphicsService:
 * le primitive completano come nel driver headless GPM (POINT=0, LOAD TEXTURE/
 * SPRITE LOAD=1, GETPIXELS dummy 1x1 valido). Condiviso da VM/C, C99 e nativo. */
int gmb_gfx_null_mode(void);

/* ---- Schermo / pagine ---- */
/* SCREEN mode [, width, height, bpp]: attiva la modalita'. mode 0=testo, 1=CGA
 * (320x200/16), 9=EGA (640x350/16), 12=VGA (640x480/16), 13=SVGA (320x200/256),
 * 16=XGA (640x480/256), 64=C64, 65=CRT (320x200/16). width/height>8 creano una
 * modalita' custom. */
void gmb_gfx_screen(int mode, int width, int height, int bpp);
/* Spazio coordinate DICHIARATO dai controlli (`SCREEN ... USE CELLS|PIXELS`).
 *   -1 = AUTO (default): grafica → PIXEL, TUI → CELLE (nessuna conversione);
 *    0 = PIXELS: i controlli dichiarano pixel (in TUI si converte in celle);
 *    1 = CELLS : i controlli dichiarano celle (in grafica si converte in pixel).
 * La conversione la applica il renderer per-frame (dipende da font/renderScale). */
void gmb_gfx_set_coord_mode(int mode);
int  gmb_gfx_coord_mode(void);
/* Spazio DICHIARATO effettivo: 1 = celle, 0 = pixel. Risolve AUTO sul backend attivo. */
int  gmb_gfx_coords_cells(void);
/* TEXTMODE n: seleziona la modalita' console (0=%SDL,1=%C64,2=%ANSI,3=%SYSTEM). Ponte
 * esportato in libgmb_gfx per il runtime C# (gmb_console_select in gmb_console.c). */
void gmb_gfx_console_select(int mode);
/* SCREENSET visible_page, active_page: cambia pagina attiva/visibile. */
void gmb_gfx_screenset(int visible_page, int active_page);
/* PCOPY src, dst: copia una pagina sull'altra. */
void gmb_gfx_pcopy(int src, int dst);
int  gmb_gfx_screen_on(void);
int  gmb_gfx_video_is_real(void);  /* 1 = finestra reale (present SDL vero) */
/* Modalità SCREEN corrente (0 se schermo off) — per il router PCodeVM. */
int  gmb_gfx_current_mode(void);
/* Dimensioni della pagina visibile (0 se schermo off). */
int  gmb_gfx_screen_width(void);
int  gmb_gfx_screen_height(void);
int  gmb_gfx_active_page(void);
int  gmb_gfx_visible_page(void);
int  gmb_gfx_width(void);
int  gmb_gfx_height(void);

/* ---- Colore / palette ---- */
/* COLOR fg, bg: imposta colore testo (primo piano) e sfondo. -1 = non toccare. */
void gmb_gfx_color(int fg, int bg);
/* PALETTE index, r, g, b: ridefinisce un colore della palette. */
void gmb_gfx_palette(int index, int r, int g, int b);
/* CLS [color]: pulisce la pagina attiva (default sfondo). */
void gmb_gfx_cls(int color);
int  gmb_gfx_fg(void);
int  gmb_gfx_bg(void);
int  gmb_gfx_page_bg_color(void);   /* RGB dello sfondo pagina (per il repaint incrementale) */
void gmb_gfx_dim_page(float factor);   /* velo del modale: oscura la pagina */

/* ---- Primitive ---- */
void gmb_gfx_pset(int x, int y, int color);            /* color<0 -> fg */
/* Varianti con coordinate FRAZIONARIE (WINDOW): mappano in double poi arrotondano. */
void gmb_gfx_pset_f(double x, double y, int color);
void gmb_gfx_map_d(double* x, double* y);
void gmb_gfx_line_f(double x1, double y1, double x2, double y2, int color, const char* style);
void gmb_gfx_line_rel_f(double x2, double y2, int color, const char* style);
void gmb_gfx_view_d(double x1, double y1, double x2, double y2);
void gmb_gfx_window_d(double x1, double y1, double x2, double y2);
void gmb_gfx_view_style(int bg, int border);
/* Palette NOMINATE (DEFINE/USE/REPLACE/SAVE/LOAD). */
void gmb_gfx_palette_use(const char* name);
void gmb_gfx_palette_define(const char* name, const double* colors, int n);
void gmb_gfx_palette_replace(const char* name, const double* colors, int n);
int  gmb_gfx_palette_save(const char* path);
int  gmb_gfx_palette_load(const char* path);
void gmb_gfx_preset(int x, int y);
int  gmb_gfx_point(int x, int y);                      /* indice palette o RGB; -1 se fuori */
void gmb_gfx_line(int x1, int y1, int x2, int y2, int color, const char* style);

/* ---- Canvas offscreen (BASIC: CANVAS ...) ----
 * Superfici RGBA8 indipendenti dallo schermo: handle >= 1 (0 = schermo/visible page).
 * Con un canvas attivo (gmb_gfx_canvas_use) le primitive disegnano lì, in coordinate
 * pixel 1:1 (VIEW/WINDOW e render-scale sono ignorati). */
int  gmb_gfx_canvas_new(int w, int h);
void gmb_gfx_canvas_free(int h);
int  gmb_gfx_canvas_use(int h);                        /* 0 = torna allo schermo */
int  gmb_gfx_canvas_active(void);
int  gmb_gfx_canvas_w(int h);
int  gmb_gfx_canvas_h(int h);
int  gmb_gfx_canvas_clear(int h, int color);
int  gmb_gfx_canvas_set(int h, int x, int y, int color);
int  gmb_gfx_canvas_get(int h, int x, int y);          /* RGB impacchettato (RGB flag); -1 se fuori */
int  gmb_gfx_canvas_save(int h, const char* path);     /* PNG (h=0: pagina visibile) */
int  gmb_gfx_canvas_load(const char* path);            /* PNG -> nuovo canvas (handle) */
int  gmb_gfx_canvas_blit(int src, int dx, int dy);     /* copia su canvas/target attivo */
/* Dispatcher unico per il comando CANVAS (op = "NEW"/"USE"/...); args in double, path per
 * SAVE/LOAD. Ritorna il valore dell'operazione (handle, colore, dimensione) o -1. */
double gmb_gfx_canvas_op(const char* op, const double* args, int nargs, const char* path);


/* COLLIDEBATCH: coppie AABB [i,j,...] su array di posizioni (buffer statico riusato). */
double* gmb_gfx_collide_batch(const double* xs, int nx, const double* ys, int ny,
                              double w, double h, int* out_len);
/* style: NULL/""=linea, "B"=rettangolo, "BF"=rettangolo pieno, "S<n>"=step.
 * fill: 1 = cerchio pieno (pseudo-radius). aspect: ellisse verticale (C#). */
void gmb_gfx_circle(int x, int y, int radius, int color, int fill, double start, double end, double aspect);
void gmb_gfx_paint(int x, int y, int fill_color, int border_color);
void gmb_gfx_draw(const char* command);
void gmb_gfx_polygon(const float* points, int count, int color);

/* ---- Testo ---- */
/* FONT "file", size: carica un font TrueType. Se il file non esiste nessun testo
 * viene renderizzato (come in C# quando SDL_ttf non trova il file). */
void gmb_gfx_font(const char* path, int size);
/* Font di default con fallback (GMB_FONT_PATH -> Monaco/SFNSMono -> Resources). */
int  gmb_gfx_font_default(int size);
const char* gmb_gfx_font_path(void);
int  gmb_gfx_font_size(void);
int  gmb_gfx_font_logical_size(void);
/* LOCATE y, x: posiziona il cursore grafico (pixel). */
void gmb_gfx_locate(int x, int y);
/* PRINT testo (senza newline; i '\n' interni vanno a capo). */
void gmb_gfx_print(const char* text);
/* DRAW TEXT "t", x, y [, align 0=L 1=C 2=R] [, color]. */
void gmb_gfx_draw_text(const char* text, int x, int y, int align, int color);
int  gmb_gfx_text_width(const char* text);   /* NATIVO (pixel pagina) */
int  gmb_gfx_text_height(void);              /* NATIVO */
int  gmb_gfx_text_width_logical(const char* text);   /* LOGICO (TEXTWIDTH BASIC) */
int  gmb_gfx_text_height_logical(void);              /* LOGICO (TEXTHEIGHT BASIC) */
/* x finale (device) del testo disegnato a (x,y) logici — stessa metrica del rendering. */
int  gmb_gfx_text_end_x(const char* text, int x, int y);
/* conversioni coordinate/pixel + info schermo (gmb_graphics.c, pure) */
double gmb_gfx_c2px(double col, double size);
double gmb_gfx_c2py(double row, double size);
double gmb_gfx_p2cx(double x, double size);
double gmb_gfx_p2cy(double y, double size);
int    gmb_gfx_screens(void);

/* ---- Texture (PNG) ---- */
int  gmb_gfx_load_texture(const char* path);           /* ritorna id (>=1) o -1 */
int  gmb_gfx_load_texture_sheet(const char* path, int cell_w, int cell_h);
int  gmb_gfx_texture_frames(int id);                   /* numero di frame (sheet) o 1 */
int  gmb_gfx_texture_size(int id, int* w, int* h);
int  gmb_gfx_create_solid_texture(int w, int h, uint32_t rgba); /* for tests: crea texture solida */
void gmb_gfx_draw_texture(int id, int x, int y, double rotation, double scale_x, double scale_y);
void gmb_gfx_draw_texture_frame(int id, int frame, int x, int y);
/* SPRITE GPU (parità motore C#): con driver video REALE il DRAW TEXTURE viene
 * registrato e composto dal present con SDL_RenderCopyEx (GPU), invece di essere
 * blittato pixel-per-pixel sulla pagina CPU. Headless/dummy → percorso software. */
void gmb_gfx_present_push_sprite(int id, int x, int y, int w, int h, double rot);
/* Puntatore/dimensioni dei pixel di una texture caricata (cache SDL del present). */
int  gmb_gfx_texture_info(int id, const uint32_t** px, int* w, int* h);
void gmb_gfx_unload_texture(int id);

/* ---- Pixel buffer (PIXELBUFFER/Mandelbrot) ---- */
/* buffer: array width*height di uint32 0xRRGGBB (row-major). Blit a schermo intero. */
void gmb_gfx_draw_pixel_buffer(int width, int height, const uint32_t* pixels);

/* ---- Sprite ---- */
int  gmb_gfx_sprite_load(const char* path);            /* registra sprite da PNG, ritorna id */
void gmb_gfx_sprite_set_position(int id, float x, float y);
void gmb_gfx_sprite_set_visible(int id, int visible);
void gmb_gfx_sprite_set_priority(int id, int priority);
void gmb_gfx_sprite_set_rotation(int id, double degrees);
void gmb_gfx_sprite_set_scale(int id, double sx, double sy);
void gmb_gfx_sprite_draw(int id);
int  gmb_gfx_sprite_count(void);
/* Stato in lettura (per i test e per la superficie BASIC): 0 se l'id non esiste. */
double gmb_gfx_sprite_x(int id);
double gmb_gfx_sprite_y(int id);
int    gmb_gfx_sprite_visible(int id);
int    gmb_gfx_sprite_priority(int id);

/* ---- Gruppi di sprite (riferimento: SPRITE n, SIZE w, h) ----
 * `gmb_gfx_sprite_set_size(head, w, h)` rende `head` la TESTA di un blocco w×h di sprite
 * consecutivi, riga per riga (w, h 1..8, w*h <= 64): membri head..head+w*h-1, una cella
 * distante (cell_w/cell_h della texture della testa, altrimenti la sua dimensione). Il blocco
 * si muove, si mostra, si ruota e collide come UN oggetto: posizione, ON/OFF, priorita',
 * rotazione, scala, matrice affine, MOVETO e PATH si impostano sulla testa; le impostazioni
 * dei membri sono ignorate e ripristinate quando il gruppo si scioglie (`SIZE 1,1`). Le
 * collisioni riferiscono la testa e i membri di uno stesso gruppo non collidono tra loro.
 * 0 = fatto; -1 = dimensioni non valide, testa inesistente, membro gia' in un altro gruppo,
 * oppure blocco oltre GMB_GFX_MAX_SPRITES. */
int  gmb_gfx_sprite_set_size(int head, int w, int h);
int  gmb_gfx_sprite_cols(int head);          /* 1 se non e' testa di un gruppo */
int  gmb_gfx_sprite_rows(int head);
int  gmb_gfx_sprite_group_head(int id);      /* 0 se non appartiene a un gruppo */

/* ---- Movimento a tick (riferimento: SPRITE n, MOVETO x, y, ticks) ----
 * Un passo per frame, interpolazione lineare verso il bersaglio, arrivo esatto all'ultimo
 * frame; una nuova posizione o un altro MOVETO sostituiscono il movimento in corso; su un
 * membro di gruppo e' ignorato (comanda la testa). `ticks <= 0` = posizione subito.
 * `gmb_gfx_sync` avanza di un frame; in headless si chiama `gmb_gfx_sprites_tick` per
 * avanzare in modo deterministico. Ritorna i frame avanzati. */
int  gmb_gfx_sprite_moveto(int id, float x, float y, int ticks);
int  gmb_gfx_sprite_moving(int id);
int  gmb_gfx_sprites_tick(int frames);

/* ---- PATH a$ (riferimento: SPRITE n, PATH a$) ----
 * La stringa si compila TUTTA prima: se c'e' un errore ritorna -1 e il path precedente
 * resta intatto (`gmb_gfx_sprite_path_error()` dice perche'). "" ferma il path lasciando lo
 * sprite dov'è; un nuovo path sostituisce il precedente. Un passo per frame:
 *   X+n | X-n | Y+n | Y-n   n px PER FRAME per k frame (`X+3*20` = 3 px × 20 frame)
 *   X=n | Y=n               coordinata impostata subito
 *   M dx,dy[*k]             spostamento per frame (k frame)
 *   W n                     n frame fermo
 *   T n                     durata di default dei movimenti successivi
 *   E n                     evento (anello da 256, si leggono con path_event)
 *   [ ... ]n                ripete il blocco n volte, fino a 4 livelli
 *   R                       riparte da capo
 * Massimo 4096 comandi e 256 comandi istantanei per frame. Su un membro di gruppo il path
 * e' ignorato. NON implementati (errore esplicito, mai no-op silenziosi, il runtime C non
 * ha sequenze di immagini ne' override per sprite): A, D, H, S, P.
 * `gmb_gfx_sprite_path_active` segue il contratto del riferimento: -1 mentre corre, 0 fermo. */
int  gmb_gfx_sprite_path(int id, const char* program);
int  gmb_gfx_sprite_path_active(int id);
const char* gmb_gfx_sprite_path_error(void);
int  gmb_gfx_sprite_path_events(void);                    /* eventi in attesa */
int  gmb_gfx_sprite_path_event(int* sprite_out);          /* -1 se non ce ne sono */

/* ---- Sync / input ---- */
/* SYNC: presenta il frame. In headless e' un no-op immediato. */
void gmb_gfx_sync(void);
/* ON SPRITE COLLISION / COLLISIONS: handler C invocati al SYNC successivo a quello che ha rilevato le collisioni.
 * Singolare: (x, y) della PRIMA coppia (centro dello sprite con id minore). Plurale: id unici degli sprite coinvolti.
 * NULL = nessun handler. */
typedef void (*gmb_gfx_collision_fn)(double x, double y);
typedef void (*gmb_gfx_collisions_fn)(const double* ids, int n);
void gmb_gfx_on_sprite_collision(gmb_gfx_collision_fn cb);
void gmb_gfx_on_sprite_collisions(gmb_gfx_collisions_fn cb);
/* Senza handler C: preleva le coppie rilevate dall'ultimo SYNC (id1, id2, x, y); ritorna quante (max `max`). */
int  gmb_gfx_take_collisions(int* id1, int* id2, float* x, float* y, int max);
/* Present SDL (also used by the TICK loop: one present per frame). */
void gmb_gfx_present_tick(void);
/* Cattura offscreen pagina+sprite GPU (SAVE/IMAGEGET): 0 = ok, -1 = usa la pagina. */
int gmb_gfx_present_capture(uint32_t* out, int w, int h);
/* WAITKEY in modalita' grafica: headless ritorna subito. */
void gmb_gfx_waitkey(void);

/* ---- Presentazione opzionale (nessuna dipendenza SDL qui, vedi nota in testa) ----
 * Framebuffer della pagina VISIBILE (g_gfx.visible_page, non active_page — sono
 * distinte per il double buffering di SCREENSET). Sola lettura, per un backend di
 * presentazione opzionale esterno (es. gmb_gfx_present_sdl.c, polling dal thread
 * main — gmb_gfx_sync() puo' essere chiamata da un task worker della VM GPM, quindi
 * l'eventuale presentazione SDL NON puo' agganciarsi qui: Cocoa/AppKit richiede che
 * le chiamate SDL stiano sempre sul vero thread main del processo). NULL se lo
 * schermo non e' attivo. Il puntatore resta valido fino alla prossima
 * gmb_gfx_screen()/gmb_gfx_screenset() che rialloca le pagine — non conservarlo
 * oltre un singolo ciclo di presentazione. */
const uint32_t* gmb_gfx_visible_pixels(int* out_w, int* out_h);

/* Copia un fotogramma ARGB (0x00RRGGBB, il formato del target 3D) nella pagina 2D
 * ATTIVA — canvas attivo se c'è, altrimenti pagina dello schermo. È l'unica sede della
 * copia `SCENE PRESENT` (le pagine vivono in questo modulo): 1:1 dall'angolo in alto a
 * sinistra, ritagliata alle due dimensioni, nessuna scalatura. Ritorna i pixel copiati,
 * -1 se non c'è una pagina (o argomenti non validi). Non tocca SDL. */
int gmb_gfx_present_argb(const uint32_t* src, int w, int h);

/* ---- Screenshot (verifica headless) ---- */
/* Salva la pagina attiva come PNG. Ritorna 0 ok, -1 errore. */
int  gmb_gfx_save_screen_png(const char* path);
/* Carica un PNG come texture in memoria (ritorna id o -1). */
int  gmb_gfx_texture_from_png(const char* path);

/* ---- Viewport / coordinate ---- */
/* VIEW (x1,y1)-(x2,y2): rettangolo di clipping. VIEW senza argomenti → full screen.
 * (0,0,0,0) = reset a schermo intero. */
void gmb_gfx_view(int x1, int y1, int x2, int y2);
/* WINDOW [SCREEN] (x1,y1)-(x2,y2): trasforma le coordinate mondo in pixel del viewport.
 * (0,0,0,0) = reset (coordinate fisiche). */
void gmb_gfx_window(int x1, int y1, int x2, int y2);

/* ---- Poligono pieno ---- */
/* POLYGON points, color: points è un array piatto (x0,y0,x1,y1,...), count = numero punti. */
void gmb_gfx_polygon_fill(const float* points, int count, int color);

/* ---- IMAGEGET / IMAGEPUT / IMAGEPUTS ---- */
/* IMAGEGET: cattura la regione (x1,y1)-(x2,y2) nella pagina attiva in out[] come
 * [width, height, pixel_index...] (righe in alto->basso; indice palette). max = capacità
 * di out. Ritorna il numero di elementi scritti. */
int  gmb_gfx_image_get(int x1, int y1, int x2, int y2, int* out, int max);
/* IMAGEPUT: disegna img[] ([w,h,pixel...]) a (x,y); indice <0 = trasparente.
 * op: NULL/"PSET"/"PRESET" = sovrascrive, "AND"/"OR"/"XOR" = operazione logica su RGB. */
void gmb_gfx_image_put(int x, int y, const int* img, int n);
void gmb_gfx_image_put_ex(int x, int y, const int* img, int n, const char* op);
/* IMAGEPUTS: disegna n sprite da un array piatto di sprite [w,h,pixels...] alle posizioni
 * positions (array piatto [x0,y0,x1,y1,...]). */
void gmb_gfx_image_puts(const int* sprites, const float* positions, int count);
/* Variante sicura con lunghezza dell'array sprites (bridge C#): count = numero sprite. */
void gmb_gfx_image_puts_n(const int* sprites, int sprites_len, const float* positions, int count);

/* ---- Sprite pixel / collisioni ---- */
void gmb_gfx_sprite_get_pixels(int src_id, int dst_id);
int  gmb_gfx_sprite_pixels_to_array(int src_id, int* out, int max);
int  gmb_gfx_collide(int id1, int id2);
/* COLLIDE per pixel OPACO: -1 se i due sprite hanno almeno un pixel VISIBILE in comune, 0
 * altrimenti. `gmb_gfx_collide` resta il test AABB (contratto delle 4 gambe); questa variante
 * usa la geometria di gmb_gfx_draw_texture (centro, scala, rotazione o matrice) e la soglia di
 * opacita' del renderer (alpha >= 8), quindi due sprite con buchi trasparenti NON collidono
 * dove non c'e' nulla. Con sprite completamente opachi coincide con l'AABB. */
int  gmb_gfx_collide_pixels(int id1, int id2);

/* ---- BSAVE / BLOAD (area catturata internamente) ---- */
void gmb_gfx_bsave(int x1, int y1, int x2, int y2);
void gmb_gfx_bload(int x, int y);

/* ---- CHART (plotting minimale) ---- */
int  gmb_gfx_chart_create(void);
void gmb_gfx_chart_plot(int handle, const double* xs, const double* ys, int count, const char* label);
void gmb_gfx_chart_plot_financial(int handle, const double* ohlc, int count, const char* plot_type, const char* label);
void gmb_gfx_chart_plot_financial_draw(int handle, int ox, int oy);
void gmb_gfx_chart_show(int handle, int x, int y);
void gmb_gfx_chart_clear(int handle);
void gmb_gfx_chart_close(int handle);
int  gmb_gfx_chart_save(int handle, const char* path);
/* Configurazione da BASIC (CHART SET/GET/GETS): rendering, view window (zoom/pan), colori. */
void        gmb_gfx_chart_set(int handle, const char* prop, double num, const char* str);
double      gmb_gfx_chart_get(int handle, const char* prop);
const char* gmb_gfx_chart_get_str(int handle, const char* prop);
/* Oggetti di disegno (CHART DRAW): op = line/hline/vline/rect/fib/move/remove/clear.
 * args in coordinate DATI. Ritorna l'id del nuovo oggetto (o id/count per move/remove/clear). */
double gmb_gfx_chart_draw(int handle, const char* op, const double* args, int nargs);
double gmb_gfx_chart_objcount(int handle);
double gmb_gfx_chart_objhit(int handle, double x, double y);

/* ---- TILEMAP ---- */
int  gmb_gfx_tilemap_create(const char* tileset, int cell_w, int cell_h, int cols, int rows);
void gmb_gfx_tilemap_set(int handle, int col, int row, int tile);
void gmb_gfx_tilemap_draw(int handle, int offset_x, int offset_y);
void gmb_gfx_tilemap_offset(int handle, int dx, int dy);
int  gmb_gfx_tilemap_offset_x(int handle);
int  gmb_gfx_tilemap_offset_y(int handle);
void gmb_gfx_tilemap_scroll(int handle, int dx, int dy);

#ifdef __cplusplus
}
#endif

#endif /* GMB_GRAPHICS_H */

/* --- estensioni motore unico (bridge C# / parità IGraphicsService) --- */
void gmb_gfx_clear_rect(int x1, int y1, int x2, int y2, int color);
int  gmb_gfx_restore_screen_png(const char* path);
/* Chrome pixel nativo (porting GfxWindowCanvas.cs). rgba = 0xAARRGGBB. */
void gmb_gfx_blend_pixel(int x, int y, unsigned int rgba, double coverage);
void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba);
void gmb_gfx_stroke_rounded_rect(int x, int y, int w, int h, int radius, unsigned int rgba);
void gmb_gfx_gradient_rect(int x, int y, int w, int h, unsigned int cstart, unsigned int cend, int vertical);
void gmb_gfx_fill_rounded_gradient(int x, int y, int w, int h, int radius, unsigned int c1, unsigned int c2);
void gmb_gfx_drop_shadow(int x, int y, int w, int h, int offset, unsigned int shadow_rgba, int radius);
void gmb_gfx_fill_circle(int cx, int cy, int d, unsigned int rgba);
int  gmb_gfx_sprite_def(int id, int w, int h, const int* pixels);
/* ---- DESIGN: immagine definita da righe ASCII (riferimento: DESIGN/@) ----
 * `rows` = righe separate da '\n' (un '\r' finale è ignorato, una '\n' finale non aggiunge
 * una riga), tutte della stessa larghezza, 1..256 colonne/righe e <= 4096 pixel. Ogni
 * carattere è un indice di palette in base 36 ('0'..'9' → 0..9, 'A'..'Z'/'a'..'z' → 10..35);
 * '.' e ' ' sono trasparenti. Lo sprite è una texture normale (`gmb_gfx_sprite_def`), quindi
 * disegno, collisione e gruppi valgono invariati. 0 = fatto; -1 = errore, motivo in
 * `gmb_gfx_design_error()` (mai uno sprite a metà). */
int  gmb_gfx_design(int id, const char* rows);
const char* gmb_gfx_design_error(void);
/* Matrice affine 2x3 (a,b,c,d,e,f) dello sprite. draw_texture conosce solo rotazione e
 * scala, quindi la matrice si DECOMPONE (gmb_gfx_sprite_rot_scale, la stessa usata da
 * disegno, collisione per pixel e gruppi): rot = atan2(b,a), scala = |(a,b)| / |(c,d)|;
 * la traslazione (e,f) non si applica — la posizione resta quella di SPRITE SET POSITION —
 * e uno shear non rappresentabile resta approssimato da quella rotazione+scala. Su una TESTA
 * di gruppo ridispone subito il blocco (come rotazione/scala); su un membro e' ignorata
 * (comanda la testa). Non c'e' API di rimozione: la matrice vale finche' lo sprite non
 * viene ridefinito con gmb_gfx_sprite_def. */
void gmb_gfx_sprite_set_matrix(int id, const double* m6);
int  gmb_gfx_tilemap_from_texture(int texture_id, int cell_w, int cell_h, int cols, int rows);
/* API canonica senza handle (tilemap CORRENTE). */
int  gmb_gfx_tilemap_load_tex(int tex_id, int cell_w, int cell_h, int per_row, int map_w, int map_h,
                              const double* tiles, int ntiles);
void gmb_gfx_tilemap_set_cur(int row, int col, int tile);
void gmb_gfx_tilemap_offset_cur(int dx, int dy);
void gmb_gfx_tilemap_scroll_cur(int dx, int dy);
void gmb_gfx_tilemap_draw_cur(void);
/* Rende una griglia di celle TUI (chars/fg/bg ARGB) sulla pagina attiva col font corrente. */
void gmb_gfx_render_cells(const int* chars, const unsigned int* fg, const unsigned int* bg, int w, int h);
void gmb_gfx_render_cells_ex(const int* chars, const unsigned int* fg, const unsigned int* bg, int w, int h, int cw, int ch);
void gmb_gfx_render_cells_at(const int* chars, const unsigned int* fg, const unsigned int* bg, int w, int h, int cw, int ch, int ox, int oy);

/* ---- Schermo di TESTO 80×25 a celle (riferimento: text screen, celle 8×12 = 640×300) ----
 * Le primitive `render_cells*`/`draw_cell` disegnano una griglia che il chiamante possiede;
 * questo modulo ne possiede una: 80 colonne × 25 righe di celle 8×12, ogni cella con
 * carattere, penna (testo) e carta (sfondo), più un cursore con a capo automatico e scroll.
 *  - `set(1)` accende la modalità, azzera la griglia con penna/carta correnti, riporta il
 *    cursore in alto a sinistra e, se c'è uno schermo, porta la pagina a 80*8 × 25*12 pixel
 *    NATIVI (divide per il render-scale, come `render_cells`); `set(0)` la spegne lasciando
 *    il contenuto (si può riaccendere senza perdere il testo).
 *  - `pen(fg, bg)` cambia i colori delle scritture successive (colori del linguaggio: indice
 *    di palette o RGB con il flag; -1 = lascia quello attuale). Default bianco su nero.
 *  - `write` scrive al cursore: `\n` va a capo, `\r` è ignorato, TAB salta alla colonna
 *    successiva multipla di 8 riempiendo di spazi, a fine riga va a capo e in fondo scorre.
 *    Ritorna i caratteri scritti (controlli esclusi), 0 a modalità spenta.
 *  - `render` disegna le 2000 celle sulla pagina ATTIVA (canvas se c'è) dall'angolo in alto
 *    a sinistra, glifi dal bitmap ASCII 8×16 del runtime campionato a 12 righe: nessun font
 *    TTF, quindi resa deterministica anche in headless. Ritorna le celle disegnate, 0 se la
 *    modalità è spenta. Il disegno è ESPLICITO per scelta: chi compone decide l'ordine con
 *    sprite e layer.
 *  - `cell(col,row,…)` legge una cella (1 = ok, 0 = fuori griglia); gli out sono opzionali.
 * Le celle sono 8×12 e i codepoint fuori da 32..126 non hanno glifo (solo carta). */
int  gmb_gfx_textscreen_set(int on);
int  gmb_gfx_textscreen_on(void);
int  gmb_gfx_textscreen_cols(void);
int  gmb_gfx_textscreen_rows(void);
int  gmb_gfx_textscreen_cell_w(void);
int  gmb_gfx_textscreen_cell_h(void);
void gmb_gfx_textscreen_pen(int fg, int bg);
void gmb_gfx_textscreen_locate(int col, int row);
int  gmb_gfx_textscreen_cursor_x(void);
int  gmb_gfx_textscreen_cursor_y(void);
int  gmb_gfx_textscreen_write(const char* text);
int  gmb_gfx_textscreen_scroll(int lines);
int  gmb_gfx_textscreen_clear(void);
int  gmb_gfx_textscreen_cell(int col, int row, int* ch, unsigned int* fg, unsigned int* bg);
int  gmb_gfx_textscreen_render(void);
/* Font a celle 8x16: disegna un codepoint in una cella (tiling esatto). */
void gmb_gfx_draw_cell(int x, int y, int cw, int ch, uint32_t cp, uint32_t fg, uint32_t bg);
void gmb_gfx_cell_bg(int x, int y, int cw, int ch, uint32_t bg);
void gmb_gfx_cell_glyph(int x, int y, int cw, int ch, uint32_t cp, uint32_t fg);
void gmb_gfx_draw_glyph(uint32_t cp, int x, int y, uint32_t color);
void gmb_gfx_draw_glyph_cell(uint32_t cp, int x, int y, int cw, int ch, uint32_t color);
/* TABZONE: avanza il cursore grafico alla zona tab successiva (14 colonne carattere). */
void gmb_gfx_tab_zone(void);
void gmb_gfx_set_cursor_state(int visible);
void gmb_gfx_set_cursor_shape(int shape);
void gmb_gfx_set_attribute(int attr);
int  gmb_gfx_get_cursor_x(void);
int  gmb_gfx_get_cursor_y(void);
