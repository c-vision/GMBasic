/* ============================================================================
 * gmb_tui_app.c — layer App (porting BlinkClock.cs/EventLoop.cs/ModalHost.cs/
 * IInputSource.cs/EventDispatcher.cs/ClipperInputSource.cs). Headless.
 * ============================================================================ */
#include "gmb_tui_app.h"
extern gmb_control_t* g_reg_ctrl[]; extern int g_reg_n; extern int g_reg_focus;
#include "gmb_ui_gfx.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_io.h"
#include "gmb_tui_clipper.h"
#include "gmb_graphics.h"
#include "gmb_form.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_tui_mouse.h"
#include "gmb_sdl_console.h"
#include "gmb_screen_config.h"
#include "gmb_gfx_input.h"
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

extern int gmb_gfx_screen_on(void);
extern void gmb_tui_screen_flush(void);

static long app_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ===================== BlinkClock ===================== */

void gmb_blink_clock_init(gmb_blink_clock_t* c)
{
    memset(c, 0, sizeof(*c));
    memset(c, 0, sizeof(*c));
    c->period_ms = GMB_BLINK_MS;
    c->blink_on = true;
}

void gmb_blink_clock_reset(gmb_blink_clock_t* c)
{
    c->next_flip = app_now_ms() + c->period_ms;
    c->has_flip = true;
    c->blink_on = true;
}

bool gmb_blink_clock_tick(gmb_blink_clock_t* c, gmb_clipper_state_t* st)
{
    long now = app_now_ms();
    if (!c->has_flip)
    {
        c->next_flip = now + c->period_ms;
        c->has_flip = true;
    }
    if (now < c->next_flip) return false;
    c->next_flip = now + c->period_ms;

    if (st && !st->blink_enabled)
    {
        c->blink_on = true;
        if (st) st->blink_on = true;
        return false;
    }
    c->blink_on = !c->blink_on;
    if (st) st->blink_on = c->blink_on;
    return true;
}

/* ===================== EventLoop ===================== */

void gmb_event_loop_init(gmb_event_loop_t* l, const gmb_input_source_t* input)
{
    memset(l, 0, sizeof(*l));
    memset(l, 0, sizeof(*l));
    if (input) l->input = *input;
    gmb_blink_clock_init(&l->clock);
}

int gmb_event_loop_next(gmb_event_loop_t* l, int timeout_ms, gmb_ui_event_t* out)
{
    if (l->input.read)
    {
        int r = l->input.read(l->input.user, timeout_ms, out);
        if (r != 0 && out->type != GMB_UIEVENT_NONE)
        {
            gmb_blink_clock_reset(&l->clock);
            return r;
        }
        return r;
    }
    if (out) out->type = GMB_UIEVENT_NONE;
    return 0;
}

bool gmb_event_loop_blink_tick(gmb_event_loop_t* l, gmb_clipper_state_t* st)
{
    return gmb_blink_clock_tick(&l->clock, st);
}

/* ===================== ModalHost ===================== */

static gmb_modal_host_t* g_current_host = NULL;

void gmb_modal_host_init(gmb_modal_host_t* h, const gmb_input_source_t* input)
{
    memset(h, 0, sizeof(*h));
    memset(h, 0, sizeof(*h));
    if (input) h->input = *input;
    gmb_event_loop_init(&h->loop, &h->input);
}

gmb_event_loop_t* gmb_modal_host_current_loop(gmb_event_loop_t* fallback)
{
    if (g_current_host) return &g_current_host->loop;
    return fallback;
}

void gmb_modal_host_run_modal(gmb_modal_host_t* h, int (*modal)(const gmb_ui_event_t* ev, void* ctx), void* ctx)
{
    gmb_modal_host_t* prev = g_current_host;
    g_current_host = h;
    while (true)
    {
        gmb_ui_event_t ev;
        gmb_event_loop_next(&h->loop, 50, &ev);
        if (ev.type == GMB_UIEVENT_NONE && !gmb_event_loop_blink_tick(&h->loop, NULL))
            continue;
        if (!modal(&ev, ctx))
            break;
    }
    g_current_host = prev;
}

/* ===================== ClipperInputSource headless ===================== */

static int clipper_source_read(void* user, int timeout_ms, gmb_ui_event_t* out)
{
    (void)timeout_ms;
    gmb_clipper_state_t* st = (gmb_clipper_state_t*)user;
    if (!out) return 0;
    int k = gmb_clipper_input_inkey(st);
    if (k == 0)
    {
        out->type = GMB_UIEVENT_NONE;
        return 0;
    }
    if (k <= GMB_MOUSE_BASE)
    {
        gmb_mouse_action_t ma;
        if (gmb_mouse_action_from_inkey(k, st, &ma))
        {
            out->type = GMB_UIEVENT_MOUSE;
            out->mouse = ma;
            return 1;
        }
    }
    out->type = GMB_UIEVENT_KEY;
    out->key = k;
    out->mods = GMB_MODKEY_NONE;
    return 1;
}

void gmb_clipper_input_source_init(gmb_input_source_t* src, gmb_clipper_state_t* st)
{
    memset(src, 0, sizeof(*src));
    src->user = st;
    src->read = clipper_source_read;
}

/* ===================== EventDispatcher ===================== */

void gmb_event_dispatcher_init(gmb_event_dispatcher_t* d)
{
    memset(d, 0, sizeof(*d));
    memset(d, 0, sizeof(*d));
}

/* Crea (calloc) un dispatcher per i client C# via P/Invoke; vive per il processo. */
gmb_event_dispatcher_t* gmb_event_dispatcher_new(void)
{
    return (gmb_event_dispatcher_t*)calloc(1, sizeof(gmb_event_dispatcher_t));
}

void gmb_event_dispatcher_register_key(gmb_event_dispatcher_t* d, int combo,
                                       void (*fn)(void* ctx), void* ctx)
{
    if (d->key_combo_count >= GMB_DISPATCH_MAX || !fn) return;
    d->key_combos[d->key_combo_count] = combo;
    d->key_actions[d->key_combo_count] = fn;
    d->key_ctxs[d->key_combo_count] = ctx;
    d->key_combo_count++;
}

void gmb_event_dispatcher_register_window(gmb_event_dispatcher_t* d, int kind,
                                          void (*fn)(void* ctx), void* ctx)
{
    gmb_event_dispatcher_register_window_h(d, kind, -1, fn, ctx);
}

void gmb_event_dispatcher_register_window_h(gmb_event_dispatcher_t* d, int kind, int handle,
                                            void (*fn)(void* ctx), void* ctx)
{
    if (d->win_count >= GMB_DISPATCH_MAX || !fn) return;
    /* Una sola registrazione per (evento, handle): la successiva sostituisce la
     * precedente (parità tabella runtime C e VM/C). */
    for (int i = 0; i < d->win_count; i++)
        if (d->win_kinds[i] == kind && d->win_handles[i] == handle)
        {
            d->win_actions[i] = fn;
            d->win_ctxs[i] = ctx;
            return;
        }
    d->win_kinds[d->win_count] = kind;
    d->win_handles[d->win_count] = handle;
    d->win_actions[d->win_count] = fn;
    d->win_ctxs[d->win_count] = ctx;
    d->win_count++;
}

void gmb_event_dispatcher_register_mouse(gmb_event_dispatcher_t* d, int kind,
                                         void (*fn)(void* ctx, int x, int y), void* ctx)
{
    if (d->mouse_count >= GMB_DISPATCH_MAX || !fn) return;
    d->mouse_kinds[d->mouse_count] = kind;
    d->mouse_actions[d->mouse_count] = fn;
    d->mouse_ctxs[d->mouse_count] = ctx;
    d->mouse_count++;
}

int gmb_event_dispatcher_dispatch_key(gmb_event_dispatcher_t* d, int combo)
{
    int fired = 0;
    for (int i = 0; i < d->key_combo_count; i++)
    {
        if (d->key_combos[i] == combo)
        {
            d->key_actions[i](d->key_ctxs[i]);
            fired++;
        }
    }
    return fired > 0;
}

void gmb_event_dispatcher_dispatch_window(gmb_event_dispatcher_t* d, int kind)
{
    gmb_event_dispatcher_dispatch_window_h(d, kind, -1);
}

void gmb_event_dispatcher_dispatch_window_h(gmb_event_dispatcher_t* d, int kind, int handle)
{
    for (int i = 0; i < d->win_count; i++)
        if (d->win_kinds[i] == kind && (d->win_handles[i] < 0 || d->win_handles[i] == handle))
            d->win_actions[i](d->win_ctxs[i]);
}

void gmb_event_dispatcher_dispatch_mouse(gmb_event_dispatcher_t* d, int kind, int x, int y)
{
    for (int i = 0; i < d->mouse_count; i++)
        if (d->mouse_kinds[i] == kind)
            d->mouse_actions[i](d->mouse_ctxs[i], x, y);
}

int gmb_event_dispatcher_has_key(gmb_event_dispatcher_t* d, int combo)
{
    for (int i = 0; i < d->key_combo_count; i++)
        if (d->key_combos[i] == combo) return 1;
    return 0;
}

void gmb_event_dispatcher_unregister_key(gmb_event_dispatcher_t* d, int combo)
{
    int w = 0;
    for (int i = 0; i < d->key_combo_count; i++)
    {
        if (d->key_combos[i] == combo) continue;
        d->key_combos[w] = d->key_combos[i];
        d->key_actions[w] = d->key_actions[i];
        d->key_ctxs[w] = d->key_ctxs[i];
        w++;
    }
    d->key_combo_count = w;
}

void gmb_event_dispatcher_unregister_window(gmb_event_dispatcher_t* d, int kind)
{
    gmb_event_dispatcher_unregister_window_h(d, kind, -1);
}

void gmb_event_dispatcher_unregister_window_h(gmb_event_dispatcher_t* d, int kind, int handle)
{
    int w = 0;
    for (int i = 0; i < d->win_count; i++)
    {
        if (d->win_kinds[i] == kind && d->win_handles[i] == handle) continue;
        d->win_kinds[w] = d->win_kinds[i];
        d->win_handles[w] = d->win_handles[i];
        d->win_actions[w] = d->win_actions[i];
        d->win_ctxs[w] = d->win_ctxs[i];
        w++;
    }
    d->win_count = w;
}

