/* gmb_read.c — Contesto READ unificato (stadio 1). Loop park-based, non bloccante.
 * Vedi gmb_read.h. Il punto di attesa è gmb_key_wait_park (GPM): il worker cede e si
 * riprende a evento, come window/mouse/tastiera/controlli. */
#include "gmb_read.h"
#include <stdint.h>
#include "gmb_c99gpm.h"     /* gmb_key_wait_park (park-based) */
#include "gmb_tui_app.h"    /* gmb_tui_add_textinput */
#include "gmb_gfx_input.h"
#include "gmb_ui_gfx.h"
static int type_is(const char* t, const char* name);
struct gmb_read_field;
gmb_read_field_t* gmb_read_add_textfield_typed(gmb_read_ctx_t* ctx, int row, int col, int width,
                                              const char* initial, const char* type,
                                              const char* picture, const char* items,
                                              double vmin, double vmax, double step);
#include <string.h>
#include <stdio.h>

/* Presentazione grafica unificata (stessi renderer CONTROLS): senza questo la form
 * @SAY/@GET+READ resterebbe invisibile sulla pagina grafica. */
extern int  gmb_gfx_screen_on(void);
extern void gmb_form_render_if_dirty(void);
extern void gmb_gfx_present_tick(void);
extern void gmb_gfx_cls(int mode);
extern void gmb_form_autokey_tick(void);
extern void gmb_tui_draw_px_all(void);
extern void gmb_tui_px_invalidate(void);
extern void gmb_form_paint_says_px(void);
extern int  gmb_gfx_input_poll_key(char* out, int maxlen);

static gmb_read_ctx_t g_read;
static int g_read_used = 0;

gmb_read_ctx_t* gmb_read_active(void)
{
    if (!g_read_used) { memset(&g_read, 0, sizeof(g_read)); g_read.id = 1; g_read_used = 1; }
    return &g_read;
}

gmb_read_field_t* gmb_read_add_field(gmb_read_ctx_t* ctx)
{
    if (!ctx || ctx->n_fields >= GMB_READ_MAX_FIELDS) return NULL;
    gmb_read_field_t* f = &ctx->fields[ctx->n_fields++];
    memset(f, 0, sizeof(*f));
    return f;
}

/* @GET: crea il campo = controllo CONTROLS text-input unificato (nel registry). */
gmb_read_field_t* gmb_read_add_textfield(gmb_read_ctx_t* ctx, int row, int col, int width, const char* initial)
{ return gmb_read_add_textfield_typed(ctx, row, col, width, initial, NULL, NULL, NULL, 0, 0, 0); }

