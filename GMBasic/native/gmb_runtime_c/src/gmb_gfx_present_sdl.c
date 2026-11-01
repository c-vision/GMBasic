/* ============================================================================
 * gmb_gfx_present_sdl.c — implementazione SDL2 di gmb_gfx_present.h.
 *
 * Compilato solo se CMake trova SDL2 (GMB_GFX_SDL_PRESENT, vedi
 * gmb_runtime_c/CMakeLists.txt) — build headless (CI, server) restano prive di
 * questa dipendenza, coerente con la nota "nessuna dipendenza SDL" in testa a
 * gmb_graphics.h/.c.
 *
 * Formato pixel: gmb_graphics.c produce uint32_t 0x00RRGGBB (R bit 16-23, G bit
 * 8-15, B bit 0-7, vedi palette_rgb()/resolve_color() in gmb_graphics.c) — in
 * byte-order nativo questo layout numerico coincide con SDL_PIXELFORMAT_ARGB8888
 * (A=24-31, R=16-23, G=8-15, B=0-7) con alpha=0. SDL_RenderCopy con blend mode
 * di default (SDL_BLENDMODE_NONE) ignora il canale alpha in copia: alpha=0 non
 * rende il frame invisibile.
 * ============================================================================ */

/* Guardato da GMB_GFX_SDL_PRESENT (come il CMake quando trova SDL2): senza il flag questo file
 * e' VUOTO, cosi' gmb_tui_surface.c fornisce gli stub — mai simboli duplicati a link. */
#ifdef GMB_GFX_SDL_PRESENT
#include "gmb_gfx_present.h"
#include "gmb_graphics.h"
#include "gmb_gfx_input.h"
#include <stdlib.h>

#ifdef GMB_GFX_VULKAN
#include "gmb_gfx_vulkan.h"
#endif

#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

/* ── SPRITE GPU ─────────────────────────────────────────────────────────────
 * Con driver video reale i DRAW TEXTURE vengono registrati (coordinate NATIVE
 * della pagina) e composti nel present con SDL_RenderCopyEx: gli sprite li
 * disegna la GPU, come il motore C#. Headless/dummy resta il blit software. */
#define GMB_PRESENT_SPRITES_MAX 16384
typedef struct { int id, x, y, w, h, page; double rot; } gmb_pres_sprite_t;
static gmb_pres_sprite_t g_pres_sprites[GMB_PRESENT_SPRITES_MAX];
static int g_pres_sprite_count = 0;
static int g_pres_frame_ready = 0;   /* frame presentato: lista conservata per SAVE/IMAGEGET */
static SDL_Renderer* g_renderer;   /* definizione più sotto */

void gmb_gfx_present_push_sprite(int id, int x, int y, int w, int h, double rot)
{
    /* Primo DRAW TEXTURE del frame successivo: la lista del frame precedente è
     * stata consumata (present) e viene conservata per la cattura offscreen. */
    if (g_pres_frame_ready) { g_pres_sprite_count = 0; g_pres_frame_ready = 0; }
    if (g_pres_sprite_count >= GMB_PRESENT_SPRITES_MAX) return;
    gmb_pres_sprite_t* s = &g_pres_sprites[g_pres_sprite_count++];
    s->id = id; s->x = x; s->y = y; s->w = w; s->h = h; s->rot = rot;
    s->page = gmb_gfx_active_page();
}

/* Cache delle texture SDL (una per texture BASIC); ricreata se i pixel cambiano. */
static struct { int id; const uint32_t* px; int w, h; SDL_Texture* tex; } g_spr_cache[256];
static SDL_Texture* sprite_tex(int id)
{
    if (id < 0 || id >= 256 || !g_renderer) return NULL;
    const uint32_t* px = NULL; int w = 0, h = 0;
    if (!gmb_gfx_texture_info(id, &px, &w, &h) || !px || w <= 0 || h <= 0) return NULL;
    if (g_spr_cache[id].tex && g_spr_cache[id].px == px && g_spr_cache[id].w == w && g_spr_cache[id].h == h)
        return g_spr_cache[id].tex;
    if (g_spr_cache[id].tex) { SDL_DestroyTexture(g_spr_cache[id].tex); g_spr_cache[id].tex = NULL; }
    SDL_Texture* t = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, px, w * (int)sizeof(uint32_t));
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    g_spr_cache[id].id = id; g_spr_cache[id].px = px; g_spr_cache[id].w = w; g_spr_cache[id].h = h;
    g_spr_cache[id].tex = t;
    return t;
}