void gmb_event_dispatcher_unregister_mouse(gmb_event_dispatcher_t* d, int kind)
{
    int w = 0;
    for (int i = 0; i < d->mouse_count; i++)
    {
        if (d->mouse_kinds[i] == kind) continue;
        d->mouse_kinds[w] = d->mouse_kinds[i];
        d->mouse_actions[w] = d->mouse_actions[i];
        d->mouse_ctxs[w] = d->mouse_ctxs[i];
        w++;
    }
    d->mouse_count = w;
}

/* ===================== AppRunner ===================== */

void gmb_app_runner_init(gmb_app_runner_t* r)
{
    memset(r, 0, sizeof(*r));
    memset(r, 0, sizeof(*r));
}
gmb_app_runner_t* gmb_app_runner_on_key(gmb_app_runner_t* r, int key, void (*fn)(void* app_ctx))
{
    if (!r) return r;
    for (int i = 0; i < r->key_count; i++)
    {
        if (r->key_codes[i] == key) { r->key_handlers[i] = fn; return r; }
    }
    if (r->key_count < GMB_APP_RUNNER_MAX_KEYS)
    {
        r->key_codes[r->key_count] = key;
        r->key_handlers[r->key_count] = fn;
        r->key_count++;
    }
    return r;
}
gmb_app_runner_t* gmb_app_runner_on_mouse(gmb_app_runner_t* r, void (*fn)(const gmb_mouse_action_t* action, void* app_ctx))
{
    if (!r) return r;
    r->mouse_handler = fn;
    return r;
}
gmb_app_runner_t* gmb_app_runner_on_resize(gmb_app_runner_t* r, void (*fn)(int w, int h, void* app_ctx))
{
    if (!r) return r;
    r->resize_handler = fn;
    return r;
}
gmb_app_runner_t* gmb_app_runner_on_event(gmb_app_runner_t* r, void (*fn)(const gmb_ui_event_t* ev, void* app_ctx))
{
    if (!r) return r;
    r->global_handler = fn;
    return r;
}
void gmb_app_runner_set_render(gmb_app_runner_t* r, void (*fn)(gmb_screen_buffer_t* sb, void* ctx), void* ctx)
{
    if (!r) return;
    r->render = fn;
    r->render_ctx = ctx;
}

static void app_runner_position_cursor(const gmb_app_t* app, gmb_clipper_state_t* st)
{
    if (!app || !app->caret) return;
    int cx = 0, cy = 0;
    app->caret(app->user, &cx, &cy);
    bool vis = app->cursor_visible ? app->cursor_visible(app->user) : true;
    char esc[64];
    gmb_termutils_move_to(cx + 1, cy + 1, esc, (int)sizeof(esc));
    gmb_termutils_show_cursor(vis, esc, (int)sizeof(esc));
}

void gmb_app_runner_run(gmb_app_runner_t* r, const gmb_app_t* app, gmb_input_source_t* input,
                        gmb_clipper_state_t* st, int width, int height)
{
    if (!r || !app) return;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    gmb_screen_buffer_t* sb = gmb_screen_new(width, height);
    if (!sb) return;
    if (app->resize) app->resize(app->user, width, height);
    if (app->draw) app->draw(app->user, sb);
    if (r->render) r->render(sb, r->render_ctx);
    app_runner_position_cursor(app, st);

    gmb_event_loop_t fallback_loop;
    gmb_event_loop_init(&fallback_loop, input);
    gmb_event_loop_t* loop = gmb_modal_host_current_loop(&fallback_loop);
    gmb_blink_clock_reset(&loop->clock);

    while (app->close_requested ? !app->close_requested(app->user) : true)
    {
        gmb_ui_event_t ev;
        memset(&ev, 0, sizeof(ev));
        int rv = gmb_event_loop_next(loop, 50, &ev);
        if (rv == 0 || ev.type == GMB_UIEVENT_NONE)
        {
            if (gmb_event_loop_blink_tick(loop, st))
            {
                if (app->draw) app->draw(app->user, sb);
                if (r->render) r->render(sb, r->render_ctx);
                app_runner_position_cursor(app, st);
            }
            continue;
        }
        switch (ev.type)
        {
            case GMB_UIEVENT_RESIZE:
                if (ev.w > 0 && ev.h > 0)
                {
                    gmb_screen_resize(sb, ev.w, ev.h);
                    if (app->resize) app->resize(app->user, ev.w, ev.h);
                    if (r->resize_handler) r->resize_handler(ev.w, ev.h, r->resize_app_ctx);
                }
                break;
            case GMB_UIEVENT_KEY:
                for (int i = 0; i < r->key_count; i++)
                {
                    if (r->key_codes[i] == ev.key && r->key_handlers[i])
                        r->key_handlers[i](app->user);
                }
                break;
            case GMB_UIEVENT_MOUSE:
                if (r->mouse_handler) r->mouse_handler(&ev.mouse, r->mouse_app_ctx);
                break;
            default:
                break;
        }
        if (r->global_handler) r->global_handler(&ev, r->global_app_ctx);
        if (app->handle_event) app->handle_event(app->user, &ev);
        if (app->draw) app->draw(app->user, sb);
        if (r->render) r->render(sb, r->render_ctx);
        app_runner_position_cursor(app, st);
    }
    gmb_screen_free(sb);
}

/* ===================== Control modal interattivo ===================== */

static gmb_clipper_state_t* g_modal_st = NULL;
static gmb_screen_buffer_t* g_modal_buf = NULL;
static gmb_sdl_console_t* g_app_sdl = NULL;   /* SdlConsole a celle attiva (app SDL) */

static int modal_source_read(void* user, int timeout_ms, gmb_ui_event_t* out)
{
    (void)timeout_ms;
    /* SdlConsole attiva: gli eventi SDL vanno pompati QUI, altrimenti il modale non
     * riceve mai i tasti (l'app possiede il loop). Il terminale resta invariato. */
    if (g_app_sdl) gmb_tui_sdl_pump();
    gmb_clipper_state_t* st = (gmb_clipper_state_t*)user;
    if (st && st->typeahead_count > 0)
    {
        int k = gmb_clipper_input_inkey(st);
        if (k != 0)
        {
            out->type = GMB_UIEVENT_KEY;
            out->key = k;
            out->mods = GMB_MODKEY_NONE;
            return 1;
        }
    }
    return gmb_raw_input_reader_read_event_core(st, out);
}

void gmb_tui_inject_keys(const char* s)
{
    if (!g_modal_st) g_modal_st = gmb_clipper_state_new(80, 25);
    gmb_clipper_input_keyboard(g_modal_st, s);
}

gmb_clipper_state_t* gmb_tui_global_state(void)
{
    if (!g_modal_st) g_modal_st = gmb_clipper_state_new(80, 25);
    return g_modal_st;
}

/* Buffer modale condiviso (per SAVE/RESTORE SCREEN headless): NULL finché nessun
 * modale/TUI lo crea. Chi salva con buffer assente ottiene snapshot vuoto. */
gmb_screen_buffer_t* gmb_tui_modal_buffer(void)
{
    return g_modal_buf;
}

int gmb_tui_global_typeahead_read(void)
{
    gmb_clipper_state_t* st = gmb_tui_global_state();
    if (st && st->typeahead_count > 0)
    {
        int k = gmb_clipper_input_inkey(st);
        if (k != 0) return k;
    }
    return -1;
}

void gmb_tui_global_typeahead_clear(void)
{
    gmb_clipper_state_t* st = gmb_tui_global_state();
    if (!st) return;
    st->typeahead_count = 0;
    st->typeahead_head = 0;
    st->typeahead_tail = 0;
    st->has_pending = false;
    st->pending_key = 0;
}

/* Target del disegno modale: con screen grafico attivo il controllo va nel buffer TUI
 * globale (st->buffer) cosi' il compositor/present lo rende DENTRO la finestra (o a celle
 * se nessuna finestra); altrimenti nel buffer modale separato (ANSI, vecchio path). */
static gmb_screen_buffer_t* modal_target_buffer(void)
{
    gmb_clipper_state_t* st = gmb_clipper_global_state();
    if (g_app_sdl && st && st->buffer) return st->buffer;   /* SdlConsole a celle: controlli QUI */
    if (GMB_GFX_SCREEN_ON() && st && st->buffer) return st->buffer;
    if (!g_modal_buf)
    {
        int w = 80, h = 25;
        gmb_terminal_size(&w, &h);
        if (w < 20) w = 80;
        if (h < 8) h = 25;
        g_modal_buf = gmb_screen_new(w, h);
    }
    return g_modal_buf;
}

static int modal_control_cb(const gmb_ui_event_t* ev, void* ctx)
{
    gmb_control_t* ctrl = (gmb_control_t*)ctx;
    if (ev->type == GMB_UIEVENT_KEY)
    {
        if (ev->key == GMB_K_ENTER || ev->key == GMB_K_ESC)
            return 0;
        gmb_control_handle_key(ctrl, ev->key);
    }
    else if (ev->type == GMB_UIEVENT_MOUSE)
    {
        gmb_control_handle_mouse(ctrl, &ev->mouse);
    }
    gmb_screen_buffer_t* buf = modal_target_buffer();
    if (buf == g_modal_buf)
    {
        gmb_screen_clear(buf, GMB_CELLCOLOR_BLACK, GMB_CELLCOLOR_BLACK);
    }
    else
    {
        /* Nel buffer globale cancella SOLO l'area del controllo (non la finestra). */
        int x0 = ctrl->x, y0 = ctrl->y, x1 = ctrl->x + ctrl->width, y1 = ctrl->y + ctrl->height;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > gmb_screen_width(buf))  x1 = gmb_screen_width(buf);
        if (y1 > gmb_screen_height(buf)) y1 = gmb_screen_height(buf);
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++)
                gmb_screen_setchar(buf, xx, yy, ' ', GMB_CELLCOLOR_WHITE, GMB_CELLCOLOR_BLUE);
    }
    gmb_surface_set_autoattach(0);   /* rendering A CELLE (coerente con compositor/SDLConsole) */
    gmb_control_draw(ctrl, buf);
    gmb_surface_set_autoattach(1);
    if (g_app_sdl)
    {
        gmb_tui_sdl_present();    /* SdlConsole a celle: il controllo si rende nella finestra */
    }
    else if (GMB_GFX_SCREEN_ON())
    {
        gmb_tui_screen_flush();   /* present sul main thread — nessun ANSI su stdout */
    }
    else
    {
        char out[65536];
        size_t n = gmb_render_screen(buf, out, sizeof(out));
        FILE* co = gmb_tui_console_out();   /* console di sistema, non il canale dati */
        if (n && co) { fwrite(out, 1, n, co); fflush(co); }
    }
    return 1;
}