gmb_read_field_t* gmb_read_add_textfield_typed(gmb_read_ctx_t* ctx, int row, int col, int width,
                                              const char* initial, const char* type,
                                              const char* picture, const char* items,
                                              double vmin, double vmax, double step)
{
    gmb_read_ctx_t* c = ctx ? ctx : gmb_read_active();
    gmb_read_field_t* f = gmb_read_add_field(c);
    if (!f) return NULL;
    f->form_wid = -1;
    int cw = 8, ch = 16;
    { extern void gmb_form_cell_px(int*, int*); gmb_form_cell_px(&cw, &ch); }
    if (type) snprintf(f->type, sizeof(f->type), "%s", type);
    if (items) snprintf(f->items, sizeof(f->items), "%s", items);
    if (gmb_gfx_screen_on()) {
        /* GRAFICA: widget form per TIPO (text/numeric/spinner/date/check/list). */
        int fx0 = col * cw, fy0 = row * ch, fw0 = width * cw, fh0 = ch - 6;
        if (type_is(type, "spinner")) {
            extern int gmb_form_add_spinner(int,int,int,double,double,double,double*);
            f->kind = 2; f->num = initial && *initial ? atof(initial) : vmin;
            f->form_wid = gmb_form_add_spinner(fx0, fy0, fw0, vmin, vmax, step > 0 ? step : 1, &f->num);
        } else if (type_is(type, "datepicker")) {
            extern int gmb_form_add_datepicker(int,int,void*,const char*);
            f->kind = 3; f->val = gmb_string_alloc(initial ? initial : "");
            f->form_wid = gmb_form_add_datepicker(fx0, fy0, &f->val, picture && *picture ? picture : "dd/mm/yyyy");
        } else if (type_is(type, "listbox") || type_is(type, "combobox")) {
            extern int gmb_form_add_listbox(int,int,int,int,const char*,double*);
            f->kind = 5; f->num = 0;   /* widget: indice 0-based */
            f->form_wid = gmb_form_add_listbox(fx0, fy0, fw0, (fh0 > 0 ? fh0 : 20) * 4, items ? items : "", &f->num);
        } else if (type_is(type, "checkbox")) {
            extern int gmb_form_add_checkbox(int,int,const char*,double*);
            f->kind = 4; f->num = initial && *initial ? atof(initial) : 0;
            f->form_wid = gmb_form_add_checkbox(fx0, fy0, initial ? initial : "", &f->num);
        } else {
            extern int gmb_form_add_textbox(int,int,int,int,void*);
            f->kind = 0; f->val = gmb_string_alloc(initial ? initial : "");
            f->form_wid = gmb_form_add_textbox(fx0, fy0, fw0, fh0, &f->val);
        }
        f->ctrl = NULL; f->handle = -1;
    } else {
        int fx = col, fy = row, fw = width;
        f->ctrl   = gmb_tui_add_textinput(fx, fy, fw, initial);   /* testuali: celle */
        f->handle = f->ctrl ? f->ctrl->handle : -1;
    }
    f->row = row; f->col = col; f->width = width;
    if (initial) snprintf(f->initial, sizeof(f->initial), "%s", initial);
    return f;
}

/* Indice del campo (per la C#: addressare when/valid) + setter dei predicati. */
int  gmb_read_field_count(gmb_read_ctx_t* ctx) { return ctx ? ctx->n_fields : 0; }
static int type_is(const char* t, const char* name)
{ return t && strcasecmp(t, name) == 0; }

/* Concatena due liste di items con '|' (per @GET ITEMS {..,..}). */
gmb_string_t gmb_read_items_join(const char* a, const char* b)
{
    char buf[1024];
    if (a && *a) snprintf(buf, sizeof(buf), "%s|%s", a, b ? b : "");
    else snprintf(buf, sizeof(buf), "%s", b ? b : "");
    return gmb_string_alloc(buf);
}

int  gmb_read_add_field_typed(int row, int col, int width, const char* initial,
                              const char* type, const char* picture, const char* items,
                              double vmin, double vmax, double step)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (!gmb_read_add_textfield_typed(ctx, row, col, width, initial, type, picture, items, vmin, vmax, step)) return -1;
    return ctx->n_fields - 1;
}

int  gmb_read_add_field_active(int row, int col, int width, const char* initial)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (!gmb_read_add_textfield(ctx, row, col, width, initial)) return -1;
    return ctx->n_fields - 1;
}
void gmb_read_set_when(int idx, int addr)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (idx >= 0 && idx < ctx->n_fields) ctx->fields[idx].when_addr = addr;
}
void gmb_read_skip_field(int idx)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (idx >= 0 && idx < ctx->n_fields) ctx->fields[idx].skip = 1;
}
/* Applica MESSAGE/ERROR al campo idx (-1 = ultimo). */
static int read_last_field_idx(gmb_read_ctx_t* ctx, int idx)
{
    if (!ctx || ctx->n_fields <= 0) return -1;
    if (idx < 0 || idx >= ctx->n_fields) return ctx->n_fields - 1;
    return idx;
}
void gmb_read_set_message(int idx, const char* text)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    int i = read_last_field_idx(ctx, idx); if (i < 0) return;
    snprintf(ctx->fields[i].message, sizeof(ctx->fields[i].message), "%s", text ? text : "");
}
void gmb_read_set_error(int idx, const char* text)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    int i = read_last_field_idx(ctx, idx); if (i < 0) return;
    snprintf(ctx->fields[i].error, sizeof(ctx->fields[i].error), "%s", text ? text : "");
}

