/* gmb_c99_emit_3d.c — emissione del dominio `WORLD` (motore 3D, P2).
 *
 * Sintassi (specifica §2.4, comandi legacy a spazio):
 *   WORLD LOAD "mappa"                 ' mondo corrente (load atomico)
 *   WORLD CLOSE                        ' libera il mondo corrente
 *   WORLD TICK secondi
 *   WORLD SECMOVE settore, velocità, pavimento[, soffitto]
 *   WORLD HEIGHT settore               ' ferma il mover
 *   WORLD ANIM settore, prima, quante, fps
 *   WORLD SCROLL settore, what, u/s, v/s
 *   WORLD LIGHTFX settore, modo, amount, rate
 *   WORLD LAMP x, y, z, raggio, livello
 *
 * Letture (funzioni, §1: leggono e basta):
 *   WORLD.MOVING(settore) -1 in moto / 0 fermo, WORLD.LIGHTNOW(settore),
 *   WORLD.TIME(), WORLD.FLOORZ(settore), WORLD.CEILZ(settore)
 *
 * Ogni emissione è UNA chiamata alla facciata `gmb_world_*` del runtime
 * (gmb_3d_cmd.c): qui non c'è logica, solo la traduzione dei termini.
 * Nessuno stub: un verbo sconosciuto è un errore esplicito (c99_fail).
 */
#include "gmb_c99_emit_internal.h"

#include <string.h>

/* `(void)fn(a, b, …);` — il valore di ritorno dei comandi non si usa. */
static void wcall(ctx_t* c, const char* fn, gmb_expr_t** a, int n)
{
    ind(c);
    sb_printf(&c->out, "(void)%s(", fn);
    for (int i = 0; i < n; i++)
    {
        if (i) sb_put(&c->out, ", ");
        emit_expr(c, a[i]);
    }
    sb_put(&c->out, ");\n");
}

void emit_world_domain(ctx_t* c, gmb_stmt_t* s)
{
    int tk = s->act_tk;
    gmb_expr_t** a = s->go_args;
    int n = s->n_go_args;

    if (tk == GMB_TOK_Load)
    {
        if (n < 1) { c99_fail(c, s->line, "WORLD LOAD: manca il percorso della mappa"); return; }
        ind(c);
        sb_put(&c->out, "(void)gmb_world_load((");
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ").data);\n");   /* gmb_string_t -> const char* (come gli altri domini) */
        return;
    }
    if (tk == GMB_TOK_Close) { ind(c); sb_put(&c->out, "gmb_world_free();\n"); return; }
    if (tk == GMB_TOK_Tick)
    {
        if (n < 1) { c99_fail(c, s->line, "WORLD TICK: mancano i secondi"); return; }
        wcall(c, "gmb_world_tick", a, 1);
        return;
    }
    if (tk == GMB_TOK_SecMove)
    {
        if (n < 3) { c99_fail(c, s->line, "WORLD SECMOVE settore, velocità, pavimento[, soffitto]"); return; }
        if (n >= 4) wcall(c, "gmb_world_secmove_ceil", a, 4);
        else        wcall(c, "gmb_world_secmove", a, 3);
        return;
    }
    if (tk == GMB_TOK_Height)
    {
        if (n < 1) { c99_fail(c, s->line, "WORLD HEIGHT: manca il settore"); return; }
        wcall(c, "gmb_world_height", a, 1);
        return;
    }
    if (tk == GMB_TOK_Anim)
    {
        if (n < 4) { c99_fail(c, s->line, "WORLD ANIM settore, prima, quante, fps"); return; }
        wcall(c, "gmb_world_anim", a, 4);
        return;
    }
    if (tk == GMB_TOK_Scroll)
    {
        if (n < 4) { c99_fail(c, s->line, "WORLD SCROLL settore, what, u/s, v/s"); return; }
        wcall(c, "gmb_world_scroll", a, 4);
        return;
    }
    if (tk == GMB_TOK_LightFx)
    {
        if (n < 4) { c99_fail(c, s->line, "WORLD LIGHTFX settore, modo, amount, rate"); return; }
        wcall(c, "gmb_world_lightfx", a, 4);
        return;
    }
    if (tk == GMB_TOK_Lamp)
    {
        if (n < 5) { c99_fail(c, s->line, "WORLD LAMP x, y, z, raggio, livello"); return; }
        wcall(c, "gmb_world_lamp", a, 5);
        return;
    }
    c99_fail(c, s->line, "WORLD: verbo non supportato dall'emitter nativo");
}

/* Namespace della call: il parser a volte conserva `ns` a parte, altre fonde il nome
 * (`var = "WORLD.MOVING"` con `ns` vuoto). Entrambe le forme sono legittime: si
 * ricava il namespace dal prefisso quando `ns` non c'è. */
static int wns_is_world(const char* name, const char* ns)
{
    if (ns && ns[0]) return strcasecmp(ns, "WORLD") == 0;
    if (!name) return 0;
    const char* dot = strrchr(name, '.');
    if (!dot || dot == name) return 0;
    size_t n = (size_t)(dot - name);
    return n == 5 && strncasecmp(name, "WORLD", 5) == 0;
}

/* Letture qualificate: WORLD.MOVING(…) ecc. Il filtro sul namespace serve a `TIME`,
 * che altrove è la funzione omonima: senza qualifica WORLD non è roba nostra. */
int emit_bf_world(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                  gmb_expr_t** args, int nargs, int line)
{
    (void)line;
    if (!wns_is_world(name, ns)) return 0;

    const char* fn = NULL;
    switch (tk)
    {
        case GMB_TOK_Moving:   fn = "gmb_world_moving"; break;
        case GMB_TOK_LightNow: fn = "gmb_world_lightnow"; break;
        case GMB_TOK_FloorZ:   fn = "gmb_world_floor"; break;
        case GMB_TOK_CeilZ:    fn = "gmb_world_ceil"; break;
        case GMB_TOK_WorldTime: fn = "gmb_world_time"; break;
        default: return 0;
    }
    sb_printf(&c->out, "%s(", fn);
    if (tk != GMB_TOK_WorldTime)
    {
        if (nargs < 1) return 0;
        emit_expr(c, args[0]);
    }
    sb_put(&c->out, ")");
    return 1;
}