int gmb_tui_run_control_modal(gmb_control_t* ctrl)
{
    if (!g_modal_st) g_modal_st = gmb_clipper_state_new(80, 25);
    if (!g_modal_buf) g_modal_buf = gmb_screen_new(80, 25);
    gmb_input_source_t src;
    src.user = g_modal_st;
    src.read = modal_source_read;
    gmb_modal_host_t host;
    gmb_modal_host_init(&host, &src);
    gmb_modal_host_run_modal(&host, modal_control_cb, ctrl);
    return 0;
}


/* ===== Registry controlli TUI NON-bloccante (mirror del pixel gmb_form_*) =====
 * Il vecchio path modale (gmb_tui_run_control_modal) BLOCCA in attesa di input.
 * Qui i controlli si REGISTRANO e vengono disegnati/drenati da un frame che ritorna. */
#define GMB_TUI_CTRL_MAX 256

/* Canale tasti HOST: la frame cell accoda i tasti NON consumati dal controllo focalizzato
 * (es. F3/F5/F7... shortcut dell'host). L'host li legge con gmb_tui_host_key_pop(). */
#define GMB_HOST_KEYQ 64
static int g_hkq[GMB_HOST_KEYQ];
static int g_hkq_head = 0, g_hkq_tail = 0;
void gmb_tui_host_key_push(int k)
{
    int nxt = (g_hkq_tail + 1) % GMB_HOST_KEYQ;
    if (nxt == g_hkq_head) return;   /* pieno: scarta */
    g_hkq[g_hkq_tail] = k; g_hkq_tail = nxt;
}
int gmb_tui_host_key_pop(void)
{
    if (g_hkq_head == g_hkq_tail) return 0;
    int k = g_hkq[g_hkq_head]; g_hkq_head = (g_hkq_head + 1) % GMB_HOST_KEYQ;
    return k;
}
static int g_tui_firing_control = -1;
static int g_tui_firing_event = 0;


/* Registra/rimuove (cb==NULL) un gestore evento su un controllo per HANDLE. */
void gmb_tui_set_event_id(int handle, int ev, gmb_tui_cb cb)
{
    if (handle < 0 || handle >= g_reg_n || ev <= 0 || ev >= GMB_TUI_EV_COUNT) return;
    g_reg_ctrl[handle]->ev_cb[ev] = cb;
}
/* §11.7-3: eventi UserControl (thunk + istanza) per (handle, ev) — priorità sul ev_cb. */
static void (*g_uc_ev_fn[GMB_TUI_CTRL_MAX][GMB_TUI_EV_COUNT])(void*);
static void* g_uc_ev_inst[GMB_TUI_CTRL_MAX][GMB_TUI_EV_COUNT];
/* Innalza un evento: aggiorna last_event e chiama la cb registrata. */
void gmb_tui_fire(int handle, int ev)
{
    if (handle < 0 || handle >= g_reg_n || ev <= 0 || ev >= GMB_TUI_EV_COUNT) return;
    g_reg_ctrl[handle]->last_event = ev;
    if (g_uc_ev_fn[handle][ev]) {   /* §11.7-3: evento UserControl → metodo della classe.
                                     * GPM attivo → TASK GPM (mai BASIC inline sul thread dell'evento);
                                     * percorso lineare (nessuno scheduler) → singolo esecutore, inline. */
        extern void gmb_task_spawn(void (*fn)(void*), void* arg);
        extern int  gmb_c99gpm_active(void);
        g_tui_firing_control = handle; g_tui_firing_event = ev;
        if (gmb_c99gpm_active()) gmb_task_spawn(g_uc_ev_fn[handle][ev], g_uc_ev_inst[handle][ev]);
        else g_uc_ev_fn[handle][ev](g_uc_ev_inst[handle][ev]);
        g_tui_firing_control = -1; g_tui_firing_event = 0; return;
    }
    if (g_reg_ctrl[handle]->ev_cb[ev]) { g_tui_firing_control = handle; g_tui_firing_event = ev; g_reg_ctrl[handle]->ev_cb[ev](); g_tui_firing_control = -1; g_tui_firing_event = 0; }
}
/* §11.7-3: cabla un metodo-evento di una classe UserControl sul firing dell'evento. */
void gmb_tui_bind_usercontrol_event(int handle, int ev, void (*fn)(void*), void* inst)
{
    if (handle < 0 || handle >= GMB_TUI_CTRL_MAX || ev <= 0 || ev >= GMB_TUI_EV_COUNT) return;
    g_uc_ev_fn[handle][ev] = fn; g_uc_ev_inst[handle][ev] = inst;
}
static int tui_last_event_of(int handle) { return (handle >= 0 && handle < g_reg_n) ? g_reg_ctrl[handle]->last_event : 0; }
double gmb_tui_clicked(int handle)    { return tui_last_event_of(handle) == 1 ? 1.0 : 0.0; } /* 1=CLICK */
double gmb_tui_changed(int handle)    { return tui_last_event_of(handle) == 3 ? 1.0 : 0.0; } /* 3=CHANGE */
double gmb_tui_gotfocused(int handle) { return tui_last_event_of(handle) == 4 ? 1.0 : 0.0; }
double gmb_tui_lostfocused(int handle){ return tui_last_event_of(handle) == 5 ? 1.0 : 0.0; }
int    gmb_tui_last_handle(void)      { return g_reg_n > 0 ? g_reg_n - 1 : -1; }
int    gmb_tui_firing_control(void)   { return g_tui_firing_control; }
int    gmb_tui_firing_event(void)     { return g_tui_firing_event; }
int  gmb_tui_control_count(void) { return g_reg_n; }
gmb_control_t* gmb_tui_control_find_by_name(const char* name)
{
    if (!name || !*name) return NULL;
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg_ctrl[i] && strcmp(g_reg_ctrl[i]->name, name) == 0) return g_reg_ctrl[i];
    return NULL;
}

gmb_control_t* gmb_tui_control_at(int i) { return (i >= 0 && i < g_reg_n) ? g_reg_ctrl[i] : NULL; }

/* Riga selezionata di un controllo TBROWSE registrato (indice nel registry). -1 se non tbrowse. */
long long gmb_tui_control_selrow(int i)
{
    gmb_control_t* c = gmb_tui_control_at(i);
    if (!c || c->ctrl_kind != GMB_CK_TBROWSE) return -1;
    return gmb_browse_row_pos((const gmb_browse_t*)c);   /* il ctrl e' il PRIMO campo di gmb_browse_t */
}

/* Origine assoluta del contenuto di un controllo ospite (nome): il contenitore può essere una
 * window (per nome) oppure un altro controllo/UserControl (per nome) — risoluzione ricorsiva. */
int gmb_tui_control_content_origin(const char* name, int* out_x, int* out_y)
{
    extern int gmb_winstmt_content_origin(const char* name, int* out_x, int* out_y);
    if (!name || !*name) return 0;
    gmb_control_t* c = gmb_tui_control_find_by_name(name);
    if (!c) return 0;
    int x = c->x, y = c->y;
    if (c->container[0])
    {
        int ox = 0, oy = 0;
        if (gmb_winstmt_content_origin(c->container, &ox, &oy)) { x += ox; y += oy; }
        else if (gmb_tui_control_content_origin(c->container, &ox, &oy)) { x += ox; y += oy; }
    }
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    return 1;
}

/* Un frame NON bloccante: drena l'input disponibile (typeahead), dispatcha al controllo
 * focalizzato, disegna i controlli registrati, presenta, e RITORNA. Stesso modello del pixel
 * (gmb_form_frame): niente attesa bloccante. */
/* Lookup per HANDLE (non per indice di registry): gmb_tui_control_at indicizza il vettore
 * e in grafica l'handle del form puo coincidere con un indice TUI non correlato. */
static gmb_control_t* ctrl_by_handle(int handle)
{
    if (handle < 0) return NULL;
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg_ctrl[i] && g_reg_ctrl[i]->handle == handle) return g_reg_ctrl[i];
    return NULL;
}

void gmb_control_apply_props_handle(int handle, unsigned flags, const char* name, const char* tag, const char* tooltip,
                                    int tabindex, int visible, int enabled, int tabstop)
{
    /* Handles dei due registry si SOVRAPPONGONO (form 0..31/1000+i, TUI 0..63): applicare
     * "a entrambi" significava rinominare il controllo sbagliato (es. il PANEL form con
     * handle 0 veniva rinominato con il NAME del LABEL TUI, anch'esso handle 0).
     * Decide il registry dell'ULTIMO controllo creato, come per le clausole di stile. */
    extern int gmb_form_last_is_tui(void);
    gmb_control_t* c = NULL;
    if (gmb_form_last_is_tui()) {
        c = ctrl_by_handle(handle);
    } else {
        extern void gmb_form_apply_props_handle(int, unsigned, const char*, const char*, const char*, int, int, int, int);
        gmb_form_apply_props_handle(handle, flags, name, tag, tooltip, tabindex, visible, enabled, tabstop);
    }
    if (!c) return;
    if (flags & 1u)  snprintf(c->name, sizeof(c->name), "%s", name ? name : "");
    if (flags & 2u)  snprintf(c->tag, sizeof(c->tag), "%s", tag ? tag : "");
    if (flags & 4u)  snprintf(c->tooltip, sizeof(c->tooltip), "%s", tooltip ? tooltip : "");
    if (flags & 8u)  c->tab_index = tabindex;
    if (flags & 16u) c->visible = visible ? true : false;
    if (flags & 32u) c->enabled = enabled ? true : false;
    if (flags & 64u) c->tab_stop = tabstop ? true : false;
    if (flags & 128u) c->focused = true;
}