static int g_default_font_tried = 0;
void gmb_gfx_present_init_main_thread(void);  /* definita in gmb_tui_surface.c */

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static int g_tex_w = 0, g_tex_h = 0;
static int g_owns_sdl_video = 0;
static int g_quit_requested = 0;
static int g_vis_req = -1;  /* -1=none, 0=hide, 1=show (richiesta worker, applicata sul main */
#ifdef GMB_GFX_VULKAN
static int g_vk_active = 0;
#endif

/* Backend di presentazione: GMB_GFX_BACKEND=vulkan|sdl|auto (default sdl).
 * "auto"/"vulkan" tentano Vulkan; se non compilato o init fallita -> SDL. */
static int backend_wants_vulkan(void)
{
#ifdef GMB_GFX_VULKAN
    const char* b = getenv("GMB_GFX_BACKEND");
    return b && (strcmp(b, "vulkan") == 0 || strcmp(b, "auto") == 0);
#else
    return 0;
#endif
}

/* ── Input: traduzione eventi SDL → gmb_gfx_input (stesse stringhe del bridge
 * C# storico CommonGraphics/Graphics/IlSdlInput.cs) ────────────────────────── */

/* Mappa coordinate finestra → pixel dello schermo grafico (il renderer stira il
 * framebuffer sulla finestra). Senza schermo attivo passa attraverso. */
static void map_mouse(int wx, int wy, int* sx, int* sy)
{
    /* Coordinate LOGICHE di finestra (come C# Manager/SdlManager): SDL con
     * ALLOW_HIGHDPI riporta i punti-finestra = schermo logico. Nessun remap a
     * framebuffer (era la doppia-scala che mandava i click fuori schermo). */
    *sx = wx; *sy = wy;
}

/* Tasti di controllo e (fallback driver senza TEXTINPUT) stampabili. */
static void push_sdl_key(SDL_Keycode sym)
{
    /* Tasti anche ai controlli TUI (finestra grafica): F1..F12 e tasti speciali. */
    { extern void gmb_tui_push_gfx_key(int k);
      if (sym >= SDLK_F1 && sym <= SDLK_F12) { gmb_tui_push_gfx_key(300 + (int)(sym - SDLK_F1)); return; }
      switch (sym) {
        case SDLK_ESCAPE: gmb_tui_push_gfx_key(27); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: gmb_tui_push_gfx_key(13); break;
        case SDLK_BACKSPACE: gmb_tui_push_gfx_key(127); break;
        case SDLK_TAB: gmb_tui_push_gfx_key(9); break;
        case SDLK_LEFT: gmb_tui_push_gfx_key(19); break;
        case SDLK_RIGHT: gmb_tui_push_gfx_key(4); break;
        case SDLK_UP: gmb_tui_push_gfx_key(5); break;
        case SDLK_DOWN: gmb_tui_push_gfx_key(24); break;
        default: break;
      } }
    switch (sym)
    {
        case SDLK_ESCAPE:    gmb_gfx_input_push_key("\x1b"); return;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  gmb_gfx_input_push_key("\r"); return;
        case SDLK_BACKSPACE: gmb_gfx_input_push_key("\b"); return;
        case SDLK_TAB:       gmb_gfx_input_push_key("\t"); return;
        case SDLK_SPACE:     gmb_gfx_input_push_key(" "); return;
        case SDLK_LEFT:      gmb_gfx_input_push_key("\x1c"); return;
        case SDLK_RIGHT:     gmb_gfx_input_push_key("\x1d"); return;
        case SDLK_UP:        gmb_gfx_input_push_key("\x1e"); return;
        case SDLK_DOWN:      gmb_gfx_input_push_key("\x1f"); return;
        default: break;
    }
    if (sym >= SDLK_0 && sym <= SDLK_9) { char s[2] = { (char)('0' + (sym - SDLK_0)), 0 }; gmb_gfx_input_push_key(s); return; }
    if (sym >= SDLK_a && sym <= SDLK_z) { char s[2] = { (char)('a' + (sym - SDLK_a)), 0 }; gmb_gfx_input_push_key(s); }
}