void gmb_read_set_valid(int idx, int addr)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (idx >= 0 && idx < ctx->n_fields) ctx->fields[idx].valid_addr = addr;
}

int gmb_read_wait_key(int timeout_ms)
{
    gmb_string_t s = gmb_key_wait_park(timeout_ms);   /* <-- PARK (worker non bloccato) */
    const char* p = (s.data && s.data[0]) ? s.data : "";
    size_t n = strlen(p);
    if (n == 0) return 0;
    /* Frecce/canc singoli (iniettati dalla pump SDL come 0x1C..0x1F, BS 0x08/0x7F):
     * mappati sui codici GMB_K_* del text-input unificato. */
    {
        unsigned char b = (unsigned char)p[0];
        if (b == 0x1C) return GMB_K_LEFT;
        if (b == 0x1D) return GMB_K_RIGHT;
        if (b == 0x1E) return GMB_K_UP;
        if (b == 0x1F) return GMB_K_DOWN;
        if (b == 0x08 || b == 0x7F) return GMB_K_BS;
    }
    /* Tasti speciali (multi-byte, vedi GMB_KEY_*): 0x1B + lettera. */
    if (n >= 2 && (unsigned char)p[0] == 0x1B)
    {
        switch (p[1])
        {
            case 'L': return GMB_KEY_LEFT;
            case 'R': return GMB_KEY_RIGHT;
            case 'U': return GMB_KEY_UP;
            case 'D': return GMB_KEY_DOWN;
            case 'H': return GMB_KEY_HOME;
            case 'E': return GMB_KEY_END;
        }
    }
    return (unsigned char)p[0];
}

/* Un chunk di testo (es. "Mario" iniettato dagli hook di test) viene restituito
 * un carattere per volta: l'eccedenza torna in coda, così l'editing resta identico. */
/* UI OPZIONALE (vedi gmb_form.c): riferimento DEBOLE, così un eseguibile testuale non
 * trascina SDL/Vulkan. Se l'UI non c'è, l'eccedenza di tasti viene ignorata (nessun crash). */
#ifdef GMB_WITH_UI
#define GMB_UI_PRESENT_READ(fn) 1
#else
#define GMB_UI_PRESENT_READ(fn) 0
#endif

int gmb_read_wait_key_text(int timeout_ms)
{
    gmb_string_t s = gmb_key_wait_park(timeout_ms);
    const char* p = (s.data && s.data[0]) ? s.data : "";
    size_t n = strlen(p);
    if (n == 0) return 0;
    {   /* Frecce/BS dal pump SDL: 0x1C..0x1F, 0x08/0x7F -> codici GMB_K_* */
        unsigned char b = (unsigned char)p[0];
        if (n == 1) {
            if (b == 0x1C) return GMB_K_LEFT;
            if (b == 0x1D) return GMB_K_RIGHT;
            if (b == 0x1E) return GMB_K_UP;
            if (b == 0x1F) return GMB_K_DOWN;
            if (b == 0x08 || b == 0x7F) return GMB_K_BS;
        }
    }
    if (n >= 2 && (unsigned char)p[0] == 0x1B) return 0;   /* sequenze speciali: già gestite */
#ifdef GMB_WITH_UI
    if (n > 1) gmb_gfx_input_push_key(p + 1);
#endif
    return (unsigned char)p[0];
}

void gmb_read_set_when_fn(int idx, int (*fn)(void*), void* user)
{ gmb_read_ctx_t* c = gmb_read_active(); if (idx >= 0 && idx < c->n_fields) { c->fields[idx].when_fn = fn; c->fields[idx].when_user = user; } }
void gmb_read_set_apply_text(int idx, void (*fn)(void*, const char*), void* user)
{ gmb_read_ctx_t* c = gmb_read_active(); if (idx >= 0 && idx < c->n_fields) { c->fields[idx].apply_text = fn; c->fields[idx].apply_text_user = user; } }

