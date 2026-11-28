/* gmb_graphics_sprite_motion.c — P7: movimento a tick (MOVETO) e path degli sprite.
 *
 * Comportamento replicato dal riferimento (docs/graphics-subsystem.md, «Sprite paths» e
 * «MOVETO»), senza copiarne il codice:
 *  - MOVETO x, y, ticks: lo sprite va da solo verso (x,y) in `ticks` frame, un passo per
 *    frame; una nuova posizione o un altro MOVETO lo sostituiscono;
 *  - PATH a$: stringa compilata TUTTA prima (un errore non cambia nulla), eseguita un passo
 *    per frame; "" ferma il path lasciando lo sprite dov'è; un nuovo path sostituisce il
 *    precedente;
 *  - comandi: X+n / X-n / Y+n / Y-n (n px per frame per k frame: `X+3*20` = 3 px per 20
 *    frame), X=n / Y=n (coordinata subito), M dx,dy[*k], W n (n frame fermo), T n
 *    (durata di default dei movimenti successivi), E n (evento), [ ... ]n (ripete n volte,
 *    fino a 4 livelli), R (riparte da capo);
 *  - un path con soli comandi istantanei esegue al massimo 256 comandi per frame, così non
 *    blocca il frame; al massimo 4096 comandi;
 *  - path e MOVETO sulla TESTA di un gruppo muovono il blocco; quelli su un membro sono
 *    ignorati (contratto del riferimento).
 * Non implementati (errori espliciti, mai no-op silenziosi): A (sequenze di immagini: il
 * runtime C non ha sequenze per sprite), D/H/S/P (override di immagine/visibilita/priorita).
 */
#include "gmb_graphics.h"
#include "gmb_graphics_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GMB_PATH_MAX_OPS   4096
#define GMB_PATH_MAX_DEPTH 4
#define GMB_PATH_EVENTS    256
#define GMB_PATH_INSTANT   256   /* comandi istantanei per frame (limite del riferimento) */

enum {
    P_MOVE = 1,   /* a = dx/frame, b = dy/frame, c = frame totali */
    P_SETX,       /* a = x */
    P_SETY,       /* a = y */
    P_WAIT,       /* a = frame */
    P_EVT,        /* a = numero evento */
    P_LOOP,       /* a = ripetizioni, b = rimaste, c = indice di ripresa */
    P_RESTART     /* torna all'inizio */
};

typedef struct { unsigned char op; int a, b, c; } gmb_path_op_t;

typedef struct {
    gmb_path_op_t* ops;   /* malloc: solo gli sprite con un path allocano */
    int n;
    int pc;
    int wait;
    int mdx, mdy, mleft;
    int active;
} gmb_path_t;

typedef struct {
    int active;
    float sx, sy, tx, ty;
    int total, done;
} gmb_moveto_t;

typedef struct { int sprite, n; } gmb_pevent_t;

static gmb_path_t g_paths[GMB_GFX_MAX_SPRITES];
static gmb_moveto_t g_moveto[GMB_GFX_MAX_SPRITES];
static gmb_pevent_t g_events[GMB_PATH_EVENTS];
static int g_ev_head = 0, g_ev_count = 0;
static char g_path_err[160] = "";

static int id_ok(int id) { return id >= 0 && id < GMB_GFX_MAX_SPRITES; }

static void apply_position(int id, float x, float y)
{
    g_gfx.sprites[id].x = x;
    g_gfx.sprites[id].y = y;
    if (gmb_gfx_group_head_of(id) == id) gmb_gfx_group_relayout(id);
}

static void move_by(int id, int dx, int dy)
{
    gmb_gfx_sprite_t* s = &g_gfx.sprites[id];
    apply_position(id, s->x + (float)dx, s->y + (float)dy);
}

/* ------------------------------- eventi ------------------------------- */

static void push_event(int sprite, int n)
{
    if (g_ev_count >= GMB_PATH_EVENTS) return;   /* anello pieno: si scarta il nuovo */
    g_events[(g_ev_head + g_ev_count) % GMB_PATH_EVENTS].sprite = sprite;
    g_events[(g_ev_head + g_ev_count) % GMB_PATH_EVENTS].n = n;
    g_ev_count++;
}

int gmb_gfx_sprite_path_events(void) { return g_ev_count; }