static void process_input_event(const SDL_Event* ev);

void gmb_gfx_present_process_event(const void* sdl_event)
{
    if (sdl_event) process_input_event((const SDL_Event*)sdl_event);
}

static void process_input_event(const SDL_Event* ev)
{
    switch (ev->type)
    {
        case SDL_KEYDOWN:
            if (ev->key.repeat != 0) return;
            gmb_gfx_input_set_key_down(ev->key.keysym.scancode, 1);
            if ((ev->key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL | KMOD_GUI)) &&
                ev->key.keysym.sym == SDLK_c)
            {
                /* La decisione "intercettato (ON KEY) o abort" e' del runtime
                 * gestito: qui si alza solo il flag, senza accodare tasti. */
                gmb_gfx_input_set_ctrl_c();
                return;
            }
            /* F1..F12 e tasti speciali vanno SEMPRE al bridge TUI/INKEY, anche con il
             * text-input attivo (prima i tasti funzione cadevano nel default e i
             * controlli TUI non li ricevevano MAI: la FunctionKeyBar "non funzionava"). */
            if (ev->key.keysym.sym >= SDLK_F1 && ev->key.keysym.sym <= SDLK_F12)
            { push_sdl_key(ev->key.keysym.sym); break; }
            if (SDL_IsTextInputActive())
            {
                switch (ev->key.keysym.sym)
                {
                    case SDLK_ESCAPE: case SDLK_RETURN: case SDLK_KP_ENTER:
                    case SDLK_BACKSPACE: case SDLK_TAB:
                    case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN:
                    case SDLK_DELETE: case SDLK_HOME: case SDLK_END:
                        push_sdl_key(ev->key.keysym.sym);
                        break;
                    default: break; /* stampabili via SDL_TEXTINPUT */
                }
            }
            else
                push_sdl_key(ev->key.keysym.sym);
            break;
        case SDL_KEYUP:
            gmb_gfx_input_set_key_down(ev->key.keysym.scancode, 0);
            break;
        case SDL_TEXTINPUT:
        {
            const char* text = ev->text.text;
            if (text && text[0] && text[0] != '\x03') {
                gmb_gfx_input_push_key(text);
            }
            break;
        }
        case SDL_MOUSEMOTION:
        {
            int sx, sy;
            map_mouse(ev->motion.x, ev->motion.y, &sx, &sy);
            gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_MOVE, sx, sy, 0, 0);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            int sx, sy;
            map_mouse(ev->button.x, ev->button.y, &sx, &sy);
            gmb_gfx_input_push_mouse(ev->type == SDL_MOUSEBUTTONDOWN ? GMB_GFX_MOUSE_DOWN : GMB_GFX_MOUSE_UP, sx, sy, ev->button.button, 0);
            break;
        }
        case SDL_MOUSEWHEEL:
        {
            int mx = 0, my = 0;
            gmb_gfx_input_get_mouse(&mx, &my);
            if (ev->wheel.y != 0)
                gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_WHEEL, mx, my, 0, ev->wheel.y > 0 ? 1 : -1);
            break;
        }
        default: break;
    }
}

static void destroy_texture(void)
{
    if (g_texture) { SDL_DestroyTexture(g_texture); g_texture = NULL; }
    g_tex_w = 0; g_tex_h = 0;
}

/* Crea finestra+renderer al primo frame utile, poi ricrea solo la texture (o
 * ridimensiona la finestra) se la risoluzione cambia (es. nuovo SCREEN mode). */