static void apply_style_pair(gmb_cell_color_t* fg, bool* hf, gmb_cell_color_t* bg, bool* hb, const char* tok)
{
    const char* slash = strchr(tok, '/');
    if (slash) {
        char f[64]; int fn = (int)(slash - tok); if (fn > 63) fn = 63;
        for (int i = 0; i < fn; i++) f[i] = tok[i];
        f[fn] = '\0';
        *fg = gmb_clipper_parse_color(f); *hf = true;
        *bg = gmb_clipper_parse_color(slash + 1); *hb = true;
    } else { *fg = gmb_clipper_parse_color(tok); *hf = true; }
}

static void apply_style_kv(gmb_control_t* c, const char* spec)
{
    const char* p = spec;
    while (p && *p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (strncasecmp(p, "fg=", 3) == 0 || strncasecmp(p, "bg=", 3) == 0) {
            int is_fg = (p[0]=='f'||p[0]=='F'); p += 3; char tok[64]; int n = 0;
            while (*p && *p != ' ' && *p != ',' && n < 63) tok[n++] = *p++;
            tok[n] = '\0';
            gmb_cell_color_t col;
            if (tok[0] == '#') {
                unsigned v = 0; int k = 0;
                for (const char* q = tok + 1; k < 6 && isxdigit((unsigned char)*q); k++, q++)
                    v = v*16 + (unsigned)(isdigit((unsigned char)*q)?(unsigned)(*q-'0'):(unsigned)(tolower(*q)-'a'+10));
                col = (gmb_cell_color_t){ (uint8_t)((v>>16)&0xFF), (uint8_t)((v>>8)&0xFF), (uint8_t)(v&0xFF), false };
            } else col = gmb_clipper_parse_color(tok);
            if (is_fg) { c->style.fg = col; c->style.has_fg = true; } else { c->style.bg = col; c->style.has_bg = true; }
        } else if (strncasecmp(p, "focuscolor=", 11) == 0 || strncasecmp(p, "errorcolor=", 11) == 0
                   || strncasecmp(p, "selcolor=", 9) == 0 || strncasecmp(p, "color=", 6) == 0) {
            int kind = strncasecmp(p, "focuscolor=", 11) == 0 ? 1
                     : strncasecmp(p, "errorcolor=", 11) == 0 ? 2
                     : strncasecmp(p, "selcolor=", 9) == 0 ? 3 : 0;
            p += (kind == 1 || kind == 2) ? 11 : (kind == 3 ? 9 : 6);
            char tok[64]; int n = 0; while (*p && *p != ' ' && n < 63) tok[n++] = *p++;
            tok[n] = '\0';
            if (kind == 1) apply_style_pair(&c->style.fg_focus, &c->style.has_fg_focus, &c->style.bg_focus, &c->style.has_bg_focus, tok);
            else if (kind == 2) apply_style_pair(&c->style.fg_error, &c->style.has_fg_error, &c->style.bg_error, &c->style.has_bg_error, tok);
            else if (kind == 3) apply_style_pair(&c->style.sel_fg, &c->style.has_sel_fg, &c->style.sel_bg, &c->style.has_sel_bg, tok);
            else apply_style_pair(&c->style.fg, &c->style.has_fg, &c->style.bg, &c->style.has_bg, tok);
        } else if (strncasecmp(p, "uppercase", 9) == 0) { c->uppercase = true; p += 9; }
        else if (strncasecmp(p, "name=", 5) == 0)    { p += 5; char tok[64]; int n = 0; if (*p == '"') p++; while (*p && *p != '"' && *p != ' ' && n < 63) tok[n++] = *p++; tok[n] = 0; snprintf(c->name, sizeof(c->name), "%s", tok); continue; }
        else if (strncasecmp(p, "tag=", 4) == 0)     { p += 4; char tok[64]; int n = 0; if (*p == '"') p++; while (*p && *p != '"' && *p != ' ' && n < 63) tok[n++] = *p++; tok[n] = 0; snprintf(c->tag, sizeof(c->tag), "%s", tok); continue; }
        else if (strncasecmp(p, "tooltip=", 8) == 0) { p += 8; char tok[128]; int n = 0; if (*p == '"') p++; while (*p && *p != '"' && *p != ' ' && n < 127) tok[n++] = *p++; tok[n] = 0; snprintf(c->tooltip, sizeof(c->tooltip), "%s", tok); continue; }
        else if (strncasecmp(p, "visible=", 8) == 0) { p += 8; c->visible = (atoi(p) != 0); }
        else if (strncasecmp(p, "enabled=", 8) == 0) { p += 8; c->enabled = (atoi(p) != 0); }
        else if (strncasecmp(p, "tabindex=", 9) == 0) { p += 9; c->tab_index = atoi(p); }
        else if (strncasecmp(p, "tabstop=", 8) == 0) { p += 8; c->tab_stop = (atoi(p) != 0); }
        else if (strncasecmp(p, "metric=", 7) == 0) {
            /* Spazio coordinate DICHIARATO del controllo (0=default/segue SCREEN,
             * 1=CellsToPixel, 2=PixelToCells). Stessa proprietà di CONTROL SET "metric". */
            p += 7;
            c->coord_mode = atoi(p);
        }
        else if (strncasecmp(p, "font=", 5) == 0) {
            /* Dimensione font ESPLICITA del controllo (vedi gmb_control_t.font_size). */
            p += 5;
            c->font_size = atoi(p);
        }
        while (*p && *p != ' ') p++;
    }
}
void gmb_control_apply_style_handle(int handle, const char* spec)
{
    gmb_control_t* c = ctrl_by_handle(handle);
    if (c && spec) apply_style_kv(c, spec);
}
void gmb_control_apply_style(gmb_control_t* c, const char* spec)
{
    if (c && spec) apply_style_kv(c, spec);
}

/* Fallback PIXEL per i controlli senza draw_px_fn: rende il controllo nel suo
 * renderer a celle (dentro un buffer temporaneo) e lo blitta come pixel alla sua
 * origine. Cosi' OGNI controllo ha una rappresentazione grafica, con il tema,
 * senza dover scrivere un renderer pixel per ciascuno. */
static void draw_cells_px_fallback(gmb_control_t* c, int cw, int ch)
{
    int w = c->width, h = c->height;
    if (w <= 0 || h <= 0) return;
    if ((long)w * h > 64 * 64) return;
    int px = c->x, py = c->y;   /* gia' in pixel (chi le chiama ha gia' scalato) */
    gmb_screen_buffer_t* b = gmb_screen_new(w, h);
    if (!b) return;
    gmb_widget_style_t s = gmb_widget_style_current();
    gmb_cell_color_t bgrgb; bgrgb.r = (uint8_t)((s.fill >> 16) & 0xFF); bgrgb.g = (uint8_t)((s.fill >> 8) & 0xFF); bgrgb.b = (uint8_t)(s.fill & 0xFF); bgrgb.is_default = 0;
    gmb_cell_color_t fgrgb; fgrgb.r = (uint8_t)((s.text >> 16) & 0xFF); fgrgb.g = (uint8_t)((s.text >> 8) & 0xFF); fgrgb.b = (uint8_t)(s.text & 0xFF); fgrgb.is_default = 0;
    gmb_screen_clear(b, fgrgb, bgrgb);
    int sx = c->x, sy = c->y;
    c->x = 0; c->y = 0;
    /* Renderer a CELLE diretto (senza autoattach: il sink gfx su un buffer
     * temporaneo ha cella 0 e manda in crash il present). */
    if (c->draw_fn) c->draw_fn(c->user, c, b);
    c->x = sx; c->y = sy;
    int n = w * h;
    int* chars = (int*)malloc((size_t)n * sizeof(int));
    unsigned* fg = (unsigned*)malloc((size_t)n * sizeof(unsigned));
    unsigned* bg = (unsigned*)malloc((size_t)n * sizeof(unsigned));
    if (chars && fg && bg)
    {
        gmb_screen_cell_t* cells = gmb_screen_cells(b);
        for (int i = 0; i < n; i++)
        {
            gmb_screen_cell_t* cc = &cells[i];
            chars[i] = (int)cc->character;
            fg[i] = cc->fg_palette < 0 ? (((unsigned)cc->fg_rgb.r << 16) | ((unsigned)cc->fg_rgb.g << 8) | cc->fg_rgb.b)
                                       : (unsigned)(s.text & 0xFFFFFFu);
            bg[i] = cc->bg_palette < 0 ? (((unsigned)cc->bg_rgb.r << 16) | ((unsigned)cc->bg_rgb.g << 8) | cc->bg_rgb.b)
                                       : (unsigned)(s.fill & 0xFFFFFFu);
        }
        GMB_GFX_RENDER_CELLS_AT(chars, fg, bg, w, h, cw, ch, px, py);
    }
    free(chars); free(fg); free(bg);
    gmb_screen_free(b);
}
/* Dirty del render pixel dei controlli: senza questo OGNI frame ridisegna tutto il
 * testo (rasterizzazione costosa) e la finestra sembra bloccata. */
static int s_px_dirty = 1;
/* Invadlidazione del livello pixel: i setter di proprietà dei controlli TUI la chiamano,
 * altrimenti una modifica (es. visible=0) resta invisibile finché il form non si ridisegna. */
void gmb_tui_mark_dirty(void) { s_px_dirty = 1; }
static double s_px_last = 0.0;
void gmb_tui_px_mark_dirty(void) { s_px_dirty = 1; }

void gmb_tui_px_invalidate(void) { s_px_dirty = 1; }

/* 1 se il controllo ha un POPUP aperto che disegna FUORI dal proprio rettangolo
 * (lista della combobox, menu, context menu, calendario). Serve a due cose:
 *  - disegnare questi controlli per ULTIMI (sopra tutto il resto);
 *  - testarli per PRIMI nel mouse (altrimenti il click sul popup va al controllo
 *    che sta sotto). Era il difetto "la select appare in secondo piano / non si
 *    seleziona", presente in OGNI controllo con popup. */
int gmb_control_popup_open(gmb_control_t* c)
{
    if (!c || !c->user) return 0;
    switch (c->ctrl_kind)
    {
        case GMB_CK_COMBOBOX:    return ((gmb_widget_t*)c->user)->open ? 1 : 0;
        case GMB_CK_MENU:        return ((gmb_widget_t*)c->user)->open ? 1 : 0;
        case GMB_CK_CONTEXTMENU: return ((gmb_context_menu_t*)c->user)->open ? 1 : 0;
        case GMB_CK_DATEPICKER:  return ((gmb_date_picker_t*)c->user)->open ? 1 : 0;
        default: return 0;
    }
}