int gmb_gfx_sprite_path_event(int* sprite_out)
{
    int n;
    if (g_ev_count <= 0) return -1;
    if (sprite_out) *sprite_out = g_events[g_ev_head].sprite;
    n = g_events[g_ev_head].n;
    g_ev_head = (g_ev_head + 1) % GMB_PATH_EVENTS;
    g_ev_count--;
    return n;
}

/* ------------------------------ compilatore ------------------------------ */

static const char* skip_blank(const char* s)
{
    while (*s && (unsigned char)*s <= ' ') s++;
    return s;
}

/* Intero con segno. Il riferimento ignora gli spazi fra i comandi, non dentro i numeri. */
static const char* read_int(const char* s, int* out)
{
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (!isdigit((unsigned char)*s)) return NULL;
    {
        long v = 0;
        while (isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); if (v > 100000000L) return NULL; s++; }
        *out = (int)(v * sign);
    }
    return s;
}

static int emit(gmb_path_op_t* ops, int* n, int op, int a, int b, int c)
{
    if (*n >= GMB_PATH_MAX_OPS) return -1;
    ops[*n].op = (unsigned char)op;
    ops[*n].a = a;
    ops[*n].b = b;
    ops[*n].c = c;
    (*n)++;
    return 0;
}

/* Compila la stringa in opcode. Ritorna 0 = ok, -1 = errore (messaggio in g_path_err).
 * Nessuna modifica allo stato dello sprite: chi chiama sostituisce il path solo se 0. */