static void ensure_window(int w, int h)
{
    if (!g_window)
    {
        /* Finestra in pixel LOGICI (come C# Manager.cs); drawable nativo via ALLOW_HIGHDPI. */
        int logW = gmb_gfx_width(), logH = gmb_gfx_height();
        if (logW <= 0) logW = w;
        if (logH <= 0) logH = h;
        g_window = SDL_CreateWindow("GMBasic", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     logW, logH, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        if (!g_window) { fprintf(stderr, "[gmb_gfx] CreateWindow FAILED: %s\n", SDL_GetError()); fflush(stderr); return; }
        SDL_ShowWindow(g_window);
        SDL_RaiseWindow(g_window);
        /* Attiva la finestra subito: su macOS il PRIMO click serve ad attivarla e non
         * arriva all.app (la prima voce sembrava non rispondere al mouse). */
        SDL_SetWindowInputFocus(g_window);
        SDL_StartTextInput();
#ifdef GMB_GFX_VULKAN
        if (backend_wants_vulkan())
        {
            if (gmb_gfx_vk_start(g_window, logW, logH) == 0)
            {
                g_vk_active = 1;
                g_tex_w = w; g_tex_h = h;
                return;
            }
        }
#endif
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");   /* lineare: parita col motore C# */
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
        if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
        if (!g_renderer) return;
        /* renderScale = output nativo / finestra logica (es. 2 su Retina). Pagine native. */
        int ow = 0, oh = 0, ww = 0, wh = 0;
        SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
        SDL_GetWindowSize(g_window, &ww, &wh);
        float sc = (ow > 0 && ww > 0) ? (float)ow / (float)ww : 1.0f;
        { const char* ov = getenv("GMB_GFX_SCALE"); if (ov && *ov) sc = (float)atof(ov); }   /* override test */
        if (sc < 1.0f) sc = 1.0f;
        gmb_gfx_set_render_scale(sc);
        { const char* fp = gmb_gfx_font_path(); int fsz = 16; { const char* e = getenv("GMB_UI_FONT_SIZE"); if (e && *e) fsz = atoi(e); if (fsz < 6) fsz = 6; if (fsz > 48) fsz = 48; } if (fp && *fp) gmb_gfx_font(fp, (int)(fsz * sc + 0.5f)); }   /* font a risoluzione nativa */
        fprintf(stderr, "[gmb_gfx] window %dx%d (logical) renderScale=%.2f drawable=%dx%d\n",
                logW, logH, sc, ow, oh);
        { SDL_RendererInfo ri; if (SDL_GetRendererInfo(g_renderer, &ri) == 0 && ri.name)
            fprintf(stderr, "[gmb_gfx] renderer=%s flags=%s%s\n", ri.name,
                    (ri.flags & SDL_RENDERER_ACCELERATED) ? "ACCELERATED " : "",
                    (ri.flags & SDL_RENDERER_SOFTWARE) ? "SOFTWARE" : ""); }
        fflush(stderr);
        /* dims native post-scala */
        gmb_gfx_visible_pixels(&w, &h);
    }
    else
    {
        /* Ridimensiona la FINESTRA se la dimensione logica cambia (console a celle:
         * griglia x cella, come SdlConsole C#). */
        int logW = gmb_gfx_width(), logH = gmb_gfx_height();
        if (logW > 0 && logH > 0) { int ww = 0, wh = 0; SDL_GetWindowSize(g_window, &ww, &wh); if (ww != logW || wh != logH) SDL_SetWindowSize(g_window, logW, logH); }
#ifdef GMB_GFX_VULKAN
        if (g_vk_active) { g_tex_w = w; g_tex_h = h; return; }
#endif
    }
    if (!g_texture || g_tex_w != w || g_tex_h != h)
    {
        destroy_texture();
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING, w, h);
        g_tex_w = w; g_tex_h = h;
        /* Presentazione 1:1 pagina nativa -> drawable nativo (nessuna scala/filtro). */
    }
}