static void tui_draw_px_all_impl(int force);
void gmb_tui_draw_px_all(void)
{
    /* Con un MODALE aperto il livello TUI lo disegna il frame modale (PRIMA del velo):
     * qui non si tocca nulla, altrimenti i controlli ricomparirebbero SOPRA il velo. */
    { extern int gmb_form_modal_active(void); if (gmb_form_modal_active()) return; }
    tui_draw_px_all_impl(0);
}
/* Usata SOLO dal frame modale: disegna il livello TUI sotto il velo. */
void gmb_tui_draw_px_all_force(void) { tui_draw_px_all_impl(1); }
static void tui_draw_px_all_impl(int force)
{
    /* Ridisegna solo se sporco o ogni 200 ms (spinner/timer). */
    { double now = (double)app_now_ms();
      if (!force && !s_px_dirty && (now - s_px_last) < 200.0) return;
      s_px_dirty = 0; s_px_last = now; }
    /* Render pixel ricco dei controlli registrati (chiamato dal present GUI, main thread).
     * I controlli sono posizionati in CELLE: qui si converte il rect in PIXEL (cw,ch) e si
     * ripristina dopo la chiamata (draw_px_fn lavora in pixel, es. gmb_px_draw_ex). */
    extern int gmb_winstmt_content_origin(const char* name, int* out_x, int* out_y);
    int cw = 8, ch = 16;
    int filt = 0;
    { extern int gmb_form_damage_active(void); extern int gmb_form_damage_hits(int,int,int,int);
      filt = gmb_form_damage_active(); (void)gmb_form_damage_hits; }
    { extern gmb_clipper_state_t* gmb_clipper_peek_state(void);
      gmb_clipper_state_t* st = gmb_clipper_peek_state();
      if (st && st->buffer) {
          int bw = gmb_screen_width(st->buffer), bh = gmb_screen_height(st->buffer);
          /* Unità LOGICHE: gmb_gfx_* applica la render-scale al suo interno. */
          int pw = GMB_GFX_SCREEN_W();
          int ph = GMB_GFX_SCREEN_H();
          if (bw > 0 && pw > 0) cw = pw / bw;
          if (bh > 0 && ph > 0) ch = ph / bh;
      } }
    if (cw < 1) cw = 8; if (ch < 1) ch = 16;
    /* DUE PASSATE: prima i controlli normali, poi quelli con POPUP APERTO.
     * Un popup disegna fuori dal proprio rect (lista combobox, menu, context menu,
     * calendario): se lo si disegna nel suo turno, i controlli successivi lo
     * ricoprono. Con la seconda passata il popup è sempre sopra. */
    for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg_ctrl[i])
        {
            gmb_control_t* cc = g_reg_ctrl[i];
            if (!cc->visible) continue;   /* CONTROL SET name,"visible",0 → non disegnato */
            if ((gmb_control_popup_open(cc) ? 1 : 0) != pass) continue;
            /* Spazio coordinate DICHIARATO (SCREEN/USE o `USE` sul controllo).
             * Default (metric 0 + SCREEN AUTO) = PIXEL in grafica: nessuna conversione.
             * Solo una dichiarazione esplicita (USE CELLS / metric 1) moltiplica per (cw,ch);
             * le dimensioni DERIVATE dal runtime (coord_auto_*) non si convertono. */
            int tr = gmb_control_coord_transform(cc);
            cc->window_kind = gmb_base_window_kind();
            cc->px_x = cc->x; cc->px_y = cc->y;
            cc->px_w = cc->width; cc->px_h = cc->height;
            if (tr > 0)
            {
                cc->px_x = cc->x * cw; cc->px_y = cc->y * ch;
                if (!cc->coord_auto_w) cc->px_w = cc->width * cw;
                if (!cc->coord_auto_h) cc->px_h = cc->height * ch;
            }
            /* Altezza in CELLE (1) su un controllo con renderer pixel: il renderer lavora in
             * pixel, quindi un h=1 lo fa sparire (STARRATING, SCROLLBAR, FKEYBAR…).
             * Si usa l'altezza riga del tema. */
            if (cc->px_h <= 2) { gmb_widget_style_t st = gmb_widget_style_current();
                                 if (st.row_h > 0) cc->px_h = st.row_h; }
            if (filt) { extern int gmb_form_damage_hits(int,int,int,int);
                        int dx, dy, dw, dh;
                        { extern void gmb_control_paint_rect(gmb_control_t*, int*, int*, int*, int*);
                          gmb_control_paint_rect(cc, &dx, &dy, &dw, &dh); }
                        if (!gmb_form_damage_hits(dx, dy, dw, dh)) continue; }
            { extern int gmb_form_modal_active(void); extern gmb_control_t* gmb_form_modal_control(void);
              if (gmb_form_modal_active() && gmb_form_modal_control() == cc) continue; }
            if (cc->draw_px_fn) cc->draw_px_fn(cc->user, cc);
            else draw_cells_px_fallback(cc, cw, ch);
        }}

void gmb_tui_refresh_window(void)
{
    for (int i = 0; i < g_reg_n; i++)
    {
        if (!g_reg_ctrl[i]) continue;
        g_reg_ctrl[i]->window_handle = gmb_base_window_root()->handle;
        g_reg_ctrl[i]->window_kind = gmb_base_window_kind();
    }
}

/* Tastiera → finestra FOCALIZZATA. Il focus dei controlli è un indice globale
 * (g_reg_focus): quando la finestra attiva cambia (click, SELECT, WINDOW … ACTIVATE) il
 * controllo in focus può appartenere a una finestra di sfondo e continuare a ricevere i
 * tasti. Qui il focus si sposta sul primo controllo eleggibile della finestra attiva.
 * I controlli senza container (top-level) restano eleggibili sempre. */
static void tui_key_focus_sync(void)
{
    if (g_reg_n <= 0) return;
    extern const char* gmb_winstmt_active(void);
    const char* act = gmb_winstmt_active();
    if (!act) act = "";
    gmb_control_t* cur = (g_reg_focus >= 0 && g_reg_focus < g_reg_n) ? g_reg_ctrl[g_reg_focus] : NULL;
    if (cur && (!cur->container[0] || strcmp(cur->container, act) == 0)) return;
    for (int i = 0; i < g_reg_n; i++)
    {
        gmb_control_t* cc = g_reg_ctrl[i];
        if (!cc) continue;
        if (!cc->container[0] || strcmp(cc->container, act) == 0)
        {
            if (i != g_reg_focus) { g_reg_focus = i; gmb_tui_fire(i, 4); }  /* GOTFOCUS */
            return;
        }
    }
}

/* Applica un tasto ai controlli (fuoco/tab/ESC/host-key). Condivisa tra typeahead e tty. */
static void tui_process_key(int k)
{
    /* F1..F10: barra tasti GLOBALE (come Clipper), indipendente dal focus. */
    if (k >= GMB_K_F1 && k <= GMB_K_F10)
        for (int i = 0; i < g_reg_n; i++)
            if (g_reg_ctrl[i] && g_reg_ctrl[i]->ctrl_kind == GMB_CK_FKEYBAR &&
                g_reg_ctrl[i]->handle_key_fn &&
                g_reg_ctrl[i]->handle_key_fn(g_reg_ctrl[i]->user, g_reg_ctrl[i], k))
                return;
    /* Menu aperto: i tasti (frecce/Enter/ESC) vanno al MENU, anche se il focus è altrove. */
    if (gmb_tui_menu_any_open())
    {
        for (int i = 0; i < g_reg_n; i++)
        {
            gmb_control_t* c = g_reg_ctrl[i];
            if (c && c->ctrl_kind == GMB_CK_MENU && ((gmb_widget_t*)c)->open)
            { if (gmb_control_handle_key(c, k)) return; break; }
        }
    }
    if (k == '\t' && g_reg_n > 0) {
        extern const char* gmb_winstmt_active(void);
        const char* act = gmb_winstmt_active();
        for (int t = 0; t < g_reg_n; t++) {
            g_reg_focus = (g_reg_focus + 1) % g_reg_n;
            gmb_control_t* cc = g_reg_ctrl[g_reg_focus];
            if (cc && (!cc->container[0] || strcmp(cc->container, act) == 0)) break;
        }
        return;
    }
    if (k == 27) { extern void gmb_form_request_close(void); gmb_form_request_close(); return; } /* ESC = quit host */
    tui_key_focus_sync();   /* tastiera → finestra focalizzata (mai al controllo di una finestra di sfondo) */
    if (g_reg_focus >= 0 && g_reg_focus < g_reg_n)
    {
        int f = g_reg_focus;
        gmb_tui_fire(f, 4);                                  /* GOTFOCUS al primo uso */
        if (gmb_control_handle_key(g_reg_ctrl[f], k))
        {
            gmb_tui_fire(f, 3);                              /* CHANGE */
            if (k == 13) gmb_tui_fire(f, GMB_EV_SELCHANGE);  /* SELECT (Enter) verso l'host */
        }
        else if (k > 0) gmb_tui_host_key_push(k);            /* tasto non consumato -> host */
    }
    else if (k > 0) gmb_tui_host_key_push(k);                /* nessun focus -> host */
}