static int path_compile(const char* src, gmb_path_op_t** out_ops, int* out_n)
{
    gmb_path_op_t* ops;
    int n = 0;
    int depth = 0;
    int stack[GMB_PATH_MAX_DEPTH];
    int def_frames = 1;
    const char* s = src;

    ops = (gmb_path_op_t*)malloc((size_t)GMB_PATH_MAX_OPS * sizeof(*ops));
    if (!ops) { snprintf(g_path_err, sizeof(g_path_err), "memoria esaurita"); return -1; }

    while (1)
    {
        char cmd;
        s = skip_blank(s);
        if (*s == '\0') break;
        cmd = (char)toupper((unsigned char)*s);
        s++;
        switch (cmd)
        {
            case 'X':
            case 'Y':
            {
                int v, frames = def_frames;
                int is_set = 0;
                int is_x = (cmd == 'X');
                s = skip_blank(s);
                if (*s == '=') { is_set = 1; s++; }
                else if (*s == '+' || *s == '-') { /* segno gestito da read_int */ }
                else
                {
                    snprintf(g_path_err, sizeof(g_path_err), "serve =, + o - dopo %c (a %d)", cmd, (int)(s - src));
                    free(ops);
                    return -1;
                }
                s = read_int(s, &v);
                if (!s) { snprintf(g_path_err, sizeof(g_path_err), "numero mancante dopo %c", cmd); free(ops); return -1; }
                if (is_set)
                {
                    if (emit(ops, &n, is_x ? P_SETX : P_SETY, v, 0, 0) != 0) goto too_many;
                }
                else
                {
                    s = skip_blank(s);
                    if (*s == '*')
                    {
                        s++;
                        s = read_int(s, &frames);
                        if (!s || frames < 1) { snprintf(g_path_err, sizeof(g_path_err), "durata non valida (a %d)", (int)(s - src)); free(ops); return -1; }
                    }
                    if (emit(ops, &n, P_MOVE, is_x ? v : 0, is_x ? 0 : v, frames) != 0) goto too_many;
                }
                break;
            }
            case 'M':
            {
                int dx, dy, frames = def_frames;
                s = skip_blank(s);
                s = read_int(s, &dx);
                if (!s) { snprintf(g_path_err, sizeof(g_path_err), "M: manca dx"); free(ops); return -1; }
                s = skip_blank(s);
                if (*s != ',') { snprintf(g_path_err, sizeof(g_path_err), "M: manca la virgola"); free(ops); return -1; }
                s = skip_blank(s + 1);
                s = read_int(s, &dy);
                if (!s) { snprintf(g_path_err, sizeof(g_path_err), "M: manca dy"); free(ops); return -1; }
                s = skip_blank(s);
                if (*s == '*')
                {
                    s++;
                    s = read_int(s, &frames);
                    if (!s || frames < 1) { snprintf(g_path_err, sizeof(g_path_err), "durata non valida (a %d)", (int)(s - src)); free(ops); return -1; }
                }
                if (emit(ops, &n, P_MOVE, dx, dy, frames) != 0) goto too_many;
                break;
            }
            case 'W':
            case 'E':
            case 'T':
            {
                int v;
                s = skip_blank(s);
                s = read_int(s, &v);
                if (!s) { snprintf(g_path_err, sizeof(g_path_err), "%c: manca il numero", cmd); free(ops); return -1; }
                if (cmd == 'T')
                {
                    if (v < 1 || v > 65535) { snprintf(g_path_err, sizeof(g_path_err), "T: durata fuori intervallo"); free(ops); return -1; }
                    def_frames = v;
                }
                else if (v < 0)
                {
                    snprintf(g_path_err, sizeof(g_path_err), "%c: numero negativo", cmd);
                    free(ops);
                    return -1;
                }
                else if (emit(ops, &n, cmd == 'W' ? P_WAIT : P_EVT, v, 0, 0) != 0) goto too_many;
                break;
            }
            case '[':
                if (depth >= GMB_PATH_MAX_DEPTH) { snprintf(g_path_err, sizeof(g_path_err), "annidamento oltre %d livelli", GMB_PATH_MAX_DEPTH); free(ops); return -1; }
                stack[depth++] = n;
                break;
            case ']':
            {
                int count, start;
                if (depth <= 0) { snprintf(g_path_err, sizeof(g_path_err), "']' senza '['"); free(ops); return -1; }
                s = skip_blank(s);
                s = read_int(s, &count);
                if (!s || count < 1) { snprintf(g_path_err, sizeof(g_path_err), "']': manca il numero di ripetizioni"); free(ops); return -1; }
                start = stack[--depth];
                /* `b` conta i salti che restano: il blocco e' gia' scritto una volta, quindi
                 * `[ ... ]n` lo esegue n volte con n-1 ritorni (altrimenti n+1). */
                if (emit(ops, &n, P_LOOP, count, count - 1, start) != 0) goto too_many;
                break;
            }
            case 'R':
                if (emit(ops, &n, P_RESTART, 0, 0, 0) != 0) goto too_many;
                break;
            case 'A':
            case 'D':
            case 'H':
            case 'S':
            case 'P':
                /* Espliciti: preferiamo un errore a un no-op silenzioso (§0bis). */
                snprintf(g_path_err, sizeof(g_path_err),
                         "%c: comando non implementato dal runtime C (sequenze/override non ci sono)", cmd);
                free(ops);
                return -1;
            case 'J':
                snprintf(g_path_err, sizeof(g_path_err), "J: salto non previsto dal piano (mai nel riferimento)");
                free(ops);
                return -1;
            default:
                snprintf(g_path_err, sizeof(g_path_err), "comando sconosciuto '%c' (a %d)", cmd, (int)(s - src - 1));
                free(ops);
                return -1;
        }
    }
    if (depth != 0) { snprintf(g_path_err, sizeof(g_path_err), "'[' non chiusa"); free(ops); return -1; }
    *out_ops = ops;
    *out_n = n;
    return 0;

too_many:
    snprintf(g_path_err, sizeof(g_path_err), "troppi comandi (massimo %d)", GMB_PATH_MAX_OPS);
    free(ops);
    return -1;
}

/* ----------------------------- esecuzione ----------------------------- */

static void path_stop(int id)
{
    free(g_paths[id].ops);
    memset(&g_paths[id], 0, sizeof(g_paths[id]));
}

int gmb_gfx_sprite_path(int id, const char* program)
{
    gmb_path_op_t* ops = NULL;
    int n = 0;
    if (!id_ok(id) || !g_gfx.sprites[id].in_use) return -1;
    if (gmb_gfx_group_is_member(id)) return 0;      /* ignorato: comanda la testa */
    if (!program || !*program) { path_stop(id); return 0; }   /* "" ferma */
    if (path_compile(program, &ops, &n) != 0) return -1;      /* path precedente intatto */
    path_stop(id);
    g_paths[id].ops = ops;
    g_paths[id].n = n;
    g_paths[id].active = 1;
    return 0;
}

int gmb_gfx_sprite_path_active(int id)
{
    return (id_ok(id) && g_paths[id].active) ? -1 : 0;
}

const char* gmb_gfx_sprite_path_error(void) { return g_path_err; }

/* Il path e' finito appena non resta nulla da eseguire: `path_active` deve gia' valere 0
 * nello stesso frame dell'ultimo passo, non al tick successivo. */