void gmb_read_set_valid_fn(int idx, int (*fn)(void*), void* user)
{ gmb_read_ctx_t* c = gmb_read_active(); if (idx >= 0 && idx < c->n_fields) { c->fields[idx].valid_fn = fn; c->fields[idx].valid_user = user; } }

int gmb_read_when_ok(gmb_read_ctx_t* ctx, gmb_read_field_t* f)
{
    if (f && f->when_fn) return f->when_fn(f->when_user) != 0;
    if (!f || f->when_addr <= 0 || !ctx || !ctx->pred) return 1;   /* assente → pervietà */
    return ctx->pred(ctx->pred_user, f->when_addr) != 0;
}

int gmb_read_valid_ok(gmb_read_ctx_t* ctx, gmb_read_field_t* f)
{
    if (f && f->valid_fn) return f->valid_fn(f->valid_user) != 0;
    if (!f || f->valid_addr <= 0 || !ctx || !ctx->pred) return 1;
    return ctx->pred(ctx->pred_user, f->valid_addr) != 0;
}

void gmb_read_key(gmb_read_ctx_t* ctx, gmb_read_field_t* f, int key)
{
    if (!ctx) return;
    /* Clipper/xBase: il campo scrive-through nel target a ogni tasto, così VALID/WHEN
     * (che leggono la variabile, anche DYNAMIC) vedono il valore corrente. */
    if (f && f->apply_text) f->apply_text(f->apply_text_user, gmb_read_field_text(f));
    switch (key)
    {
        case GMB_KEY_ESC:
            ctx->state = GMB_READ_CANCELLED; return;
        case GMB_KEY_TAB:
        case GMB_KEY_DOWN:
        {
            /* TAB cicla fra i campi (wrap): non termina piu' la form. */
            int okv = gmb_read_valid_ok(ctx, f);
            if (f) f->err_active = okv ? 0 : 1;
            if (okv && ctx->n_fields > 0)
                ctx->active = (ctx->active + 1) % ctx->n_fields;
            return;
        }
        case GMB_KEY_ENTER:
        {
            int okv = gmb_read_valid_ok(ctx, f);
            if (f) f->err_active = okv ? 0 : 1;
            if (okv) {
                if (ctx->active + 1 >= ctx->n_fields) ctx->state = GMB_READ_CONFIRMED;
                else ctx->active++;
            }
            return;
        }
        case GMB_KEY_UP:
            if (ctx->n_fields > 0) ctx->active = (ctx->active + ctx->n_fields - 1) % ctx->n_fields; return;
        default:
            /* Edit del campo: delega al controllo attivo (text-input unificato). */
            if (f && f->ctrl && f->ctrl->handle_key_fn)
                f->ctrl->handle_key_fn(f->ctrl->user, f->ctrl, key);
            return;
    }
}

/* MESSAGE/ERROR in TUI: una label condivisa in fondo (row 24), aggiornata solo se cambia.
 * In grafica non c'e' una riga di stato: il testo resta disponibile ma non viene reso. */
extern gmb_control_t* gmb_tui_add_label(int x, int y, int w, const char* text);
static void read_show_status(gmb_read_ctx_t* ctx, const char* text)
{
    if (!ctx) return;
    if (!text) text = "";
    if (strcmp(ctx->msg_shown, text) == 0) return;
    snprintf(ctx->msg_shown, sizeof(ctx->msg_shown), "%s", text);
    if (!ctx->msg_ctrl) {
        if (!text[0]) return;
        ctx->msg_ctrl = gmb_tui_add_label(0, 24, 78, text);
    } else {
        gmb_label_set_text((gmb_label_t*)ctx->msg_ctrl, text);
    }
}