void gmb_gfx_present_start(void)
{
    /* Idempotente e NON tocca la coda input se SDL è già attivo: present_gfx la
     * chiama ogni frame, quindi un reset qui perderebbe gli eventi mouse/tastiera. */
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) return;
    gmb_gfx_input_reset();
    /* Font di default per il rendering celle→pixel (senza font, niente glifi). */
    if (!g_default_font_tried)
    {
        g_default_font_tried = 1;
        gmb_gfx_font_default(16);
    }
    /* SDL_INIT_EVENTS esplicito: con sdl2-compat (SDL3 sotto) gli eventi
     * sono un sottosistema separato e SDL_PushEvent senza init va in crash. */
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) g_owns_sdl_video = 1;
    {   /* Misura renderScale (HiDPI/Retina) PRIMA che il programma disegni, cosi'
         * gmb_gfx_screen() alloca pagine NATIVE e tutte le primitive/testo sono scalate
         * x renderScale (stesso modello del motore C#). Finestra nascosta usa-e-getta. */
        SDL_Window* tw = SDL_CreateWindow("probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
        if (tw) {
            SDL_Renderer* tr = SDL_CreateRenderer(tw, -1, SDL_RENDERER_ACCELERATED);
            int ow = 0, oh = 0, ww = 0, wh = 0;
            if (tr) { SDL_GetRendererOutputSize(tr, &ow, &oh); SDL_GetWindowSize(tw, &ww, &wh); SDL_DestroyRenderer(tr); }
            SDL_DestroyWindow(tw);
            float sc = (ow > 0 && ww > 0) ? (float)ow / (float)ww : 1.0f;
            { const char* ov = getenv("GMB_GFX_SCALE"); if (ov && *ov) sc = (float)atof(ov); }
            if (sc > 1.0f) { gmb_gfx_set_render_scale(sc); fprintf(stderr, "[gmb_gfx] probe renderScale=%.2f\n", sc); fflush(stderr); }
        }
    }
    fprintf(stderr, "[gmb_gfx] present_start: video=%d video_driver=%s\n",
            SDL_WasInit(SDL_INIT_VIDEO) != 0, SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");
    fflush(stderr);
}

void gmb_gfx_present_request_quit(void) { g_quit_requested = 1; }
void gmb_gfx_window_set_visible(int on) { g_vis_req = on ? 1 : 0; }

void gmb_gfx_present_tick(void)
{
    /* Hook di test FORM (main thread = unico scrittore delle code input): dopo
     * N frame inietta un click sintetico su coordinate LOGICHE (mappate a native
     * con render_scale). GMB_FORM_AUTOCLICK="frame,x,y". Vedi gmb_form.* . */
    {
        static long s_frame = 0;
        const char* ac = getenv("GMB_FORM_AUTOCLICK");
        if (ac && *ac)
        {
            /* Lista separata da ';': "frame,x,y;frame,x,y;..." (coord LOGICHE). */
            char buf[256]; snprintf(buf, sizeof(buf), "%s", ac);
            char* sv = NULL;
            for (char* it = strtok_r(buf, ";", &sv); it; it = strtok_r(NULL, ";", &sv))
            {
                int f = 0, lx = 0, ly = 0;
                if (sscanf(it, "%d,%d,%d", &f, &lx, &ly) == 3 && s_frame == f)
                {
                    /* coordinate LOGICHE di finestra (come il mouse reale da SDL). */
                    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_DOWN, (int)lx, (int)ly, 1, 0);
                    gmb_gfx_input_push_mouse(GMB_GFX_MOUSE_UP, (int)lx, (int)ly, 0, 0);
                }
            }
        }
        s_frame++;
    }
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT) g_quit_requested = 1;
        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
            g_quit_requested = 1;
        if (ev.type == SDL_WINDOWEVENT &&
            (ev.window.event == SDL_WINDOWEVENT_RESIZED || ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
            gmb_gfx_resize_screen(ev.window.data1, ev.window.data2);   /* desktop resizable */
        process_input_event(&ev);
    }
    if (g_vis_req >= 0 && g_window) { if (g_vis_req) { SDL_ShowWindow(g_window); SDL_RaiseWindow(g_window); } else SDL_HideWindow(g_window); g_vis_req = -1; }

    int w = 0, h = 0;
    const uint32_t* pixels = gmb_gfx_visible_pixels(&w, &h);
    if (!pixels || w <= 0 || h <= 0) return;

    /* Cap del present (~60 fps): il main loop gira a ~1ms; senza cap si presenta
     * a ~1000 fps -> tearing/repaint continui anche su contenuto statico. Gli
     * eventi SDL restano pompati ogni giro (sopra); qui si limita solo il blit. */
    { Uint32 now = SDL_GetTicks(); static Uint32 last = 0;
      if (last && (now - last) < 15) return;
      last = now; }

    /* Present SOLO se il framebuffer e' cambiato (hash FNV): a riposo zero
     * repaint, senza flag per-primitiva e senza che il loop eventi disegni.
     * Copre qualunque scrittura sulla pagina (primitive, testo, celle, blit). */
    {
        static uint32_t s_last_hash = 0; static int s_last_w = -1, s_last_h = -1;
        const unsigned char* bp = (const unsigned char*)pixels;
        size_t nbytes = (size_t)w * (size_t)h * 4u;
        uint32_t hh = 2166136261u;
        for (size_t i = 0; i < nbytes; i++) { hh ^= bp[i]; hh *= 16777619u; }
        if (!getenv("GMB_PRESENT_DEBUG") && hh == s_last_hash && w == s_last_w && h == s_last_h && g_pres_sprite_count == 0) return;
        s_last_hash = hh; s_last_w = w; s_last_h = h;
        if (getenv("GMB_PRESENT_DEBUG")) { static int _n=0; fprintf(stderr, "[PRESENT] %d\n", ++_n); }
    }

    ensure_window(w, h);
    /* ensure_window può cambiare lo scale (prima apertura) e riallocare la pagina:
     * rileggi il puntatore (nativo) prima di presentare. */
    pixels = gmb_gfx_visible_pixels(&w, &h);
    if (!pixels || w <= 0 || h <= 0) return;
#ifdef GMB_GFX_VULKAN
    if (g_vk_active)
    {
        gmb_gfx_vk_present(pixels, w, h);
        return;
    }
#endif
    if (!g_renderer || !g_texture) return;

    SDL_UpdateTexture(g_texture, NULL, pixels, w * (int)sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    {   /* Present a TUTTA la finestra: dst=NULL = intero render target nativo (drawable),
         * qualunque sia il renderScale/DPI. Prima si usava SDL_GetRendererOutputSize:
         * su Retina/sdl2-compat può tornare la dimensione LOGICA (640x480) e l'immagine
         * finiva in un quarto della finestra (bug Mandelbrot). */
        int _ow=0,_oh=0,_ww=0,_wh=0;
        SDL_GetRendererOutputSize(g_renderer,&_ow,&_oh);
        SDL_GetWindowSize(g_window,&_ww,&_wh);
        if (getenv("GMB_GFX_DBG")) fprintf(stderr,"[present] page=%dx%d tex=%dx%d scale=%.2f vis=%d px0=%08X pxm=%08X\n", w,h,g_tex_w,g_tex_h,gmb_gfx_render_scale(),gmb_gfx_visible_page(),pixels[0],pixels[(size_t)(h/2)*w + w/2]);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    }
    /* SPRITE GPU: composizione degli sprite registrati in questo frame (parità
     * col motore C#: RenderCopyEx), scalati dal page nativo al drawable. */
    if (g_pres_sprite_count > 0)
    {
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(g_renderer, &ow, &oh);
        if (ow <= 0 || oh <= 0) { ow = w; oh = h; }
        float fx = (w > 0) ? (float)ow / (float)w : 1.0f;
        float fy = (h > 0) ? (float)oh / (float)h : 1.0f;
        int vis = gmb_gfx_visible_page();
        for (int i = 0; i < g_pres_sprite_count; i++)
        {
            gmb_pres_sprite_t* s = &g_pres_sprites[i];
            if (s->page != vis) continue;
            SDL_Texture* tex = sprite_tex(s->id);
            if (!tex) continue;
            SDL_Rect dst = { (int)(s->x * fx), (int)(s->y * fy),
                             (int)(s->w * fx), (int)(s->h * fy) };
            SDL_RenderCopyEx(g_renderer, tex, NULL, &dst, s->rot, NULL, SDL_FLIP_NONE);
        }
        /* NON azzerare: la lista resta valida per la cattura offscreen
         * (SAVE SCREEN / IMAGEGET devono includere gli sprite GPU). */
        g_pres_frame_ready = 1;
    }
    else g_pres_frame_ready = 1;
    SDL_RenderPresent(g_renderer);
}

/* Cattura offscreen pagina+sprite GPU (SAVE SCREEN / IMAGEGET in finestra reale).
 * Ritorna 0 e riempie out (ARGB 0xAARRGGBB, w==pagina). -1 = non disponibile
 * (headless, thread non UI, nessun renderer): il chiamante usa la pagina. */
int gmb_gfx_present_capture(uint32_t* out, int w, int h)
{
    extern int gmb_gfx_present_video_is_real(void);
    extern int gmb_gfx_present_on_main_thread(void);
    if (!out || w <= 0 || h <= 0) return -1;
    if (!gmb_gfx_present_video_is_real() || !gmb_gfx_present_on_main_thread()) return -1;
    if (!g_renderer) return -1;
    const uint32_t* page = gmb_gfx_visible_pixels(NULL, NULL);
    if (!page) return -1;
    if (!g_texture || g_tex_w != w || g_tex_h != h)
    {
        if (g_texture) SDL_DestroyTexture(g_texture);
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!g_texture) return -1;
        g_tex_w = w; g_tex_h = h;
    }
    SDL_UpdateTexture(g_texture, NULL, page, w * (int)sizeof(uint32_t));
    SDL_Texture* target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_TARGET, w, h);
    if (!target) return -1;
    if (SDL_SetRenderTarget(g_renderer, target) != 0) { SDL_DestroyTexture(target); return -1; }
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    { int vis = gmb_gfx_visible_page();
      for (int i = 0; i < g_pres_sprite_count; i++)
      {
          gmb_pres_sprite_t* s = &g_pres_sprites[i];
          if (s->page != vis) continue;
          SDL_Texture* tex = sprite_tex(s->id);
          if (!tex) continue;
          SDL_Rect dst = { s->x, s->y, s->w, s->h };
          SDL_RenderCopyEx(g_renderer, tex, NULL, &dst, s->rot, NULL, SDL_FLIP_NONE);
      } }
    int rc = SDL_RenderReadPixels(g_renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                  out, w * (int)sizeof(uint32_t));
    SDL_SetRenderTarget(g_renderer, NULL);
    SDL_DestroyTexture(target);
    return rc == 0 ? 0 : -1;
}

