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

/* `gmb_scene_checked(fn(a, b, …), riga);` — le superfici SCENE riportano il
 * fallimento (argomento fuori range, NaN/Inf, stato mancante) come errore BASIC
 * con il messaggio canonico del runtime, non come no-op silenzioso. Una sola
 * forma per tutte le gambe. */
static void wscene(ctx_t* c, const char* fn, gmb_expr_t** a, int n, int line)
{
    ind(c);
    sb_printf(&c->out, "gmb_scene_checked(%s(", fn);
    for (int i = 0; i < n; i++)
    {
        if (i) sb_put(&c->out, ", ");
        emit_expr(c, a[i]);
    }
    sb_printf(&c->out, "), %d);\n", line);
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

/* ═══ SCENE — renderer mesh (P3) ══════════════════════════════════════════════
 *
 * Sintassi (piano §4.2, ridotta a ciò che il kernel P3 espone):
 *   SCENE RESET                              ' azzera geometria e camera
 *   SCENE VIEW w, h, fov                     ' target + campo visivo VERTICALE in gradi
 *   SCENE TEXTURE celle                      ' texture a scacchi procedurale (1..64)
 *   SCENE TEXTURE "file.png"                 ' texture da PNG (stesso verbo, argomento stringa)
 *   SCENE OBJECT r, g, b                     ' apre un oggetto con la sua tinta
 *   SCENE VERTEX x, y, z, u, v               ' vertice dell'oggetto corrente
 *   SCENE QUAD a, b, c, d                    ' due triangoli sui vertici dichiarati
 *   SCENE BOX x0, y0, z0, x1, y1, z1         ' 24 vertici, 12 triangoli, UV per faccia
 *   SCENE OBJPOS x, y, z, yaw                ' posizione/rotazione dell'oggetto corrente
 *   SCENE CAMERA x, y, z, yaw, pitch         ' yaw 0 = +z, pitch < 0 = in basso
 *   SCENE LIGHT dx, dy, dz, livello           ' luce direzionale, normale di faccia (idea #4)
 *   SCENE SMOOTH|ALPHA|TWOSIDED on|off        ' flag di materiale del kernel (idea #5)
 *   SCENE PRESENT                             ' target 3D → pagina 2D corrente (idea #2)
 *   SCENE RENDER                             ' rasterizza nel target
 *   SCENE SAVE path$                         ' PNG del frame corrente
 *
 * Due forme di TEXTURE: `celle` (numero) è la scacchiera procedurale, `"file.png"`
 * (stringa) carica un PNG con l'unico decoder del runtime (`png_decode`, §0bis).
 *
 * Letture: SCENE.VERTS/TRIS/DRAWN/CULLED/PIXELS/HASH/PIXEL(x,y)/PICK(x,y).
 *
 * `SCENE PICK on|off` accende il buffer di id per pixel usato da SCENE.PICK:
 * additivo e spento di default, quindi le scene che non lo chiamano restano
 * byte per byte quelle di prima.
 *
 * Anche qui nessuna logica: ogni verbo è UNA chiamata alla facciata `gmb_scene_*` di
 * gmb_3d_cmd.c, che a sua volta chiama il kernel `gmb_3d_mesh_draw` (§0bis).
 */
void emit_scene_domain(ctx_t* c, gmb_stmt_t* s)
{
    int tk = s->act_tk;
    gmb_expr_t** a = s->go_args;
    int n = s->n_go_args;

    if (tk == GMB_TOK_SceneReset)
    {
        if (n > 0) { c99_fail(c, s->line, "SCENE RESET non accetta argomenti"); return; }
        wscene(c, "gmb_scene_reset", a, 0, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneView)
    {
        if (n != 3) { c99_fail(c, s->line, "SCENE VIEW w, h, fov"); return; }
        wscene(c, "gmb_scene_view", a, 3, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneTexture)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE TEXTURE celle | \"file.png\""); return; }
        /* Due forme, un verbo: numero = scacchiera procedurale, stringa = PNG su disco.
         * La scelta sta nel TIPO dell'espressione (etype), non nel lessema (§12): una
         * variabile stringa (`p$`) passa da `gmb_scene_texture_file`, un numero da
         * `gmb_scene_checker`. TY_VAL (dinamico) resta sul ramo numerico, il suo valore
         * a runtime non è una stringa in nessun programma del dominio. */
        if (etype(c, a[0]) == TY_STR)
        {
            ind(c);
            sb_printf(&c->out, "gmb_scene_checked(gmb_scene_texture_file((");
            emit_str_operand(c, a[0]);
            sb_printf(&c->out, ").data), %d);\n", s->line);
            return;
        }
        wscene(c, "gmb_scene_checker", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneObject)
    {
        /* 3 argomenti = tinta; 5 = tinta + flag M-FLAGS + luce base dell'oggetto.
         * Le forme intermedie non esistono nel kernel e non si accettano. */
        if (n == 5) { wscene(c, "gmb_scene_object_flags", a, 5, s->line); return; }
        if (n != 3) { c99_fail(c, s->line, "SCENE OBJECT r, g, b [, flags [, luce]]"); return; }
        wscene(c, "gmb_scene_object", a, 3, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneVertex)
    {
        /* 5 argomenti = vertice senza normale; 8 = con la normale MONDO (M-LIGHT-N).
         * Le forme intermedie non esistono nel kernel. */
        if (n == 8) { wscene(c, "gmb_scene_vertex_n", a, 8, s->line); return; }
        if (n != 5) { c99_fail(c, s->line, "SCENE VERTEX x, y, z, u, v [, nx, ny, nz]"); return; }
        wscene(c, "gmb_scene_vertex", a, 5, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneQuad)
    {
        if (n != 4) { c99_fail(c, s->line, "SCENE QUAD a, b, c, d"); return; }
        wscene(c, "gmb_scene_quad", a, 4, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneBox)
    {
        if (n != 6) { c99_fail(c, s->line, "SCENE BOX x0, y0, z0, x1, y1, z1"); return; }
        wscene(c, "gmb_scene_box", a, 6, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneObjPos)
    {
        if (n != 4) { c99_fail(c, s->line, "SCENE OBJPOS x, y, z, yaw"); return; }
        wscene(c, "gmb_scene_objpos", a, 4, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneKey)
    {
        /* M-KEY: oggetto, tempo e sei componenti TRS. L'indice è il primo argomento
         * (come SCENE THING/ZONE): la facciata C converte e valida. */
        if (n != 8) { c99_fail(c, s->line, "SCENE KEY o, t, x, y, z, yaw, pitch, roll"); return; }
        wscene(c, "gmb_scene_key", a, 8, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneTime)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE TIME t"); return; }
        wscene(c, "gmb_scene_time", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneLod)
    {
        /* M-LOD: oggetto, distanza in unità mondo e mesh sostitutiva. Indici e
         * distanza li valida la facciata C (gmb_3d_lod.c): qui solo l'arità. */
        if (n != 3) { c99_fail(c, s->line, "SCENE LOD o, dist, mesh"); return; }
        wscene(c, "gmb_scene_lod", a, 3, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneInstance)
    {
        /* M-INST: oggetto e sorgente (indici 1..nobj). La facciata C valida e
         * risolve geometria/materiale: qui solo l'arità. */
        if (n != 2) { c99_fail(c, s->line, "SCENE INSTANCE o, src"); return; }
        wscene(c, "gmb_scene_instance", a, 2, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneBones)
    {
        /* M-SKIN: numero di ossa (0..64), validato dalla facciata C. */
        if (n != 1) { c99_fail(c, s->line, "SCENE BONES n"); return; }
        wscene(c, "gmb_scene_bones", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_ScenePoseBone)
    {
        /* M-SKIN: osso + TRS nello spazio dell'oggetto; la forma corta non ha
         * pitch/roll (come OBJECT 3/5 e LAMP 6/10). */
        if (n != 5 && n != 7)
        {
            c99_fail(c, s->line, "SCENE POSEBONE b, x, y, z, yaw [, pitch, roll]");
            return;
        }
        wscene(c, n == 7 ? "gmb_scene_posebone_trs" : "gmb_scene_posebone", a, n, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneWeight)
    {
        /* M-SKIN: oggetto, vertice (0-based, relativo all'oggetto), osso e peso.
         * Range e massimo due influenze li valida gmb_3d_skin.c. */
        if (n != 4) { c99_fail(c, s->line, "SCENE WEIGHT o, v, bone, w"); return; }
        wscene(c, "gmb_scene_weight", a, 4, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneAnim)
    {
        /* M-SKIN-KEY: osso e tempo, poi la TRS; forma corta senza pitch/roll,
         * come POSEBONE. Le piste le campiona RENDER. */
        if (n != 6 && n != 8)
        {
            c99_fail(c, s->line, "SCENE ANIM b, t, x, y, z, yaw [, pitch, roll]");
            return;
        }
        wscene(c, n == 8 ? "gmb_scene_anim_trs" : "gmb_scene_anim", a, n, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneCamera)
    {
        if (n != 5) { c99_fail(c, s->line, "SCENE CAMERA x, y, z, yaw, pitch"); return; }
        wscene(c, "gmb_scene_camera", a, 5, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneSmooth || tk == GMB_TOK_SceneAlpha || tk == GMB_TOK_SceneTwoSided
        || tk == GMB_TOK_SceneAa || tk == GMB_TOK_ScenePick)
    {
        /* Flag di scena/materiale: un argomento numerico 0/1, che il parser ha già
         * ricavato dalla parola ON/OFF. Stessa facciata C per tutte e quattro le gambe. */
        const char* fn = tk == GMB_TOK_SceneSmooth ? "gmb_scene_smooth"
                       : tk == GMB_TOK_SceneAlpha ? "gmb_scene_alpha"
                       : tk == GMB_TOK_SceneTwoSided ? "gmb_scene_twosided"
                       : tk == GMB_TOK_SceneAa ? "gmb_scene_aa" : "gmb_scene_pick_enable";
        const char* usage = tk == GMB_TOK_ScenePick ? "SCENE PICK on|off"
                                                    : "SCENE SMOOTH|ALPHA|TWOSIDED|AA on|off";
        if (n != 1) { c99_fail(c, s->line, usage); return; }
        wscene(c, fn, a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneLight)
    {
        if (n != 4) { c99_fail(c, s->line, "SCENE LIGHT dx, dy, dz, livello"); return; }
        wscene(c, "gmb_scene_light", a, 4, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneLamp)
    {
        /* 6 argomenti = lampada puntiforme; 10 = cono spot (direzione + semiangolo in
         * gradi). La scelta sta nel NUMERO di argomenti, non nel lessema (§12). */
        if (n == 10) { wscene(c, "gmb_scene_lamp_spot", a, 10, s->line); return; }
        if (n != 6)
        {
            c99_fail(c, s->line, "SCENE LAMP n, x, y, z, raggio, quantità [, dx, dy, dz, angolo]");
            return;
        }
        wscene(c, "gmb_scene_lamp", a, 6, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneFog)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE FOG distanza"); return; }
        wscene(c, "gmb_scene_fog", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneFar)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE FAR distanza"); return; }
        wscene(c, "gmb_scene_far", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneColormap)
    {
        if (n > 0) { c99_fail(c, s->line, "SCENE COLORMAP non accetta argomenti"); return; }
        wscene(c, "gmb_scene_colormap", a, 0, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneSky)
    {
        /* Cielo per angolo con la texture corrente: repeat (giri della picture sui 360)
         * e shift (sfasamento in giri). Due argomenti, nessun indice di texture: SCENE ha
         * UNA texture alla volta (forma dichiarata in guida). */
        if (n != 2) { c99_fail(c, s->line, "SCENE SKY repeat, shift"); return; }
        wscene(c, "gmb_scene_sky", a, 2, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneThing)
    {
        if (n != 6) { c99_fail(c, s->line, "SCENE THING n, x, y, z, w, h"); return; }
        wscene(c, "gmb_scene_thing", a, 6, s->line);
        return;
    }
    /* P5 — superficie interattiva (specifica §3.3). Cambia solo la forma degli
     * argomenti; la semantica è nella facciata gmb_scene_* del runtime. */
    if (tk == GMB_TOK_SceneZone)
    {
        if (n != 7 && n != 8)
        {
            c99_fail(c, s->line, "SCENE ZONE id, x1, y1, x2, y2, z1, z2 [, tag]");
            return;
        }
        ind(c);
        sb_put(&c->out, "gmb_scene_checked(gmb_scene_zone(");
        for (int i = 0; i < 7; i++)
        {
            if (i) sb_put(&c->out, ", ");
            emit_expr(c, a[i]);
        }
        sb_put(&c->out, ", ");
        if (n == 8)
        {
            sb_put(&c->out, "(");
            emit_str_operand(c, a[7]);
            sb_put(&c->out, ").data");
        }
        else sb_put(&c->out, "NULL");
        sb_printf(&c->out, "), %d);\n", s->line);
        return;
    }
    if (tk == GMB_TOK_ScenePlayer)
    {
        if (n != 4) { c99_fail(c, s->line, "SCENE PLAYER raggio, altezza, occhio, step"); return; }
        wscene(c, "gmb_scene_player", a, 4, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneMove)
    {
        if (n != 2) { c99_fail(c, s->line, "SCENE MOVE avanti, destra"); return; }
        wscene(c, "gmb_scene_move", a, 2, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneTurn)
    {
        if (n < 1 || n > 2) { c99_fail(c, s->line, "SCENE TURN dyaw [, dpitch]"); return; }
        if (n == 1)
        {
            /* `TURN dyaw` = `TURN dyaw, 0`: la facciata ha una sola forma. */
            ind(c);
            sb_printf(&c->out, "gmb_scene_checked(gmb_scene_turn(");
            emit_expr(c, a[0]);
            sb_printf(&c->out, ", 0.0), %d);\n", s->line);
            return;
        }
        wscene(c, "gmb_scene_turn", a, 2, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneLift)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE LIFT dy"); return; }
        wscene(c, "gmb_scene_lift", a, 1, s->line);
        return;
    }
    if (tk == GMB_TOK_ScenePresent)
    {
        /* Ponte target 3D → pagina 2D: nessuna scalatura, nessun argomento. La guardia
         * GPM e il ritaglio stanno nella facciata C, unica sede per le quattro gambe. */
        if (n > 0) { c99_fail(c, s->line, "SCENE PRESENT non accetta argomenti"); return; }
        wscene(c, "gmb_scene_present", a, 0, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneRender)
    {
        if (n > 0) { c99_fail(c, s->line, "SCENE RENDER non accetta argomenti"); return; }
        wscene(c, "gmb_scene_render", a, 0, s->line);
        return;
    }
    if (tk == GMB_TOK_SceneSave)
    {
        if (n != 1) { c99_fail(c, s->line, "SCENE SAVE: manca il percorso del PNG"); return; }
        ind(c);
        sb_put(&c->out, "gmb_scene_checked(gmb_scene_save((");
        emit_str_operand(c, a[0]);
        sb_printf(&c->out, ").data), %d);\n", s->line);
        return;
    }
    c99_fail(c, s->line, "SCENE: verbo non supportato dall'emitter nativo");
}

/* Namespace della call per SCENE, con la stessa doppia forma di WORLD: `ns` a parte oppure
 * fuso nel nome (`var = "SCENE.VERTS"`). */
static int sns_is_scene(const char* name, const char* ns)
{
    if (ns && ns[0]) return strcasecmp(ns, "SCENE") == 0;
    if (!name) return 0;
    const char* dot = strrchr(name, '.');
    if (!dot || dot == name) return 0;
    size_t n = (size_t)(dot - name);
    return n == 5 && strncasecmp(name, "SCENE", 5) == 0;
}

/* Letture qualificate della scena: SCENE.VERTS() ecc. */
int emit_bf_scene(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                  gmb_expr_t** args, int nargs, int line)
{
    (void)line;
    if (!sns_is_scene(name, ns)) return 0;

    switch (tk)
    {
        case GMB_TOK_SceneVerts:  sb_put(&c->out, "gmb_scene_verts()");  return 1;
        case GMB_TOK_SceneTris:   sb_put(&c->out, "gmb_scene_tris()");   return 1;
        case GMB_TOK_SceneDrawn:  sb_put(&c->out, "gmb_scene_drawn()");  return 1;
        case GMB_TOK_SceneCulled: sb_put(&c->out, "gmb_scene_culled()"); return 1;
        case GMB_TOK_ScenePixels: sb_put(&c->out, "gmb_scene_pixels()"); return 1;
        case GMB_TOK_SceneHash:   sb_put(&c->out, "gmb_scene_hash()");   return 1;
        /* PICK(x, y): id dell'oggetto nel pixel (0 fondo, 1..256 oggetto,
         * -1 senza target/fuori target, -2 buffer spento). Stessa facciata C
         * delle altre gambe. */
        case GMB_TOK_ScenePick:
            if (nargs < 2) return 0;
            sb_put(&c->out, "gmb_scene_pick(");
            emit_expr(c, args[0]);
            sb_put(&c->out, ", ");
            emit_expr(c, args[1]);
            sb_put(&c->out, ")");
            return 1;
        case GMB_TOK_ScenePixel:
            if (nargs < 2) return 0;
            sb_put(&c->out, "gmb_scene_pixel(");
            emit_expr(c, args[0]);
            sb_put(&c->out, ", ");
            emit_expr(c, args[1]);
            sb_put(&c->out, ")");
            return 1;
        /* P5: camera, corpo, quote e zone. Le letture senza argomenti sono
         * chiamate dirette; FLOORAT/CEILAT/ZONE prendono (x, y, z). */
        case GMB_TOK_SceneX:   sb_put(&c->out, "gmb_scene_x()");   return 1;
        case GMB_TOK_SceneY:   sb_put(&c->out, "gmb_scene_y()");   return 1;
        case GMB_TOK_SceneZ:   sb_put(&c->out, "gmb_scene_z()");   return 1;
        case GMB_TOK_SceneEye: sb_put(&c->out, "gmb_scene_eye()"); return 1;
        case GMB_TOK_SceneYaw: sb_put(&c->out, "gmb_scene_yaw()"); return 1;
        case GMB_TOK_ScenePitch: sb_put(&c->out, "gmb_scene_pitch()"); return 1;
        case GMB_TOK_SceneHit: sb_put(&c->out, "gmb_scene_hit()"); return 1;
        case GMB_TOK_SceneFloorAt:
        case GMB_TOK_SceneCeilAt:
        case GMB_TOK_SceneZone:
            if (nargs < 3) return 0;
            sb_printf(&c->out, "%s(", tk == GMB_TOK_SceneFloorAt ? "gmb_scene_floor_at"
                                         : tk == GMB_TOK_SceneCeilAt ? "gmb_scene_ceil_at"
                                         : "gmb_scene_zone_at");
            for (int i = 0; i < 3; i++)
            {
                if (i) sb_put(&c->out, ", ");
                emit_expr(c, args[i]);
            }
            sb_put(&c->out, ")");
            return 1;
        /* ZONETAG(id): stringa BASIC costruita dal runtime (gmb_string_alloc
         * copia il puntatore, che vale fino alla prossima chiamata). */
        /* STATS(nome$): una misura della scena per nome (M-STATS); -1 = ignota. */
        case GMB_TOK_SceneStats:
            if (nargs < 1) return 0;
            sb_put(&c->out, "gmb_scene_stats((");
            emit_str_operand(c, args[0]);
            sb_put(&c->out, ").data)");
            return 1;
        case GMB_TOK_SceneZoneTag:
            if (nargs < 1) return 0;
            sb_put(&c->out, "gmb_string_alloc(gmb_scene_zonetag_text(");
            emit_expr(c, args[0]);
            sb_put(&c->out, "))");
            return 1;
        /* P5/M-RAYS: raggi. SHOOT(range) e USE(reach) prendono un double e
         * tornano l'id; SEES(n) torna 1/0/−1; SHOT* leggono l'ultimo colpo. */
        case GMB_TOK_SceneShoot:
            if (nargs < 1) return 0;
            sb_put(&c->out, "gmb_scene_shoot(");
            emit_expr(c, args[0]);
            sb_put(&c->out, ")");
            return 1;
        case GMB_TOK_SceneUse:
            if (nargs < 1) return 0;
            sb_put(&c->out, "gmb_scene_use(");
            emit_expr(c, args[0]);
            sb_put(&c->out, ")");
            return 1;
        case GMB_TOK_SceneSees:
            if (nargs < 1) return 0;
            sb_put(&c->out, "gmb_scene_sees(");
            emit_expr(c, args[0]);
            sb_put(&c->out, ")");
            return 1;
        case GMB_TOK_SceneShotKind: sb_put(&c->out, "gmb_scene_shotkind()"); return 1;
        case GMB_TOK_SceneShotObj:  sb_put(&c->out, "gmb_scene_shotobj()");  return 1;
        case GMB_TOK_SceneShotDist: sb_put(&c->out, "gmb_scene_shotdist()"); return 1;
        case GMB_TOK_SceneShotX:    sb_put(&c->out, "gmb_scene_shotx()");    return 1;
        case GMB_TOK_SceneShotY:    sb_put(&c->out, "gmb_scene_shoty()");    return 1;
        case GMB_TOK_SceneShotZ:    sb_put(&c->out, "gmb_scene_shotz()");    return 1;
        default: return 0;
    }
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