/* Loop READ (park-based): mostra i campi, attende un tasto per volta, applica i valori. */
int gmb_read_run(gmb_read_ctx_t* ctx)
{
    if (!ctx) return GMB_READ_CANCELLED;
    ctx->state = GMB_READ_RUNNING;
    ctx->active = 0;

    int gfx = gmb_gfx_screen_on();
    if (gfx) { extern void gmb_form_set_read_mode(int); gmb_form_set_read_mode(1); }
    /* int64_t, NON `long long`: su Linux aarch64 int64_t è `long`, e la dichiarazione
     * locale `long long` andava in conflitto con quella del header (gmb_c99gpm.h). */
    extern int64_t gmb_c99_now_ms(void);
    int64_t t0 = gmb_c99_now_ms();
    if (gfx) {
        /* Scarta i tasti rimasti in coda PRIMA della form (es. ESC sintetico del
         * bootstrap video): sono eventi passati, non input di READ. */
        char stale[64];
        while (gmb_gfx_input_poll_key(stale, (int)sizeof(stale))) { }
        gmb_gfx_cls(-1); gmb_form_render_if_dirty(); gmb_form_paint_says_px(); gmb_gfx_present_tick();
    }

    while (ctx->state == GMB_READ_RUNNING)
    {
        if (gfx) { extern int gmb_form_closed(void);
          if (gmb_form_closed()) { ctx->state = GMB_READ_CANCELLED; break; } }
        if (ctx->active < 0 || ctx->active >= ctx->n_fields)
        {
            ctx->state = (ctx->n_fields > 0) ? GMB_READ_CONFIRMED : GMB_READ_CANCELLED;
            break;
        }
        gmb_read_field_t* f = &ctx->fields[ctx->active];
        /* Focus visibile: il campo attivo e' l'unico con c->focused. */
        for (int i = 0; i < ctx->n_fields; i++)
            if (ctx->fields[i].ctrl) ctx->fields[i].ctrl->focused = (i == ctx->active);
        if (f->handle >= 0) { extern void gmb_tui_control_focus(int); gmb_tui_control_focus((int)f->handle); }
        if (gfx) {
            /* Il click sul widget (dispatch del form) ha gia' spostato il focus: adottalo. */
            extern int gmb_form_focused_widget(void);
            int fw = gmb_form_focused_widget();
            static int last_fw = -1; static int last_active = -1;
            int user_click = (fw >= 0 && fw != last_fw);
            if (user_click) {
                for (int i = 0; i < ctx->n_fields; i++)
                    if (ctx->fields[i].form_wid == fw) { ctx->active = i; f = &ctx->fields[i]; break; }
            }
            if (ctx->active != last_active && ctx->active >= 0 && ctx->active < ctx->n_fields &&
                ctx->fields[ctx->active].form_wid >= 0) {
                extern void gmb_form_focus_widget(int);
                gmb_form_focus_widget(ctx->fields[ctx->active].form_wid);   /* TAB/ENTER: focus al campo attivo */
                fw = ctx->fields[ctx->active].form_wid;
            }
            last_fw = fw; last_active = ctx->active;
            if (fw < 0 && f->form_wid >= 0) {
                extern void gmb_form_focus_widget(int); gmb_form_focus_widget(f->form_wid);
            }
        } else if (f->form_wid >= 0) { extern void gmb_form_focus_widget(int); gmb_form_focus_widget(f->form_wid); }
        if (gfx) {
            /* Click del mouse: il campo sotto il puntatore prende il focus. */
            extern int gmb_gfx_input_poll_mouse(int*, int*, int*, int*, int*);
            int mt, mx, my, mb, mw;
            while (gmb_gfx_input_poll_mouse(&mt, &mx, &my, &mb, &mw)) {
                if (mt != GMB_GFX_MOUSE_DOWN) continue;
                /* Mouse = coordinate LOGICHE finestra; i rect widget sono FISICI. */
                float msc = GMB_GFX_RENDER_SCALE(); if (msc <= 0.0f) msc = 1.0f;
                int px = (int)(mx * msc + 0.5f), py = (int)(my * msc + 0.5f);
                for (int i = 0; i < ctx->n_fields; i++) {
                    gmb_control_t* cc = ctx->fields[i].ctrl;
                    if (!cc) continue;
                    if (px >= cc->px_x && px < cc->px_x + cc->px_w &&
                        py >= cc->px_y && py < cc->px_y + cc->px_h) { ctx->active = i; break; }
                }
            }
        }
        if (f->skip || !gmb_read_when_ok(ctx, f)) { ctx->active++; continue; }   /* WHEN: skip, nessun park */
        f->visited = 1;   /* mostrato: solo i campi mostrati tornano nel target (apply_all) */

        if (!gfx) read_show_status(ctx, (f->err_active && f->error[0]) ? f->error : f->message);
        if (ctx->render_hook) ctx->render_hook(ctx);

        if (gfx) {
            gmb_form_render_if_dirty(); gmb_form_paint_says_px(); gmb_gfx_present_tick();
        }
        gmb_form_autokey_tick();

        int key = 0;
        if (gfx) {
            extern void gmb_form_poll(void);
            gmb_form_poll();      /* drena input: edit del widget + pending Enter/ESC/Tab */
            extern int gmb_form_take_read_key(void);
            int rk = gmb_form_take_read_key();
            if (rk == '\r' || rk == '\n') key = GMB_KEY_ENTER;
            else if (rk == '\t') key = GMB_KEY_TAB;
            else if (rk == 0x1b) key = GMB_KEY_ESC;
        } else key = gmb_read_wait_key_text(50);
        /* PARK; 0 = timeout → ridisegna */
        if (key == 0) continue;
        ctx->last_key = key;
        if (f->form_wid >= 0 && key != GMB_KEY_ESC && key != GMB_KEY_ENTER &&
            key != GMB_KEY_TAB && key != GMB_KEY_UP && key != GMB_KEY_DOWN && key != GMB_KEY_LEFT && key != GMB_KEY_RIGHT) {
            char kb[2] = { (char)key, 0 };
            extern void gmb_form_dispatch_key(const char*);
            gmb_form_dispatch_key(kb);   /* editing del widget form (testo/BS) */
        } else if (f->form_wid >= 0 && (key == GMB_KEY_LEFT || key == GMB_KEY_RIGHT)) {
            char kb[2] = { (char)(key == GMB_KEY_LEFT ? 0x1c : 0x1d), 0 };
            extern void gmb_form_dispatch_key(const char*);
            gmb_form_dispatch_key(kb);
        } else {
            gmb_read_key(ctx, f, key);   /* navigazione/confirm + fallback registry */
        }
    }

    if (ctx->state == GMB_READ_CONFIRMED) gmb_read_apply_all(ctx);
    else {
        /* ESC/annullato: i campi tornano al valore iniziale (l'emissione legge
         * gmb_read_value_at() comunque, quindi va ripristinato qui). */
        extern void gmb_text_input_set_text(gmb_text_input_t*, const char*);
        for (int i = 0; i < ctx->n_fields; i++) {
            gmb_read_field_t* ff = &ctx->fields[i];
            if (ff->ctrl && ff->ctrl->user) gmb_text_input_set_text((gmb_text_input_t*)ff->ctrl->user, ff->initial);
            else if (ff->form_wid >= 0) { ff->val = gmb_string_alloc(ff->initial); extern void gmb_form_focus_widget(int); (void)ff; }
        }
    }
    if (gfx) { extern void gmb_form_set_read_mode(int); gmb_form_set_read_mode(0); }
    int st = ctx->state;
    /* NON si azzera qui: l emissione legge i valori con gmb_read_value_at() e poi
     * chiama gmb_read_reset() esplicitamente. */
    return st;
}