/* ── Clipboard di sistema (SDL) ── */
void gmb_gfx_clipboard_set(const char* text)
{
    SDL_SetClipboardText(text ? text : "");
}

int gmb_gfx_clipboard_get(char* out, int maxlen)
{
    if (!out || maxlen <= 0) return 0;
    out[0] = '\0';
    char* text = SDL_GetClipboardText();
    if (!text) return 0;
    strncpy(out, text, (size_t)maxlen - 1);
    out[maxlen - 1] = '\0';
    SDL_free(text);
    return (int)strlen(out);
}

void gmb_gfx_clipboard_clear(void)
{
    SDL_SetClipboardText("");
}

/* ── Cursore / puntatore ── */
static SDL_Cursor* g_cursor = NULL;
static int g_mouse_lim[4]; /* x1,y1,x2,y2 in pixel SCHERMO; tutti 0 = nessun limite */
static int g_mouse_lim_on = 0;

void gmb_gfx_present_show_cursor(int visible)
{
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

void gmb_gfx_present_warp_mouse(int x, int y)
{
    if (!g_window) return;
    if (g_mouse_lim_on)
    {
        if (x < g_mouse_lim[0]) x = g_mouse_lim[0];
        if (y < g_mouse_lim[1]) y = g_mouse_lim[1];
        if (x > g_mouse_lim[2]) x = g_mouse_lim[2];
        if (y > g_mouse_lim[3]) y = g_mouse_lim[3];
    }
    int win_w = 0, win_h = 0, px_w = 0, px_h = 0;
    SDL_GetWindowSize(g_window, &win_w, &win_h);
    const uint32_t* pixels = gmb_gfx_visible_pixels(&px_w, &px_h);
    int wx = x, wy = y;
    if (pixels && px_w > 0 && px_h > 0 && win_w > 0 && win_h > 0)
    {
        wx = (int)((long)x * win_w / px_w);
        wy = (int)((long)y * win_h / px_h);
    }
    SDL_WarpMouseInWindow(g_window, wx, wy);
}

void gmb_gfx_present_set_cursor(int cursor_id)
{
    SDL_Cursor* cur = SDL_CreateSystemCursor((SDL_SystemCursor)cursor_id);
    if (!cur) return;
    SDL_SetCursor(cur);
    if (g_cursor) SDL_FreeCursor(g_cursor);
    g_cursor = cur;
}

void gmb_gfx_present_mouse_limit(int x1, int y1, int x2, int y2)
{
    if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0) { g_mouse_lim_on = 0; return; }
    g_mouse_lim[0] = x1 < x2 ? x1 : x2;
    g_mouse_lim[1] = y1 < y2 ? y1 : y2;
    g_mouse_lim[2] = x1 < x2 ? x2 : x1;
    g_mouse_lim[3] = y1 < y2 ? y2 : y1;
    g_mouse_lim_on = 1;
}