/* Hit-test + dispatch di un'azione mouse al controllo sotto il cursore (+ focus). */
static int g_evt_px = 0;   /* evento mouse in coordinate PIXEL (graphics) */
static void tui_process_mouse(const gmb_mouse_action_t* ma)
{
    /* DUE PASSATE: prima i controlli con POPUP APERTO (sono disegnati sopra, quindi
     * devono ricevere per primi il click), poi gli altri. L'ordine dentro la passata
     * resta inverso alla registrazione (l'ultimo creato è il più in alto). */
    for (int pass = 1; pass >= 0; pass--)   /* popup (1) PRIMA: sono disegnati sopra */
    for (int i = g_reg_n - 1; i >= 0; i--)
    {
        gmb_control_t* cc = g_reg_ctrl[i];
        if (!cc) continue;
        /* Un controllo NASCOSTO non deve catturare il mouse: prima un controllo
         * invisibile di un'altra scheda (rect sovrapposto) si mangiava il click e il
         * controllo visibile sotto non rispondeva ("sembrano solo disegnati"). */
        if (!cc->visible) continue;
        if ((gmb_control_popup_open(cc) ? 1 : 0) != pass) continue;
        { extern int gmb_form_modal_active(void);
          if (gmb_form_modal_active()) continue; }   /* il modale ha già gestito l'evento */
        /* Lo spazio dell'EVENTO dipende dal backend (pixel in grafica, celle in TUI);
         * lo spazio del CONTROLLO e' quello DICHIARATO (gl_control_coord_transform).
         * La stessa funzione usata dal render garantisce disegno e click coerenti. */
        int tr = gmb_control_coord_transform(cc);
        int hx, hy, hw, hh;
        if (g_evt_px)                      /* evento in PIXEL */
        {
            /* AREA = rettangolo ∪ popup aperto (regola unica col render). */
            { extern void gmb_control_hit_rect(gmb_control_t*, int*, int*, int*, int*);
              gmb_control_hit_rect(cc, &hx, &hy, &hw, &hh); }

        }
        else                               /* evento in CELLE */
        {
            if (tr < 0) { hx = cc->x / GMB_CELL_PX_W; hy = cc->y / GMB_CELL_PX_H;
                          hw = cc->width / GMB_CELL_PX_W; hh = cc->height / GMB_CELL_PX_H;
                          if (hw < 1) hw = 1; if (hh < 1) hh = 1; }
            else        { hx = cc->x; hy = cc->y; hw = cc->width; hh = cc->height; }
        }
        int inside = (ma->x >= hx && ma->x < hx + hw && ma->y >= hy && ma->y < hy + hh);
        if (!inside && cc->hit_extra_fn) inside = cc->hit_extra_fn(cc->user, cc, ma->x, ma->y);
        if (inside)
        {
            gmb_control_handle_mouse(cc, ma);
            g_reg_focus = i;
            return;
        }
    }
}

/* Valore corrente di un controllo TUI (per la sincronizzazione INTO del BASIC). */
static double tui_ctrl_value(gmb_control_t* c)
{
    if (!c) return 0.0;
    switch (c->ctrl_kind)
    {
    case GMB_CK_CHECKBOX: return ((gmb_widget_t*)c)->ival != 0 ? 1.0 : 0.0;
    case GMB_CK_TOGGLE:   return ((gmb_widget_t*)c)->ival != 0 ? 1.0 : 0.0;
    case GMB_CK_SLIDER:   return ((gmb_widget_t*)c)->ival;
    case GMB_CK_COMBOBOX: return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_LISTBOX:  return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_SPINNER:  return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_RADIO:    return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_TAB:      return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_NUMERICUPDOWN: return ((gmb_widget_t*)c)->ival;
    case GMB_CK_STARRATING:    return ((gmb_widget_t*)c)->ival;
    case GMB_CK_SCROLLBAR:     return ((gmb_widget_t*)c)->ival;
    case GMB_CK_DATALIST:      return (double)(int)((gmb_widget_t*)c)->ival;
    case GMB_CK_CONTEXTMENU:   return (double)((gmb_context_menu_t*)c)->chosen;
    default: return 0.0;
    }
}

/* Testo "corrente" per INTO stringa (TV: il testo selezionato dell'item). */
static const char* tui_ctrl_text(gmb_control_t* c)
{
    if (!c) return "";
    switch (c->ctrl_kind)
    {
    case GMB_CK_COMBOBOX: return gmb_combobox_selected_value((gmb_combobox_t*)c);
    case GMB_CK_LISTBOX:  return gmb_listbox_selected_value((gmb_listbox_t*)c);
    case GMB_CK_RADIO:    return gmb_radio_group_selected_value((gmb_radio_group_t*)c);
    case GMB_CK_MENU:     return gmb_menu_get_selected_label((gmb_menu_t*)c);
    case GMB_CK_TAB: { gmb_widget_t* t = (gmb_widget_t*)c; int s = (int)t->ival; return (s >= 0 && s < t->nitems) ? t->items[s] : ""; }
    case GMB_CK_DATALIST: { gmb_widget_t* d = (gmb_widget_t*)c; int si = (int)d->ival; return (si >= 0 && si < d->nitems) ? d->items[si] : ""; }
    default: return "";
    }
}

static void tui_bind_str_sync(gmb_control_t* c)
{
    if (!c || !c->bind_str) return;
    const char* txt = tui_ctrl_text(c);
    const char* cur = c->bind_str->data ? c->bind_str->data : "";
    if (strcmp(txt, cur) != 0) *c->bind_str = gmb_string_alloc(txt);
}

/* Collega/sgancia (out==NULL) la variabile INTO a un controllo del registry. */
/* Sincronizza le variabili INTO dei controlli TUI (usata anche in GRAPHICS:
 * il frame TUI non gira li', ma i controlli cell-based restano vivi). */
void gmb_tui_sync_bindings(void)
{
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg_ctrl[i]) {
            if (g_reg_ctrl[i]->bind_value)
                *g_reg_ctrl[i]->bind_value = tui_ctrl_value(g_reg_ctrl[i]);
            tui_bind_str_sync(g_reg_ctrl[i]);
        }
}

void gmb_tui_bind_value(int handle, double* out)
{
    if (handle < 0 || handle >= g_reg_n) return;
    g_reg_ctrl[handle]->bind_value = out;
}

void gmb_tui_bind_str(int handle, gmb_string_t* out)
{
    if (handle < 0 || handle >= g_reg_n) return;
    g_reg_ctrl[handle]->bind_str = out;
    tui_bind_str_sync(g_reg_ctrl[handle]);   /* valore subito leggibile (parità IL) */
}

static int g_gfxq[128]; static int g_gfxq_h = 0, g_gfxq_t = 0;
void gmb_tui_dispatch_mouse_px(int type, int x, int y, int button, int wheel)
{
    (void)button;
    int cw = 8, ch = 16;
    { gmb_clipper_state_t* st = gmb_clipper_peek_state();
      if (st && st->buffer) {
          int bw = gmb_screen_width(st->buffer), bh = gmb_screen_height(st->buffer);
          if (bw > 0) cw = GMB_GFX_SCREEN_W() / bw;
          if (bh > 0) ch = GMB_GFX_SCREEN_H() / bh;
      } }
    if (cw < 1) cw = 8; if (ch < 1) ch = 16;
    gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
    /* Vocabolario per VALORE: un tipo touch (TOUCH_DOWN=5/TOUCH_UP=6) cadeva
     * nel ramo finale e diventava un MOVE, quindi il dito non cliccava mai un
     * controllo TUI. Si normalizza con la stessa regola dei form. */
    type = gmb_gfx_input_touch_to_mouse(type);
    if (type < 0) return;   /* LONGPRESS: gia' in pressione */
    if (type == GMB_GFX_MOUSE_DOWN) ma.kind = GMB_MA_CLICK;
    else if (type == GMB_GFX_MOUSE_UP) ma.kind = GMB_MA_RELEASE;
    else if (type == GMB_GFX_MOUSE_WHEEL) ma.kind = (wheel > 0) ? GMB_MA_SCROLLUP : GMB_MA_SCROLLDOWN;
    else ma.kind = GMB_MA_MOVE;
    (void)cw; (void)ch;
    ma.x = x; ma.y = y;   /* PIXEL: il render dei controlli e in pixel (draw_px_fn) */
    /* Repaint incrementale: sporca solo i controlli TUI sotto il puntatore (quello
     * che riceve l'evento). Senza, ogni movimento/click ridisegnava tutto il livello. */
    { extern int gmb_form_damage_hits(int,int,int,int);
      for (int i = 0; i < g_reg_n; i++)
      {
          gmb_control_t* cc = g_reg_ctrl[i];
          if (!cc || !cc->visible) continue;
          int rx, ry, rw, rh;
          { extern void gmb_control_paint_rect(gmb_control_t*, int*, int*, int*, int*);
            gmb_control_paint_rect(cc, &rx, &ry, &rw, &rh); }
          if (x >= rx && x < rx + rw && y >= ry && y < ry + rh)
          { extern void gmb_control_invalidate_paint(gmb_control_t*); gmb_control_invalidate_paint(cc); }
      } }
    g_evt_px = 1;
    s_px_dirty = 1;
    tui_process_mouse(&ma);
    g_evt_px = 0;
}
void gmb_tui_process_pending_mouse(void)
{
    int mt, mx, my, mb, mw;
    while (gmb_form_poll_mouse(&mt, &mx, &my, &mb, &mw))
    {
        gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
        /* Stessa normalizzazione della corsia px (touch -> mouse). */
        mt = gmb_gfx_input_touch_to_mouse(mt);
        if (mt < 0) continue;
        if (mt == GMB_GFX_MOUSE_DOWN) ma.kind = GMB_MA_CLICK;
        else if (mt == GMB_GFX_MOUSE_UP) ma.kind = GMB_MA_RELEASE;
        else if (mt == GMB_GFX_MOUSE_WHEEL) ma.kind = (mw > 0) ? GMB_MA_SCROLLUP : GMB_MA_SCROLLDOWN;
        else ma.kind = GMB_MA_MOVE;
        ma.x = mx; ma.y = my;
        tui_process_mouse(&ma);
    }
}
void gmb_tui_process_pending_keys(void)
{
    s_px_dirty = 1;
    while (g_gfxq_h != g_gfxq_t) { int k = g_gfxq[g_gfxq_h]; g_gfxq_h = (g_gfxq_h + 1) % 128; tui_process_key(k); }
    /* Tasti arrivati dalla finestra SDL (gmb_tui_sdl_pump li mette nella TYPEAHEAD del clipper):
     * qui vengono consumati dai controlli. Serve alle app che compongono finestre WINDOW +
     * controlli e quindi NON chiamano gmb_tui_controls_frame (che ripulirebbe il desktop). */
    { gmb_clipper_state_t* st = gmb_tui_global_state();
      while (st && st->typeahead_count > 0)
      {
          int k = gmb_clipper_input_inkey(st);
          if (k == 0) break;
          tui_process_key(k);
      } }
}
void gmb_tui_push_gfx_key(int k)
{
    int n = (g_gfxq_t + 1) % 128;
    if (n == g_gfxq_h) return;
    g_gfxq[g_gfxq_t] = k; g_gfxq_t = n;
}
/* Disegna TUTTI i controlli rispettando lo Z-ORDER di Turbo Vision: i MENU APERTI vanno
 * per ULTIMI, così la dropdown sta sopra gli altri controlli. Prima si disegnava in ordine
 * di registrazione: il menu (indice 0) finiva SOTTO i controlli disegnati dopo. */