void gmb_read_apply_all(gmb_read_ctx_t* ctx)
{
    if (!ctx) return;
    for (int i = 0; i < ctx->n_fields; i++)
    {
        gmb_read_field_t* f = &ctx->fields[i];
        if (!f->visited) continue;   /* saltato da WHEN: il campo non è mai stato mostrato, la
                                      * variabile NON va riscritta (eviterebbe un valore spurio,
                                      * es. 0 per un numerico dal testo iniziale vuoto) */
        if (ctx->apply) ctx->apply(ctx->apply_user, f, gmb_read_field_text(f));
    }
}

const char* gmb_read_field_text(gmb_read_field_t* f)
{
    if (f && (f->kind == 2 || f->kind == 4)) {         /* spinner / checkbox */
        snprintf(f->valbuf, sizeof(f->valbuf), "%.15g", f->num);
        return f->valbuf;
    }
    if (f && f->kind == 5) {                           /* list/combobox: indice 1-based -> label */
        int idx = (int)(f->num + 0.5) + 1;   /* 0-based widget -> 1-based Clipper */
        if (idx > 0 && idx <= 64 && f->items[0]) {
            int i = 1; const char* p = f->items;
            while (p && *p && i < idx) { const char* b = strchr(p, '|'); if (!b) break; p = b + 1; i++; }
            if (i == idx) {
                const char* e = strchr(p, '|'); size_t n = e ? (size_t)(e - p) : strlen(p);
                if (n > sizeof(f->valbuf) - 1) n = sizeof(f->valbuf) - 1;
                memcpy(f->valbuf, p, n); f->valbuf[n] = 0; return f->valbuf;
            }
        }
        snprintf(f->valbuf, sizeof(f->valbuf), "%d", idx);
        return f->valbuf;
    }
    if (f && f->form_wid >= 0) return f->val.data ? f->val.data : "";
    if (!f || !f->ctrl || !f->ctrl->user) return "";
    return ((gmb_text_input_t*)f->ctrl->user)->text;
}