int gmb_gfx_present_should_quit(void)
{
    return g_quit_requested;
}

void gmb_gfx_present_stop(void)
{
    gmb_gfx_input_reset();
    g_pres_sprite_count = 0;
    for (int i = 0; i < 256; i++)
        if (g_spr_cache[i].tex) { SDL_DestroyTexture(g_spr_cache[i].tex); g_spr_cache[i].tex = NULL; g_spr_cache[i].px = NULL; }
#ifdef GMB_GFX_VULKAN
    if (g_vk_active) { gmb_gfx_vk_stop(); g_vk_active = 0; }
#endif
    g_mouse_lim_on = 0;
    if (g_cursor) { SDL_FreeCursor(g_cursor); g_cursor = NULL; }
    destroy_texture();
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    if (g_owns_sdl_video) { SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS); g_owns_sdl_video = 0; }
    g_quit_requested = 0;
}


/* Chiude la finestra grafica (SCREEN 0) ma mantiene SDL inizializzato, cosi' un
 * successivo SCREEN>0 la riapre. */
void gmb_gfx_present_close_window(void)
{
    destroy_texture();
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    g_tex_w = g_tex_h = 0;
}

/* Crea SUBITO la finestra reale e determina il renderScale nativo PRIMA che il programma
 * disegni (chiamata da gmb_gfx_screen dopo l'allocazione della pagina). Senza, il probe
 * su finestra nascosta misurava 1 e la pagina nasceva a bassa risoluzione; al primo present
 * la scala passava a 2 e il contenuto restava nell'angolo (o veniva upscalato). */