void gmb_tui_draw_controls(gmb_screen_buffer_t* buf)
{
    if (!buf) return;
    /* Questi controlli sono a CELLE: va disattivato l'auto-attach e va RIMOSSO un
     * eventuale sink già agganciato al buffer (gmb_screen_setchar, con sink, dirotta le
     * scritture sulla pagina gfx e le celle restano STANTIE → righe con caratteri
     * vecchi/casuali, es. listbox). Il sink viene ripristinato dopo. */
    gmb_surface_set_autoattach(0);
    gmb_screen_sink_t* saved_sink = gmb_screen_get_sink(buf);
    if (saved_sink) gmb_screen_set_sink(buf, NULL);
    for (int i = 0; i < g_reg_n; i++)
    {
        gmb_control_t* c = g_reg_ctrl[i];
        if (!c || !c->visible) continue;
        if (c->ctrl_kind == GMB_CK_MENU && ((gmb_widget_t*)c)->open) continue;
        /* Spazio coordinate DICHIARATO: `USE PIXELS` (metric 2, o SCREEN USE PIXELS) su un
         * backend a celle → il controllo dichiara PIXEL e qui si riporta sulla griglia.
         * Default (metric 0 senza SCREEN) = celle: nessuna conversione. */
        int tr = gmb_control_coord_transform(c);
        if (tr < 0)
        {
            int sx = c->x, sy = c->y, sw = c->width, sh = c->height;
            c->x = sx / GMB_CELL_PX_W; c->y = sy / GMB_CELL_PX_H;
            if (!c->coord_auto_w) { c->width = sw / GMB_CELL_PX_W; if (c->width < 1) c->width = 1; }
            if (!c->coord_auto_h) { c->height = sh / GMB_CELL_PX_H; if (c->height < 1) c->height = 1; }
            gmb_control_draw(c, buf);
            c->x = sx; c->y = sy; c->width = sw; c->height = sh;
        }
        else gmb_control_draw(c, buf);
    }
    for (int i = 0; i < g_reg_n; i++)
    {
        gmb_control_t* c = g_reg_ctrl[i];
        if (!c) continue;
        if (c->ctrl_kind == GMB_CK_MENU && ((gmb_widget_t*)c)->open)
        {
            int tr = gmb_control_coord_transform(c);
            if (tr < 0)
            {
                int sx = c->x, sy = c->y, sw = c->width, sh = c->height;
                c->x = sx / GMB_CELL_PX_W; c->y = sy / GMB_CELL_PX_H;
                if (!c->coord_auto_w) { c->width = sw / GMB_CELL_PX_W; if (c->width < 1) c->width = 1; }
                if (!c->coord_auto_h) { c->height = sh / GMB_CELL_PX_H; if (c->height < 1) c->height = 1; }
                gmb_control_draw(c, buf);
                c->x = sx; c->y = sy; c->width = sw; c->height = sh;
            }
            else gmb_control_draw(c, buf);
        }
    }
    if (saved_sink) gmb_screen_set_sink(buf, saved_sink);
    gmb_surface_set_autoattach(1);
}

int gmb_tui_controls_frame(int frame_ms)
{
    int had_input = 0;
    /* Tasti dalla finestra grafica (SDL): vanno ai controlli TUI. */
    while (g_gfxq_h != g_gfxq_t) { int k = g_gfxq[g_gfxq_h]; g_gfxq_h = (g_gfxq_h + 1) % 128; tui_process_key(k); had_input = 1; }
    gmb_clipper_state_t* st = gmb_tui_global_state();
    /* 1) Tasti iniettati/scriptati (typeahead). */
    while (st && st->typeahead_count > 0)
    {
        int k = gmb_clipper_input_inkey(st);
        if (k == 0) break;
        had_input = 1;
        tui_process_key(k);
    }
    /* 2) Lettura dal TTY: un'app standalone (BASIC) non ha chi inietta tasti nella typeahead ->
     *    si legge /dev/tty in raw mode (non bloccante). */
    if (!had_input)
    {
        gmb_raw_input_reader_ensure_raw();
        if (gmb_raw_input_reader_has_byte())
        {
            gmb_ui_event_t ev; memset(&ev, 0, sizeof(ev));
            if (gmb_raw_input_reader_read_event_core(st, &ev) && ev.type != GMB_UIEVENT_NONE)
            {
                had_input = 1;
                if (ev.type == GMB_UIEVENT_KEY) tui_process_key(ev.key);
                else if (ev.type == GMB_UIEVENT_MOUSE) tui_process_mouse(&ev.mouse);
            }
        }
    }
    /* 3) Mouse emulato/iniettato dall'host C# (form). */
    {
        int mt, mx, my, mb, mw;
        while (gmb_form_poll_mouse(&mt, &mx, &my, &mb, &mw))
        {
            gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
            if (mt == 2) ma.kind = GMB_MA_CLICK;
            else if (mt == 3) ma.kind = GMB_MA_RELEASE;
            else if (mt == 4) ma.kind = (mw > 0) ? GMB_MA_SCROLLUP : GMB_MA_SCROLLDOWN;
            else ma.kind = GMB_MA_MOVE;
            ma.x = mx; ma.y = my;
            tui_process_mouse(&ma);
        }
    }
    /* Sincronizza le variabili INTO dei controlli TUI (stato persistente nel registry). */
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg_ctrl[i]) {
            if (g_reg_ctrl[i]->bind_value)
                *g_reg_ctrl[i]->bind_value = tui_ctrl_value(g_reg_ctrl[i]);
            tui_bind_str_sync(g_reg_ctrl[i]);
        }
    gmb_screen_buffer_t* buf = modal_target_buffer();
    gmb_surface_set_autoattach(0);
    /* Desktop ad OGNI frame (prima restavano le celle vecchie, es. la dropdown di un menu
     * appena chiuso). UNICA implementazione: gmb_theme_paint_desktop -> desktop + retino
     * dal TEMA (prima fg e bg erano lo stesso colore: retino invisibile). */
    gmb_theme_paint_desktop(buf);
    gmb_tui_draw_controls(buf);
    gmb_surface_set_autoattach(1);
    if (gmb_base_window_kind() == GMB_WIN_GRAPHICS) gmb_tui_screen_flush();
    else if (gmb_base_window_kind() == GMB_WIN_SDL || g_app_sdl)
    {
        /* SdlConsole a celle: la present la fa l'app con gmb_tui_sdl_present() (gmb_sdl_console_render).
         * Niente dump ANSI sul terminale. */
    }
    else
    {
        /* Render con DEDUP + DIFF: niente riscrittura quando nulla cambia, solo le celle
         * cambiate altrimenti. Al primo frame TEXT: pulizia schermo + cursore nascosto. */
        static gmb_screen_buffer_t* s_prev = NULL;
        static int s_have_prev = 0;
        static int s_cursor_hidden = 0;
        static gmb_cell_color_t s_fg = GMB_CELLCOLOR_DEFAULT;
        static gmb_cell_color_t s_bg = GMB_CELLCOLOR_DEFAULT;
        if (!s_cursor_hidden)
        {
            /* Primo frame TEXT: pulisci, nascondi il cursore, abilita il mouse reporting.
             * Sulla CONSOLE DI SISTEMA (gmb_tui_console_out): mai sul canale dati. */
            FILE* co0 = gmb_tui_console_out();
            if (co0) { fputs("\x1b[2J\x1b[H\x1b[?25l\x1b[?1000h\x1b[?1003h\x1b[?1006h", co0); fflush(co0); }
            gmb_clipper_mouse_enable();
            s_cursor_hidden = 1;
        }
        if (!s_prev) s_prev = gmb_screen_new(gmb_screen_width(buf), gmb_screen_height(buf));
        if (!s_have_prev || (gmb_screen_has_sink(buf) ? 0 : !gmb_screen_buffer_equal(buf, s_prev)))
        {
            char out[1 << 20];
            size_t n = gmb_render_screen_diff(buf, s_have_prev ? s_prev : NULL, &s_fg, &s_bg, out, sizeof(out));
            FILE* co = gmb_tui_console_out();
            if (n && co) { fwrite(out, 1, n, co); fflush(co); }
            if (s_prev) gmb_screen_buffer_copy_to(s_prev, buf);
            s_have_prev = 1;
        }
    }
    /* Park GPM (§0ter): dentro un task il sleep cooperativo libera il worker SENZA spin;
     * fuori da un task (main thread) breve attesa reale. MAI gmb_sleep_ms dentro un task:
     * blocca il worker e una successiva operazione con park (es. I/O file) va in deadlock. */
    if (!had_input && frame_ms > 0) {
        extern void* gmb_cur(void);
        extern void gmb_yield_ticks(long long ms);
        if (gmb_cur()) gmb_yield_ticks(frame_ms);
        else gmb_sleep_ms(frame_ms);
    }
    return 0;
}

/* ---- Coda eventi MOUSE verso l'host (BASIC): pop() ritorna il kind, poi x/y/ctrl ----
 * Ritorni `long long` (non `int`): la FFI legge il valore come int64 (gmb_int_t); un `int`
 * a 32-bit lascerebbe l'upper 32 non sign-extended (es. -1 -> 4294967295). */
static struct { int kind, x, y, ctrl; } g_host_mouse[64];
static int g_hm_head = 0, g_hm_count = 0, g_hm_kind = -1, g_hm_x = 0, g_hm_y = 0, g_hm_ctrl = -1;