const char* gmb_read_value_at(int idx)
{
    gmb_read_ctx_t* ctx = gmb_read_active();
    if (idx < 0 || idx >= ctx->n_fields) return "";
    return gmb_read_field_text(&ctx->fields[idx]);
}

int         gmb_read_native_run(void)   { return gmb_read_run(gmb_read_active()); }
/* Ri-prompt VALID dei backend gestiti: il prossimo run mostra SOLO il campo idx
 * (gli altri restano con il testo confermato e non vengono riproposti). */
void        gmb_read_native_focus_only(int idx)
{
    gmb_read_ctx_t* c = gmb_read_active();
    for (int i = 0; i < c->n_fields; i++) c->fields[i].skip = (i != idx);
}
const char* gmb_read_native_value(int i){ return gmb_read_value_at(i); }
void        gmb_read_native_reset(void) { gmb_read_reset(gmb_read_active()); }
void        gmb_read_native_clear(void) { gmb_read_ctx_t* c = gmb_read_active(); c->n_fields = 0; c->active = 0; c->state = GMB_READ_RUNNING; }

gmb_string_t gmb_read_items_join_arr(const gmb_string_t* items, int n)
{
    char buf[1024];
    size_t used = 0;
    buf[0] = 0;
    for (int i = 0; i < n; i++) {
        const char* s = (items && items[i].data) ? items[i].data : "";
        size_t len = strlen(s);
        if (used && used + 1 < sizeof(buf)) buf[used++] = '|';
        if (used + len >= sizeof(buf)) len = sizeof(buf) - 1 - used;
        memcpy(buf + used, s, len);
        used += len;
        buf[used] = 0;
    }
    return gmb_string_alloc(buf);
}

void gmb_read_resume(gmb_read_ctx_t* ctx, int active)
{
    if (!ctx) return;
    if (active < 0) active = 0;
    if (active >= ctx->n_fields) active = ctx->n_fields - 1;
    ctx->active = active;
    ctx->state = GMB_READ_RUNNING;
}

void gmb_read_reset(gmb_read_ctx_t* ctx)
{
    if (!ctx) return;
    ctx->n_fields = 0;
    ctx->active = 0;
    ctx->state = GMB_READ_RUNNING;
}