void gmb_gfx_present_ensure(void)
{
    if (!gmb_gfx_screen_on()) return;
    int w = 0, h = 0;
    const uint32_t* p = gmb_gfx_visible_pixels(&w, &h);
    if (p && w > 0 && h > 0) ensure_window(w, h);
}

/* 1 = driver video REALE (Cocoa/X11/…): la tastiera della finestra va letta dall'SDL.
 * 0 = "dummy"/headless (test, CI): INKEY/WAITKEY mantengono il comportamento ESC. */
int gmb_gfx_present_video_is_real(void)
{
    const char* drv = SDL_GetCurrentVideoDriver();
    return (drv && *drv && strcmp(drv, "dummy") != 0) ? 1 : 0;
}

#else
int gmb_gfx_present_video_is_real(void) { return 0; }
/* Stub richiesto dai consumer che NON definiscono GMB_GFX_SDL_PRESENT (gate  * runtime c99n headless): gmb_graphics_texture.c chiama gmb_gfx_present_push_sprite
 * (guardata solo a runtime da gmb_gfx_present_video_is_real, non dal preprocessore) →
 * senza questo stub il link fallisce con "Undefined _gmb_gfx_present_push_sprite". */
void gmb_gfx_present_push_sprite(int id, int x, int y, int w, int h, double rot)
{
    (void)id; (void)x; (void)y; (void)w; (void)h; (void)rot;
}
#endif /* GMB_GFX_SDL_PRESENT */