void gmb_tui_host_mouse_push(int kind, int x, int y, int ctrl)
{
    if (g_hm_count >= 64) return;
    int idx = (g_hm_head + g_hm_count) % 64;
    g_host_mouse[idx].kind = kind; g_host_mouse[idx].x = x; g_host_mouse[idx].y = y; g_host_mouse[idx].ctrl = ctrl;
    g_hm_count++;
}
long long gmb_tui_host_mouse_pop(void)
{
    if (g_hm_count <= 0) return -1;
    g_hm_kind = g_host_mouse[g_hm_head].kind; g_hm_x = g_host_mouse[g_hm_head].x;
    g_hm_y = g_host_mouse[g_hm_head].y; g_hm_ctrl = g_host_mouse[g_hm_head].ctrl;
    g_hm_head = (g_hm_head + 1) % 64; g_hm_count--;
    return g_hm_kind;
}
long long gmb_tui_host_mouse_x(void) { return g_hm_x; }
long long gmb_tui_host_mouse_y(void) { return g_hm_y; }
long long gmb_tui_host_mouse_ctrl(void) { return g_hm_ctrl; }

/* Hit-test del mouse (coordinate in CELLE) sui controlli TUI registrati. kind: 0=move,1=down,2=up.
 * Usata dal pump della SdlConsole per far arrivare click/scroll ai controlli a celle. */
void gmb_tui_handle_mouse_cell(int x, int y, int kind)
{
    int k = (kind == 1) ? GMB_MA_CLICK : (kind == 2) ? GMB_MA_RELEASE : GMB_MA_MOVE;
    /* MENU APERTO: la dropdown copre i controlli sotto (CodeArea) -> il menu' ha la
     * PRIORITA' su click/hover. Il click su una voce ATTIVA l'item e NON muove il caret
     * del controllo sottostante (cursore CONGELATO: spento solo se coperto, riacceso alla
     * chiusura). Prima, click sulla dropdown cadeva sul CodeArea -> caret spostato. */
    if (gmb_tui_menu_any_open())
    {
        gmb_control_t* mo = NULL; int moi = -1;
        for (int i = g_reg_n - 1; i >= 0; i--)
        { gmb_control_t* c = g_reg_ctrl[i]; if (c && c->ctrl_kind == GMB_CK_MENU && ((gmb_widget_t*)c)->open) { mo = c; moi = i; break; } }
        if (mo)
        {
            gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
            ma.kind = k; ma.x = x; ma.y = y;
            int handled = gmb_control_handle_mouse(mo, &ma);
            /* gestito, oppure hover/release: consuma (NIENTE hit-test sotto -> caret fermo). */
            if (handled || kind != 1) { gmb_tui_host_mouse_push(kind, x, y, moi); return; }
            gmb_tui_menu_close_all();   /* click FUORI dal menu: chiudi e prosegui con l'hit-test */
        }
    }
    int hit = -1;
    for (int i = g_reg_n - 1; i >= 0; i--)
    {
        gmb_control_t* c = g_reg_ctrl[i];
        if (!c) continue;
        if (x >= c->x && x < c->x + c->width && y >= c->y && y < c->y + c->height)
        {
            gmb_mouse_action_t ma; memset(&ma, 0, sizeof(ma));
            ma.kind = k; ma.x = x; ma.y = y;
            gmb_control_handle_mouse(c, &ma);
            /* Il menu e' un OVERLAY: NON ruba il focus (il controllo sotto mantiene il caret,
             * cosi' alla chiusura il cursore si ripristina). Il menu riceve i tasti comunque. */
            if (c->ctrl_kind != GMB_CK_MENU) g_reg_focus = i;
            hit = i;
            break;
        }
    }
    /* Notifica l'host (l'app BASIC decide l'azione). */
    gmb_tui_host_mouse_push(kind, x, y, hit);
    (void)k;
}

/* ---- App SDL: SdlConsole a CELLE (finestra SDL che rende la griglia a celle) ----
 * Nel build C99Native l'app gira sul MAIN THREAD (il `main` generato chiama gmb_user_Main
 * direttamente), quindi e' corretta la console posseduta dall'app: start (crea+mostra),
 * pump (eventi SDL + tasti ai controlli), present (blink cursore + render). Niente finestre
 * dal worker: questo codice NON viene usato da task GPM secondari. */

void gmb_tui_sdl_start(int grid_w, int grid_h)
{
    if (g_app_sdl) return;
    gmb_screen_config_t cfg = gmb_screen_config_get_default(GMB_SM_TEXT);
    if (grid_w < 20) grid_w = 80;
    if (grid_h < 8) grid_h = 25;
    cfg.char_grid_width = grid_w;
    cfg.char_grid_height = grid_h;
    g_app_sdl = gmb_sdl_console_new(&cfg, grid_w, grid_h);
    if (!g_app_sdl) return;
    gmb_sdl_console_show(g_app_sdl);
    gmb_sdl_console_set_cursor_visible(g_app_sdl, 1);
    gmb_screen_buffer_t* cb = gmb_sdl_console_screen(g_app_sdl);
    gmb_clipper_state_t* stb = gmb_clipper_global_state();   /* stato da cui la frame legge il BUFFER */
    if (stb && cb) stb->buffer = cb;
    gmb_base_window_set_kind(GMB_WIN_SDL);
    gmb_base_window_set_handle(1);
}

/* Pompa gli eventi SDL + inoltra i tasti ai controlli. Ritorna 1 se la finestra e' chiusa. */
int gmb_tui_sdl_pump(void)
{
    if (!g_app_sdl) return 0;
    if (gmb_sdl_console_pump(g_app_sdl)) return 1;
    char k[16];
    gmb_clipper_state_t* st = gmb_tui_global_state();   /* stato da cui la frame legge l'INPUT */
    while (GMB_GFX_POLL_KEY(k, (int)sizeof(k))) { if (st) gmb_clipper_input_keyboard(st, k); }
    return 0;
}

/* Present: blink del cursore + posizione dal controllo a fuoco + render nella finestra.
 * La visibilita' del cursore e' SEMPRE ricalcolata dalla present (nessuno stato residuo);
 * g_cursor_epoch ancora la fase di blink all'ultimo tasto (cursore solido mentre si digita). */
static long long g_cursor_epoch = 0;
/* 1 se la cella del caret del controllo a fuoco e' COPERTA da una window/overlay sopra di esso
 * (z-order: un controllo registrato DOPO, o una dropdown di menu). Regola generale delle window. */
int gmb_tui_cursor_covered(void)
{
    int fr = gmb_tui_control_focused();
    if (fr < 0 || fr >= g_reg_n) return 0;
    int row = 0, col = 0;
    if (!gmb_tui_control_cursor(fr, &row, &col)) return 0;
    for (int i = fr + 1; i < g_reg_n; i++)
    {
        gmb_control_t* c = g_reg_ctrl[i];
        if (c && col >= c->x && col < c->x + c->width && row >= c->y && row < c->y + c->height) return 1;
    }
    return gmb_tui_menu_covers(col, row);
}

void gmb_tui_sdl_present(void)
{
    if (!g_app_sdl) return;
    /* REGOLA UNICA, ricalcolata OGNI frame (nessuno stato residuo):
     *   visibile  <=>  il controllo a fuoco HA un caret  AND  non coperto  AND  blink acceso.
     * Se il focus non ha un caret (es. menu/bottone) NON c'e' cursore -> spento. Il blink e'
     * una pura funzione del tempo (500 ms) ancorata all'ultimo tasto (cursore solido). */
    int fr = gmb_tui_control_focused();
    int cr = 0, cc = 0;
    int has_caret = (fr >= 0) && gmb_tui_control_cursor(fr, &cr, &cc);
    int covered = has_caret ? gmb_tui_cursor_covered() : 1;
    struct timeval tv; gettimeofday(&tv, NULL);
    long long ms = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    int blink = (((ms - g_cursor_epoch) / 500) & 1) == 0;
    gmb_sdl_console_set_cursor_visible(g_app_sdl, (has_caret && blink && !covered) ? 1 : 0);
    if (has_caret) gmb_sdl_console_set_cursor_position(g_app_sdl, cr + 1, cc + 1);
    gmb_sdl_console_render(g_app_sdl);
}

/* Cursore SOLIDO (chiamata quando si preme un tasto): ancora la fase di blink a ORA, cosi'
 * il cursore e' subito acceso e poi lampeggia. La visibilita' resta decisa dalla present. */
void gmb_tui_sdl_cursor_solid(void)
{
    struct timeval tv; gettimeofday(&tv, NULL);
    g_cursor_epoch = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Chiude (libera) la console. */
void gmb_tui_sdl_stop(void) { if (g_app_sdl) { gmb_sdl_console_free(g_app_sdl); g_app_sdl = NULL; } }

int gmb_tui_sdl_active(void) { return g_app_sdl ? 1 : 0; }

/* Screenshot PNG della SDLConsole dell'app (verifica visiva/demo). 0 = scritto. */
int gmb_tui_sdl_snapshot(const char* path)
{
    if (!g_app_sdl || !path) return -1;
    extern int gmb_sdl_console_save_png(gmb_sdl_console_t*, const char*);
    return gmb_sdl_console_save_png(g_app_sdl, path);
}

/* Stato del cursore nativo della console (per test/selfcheck headless). */
int gmb_tui_sdl_cursor_visible(void) { return g_app_sdl ? gmb_sdl_console_cursor_visible(g_app_sdl) : 0; }
int gmb_tui_sdl_cursor_row(void) { return g_app_sdl ? gmb_sdl_console_cursor_row(g_app_sdl) : -1; }
int gmb_tui_sdl_cursor_col(void) { return g_app_sdl ? gmb_sdl_console_cursor_col(g_app_sdl) : -1; }

/* Tempo corrente in millisecondi (monotono) — per time-out del menu'/hint nel BASIC. */
long long gmb_tui_now_ms(void)
{
    struct timeval tv; gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* ---- FFI-friendly per il BASIC: dimensione del buffer di rendering + mostra cursore ---- */
int gmb_tui_screen_width(void) { gmb_screen_buffer_t* b = modal_target_buffer(); return b ? gmb_screen_width(b) : 80; }
int gmb_tui_screen_height(void) { gmb_screen_buffer_t* b = modal_target_buffer(); return b ? gmb_screen_height(b) : 25; }
void gmb_tui_show_cursor(void) { fputs("\x1b[?25h", stdout); fflush(stdout); }
/* Imposta/legge il controllo a fuoco (indice nel registry). */
void gmb_tui_control_focus(int i) { if (i >= 0 && i < g_reg_n) g_reg_focus = i; }
long long gmb_tui_control_focused(void) { return g_reg_focus; }