static void path_maybe_finish(gmb_path_t* p)
{
    if (p->pc >= p->n && p->mleft <= 0 && p->wait <= 0) p->active = 0;
}

static void path_step(int id)
{
    gmb_path_t* p = &g_paths[id];
    int budget = GMB_PATH_INSTANT;
    if (p->mleft > 0)
    {
        move_by(id, p->mdx, p->mdy);
        p->mleft--;
        path_maybe_finish(p);
        return;
    }
    if (p->wait > 0)
    {
        p->wait--;
        path_maybe_finish(p);
        return;
    }
    while (budget-- > 0)
    {
        gmb_path_op_t* op;
        if (p->pc >= p->n) { p->active = 0; return; }
        op = &p->ops[p->pc++];
        switch (op->op)
        {
            case P_MOVE:
                move_by(id, op->a, op->b);
                p->mdx = op->a;
                p->mdy = op->b;
                p->mleft = op->c - 1;
                path_maybe_finish(p);
                return;                       /* il frame e' consumato */
            case P_SETX:
                apply_position(id, (float)op->a, g_gfx.sprites[id].y);
                break;
            case P_SETY:
                apply_position(id, g_gfx.sprites[id].x, (float)op->a);
                break;
            case P_WAIT:
                p->wait = op->a - 1;
                path_maybe_finish(p);
                return;                       /* il frame e' consumato */
            case P_EVT:
                push_event(id, op->a);
                break;
                        case P_LOOP:
                if (op->b > 0) { op->b--; p->pc = op->c; }
                else { op->b = op->a - 1; }       /* giro finito: si riarma per i prossimi passaggi */
                break;
            case P_RESTART:
                p->pc = 0;
                break;
            default:
                p->active = 0;
                return;
        }
    }
    /* 256 comandi istantanei: il resto al frame dopo (nessun frame bloccato). */
}

/* ------------------------------ MOVETO ------------------------------ */

int gmb_gfx_sprite_moveto(int id, float x, float y, int ticks)
{
    gmb_moveto_t* m;
    if (!id_ok(id) || !g_gfx.sprites[id].in_use) return -1;
    if (gmb_gfx_group_is_member(id)) return 0;      /* ignorato: comanda la testa */
    m = &g_moveto[id];
    if (ticks <= 0)
    {
        m->active = 0;
        apply_position(id, x, y);
        return 0;
    }
    m->active = 1;
    m->sx = g_gfx.sprites[id].x;
    m->sy = g_gfx.sprites[id].y;
    m->tx = x;
    m->ty = y;
    m->total = ticks;
    m->done = 0;
    return 0;
}

int gmb_gfx_sprite_moving(int id)
{
    return (id_ok(id) && g_moveto[id].active) ? 1 : 0;
}

static void moveto_step(int id)
{
    gmb_moveto_t* m = &g_moveto[id];
    if (!m->active) return;
    m->done++;
    if (m->done >= m->total)
    {
        m->active = 0;
        apply_position(id, m->tx, m->ty);      /* il bersaglio e' esatto */
        return;
    }
    apply_position(id,
                   m->sx + (m->tx - m->sx) * (float)m->done / (float)m->total,
                   m->sy + (m->ty - m->sy) * (float)m->done / (float)m->total);
}

/* Una nuova posizione scritta dal programma sostituisce il movimento in corso
 * (contratto del riferimento: «a new position or MOVETO replaces it»). */
void gmb_gfx_motion_cancel(int id)
{
    if (!id_ok(id)) return;
    g_moveto[id].active = 0;
}

int gmb_gfx_sprites_tick(int frames)
{
    int done = 0;
    if (frames <= 0) return 0;
    for (int f = 0; f < frames; f++)
    {
        for (int i = 0; i < GMB_GFX_MAX_SPRITES; i++)
        {
            if (!g_gfx.sprites[i].in_use) continue;
            if (gmb_gfx_group_is_member(i)) continue;   /* il blocco lo muove la testa */
            moveto_step(i);
            if (g_paths[i].active) path_step(i);
        }
        done++;
    }
    return done;
}

void gmb_gfx_motion_reset(void)
{
    for (int i = 0; i < GMB_GFX_MAX_SPRITES; i++)
    {
        free(g_paths[i].ops);
        memset(&g_paths[i], 0, sizeof(g_paths[i]));
        memset(&g_moveto[i], 0, sizeof(g_moveto[i]));
    }
    g_ev_head = 0;
    g_ev_count = 0;
    g_path_err[0] = '\0';
}
