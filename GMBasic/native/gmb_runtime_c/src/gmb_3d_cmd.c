/* gmb_3d_cmd.c — superficie dei comandi BASIC del motore 3D (P2, specifica §2.4).
 *
 * Il BASIC non maneggia handle: il dominio `WORLD` lavora su UN mondo corrente,
 * come gli altri domini a stato globale (DBF, ISAM, …). Questo file è SOLO la
 * faccia C delle funzioni `gmb_3d_*`: nessuna logica, nessuna copia di stato
 * (§0bis: la logica vive una volta sola, in gmb_3d_time.c/gmb_3d_io.c).
 *
 * Comandi:  WORLD LOAD|CLOSE|TICK, SECMOVE, HEIGHT, ANIM, SCROLL, LIGHTFX, LAMP
 * Letture:  WORLD.MOVING, WORLD.LIGHTNOW, WORLD.TIME, WORLD.FLOORZ, WORLD.CEILZ
 *           (funzioni: leggono e basta)
 *
 * In coda la superficie SCENE (P3): le stesse funzioni servono i comandi
 * `SCENE` delle quattro gambe e le `DECLARE NATIVE ... LIB "gmb_3d"`.
 */
/* png_decode + read_whole_file: l'UNICO decode PNG del runtime (gmb_image.c usa gli
 * stessi due, §0bis). Sono in gmb_graphics_internal.h perché servono anche a texture
 * 2D, sprite e screenshot. */
#include "gmb_3d_internal.h"
#include "gmb_graphics_internal.h"
/* Guardia GPM di SCENE PRESENT: gmb_c99gpm_active()/gmb_cur() — gli stessi simboli
 * (riferimenti FORTI) con cui gmb_3d_frame.c protegge il claim del renderer. */
#include "gmb_c99gpm.h"
/* gmb_raise_error: l'errore controllato delle superfici SCENE diventa un errore
 * BASIC catturabile (ON ERROR/TRY) sulla gamba C99Native. */
#include "gmb_runtime.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static gmb_3d_world_t* g_world = NULL;

static int no_world(void)
{
    gmb_3d_set_error("WORLD: nessun mondo caricato (usa WORLD LOAD)");
    return GMB_3D_E_ARG;
}

/* --- comandi ------------------------------------------------------------- */

int gmb_world_load(const char* path)
{
    gmb_3d_world_t* w = NULL;
    int rc = gmb_3d_world_load(path, &w);
    if (rc != GMB_3D_OK) return rc;   /* load atomico: il mondo precedente resta */
    if (g_world) gmb_3d_world_free(g_world);
    g_world = w;
    return GMB_3D_OK;
}

void gmb_world_free(void)
{
    if (!g_world) return;
    gmb_3d_world_free(g_world);
    g_world = NULL;
}

int gmb_world_tick(double seconds)
{
    if (!g_world) return no_world();
    return gmb_3d_tick(g_world, seconds);
}

int gmb_world_secmove(int sec, double speed, double floor_z)
{
    if (!g_world) return no_world();
    return gmb_3d_secmove(g_world, sec, (float)speed, (float)floor_z);
}

int gmb_world_secmove_ceil(int sec, double speed, double floor_z, double ceil_z)
{
    if (!g_world) return no_world();
    return gmb_3d_secmove_ceil(g_world, sec, (float)speed, (float)floor_z, (float)ceil_z);
}

/* HEIGHT: ferma il mover del settore (le quote si leggono con WORLD.FLOOR/CEIL). */
int gmb_world_height(int sec)
{
    if (!g_world) return no_world();
    return gmb_3d_sector_height(g_world, sec, NULL, NULL);
}

int gmb_world_anim(int sec, int first, int count, double fps)
{
    if (!g_world) return no_world();
    return gmb_3d_anim(g_world, sec, first, count, (float)fps);
}

int gmb_world_scroll(int sec, int what, double u_per_s, double v_per_s)
{
    if (!g_world) return no_world();
    return gmb_3d_scroll(g_world, sec, what, (float)u_per_s, (float)v_per_s);
}

int gmb_world_lightfx(int sec, int mode, double amount, double rate)
{
    if (!g_world) return no_world();
    return gmb_3d_lightfx(g_world, sec, mode, (float)amount, (float)rate);
}

int gmb_world_lamp(double x, double y, double z, double radius, double level)
{
    if (!g_world) return no_world();
    return gmb_3d_lamp(g_world, (float)x, (float)y, (float)z, (float)radius, (float)level);
}

/* --- letture (non toccano il mondo) -------------------------------------- */

double gmb_world_moving(int sec)
{
    return g_world ? (double)gmb_3d_moving(g_world, sec) : -1.0;
}

double gmb_world_lightnow(int sec)
{
    return g_world ? (double)gmb_3d_light_now(g_world, sec) : 0.0;
}

double gmb_world_time(void)
{
    return g_world ? gmb_3d_time(g_world) : 0.0;
}

double gmb_world_floor(int sec)
{
    float f = 0.0f;
    return (g_world && gmb_3d_sector_z(g_world, sec, &f, NULL) == GMB_3D_OK) ? (double)f : 0.0;
}

double gmb_world_ceil(int sec)
{
    float c = 0.0f;
    return (g_world && gmb_3d_sector_z(g_world, sec, NULL, &c) == GMB_3D_OK) ? (double)c : 0.0;
}

/* ═══ SCENE — superficie del renderer mesh (P3) ═════════════════════════════
 *
 * Un solo stato di scena per processo, come WORLD (§2.0: il BASIC non maneggia
 * handle). Qui vive SOLO l'accumulo della geometria e la scelta del materiale;
 * il disegno è `gmb_3d_mesh_draw` (gmb_3d_frame.c), la profondità è la sua e il
 * campionamento delle texture è `gm_tex_sample` (§0bis: nessuna copia).
 *
 * Una implementazione, due facce: i comandi `SCENE` (piano §4.2) e le
 * `DECLARE NATIVE ... LIB "gmb_3d"` chiamano queste stesse funzioni. L'ABI è
 * `int64_t`/`double`/`const char*` perché è quella che il FFI sa passare.
 *
 * Winding: antiorario VISTO DA FUORI (come il kernel). `BOX` riusa la geometria
 * del cubo unitario con le facce già orientate; una faccia inversa finisce nei
 * contatori come back-face, non sullo schermo.
 */

/* I tetti statici sono diventati CAPACITÀ DINAMICA (gmb_3d_scene_buf.c, P5):
 * si parte dai valori storici e si raddoppia alla dichiarazione della
 * geometria, così i 200k triangoli dell'accettazione ci stanno. Qui resta solo
 * il fondo della scena. */
#define GMB_SCENE_BG        0x0F1729u   /* lo stesso fondo dell'oracolo */
#define GMB_SCENE_PI        3.14159265358979

/* Una faccia del cubo unitario ha una luce fissa: il kernel ha UNA luce per
 * draw, quindi l'ombreggiatura delle 6 facce si ottiene con 6 draw (tinta per
 * faccia, non una seconda illuminazione). */
static const unsigned char g_scene_face_light[6] = { 220, 160, 255, 110, 190, 140 };

static const float g_scene_unit_pos[24 * 3] = {
    /* +X */  1,-1,-1,  1, 1,-1,  1, 1, 1,  1,-1, 1,
    /* -X */ -1,-1, 1, -1, 1, 1, -1, 1,-1, -1,-1,-1,
    /* +Y */ -1, 1,-1, -1, 1, 1,  1, 1, 1,  1, 1,-1,
    /* -Y */ -1,-1, 1, -1,-1,-1,  1,-1,-1,  1,-1, 1,
    /* +Z */ -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,
    /* -Z */  1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1
};
/* Le stesse 4 UV per ognuna delle 6 facce (indice locale del vertice = i & 3). */
static const float g_scene_unit_uv[4 * 2] = {
    0,1, 1,1, 1,0, 0,0
};
static const int g_scene_unit_idx[12 * 3] = {
     0, 1, 2,  0, 2, 3,
     4, 5, 6,  4, 6, 7,
     8, 9,10,  8,10,11,
    12,13,14, 12,14,15,
    16,17,18, 16,18,19,
    20,21,22, 20,22,23
};

/* La geometria (vertici, triangoli, oggetti) e i suoi scratch vivono in
 * gmb_3d_scene_buf.c, che ne possiede i buffer e la crescita dinamica: qui si
 * nomina solo, così la facciata resta una facciata (§0bis: una sola sede).
 * `gmb_scene_obj_t` e le capacità (INIT/MAX) stanno in gmb_3d_internal.h. */
#define g_scene (*gmb_3d_scene_geom())

#define GMB_SCENE_MAX_THINGS 256

/* Il thing (M-THINGS) è dichiarato in gmb_3d_internal.h: lo leggono anche i raggi
 * (gmb_3d_ray.c), che ne hanno un accessore in sola lettura qui sotto. */
typedef gmb_3d_scene_thing_rt gmb_scene_thing_t;

static gmb_3d_target_t g_scene_target;   /* argb != NULL = target allocato */
static double  g_scene_fov = 60.0;
static float   g_scene_eye[3] = { 0.0f, 1.5f, 6.0f };
static float   g_scene_yaw = 0.0f, g_scene_pitch = 0.0f;
/* Buffer della geometria: puntatori e capacità stanno in `g_scene` (vedi sopra). */

/* Luce direzionale per faccia (idea #4). `SCENE LIGHT dx, dy, dz, livello` imposta la
 * direzione VERSO la luce (il vettore L di Lambert); la normale di faccia si calcola a
 * ogni RENDER dalle posizioni MONDO (dopo il modello), quindi anche un cubo ruotato si
 * illumina come è davvero. Aritmetica: normale e prodotto scalare in double (la
 * geometria è in virgola), ma il coseno viene quantizzato UNA volta in un intero 0..255
 * e da lì la miscela ambiente→luce è intera: `amb + (livello - amb) * q / 255`. La sede
 * è questa sola, quindi le quattro gambe danno gli stessi byte.
 *
 * Con LIGHT attiva la luce calcolata SOSTITUISCE la tabella fissa delle 6 facce del cubo
 * (che è una stilizzazione); senza LIGHT tutto resta com'era, hash compresi. */
#define GMB_SCENE_AMBIENT 40

static int      g_scene_lit = 0;
static double   g_scene_light_dir[3] = { 0.0, 1.0, 0.0 };
static int      g_scene_light_level = 255;
/* `g_scene.lit_tri`: luce calcolata per triangolo quando LIGHT è attiva. */

/* Flag di MATERIALE (idea #5): il kernel li ha già in `gmb_3d_material_t` (bilinear,
 * masked, two_sided) ma il BASIC non li raggiungeva. `SCENE SMOOTH|ALPHA|TWOSIDED on|off`
 * li espone senza toccare il rasterizzatore. I default sono quelli STORICI — bilineare
 * attivo, alpha-test attivo, back-face cull attivo — quindi ogni scena che non chiama i
 * nuovi verbi dà gli stessi byte di prima (hash dei MiniTest invariati). Sono stato del
 * materiale: sopravvivono a RESET, come TEXTURE e LIGHT. */
static int g_scene_smooth = 1;        /* 1 = bilineare, 0 = nearest */
static int g_scene_alpha = 1;         /* 1 = alpha 0 scartato (alpha-test) */
static int g_scene_two_sided = 0;     /* 1 = niente back-face cull */
static int g_scene_aa = 0;            /* 1 = SSAA 2x per draw (spento: come P4) */
/* Contatori in `g_scene`: nv, nt, nobj, cur (capacità: cap_v, cap_t, cap_o). */
static unsigned char* g_scene_tex = NULL;
static int     g_scene_tex_w = 0, g_scene_tex_h = 0;
static gmb_3d_draw_stats_t g_scene_stats;
static uint64_t g_scene_hash = 0;

/* P4 — ambiente della scena: lampade (M-LIGHT), fog e FAR (M-FOG), colormap a
 * 32 livelli (C-SAME-COLORMAP), cielo per angolo (M-SKY) e thing (M-THINGS).
 * Sono stato della SCENA e sopravvivono a RESET? No: RESET azzera tutto tranne
 * texture e flag di materiale, come gia' faceva per la geometria. */
static gmb_3d_env_t g_scene_env;
static int      g_scene_colormap = 0;
static unsigned char* g_scene_sky = NULL;
static int      g_scene_sky_w = 0, g_scene_sky_h = 0;
static float    g_scene_sky_repeat = 1.0f, g_scene_sky_shift = 0.0f;
static gmb_scene_thing_t g_scene_thing[GMB_SCENE_MAX_THINGS];

/* accessore in sola lettura per i raggi (gmb_3d_ray.c): l'array resta di questo
 * file, una sola copia (§0bis). */
const gmb_3d_scene_thing_rt* gmb_3d_scene_things(int* count)
{
    if (count) *count = GMB_SCENE_MAX_THINGS;
    return g_scene_thing;
}

/* Matrici e culling per oggetto vivono in gmb_3d_scene.c (P5): qui si chiamano
 * soltanto, così questo file resta la facciata dei comandi e sta sotto le 1200
 * righe (§0quater). */

/* --- accumulo della geometria -------------------------------------------- */

/* `n` NULL = vertice senza normale. Le normali sono per OGGETTO, non per vertice:
 * il primo vertice decide, e uno che non concorda e' un errore esplicito (non si
 * mescolano vertici con e senza, altrimenti la normale sarebbe 0 per alcuni). */
/* Messaggio canonico + E_ARG: UNA sede per le superfici SCENE. Il testo arriva
 * alle quattro gambe tramite gmb_3d_last_error() e diventa un errore BASIC di
 * runtime, quindi deve essere stabile e identico ovunque (suite negativa
 * tools/sh/check_scene_negatives.sh). */
static int g_scene_err(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

/* NaN e Inf non hanno senso in una scena: entrano nel kernel come coordinate e
 * producono pixel casuali in silenzio. Qui si rifiutano. */
static int g_scene_finite(const char* msg, double v)
{
    return isfinite(v) ? GMB_3D_OK : g_scene_err(msg);
}

static int g_scene_push_vert_ex(float x, float y, float z, float u, float v, const float* n)
{
    if (g_scene.cur < 0)
        return g_scene_err("SCENE VERTEX: nessun oggetto corrente (SCENE OBJECT prima)");
    /* P5: crescita dinamica (raddoppio) alla DICHIARAZIONE, mai nel frame. */
    if (gmb_3d_scene_reserve_verts(g_scene.nv + 1) != GMB_3D_OK) return GMB_3D_E_ARG;
    gmb_scene_obj_t* o = &g_scene.obj[g_scene.cur];
    int has = (n != NULL);
    if (o->nvert == 0) o->has_nrm = (unsigned char)has;
    else if (o->has_nrm != has)
        return g_scene_err("SCENE VERTEX: normali miste con e senza nello stesso oggetto");
    int fc = g_scene_finite("SCENE VERTEX: coordinate non finite", x);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE VERTEX: coordinate non finite", y);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE VERTEX: coordinate non finite", z);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE VERTEX: UV non finite", u);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE VERTEX: UV non finite", v);
    if (fc != GMB_3D_OK) return fc;
    g_scene.pos[g_scene.nv * 3 + 0] = x;
    g_scene.pos[g_scene.nv * 3 + 1] = y;
    g_scene.pos[g_scene.nv * 3 + 2] = z;
    /* P5: il primo vertice dell'oggetto apre l'AABB, gli altri lo allargano. */
    if (o->nvert == 0)
    {
        o->bmin[0] = o->bmax[0] = x;
        o->bmin[1] = o->bmax[1] = y;
        o->bmin[2] = o->bmax[2] = z;
    }
    else
    {
        if (x < o->bmin[0]) o->bmin[0] = x; else if (x > o->bmax[0]) o->bmax[0] = x;
        if (y < o->bmin[1]) o->bmin[1] = y; else if (y > o->bmax[1]) o->bmax[1] = y;
        if (z < o->bmin[2]) o->bmin[2] = z; else if (z > o->bmax[2]) o->bmax[2] = z;
    }
    g_scene.uv[g_scene.nv * 2 + 0] = u;
    g_scene.uv[g_scene.nv * 2 + 1] = v;
    if (has)
    {
        g_scene.nrm[g_scene.nv * 3 + 0] = n[0];
        g_scene.nrm[g_scene.nv * 3 + 1] = n[1];
        g_scene.nrm[g_scene.nv * 3 + 2] = n[2];
    }
    g_scene.nv++;
    o->nvert++;
    return GMB_3D_OK;
}

static int g_scene_push_vert(float x, float y, float z, float u, float v)
{
    return g_scene_push_vert_ex(x, y, z, u, v, NULL);
}

static int g_scene_push_tri(int a, int b, int c, unsigned char light)
{
    if (g_scene.cur < 0)
        return g_scene_err("SCENE QUAD: nessun oggetto corrente (SCENE OBJECT prima)");
    if (gmb_3d_scene_reserve_tris(g_scene.nt + 1) != GMB_3D_OK) return GMB_3D_E_ARG;
    if (a < 0 || b < 0 || c < 0
        || a >= g_scene.obj[g_scene.cur].nvert || b >= g_scene.obj[g_scene.cur].nvert
        || c >= g_scene.obj[g_scene.cur].nvert)
        return g_scene_err("SCENE QUAD: indice di vertice fuori range");
    g_scene.idx[g_scene.nt * 3 + 0] = a;
    g_scene.idx[g_scene.nt * 3 + 1] = b;
    g_scene.idx[g_scene.nt * 3 + 2] = c;
    g_scene.light[g_scene.nt] = light;
    g_scene.nt++;
    g_scene.obj[g_scene.cur].ntri++;
    return GMB_3D_OK;
}

static int g_scene_clampi(int64_t v)
{
    return (v < 0) ? 0 : (v > 255 ? 255 : (int)v);
}

/* --- comandi -------------------------------------------------------------- */

int64_t gmb_scene_reset(void)
{
    /* Contatori a zero, capacità CONSERVATA: un RESET non alloca. */
    gmb_3d_scene_geom_clear();
    memset(&g_scene_stats, 0, sizeof(g_scene_stats));
    g_scene_hash = 0;
    gmb_3d_env_clear(&g_scene_env);
    g_scene_colormap = 0;
    memset(g_scene_thing, 0, sizeof(g_scene_thing));
    gmb_3d_ray_reset();
    gmb_3d_stats_reset();   /* renders/renders_max del frame (M-STATS) */
    free(g_scene_sky);
    g_scene_sky = NULL;
    g_scene_sky_w = g_scene_sky_h = 0;
    g_scene_sky_repeat = 1.0f;
    g_scene_sky_shift = 0.0f;
    g_scene_eye[0] = 0.0f; g_scene_eye[1] = 1.5f; g_scene_eye[2] = 6.0f;
    g_scene_yaw = 0.0f;
    g_scene_pitch = 0.0f;
    gmb_3d_actor_reset();   /* corpo ai default M-PLAYER, zone azzerate */
    g_scene_fov = 60.0;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_view(int64_t w, int64_t h, double fov_deg)
{
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
        return g_scene_err("SCENE VIEW: w e h devono essere tra 1 e 8192");
    if (!isfinite(fov_deg))
        return g_scene_err("SCENE VIEW: fov deve essere un numero finito");
    if (fov_deg <= 1.0 || fov_deg >= 179.0)
        return g_scene_err("SCENE VIEW: fov deve essere tra 1 e 179 gradi");
    int rc = gmb_3d_target_alloc(&g_scene_target, (int)w, (int)h);
    if (rc != GMB_3D_OK) return rc;
    g_scene_fov = fov_deg;
    rc = gmb_3d_target_clear(&g_scene_target, GMB_SCENE_BG, 0.0f);
    if (rc != GMB_3D_OK) return rc;
    return gmb_scene_pick_attach(&g_scene_target);   /* SCENE PICK acceso: id della nuova dimensione */
}

/* Sostituzione ATOMICA della texture corrente, unica sede: la usano la
 * scacchiera procedurale (gmb_3d_scene_tex.c) e il PNG, e la leggono RENDER,
 * SKY e THING. */
void gmb_3d_scene_set_texture(unsigned char* buf, int w, int h)
{
    free(g_scene_tex);
    g_scene_tex = buf;
    g_scene_tex_w = w;
    g_scene_tex_h = h;
}

/* SCENE TEXTURE "file.png": il PNG diventa il materiale di tutti i draw successivi.
 *
 * ATOMICO: la texture si sostituisce solo a decode e conversione riusciti, così un
 * percorso sbagliato non lascia la scena senza texture (e l'errore resta leggibile
 * da gmb_3d_last_error(), come per WORLD LOAD). La riga 0 del PNG resta in ALTO:
 * è la convenzione che il kernel (`gm_tex_sample`) e `SCENE BOX` già usano. */
int64_t gmb_scene_texture_file(const char* path)
{
    if (path == NULL || *path == '\0')
        return g_scene_err("SCENE TEXTURE: manca il percorso del PNG");

    size_t len = 0;
    unsigned char* data = read_whole_file(path, &len);
    if (data == NULL)
    {
        gmb_3d_set_error("SCENE TEXTURE: file non leggibile");
        return GMB_3D_E_IO;
    }

    uint32_t* px = NULL;
    int w = 0, h = 0;
    int rc = png_decode(data, len, &px, &w, &h);
    free(data);
    if (rc != 0 || px == NULL)
    {
        free(px);
        gmb_3d_set_error("SCENE TEXTURE: PNG non decodificabile");
        return GMB_3D_E_IO;
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
    {
        free(px);
        gmb_3d_set_error("SCENE TEXTURE: dimensioni fuori limite (max 4096 per lato)");
        return GMB_3D_E_ARG;
    }

    /* 0xAARRGGBB (decoder) -> RGBA8 (materiale). */
    size_t np = (size_t)w * (size_t)h;
    unsigned char* buf = (unsigned char*)malloc(np * 4u);
    if (buf == NULL)
    {
        free(px);
        return GMB_3D_E_NOMEM;
    }
    for (size_t i = 0; i < np; i++)
    {
        uint32_t v = px[i];
        buf[i * 4 + 0] = (unsigned char)((v >> 16) & 0xFFu);
        buf[i * 4 + 1] = (unsigned char)((v >> 8) & 0xFFu);
        buf[i * 4 + 2] = (unsigned char)(v & 0xFFu);
        buf[i * 4 + 3] = (unsigned char)((v >> 24) & 0xFFu);
    }
    free(px);

    free(g_scene_tex);
    g_scene_tex = buf;
    g_scene_tex_w = w;
    g_scene_tex_h = h;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* SCENE LIGHT dx, dy, dz, livello: direzione unitaria verso la luce + livello 0..255.
 * La direzione viene normalizzata qui; una direzione nulla è un errore esplicito (non
 * un "luce da nessuna parte" che spegnerebbe la scena in silenzio). */
int64_t gmb_scene_light(double dx, double dy, double dz, int64_t level)
{
    int frc = g_scene_finite("SCENE LIGHT: direzione non finita", dx);
    if (frc == GMB_3D_OK) frc = g_scene_finite("SCENE LIGHT: direzione non finita", dy);
    if (frc == GMB_3D_OK) frc = g_scene_finite("SCENE LIGHT: direzione non finita", dz);
    if (frc != GMB_3D_OK) return frc;
    double len = sqrt(dx * dx + dy * dy + dz * dz);
    if (!(len > 1e-9)) return g_scene_err("SCENE LIGHT: direzione nulla");
    g_scene_light_dir[0] = dx / len;
    g_scene_light_dir[1] = dy / len;
    g_scene_light_dir[2] = dz / len;
    g_scene_light_level = g_scene_clampi(level);
    g_scene_lit = 1;
    return GMB_3D_OK;
}

/* SCENE SMOOTH|ALPHA|TWOSIDED on|off: i tre flag del materiale del kernel. ON = 1.
 * Nessuna logica qui: il rasterizzatore legge già `gmb_3d_material_t`. */
int64_t gmb_scene_smooth(int64_t on)
{
    g_scene_smooth = (on != 0);
    return GMB_3D_OK;
}

int64_t gmb_scene_alpha(int64_t on)
{
    g_scene_alpha = (on != 0);
    return GMB_3D_OK;
}

int64_t gmb_scene_twosided(int64_t on)
{
    g_scene_two_sided = (on != 0);
    return GMB_3D_OK;
}

/* SCENE AA on|off: il flag e' l'unica cosa che si tocca qui. Il render sceglie il
 * target doppio e lo risolve; la superficie (PIXEL/SAVE/PRESENT) resta sul target
 * logico. Spento di default: nessun byte cambia nelle scene esistenti. */
int64_t gmb_scene_aa(int64_t on)
{
    g_scene_aa = (on != 0);
    return GMB_3D_OK;
}

/* --- P4: lampade, fog, FAR, colormap, cielo, thing (specifica M-*) -------- */

/* SCENE LAMP n, x, y, z, raggio, quantità: lampada puntiforme 0..7 (M-LIGHT).
 * Raggio <= 0 o quantità 0 la spengono; `amount` si clampa 0..255. Il falloff e
 * l'eventuale cono si applicano nel kernel (`gmb_3d_light_eval`), non qui. */
int64_t gmb_scene_lamp(int64_t n, double x, double y, double z, double radius, double amount)
{
    if (n < 0 || n >= GMB_3D_MAX_LAMPS)
        return g_scene_err("SCENE LAMP: lo slot deve essere tra 0 e 7");
    if (!isfinite(x) || !isfinite(y) || !isfinite(z))
        return g_scene_err("SCENE LAMP: posizione non finita");
    if (!isfinite(radius) || !(radius >= 0.0))
        return g_scene_err("SCENE LAMP: raggio non negativo e finito");
    if (!isfinite(amount))
        return g_scene_err("SCENE LAMP: quantita' non finita");
    gmb_3d_lamp_t* L = &g_scene_env.lamps[n];
    memset(L, 0, sizeof(*L));
    L->x = (float)x;
    L->y = (float)y;
    L->z = (float)z;
    L->radius = (float)radius;
    L->amount = (unsigned char)g_scene_clampi((int64_t)(amount + 0.5));
    if (g_scene_env.nlamps < (int)n + 1) g_scene_env.nlamps = (int)n + 1;
    return GMB_3D_OK;
}

/* SCENE LAMP n, x, y, z, raggio, quantità, dx, dy, dz, angolo: spot. La
 * direzione punta VERSO l'illuminato (si normalizza qui), `angolo` è il
 * semiangolo del cono in gradi (0 < angolo < 90): pieno sull'asse, zero al bordo
 * (M-LIGHT). Una direzione nulla è un errore esplicito, non un cono ignorato. */
int64_t gmb_scene_lamp_spot(int64_t n, double x, double y, double z, double radius,
                            double amount, double dx, double dy, double dz, double angle_deg)
{
    int frc = g_scene_finite("SCENE LAMP SPOT: direzione non finita", dx);
    if (frc == GMB_3D_OK) frc = g_scene_finite("SCENE LAMP SPOT: direzione non finita", dy);
    if (frc == GMB_3D_OK) frc = g_scene_finite("SCENE LAMP SPOT: direzione non finita", dz);
    if (frc == GMB_3D_OK) frc = g_scene_finite("SCENE LAMP SPOT: angolo non finito", angle_deg);
    if (frc != GMB_3D_OK) return frc;
    double len = sqrt(dx * dx + dy * dy + dz * dz);
    if (!(len > 1e-9)) return g_scene_err("SCENE LAMP SPOT: direzione nulla");
    if (!(angle_deg > 0.0) || !(angle_deg < 90.0))
        return g_scene_err("SCENE LAMP SPOT: angolo tra 0 e 90 gradi");
    int64_t rc = gmb_scene_lamp(n, x, y, z, radius, amount);
    if (rc != GMB_3D_OK) return rc;
    gmb_3d_lamp_t* L = &g_scene_env.lamps[n];
    L->dir[0] = (float)(dx / len);
    L->dir[1] = (float)(dy / len);
    L->dir[2] = (float)(dz / len);
    L->half_angle = (float)(angle_deg * (GMB_SCENE_PI / 180.0));
    return GMB_3D_OK;
}

/* SCENE FOG d: buio alla distanza d (0 = spento). SCENE FAR d: oltre d il
 * triangolo si salta (0 = spento). Distanze negative = errore esplicito. */
int64_t gmb_scene_fog(double d)
{
    if (!isfinite(d) || !(d >= 0.0))
        return g_scene_err("SCENE FOG: distanza non negativa e finita");
    g_scene_env.fog = (float)d;
    return GMB_3D_OK;
}

int64_t gmb_scene_far(double d)
{
    if (!isfinite(d) || !(d >= 0.0))
        return g_scene_err("SCENE FAR: distanza non negativa e finita");
    g_scene_env.far = (float)d;
    return GMB_3D_OK;
}

/* SCENE COLORMAP: la luce si quantizza a 32 livelli (C-SAME-COLORMAP). Senza,
 * la miscela resta continua: le scene che non la chiamano non cambiano byte. */
int64_t gmb_scene_colormap(void)
{
    g_scene_colormap = 1;
    g_scene_env.colormap = 1;
    return GMB_3D_OK;
}

/* SCENE SKY [repeat [, shift]]: il cielo per angolo (M-SKY) con la texture
 * corrente, copiata qui perché TEXTURE può cambiare dopo. Senza texture è un
 * errore esplicito: un cielo nero silenzioso sembrerebbe un bug di rendering. */
int64_t gmb_scene_sky(double repeat, double shift)
{
    if (!isfinite(repeat) || !isfinite(shift))
        return g_scene_err("SCENE SKY: repeat e shift devono essere finiti");
    if (g_scene_tex == NULL || g_scene_tex_w <= 0 || g_scene_tex_h <= 0)
        return g_scene_err("SCENE SKY: nessuna texture (SCENE TEXTURE prima di SKY)");
    size_t np = (size_t)g_scene_tex_w * (size_t)g_scene_tex_h * 4u;
    unsigned char* buf = (unsigned char*)malloc(np);
    if (buf == NULL) return GMB_3D_E_NOMEM;
    memcpy(buf, g_scene_tex, np);
    free(g_scene_sky);
    g_scene_sky = buf;
    g_scene_sky_w = g_scene_tex_w;
    g_scene_sky_h = g_scene_tex_h;
    g_scene_sky_repeat = (repeat > 0.0) ? (float)repeat : 1.0f;
    g_scene_sky_shift = (float)shift;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

/* SCENE THING n, x, y, z, w, h: billboard 0..255 con la texture corrente, base
 * in (x,y,z) con y = altezza (la convenzione della superficie SCENE), luce piena
 * salvo THINGLIGHT (M-THINGS). Il disegno lo fa il kernel, con depth test e
 * ordinamento da lontano a vicino deciso da RENDER. */
int64_t gmb_scene_thing(int64_t n, double x, double y, double z, double w, double h)
{
    if (n < 0 || n >= GMB_SCENE_MAX_THINGS)
        return g_scene_err("SCENE THING: indice tra 0 e 255");
    if (!isfinite(x) || !isfinite(y) || !isfinite(z))
        return g_scene_err("SCENE THING: posizione non finita");
    if (!isfinite(w) || !isfinite(h) || !(w > 0.0) || !(h > 0.0))
        return g_scene_err("SCENE THING: w e h positivi e finiti");
    gmb_scene_thing_t* th = &g_scene_thing[n];
    th->used = 1;
    th->x = (float)x;
    th->y = (float)y;
    th->z = (float)z;
    th->w = (float)w;
    th->h = (float)h;
    th->light = 255u;
    return GMB_3D_OK;
}

int64_t gmb_scene_thing_light(int64_t n, int64_t level)
{
    if (n < 0 || n >= GMB_SCENE_MAX_THINGS || !g_scene_thing[n].used) return GMB_3D_E_ARG;
    g_scene_thing[n].light = (unsigned char)g_scene_clampi(level);
    return GMB_3D_OK;
}

int64_t gmb_scene_object(int64_t r, int64_t g, int64_t b)
{
    if (gmb_3d_scene_reserve_objs(g_scene.nobj + 1) != GMB_3D_OK) return GMB_3D_E_ARG;
    gmb_scene_obj_t* o = &g_scene.obj[g_scene.nobj];
    o->first_vert = g_scene.nv;
    o->nvert = 0;
    o->first_tri = g_scene.nt;
    o->ntri = 0;
    o->x = o->y = o->z = 0.0f;
    o->yaw = 0.0f;
    o->r = (unsigned char)g_scene_clampi(r);
    o->g = (unsigned char)g_scene_clampi(g);
    o->b = (unsigned char)g_scene_clampi(b);
    o->flags = 0;
    o->light = 255u;
    o->has_nrm = 0;
    g_scene.cur = g_scene.nobj++;
    return GMB_3D_OK;
}

/* SCENE OBJECT r, g, b, flags, luce: come OBJECT, con i flag M-FLAGS (1 masked,
 * 2 traslucido, 16 due lati; gli altri bit si ignorano) e la luce base
 * dell'oggetto, a cui le lampade si sommano. */
int64_t gmb_scene_object_flags(int64_t r, int64_t g, int64_t b, int64_t flags, int64_t light)
{
    int64_t rc = gmb_scene_object(r, g, b);
    if (rc != GMB_3D_OK) return rc;
    /* 1 masked, 2 traslucido, 4 passabile, 8 see-through, 16 due lati. */
    g_scene.obj[g_scene.cur].flags = (unsigned char)(flags & 0x1F);
    g_scene.obj[g_scene.cur].light = (unsigned char)g_scene_clampi(light);
    return GMB_3D_OK;
}

int64_t gmb_scene_vertex(double x, double y, double z, double u, double v)
{
    return g_scene_push_vert((float)x, (float)y, (float)z, (float)u, (float)v);
}

int64_t gmb_scene_vertex_n(double x, double y, double z, double u, double v,
                           double nx, double ny, double nz)
{
    float n[3] = { (float)nx, (float)ny, (float)nz };
    return g_scene_push_vert_ex((float)x, (float)y, (float)z, (float)u, (float)v, n);
}

int64_t gmb_scene_tri(int64_t a, int64_t b, int64_t c)
{
    return g_scene_push_tri((int)a, (int)b, (int)c, 255u);
}

int64_t gmb_scene_quad(int64_t a, int64_t b, int64_t c, int64_t d)
{
    int rc = g_scene_push_tri((int)a, (int)b, (int)c, 255u);
    if (rc != GMB_3D_OK) return rc;
    return g_scene_push_tri((int)a, (int)c, (int)d, 255u);
}

int64_t gmb_scene_box(double x0, double y0, double z0,
                      double x1, double y1, double z1)
{
    if (g_scene.cur < 0)
        return g_scene_err("SCENE BOX: serve un oggetto corrente (SCENE OBJECT prima)");
    if (g_scene.nv > GMB_SCENE_VERTS_MAX - 24 || g_scene.nt > GMB_SCENE_TRIS_MAX - 12)
        return g_scene_err("SCENE BOX: limite di geometria della scena");
    /* Riserva PRIMA di emettere: o entra il box intero o la scena non cambia. */
    if (gmb_3d_scene_reserve_verts(g_scene.nv + 24) != GMB_3D_OK) return GMB_3D_E_ARG;
    if (gmb_3d_scene_reserve_tris(g_scene.nt + 12) != GMB_3D_OK) return GMB_3D_E_ARG;
    int fc = g_scene_finite("SCENE BOX: coordinate non finite", x0);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE BOX: coordinate non finite", y0);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE BOX: coordinate non finite", z0);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE BOX: coordinate non finite", x1);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE BOX: coordinate non finite", y1);
    if (fc == GMB_3D_OK) fc = g_scene_finite("SCENE BOX: coordinate non finite", z1);
    if (fc != GMB_3D_OK) return fc;
    float cx = (float)((x0 + x1) * 0.5), hx = (float)((x1 - x0) * 0.5);
    float cy = (float)((y0 + y1) * 0.5), hy = (float)((y1 - y0) * 0.5);
    float cz = (float)((z0 + z1) * 0.5), hz = (float)((z1 - z0) * 0.5);
    int base = g_scene.obj[g_scene.cur].nvert;
    for (int i = 0; i < 24; i++)
    {
        int rc = g_scene_push_vert(cx + g_scene_unit_pos[i * 3 + 0] * hx,
                                   cy + g_scene_unit_pos[i * 3 + 1] * hy,
                                   cz + g_scene_unit_pos[i * 3 + 2] * hz,
                                   g_scene_unit_uv[(i & 3) * 2 + 0],
                                   g_scene_unit_uv[(i & 3) * 2 + 1]);
        if (rc != GMB_3D_OK) return rc;
    }
    for (int t = 0; t < 12; t++)
    {
        int rc = g_scene_push_tri(base + g_scene_unit_idx[t * 3 + 0],
                                  base + g_scene_unit_idx[t * 3 + 1],
                                  base + g_scene_unit_idx[t * 3 + 2],
                                  g_scene_face_light[t / 2]);
        if (rc != GMB_3D_OK) return rc;
    }
    return GMB_3D_OK;
}

int64_t gmb_scene_objpos(double x, double y, double z, double yaw_deg)
{
    if (g_scene.cur < 0)
        return g_scene_err("SCENE OBJPOS: serve un oggetto corrente (SCENE OBJECT prima)");
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw_deg))
        return g_scene_err("SCENE OBJPOS: coordinate non finite");
    g_scene.obj[g_scene.cur].x = (float)x;
    g_scene.obj[g_scene.cur].y = (float)y;
    g_scene.obj[g_scene.cur].z = (float)z;
    g_scene.obj[g_scene.cur].yaw = (float)yaw_deg;
    return GMB_3D_OK;
}

int64_t gmb_scene_camera(double x, double y, double z, double yaw_deg, double pitch_deg)
{
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw_deg) || !isfinite(pitch_deg))
        return g_scene_err("SCENE CAMERA: coordinate non finite");
    if (pitch_deg <= -89.0 || pitch_deg >= 89.0)
        return g_scene_err("SCENE CAMERA: pitch tra -89 e 89 gradi");
    gmb_3d_scene_camera_set((float)x, (float)y, (float)z, (float)yaw_deg, (float)pitch_deg);
    return GMB_3D_OK;
}
/* --- camera condivisa con la superficie interattiva ----------------------
 * UNA sola camera (P3): l'actor la legge per MOVE/TURN/LIFT e le letture
 * SCENE.X/Y/Z/EYE/YAW/PITCH; `set` riallinea i piedi del corpo all'occhio. */
void gmb_3d_scene_camera_get(float eye[3], float* yaw, float* pitch)
{
    eye[0] = g_scene_eye[0]; eye[1] = g_scene_eye[1]; eye[2] = g_scene_eye[2];
    *yaw = g_scene_yaw; *pitch = g_scene_pitch;
}
void gmb_3d_scene_camera_set(float x, float y, float z, float yaw, float pitch)
{
    g_scene_eye[0] = x; g_scene_eye[1] = y; g_scene_eye[2] = z;
    g_scene_yaw = yaw; g_scene_pitch = pitch;
    gmb_3d_actor_sync_feet((double)y);
}

/* AABB MONDO degli oggetti con geometria, per la superficie interattiva
 * (gmb_3d_scene_actor.c). Il modello è traslazione + rotazione su y: si
 * ruotano gli otto angoli dell'AABB locale e si ricava l'AABB che li contiene. */
/* Gli AABB mondo sono in gmb_3d_scene_buf.c (`gmb_3d_scene_bounds_all`): lo
 * stesso buffer persistente serve al corpo e a FLOORAT/CEILAT, senza copie. */

/* --- disegno -------------------------------------------------------------- */

/* Disegna UN oggetto con il suo modello, la sua luce base e i suoi flag
 * (M-FLAGS). Estratto da gmb_scene_render perché i pass sono due — opachi e
 * traslucidi — e devono usare la stessa strada: una sola implementazione. */
static int scene_draw_object(int oi, const float* vp, const float planes[6][4],
                             const gmb_3d_env_t* env)
{
    const gmb_scene_obj_t* o = &g_scene.obj[oi];
    if (o->ntri <= 0 || o->nvert <= 0) return GMB_3D_OK;

    float model[16], mvp[16];
    gmb_3d_m4_model(model, o->x, o->y, o->z, o->yaw);

    /* P5 — culling per oggetto (M-CULL): interamente fuori dal frustum non entra nel
     * rasterizzatore. I suoi triangoli finiscono in CULLED esattamente come li avrebbe
     * contati il clip per triangolo, quindi DRAWN, PIXELS e l'hash del frame non
     * cambiano: si risparmia solo il lavoro di preparazione. */
    if (gmb_3d_object_outside(planes, model, o->bmin, o->bmax))
    {
        g_scene_stats.tris_culled += (unsigned long)o->ntri;
        return GMB_3D_OK;
    }

    gmb_3d_m4_mul(mvp, vp, model);

    /* Luce per faccia: una luce per triangolo, dalle posizioni MONDO. Un piano
     * orizzontale illuminato dall'alto dà tutti i triangoli uguali (un run solo),
     * un cubo dà 6 run: il batching sotto resta quello di prima. */
    if (g_scene_lit)
    {
        for (int t = 0; t < o->ntri; t++)
        {
            const float* pa = g_scene.pos + (size_t)(o->first_vert + g_scene.idx[(size_t)(o->first_tri + t) * 3 + 0]) * 3;
            const float* pb = g_scene.pos + (size_t)(o->first_vert + g_scene.idx[(size_t)(o->first_tri + t) * 3 + 1]) * 3;
            const float* pc = g_scene.pos + (size_t)(o->first_vert + g_scene.idx[(size_t)(o->first_tri + t) * 3 + 2]) * 3;
            double ax = (double)model[0] * pa[0] + (double)model[4] * pa[1] + (double)model[8]  * pa[2] + (double)model[12];
            double ay = (double)model[1] * pa[0] + (double)model[5] * pa[1] + (double)model[9]  * pa[2] + (double)model[13];
            double az = (double)model[2] * pa[0] + (double)model[6] * pa[1] + (double)model[10] * pa[2] + (double)model[14];
            double bx = (double)model[0] * pb[0] + (double)model[4] * pb[1] + (double)model[8]  * pb[2] + (double)model[12];
            double by = (double)model[1] * pb[0] + (double)model[5] * pb[1] + (double)model[9]  * pb[2] + (double)model[13];
            double bz = (double)model[2] * pb[0] + (double)model[6] * pb[1] + (double)model[10] * pb[2] + (double)model[14];
            double cx = (double)model[0] * pc[0] + (double)model[4] * pc[1] + (double)model[8]  * pc[2] + (double)model[12];
            double cy = (double)model[1] * pc[0] + (double)model[5] * pc[1] + (double)model[9]  * pc[2] + (double)model[13];
            double cz = (double)model[2] * pc[0] + (double)model[6] * pc[1] + (double)model[10] * pc[2] + (double)model[14];
            double e1x = bx - ax, e1y = by - ay, e1z = bz - az;
            double e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
            double nx = e1y * e2z - e1z * e2y;
            double ny = e1z * e2x - e1x * e2z;
            double nz = e1x * e2y - e1y * e2x;
            double nl = sqrt(nx * nx + ny * ny + nz * nz);
            int q = 0;                       /* coseno quantizzato 0..255 */
            if (nl > 1e-12)
            {
                double dot = (nx * g_scene_light_dir[0] + ny * g_scene_light_dir[1]
                              + nz * g_scene_light_dir[2]) / nl;
                if (dot > 0.0)
                {
                    q = (int)(dot * 255.0 + 0.5);
                    if (q > 255) q = 255;
                }
            }
            int l = GMB_SCENE_AMBIENT + ((g_scene_light_level - GMB_SCENE_AMBIENT) * q) / 255;
            if (l < 0) l = 0; else if (l > 255) l = 255;
            g_scene.lit_tri[t] = (unsigned char)l;
        }
    }

    gmb_3d_mesh_t m;
    m.pos = g_scene.pos + (size_t)o->first_vert * 3;
    m.uv = g_scene.uv + (size_t)o->first_vert * 2;
    m.nv = o->nvert;
    /* Normali: dichiarate nello spazio dell'oggetto, quindi girano con lui. Il
     * modello e' traslazione + rotazione su y, quindi basta la sua 3x3 (niente
     * traslazione, niente scala); la normalizzazione la fa il nucleo della luce. */
    if (o->has_nrm)
    {
        const float* src = g_scene.nrm + (size_t)o->first_vert * 3;
        float* dst = g_scene.nrm_w + (size_t)o->first_vert * 3;
        for (int i = 0; i < o->nvert; i++)
        {
            float x = src[i * 3 + 0], y = src[i * 3 + 1], z = src[i * 3 + 2];
            dst[i * 3 + 0] = model[0] * x + model[4] * y + model[8]  * z;
            dst[i * 3 + 1] = model[1] * x + model[5] * y + model[9]  * z;
            dst[i * 3 + 2] = model[2] * x + model[6] * y + model[10] * z;
        }
        m.nrm = dst;
    }
    else
        m.nrm = NULL;

    gmb_3d_material_t mat;
    memset(&mat, 0, sizeof(mat));
    if (g_scene_tex != NULL)
    {
        mat.rgba = g_scene_tex;
        mat.tw = g_scene_tex_w;
        mat.th = g_scene_tex_h;
    }
    /* Flag del materiale: il kernel li consuma, qui si passano soltanto. */
    mat.bilinear = g_scene_smooth;
    mat.masked = g_scene_alpha || ((o->flags & 1) != 0);
    mat.two_sided = g_scene_two_sided || ((o->flags & 16) != 0);
    mat.translucent = (o->flags & 2) != 0;
    mat.aa = g_scene_aa;   /* SSAA 2x per draw: spento = nessun target doppio, byte di P4 */
    mat.id = (unsigned)(oi + 1);   /* SCENE PICK: 1..256, ordine di dichiarazione */
    mat.light = o->light;
    mat.tint_r = o->r;
    mat.tint_g = o->g;
    mat.tint_b = o->b;

    /* Un draw per RUN di triangoli con la stessa luce: le 6 facce del cubo hanno
     * luci diverse, il resto è un run solo. Con LIGHT attiva la luce è quella
     * calcolata per faccia (g_scene.lit_tri); senza, vale la luce dell'oggetto
     * (default 255, così le scene P3 che non usano OBJECT a 5 argomenti non
     * cambiano byte). */
    const unsigned char* lights = g_scene_lit ? g_scene.lit_tri : NULL;
    int start = 0;
    while (start < o->ntri)
    {
        unsigned char light = lights ? lights[start] : o->light;
        int n = 1;
        while (start + n < o->ntri && (lights ? lights[start + n] : o->light) == light) n++;

        mat.light = light;
        m.idx = g_scene.idx + (size_t)(o->first_tri + start) * 3;
        m.nt = n;

        gmb_3d_draw_stats_t s;
        int rc = gmb_3d_mesh_draw_env(&g_scene_target, mvp, &m, &mat, env, &s);
        if (rc != GMB_3D_OK) return rc;
        g_scene_stats.tris_drawn += s.tris_drawn;
        g_scene_stats.tris_culled += s.tris_culled;
        g_scene_stats.pixels += s.pixels;
        start += n;
    }
    return GMB_3D_OK;
}

int64_t gmb_scene_render(void)
{
    if (g_scene_target.argb == NULL)
        return g_scene_err("SCENE RENDER: nessun target (SCENE VIEW prima)");
    /* Una scena può essere fatta di solo cielo o di soli thing: il render non
     * deve pretendere una mesh (M-ORDER/M-SKY/M-THINGS). */
    if (g_scene.nobj <= 0 && g_scene.nt <= 0 && g_scene_sky == NULL)
    {
        int have_thing = 0;
        for (int i = 0; i < GMB_SCENE_MAX_THINGS; i++)
            if (g_scene_thing[i].used) { have_thing = 1; break; }
        if (!have_thing) return g_scene_err("SCENE RENDER: scena vuota (nessuna geometria, cielo o thing)");
    }

    int rc = gmb_3d_target_clear(&g_scene_target, GMB_SCENE_BG, 0.0f);
    if (rc != GMB_3D_OK) return rc;
    if (gmb_scene_pick_active())
    {
        rc = gmb_scene_pick_attach(&g_scene_target);
        if (rc != GMB_3D_OK) return rc;
    }

    float yr = g_scene_yaw * (float)(GMB_SCENE_PI / 180.0);
    float pr = g_scene_pitch * (float)(GMB_SCENE_PI / 180.0);
    float cp = cosf(pr);
    float fwd[3] = { sinf(yr) * cp, sinf(pr), cosf(yr) * cp };
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    float at[3] = { g_scene_eye[0] + fwd[0], g_scene_eye[1] + fwd[1], g_scene_eye[2] + fwd[2] };
    float view[16], proj[16], vp[16];
    gmb_3d_m4_look_at(view, g_scene_eye, at, up);
    gmb_3d_m4_perspective(proj, (float)(g_scene_fov * (GMB_SCENE_PI / 180.0)),
                          (float)g_scene_target.w / (float)g_scene_target.h, 0.1f, 300.0f);
    gmb_3d_m4_mul(vp, proj, view);

    /* P5: i sei piani una volta per frame, condivisi da tutti gli oggetti. */
    float planes[6][4];
    gmb_3d_frustum_planes(vp, planes);

    memset(&g_scene_stats, 0, sizeof(g_scene_stats));
    g_scene_env.colormap = g_scene_colormap;

    /* M-ORDER: cielo -> opachi/masked -> thing da lontano a vicino ->
     * traslucidi da lontano a vicino. */
    if (g_scene_sky != NULL)
    {
        rc = gmb_3d_sky_fill(&g_scene_target, g_scene_sky, g_scene_sky_w, g_scene_sky_h,
                             g_scene_yaw, g_scene_pitch, (float)g_scene_fov,
                             g_scene_sky_repeat, g_scene_sky_shift);
        if (rc != GMB_3D_OK) return rc;
    }

    for (int oi = 0; oi < g_scene.nobj; oi++)
    {
        if ((g_scene.obj[oi].flags & 2) != 0) continue;   /* traslucido: dopo i thing */
        rc = scene_draw_object(oi, vp, planes, &g_scene_env);
        if (rc != GMB_3D_OK) return rc;
    }

    /* Thing: da lontano a vicino, con depth test (M-THINGS). Usano la texture
     * corrente; senza texture non c'è picture e si saltano. */
    if (g_scene_tex != NULL && g_scene_tex_w > 0 && g_scene_tex_h > 0)
    {
        int order[GMB_SCENE_MAX_THINGS];
        float dist[GMB_SCENE_MAX_THINGS];
        int cnt = 0;
        for (int i = 0; i < GMB_SCENE_MAX_THINGS; i++)
        {
            if (!g_scene_thing[i].used) continue;
            float dx = g_scene_thing[i].x - g_scene_eye[0];
            float dy = g_scene_thing[i].y - g_scene_eye[1];
            float dz = g_scene_thing[i].z - g_scene_eye[2];
            dist[cnt] = dx * dx + dy * dy + dz * dz;
            order[cnt] = i;
            cnt++;
        }
        for (int i = 1; i < cnt; i++)                     /* decrescente */
        {
            int oi = order[i];
            float di = dist[i];
            int j = i - 1;
            while (j >= 0 && dist[j] < di) { dist[j + 1] = dist[j]; order[j + 1] = order[j]; j--; }
            dist[j + 1] = di;
            order[j + 1] = oi;
        }
        for (int k = 0; k < cnt; k++)
        {
            const gmb_scene_thing_t* th = &g_scene_thing[order[k]];
            gmb_3d_thing_t gt;
            memset(&gt, 0, sizeof(gt));
            gt.x = th->x; gt.y = th->y; gt.z = th->z;
            gt.w = th->w; gt.h = th->h;
            gt.rgba = g_scene_tex;
            gt.tw = g_scene_tex_w;
            gt.th = g_scene_tex_h;
            gt.light = th->light;
            gmb_3d_draw_stats_t s;
            rc = gmb_3d_thing_draw(&g_scene_target, view, proj, &gt, g_scene_eye, &g_scene_env, &s);
            if (rc != GMB_3D_OK) return rc;
            g_scene_stats.tris_drawn += s.tris_drawn;
            g_scene_stats.tris_culled += s.tris_culled;
            g_scene_stats.pixels += s.pixels;
        }
    }

    /* Traslucidi: da lontano a vicino, con fusione (M-PALALPHA). Ordine e
     * distanza usano gli scratch PERSISTENTI (capacità = oggetti riservati):
     * il frame non alloca nemmeno con migliaia di oggetti. */
    {
        int* order = g_scene.order;
        float* dist = g_scene.dist;
        int cnt = 0;
        for (int oi = 0; oi < g_scene.nobj; oi++)
        {
            if ((g_scene.obj[oi].flags & 2) == 0) continue;
            float dx = g_scene.obj[oi].x - g_scene_eye[0];
            float dy = g_scene.obj[oi].y - g_scene_eye[1];
            float dz = g_scene.obj[oi].z - g_scene_eye[2];
            dist[cnt] = dx * dx + dy * dy + dz * dz;
            order[cnt] = oi;
            cnt++;
        }
        for (int i = 1; i < cnt; i++)
        {
            int oi = order[i];
            float di = dist[i];
            int j = i - 1;
            while (j >= 0 && dist[j] < di) { dist[j + 1] = dist[j]; order[j + 1] = order[j]; j--; }
            dist[j + 1] = di;
            order[j + 1] = oi;
        }
        for (int k = 0; k < cnt; k++)
        {
            rc = scene_draw_object(order[k], vp, planes, &g_scene_env);
            if (rc != GMB_3D_OK) return rc;
        }
    }

    /* FNV-1a del frame: due render identici devono dare lo stesso numero. */
    uint64_t h = 1469598103934665603ull;
    const size_t np = (size_t)g_scene_target.w * (size_t)g_scene_target.h;
    for (size_t i = 0; i < np; i++)
    {
        h ^= g_scene_target.argb[i];
        h *= 1099511628211ull;
    }
    g_scene_hash = h;
    gmb_3d_stats_note_render();   /* RENDER nel frame aperto (M-STATS) */
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_save(const char* path)
{
    if (g_scene_target.argb == NULL)
        return g_scene_err("SCENE SAVE: nessun frame (manca VIEW o RENDER)");
    if (path == NULL || *path == '\0')
        return g_scene_err("SCENE SAVE: manca il percorso del PNG");
    int rc = gmb_3d_target_capture_png(&g_scene_target, path);
    return (int64_t)rc;
}

/* --- letture -------------------------------------------------------------- */

int64_t gmb_scene_verts(void)  { return (int64_t)g_scene.nv; }
int64_t gmb_scene_tris(void)   { return (int64_t)g_scene.nt; }
int64_t gmb_scene_drawn(void)  { return (int64_t)g_scene_stats.tris_drawn; }
int64_t gmb_scene_culled(void) { return (int64_t)g_scene_stats.tris_culled; }
int64_t gmb_scene_pixels(void) { return (int64_t)g_scene_stats.pixels; }
int64_t gmb_scene_hash(void)   { return (int64_t)(g_scene_hash & 0xFFFFFFFFull); }

/* SCENE PRESENT: copia il TARGET 3D nella pagina 2D corrente (il canvas attivo, se
 * c'è; altrimenti la pagina dello schermo). È il ponte che rende il 3D usabile
 * interattivamente: RENDER disegna nel target, PRESENT lo porta nella pagina, e il
 * pump SDL sul MAIN thread mostra la pagina nella finestra. Qui NON si tocca SDL:
 * SDL/Cocoa si chiamano solo dal thread main (gmb_gfx_present.h), mentre questo
 * comando gira dentro un task GPM sul worker.
 *
 * GPM (regola inderogabile): con lo scheduler attivo, fuori da un task è un errore
 * esplicito (E_NO_TASK), come gmb_3d_render_begin. Senza scheduler (tool, oracolo,
 * selfcheck — T3) la stessa API è usabile sincronamente.
 *
 * Copia 1:1 dall'angolo in alto a sinistra, ritagliata alle due dimensioni: nessuna
 * scalatura implicita (stessa regola di gmb_3d_target_blit_argb). Il target contiene
 * 0x00RRGGBB, lo stesso formato delle pagine: nessuna conversione. */
int64_t gmb_scene_present_check(void)
{
    if (gmb_c99gpm_active() && gmb_cur() == NULL)
    {
        gmb_3d_set_error("SCENE PRESENT: fuori dal task GPM");
        return GMB_3D_E_NO_TASK;
    }
    if (g_scene_target.argb == NULL)
    {
        gmb_3d_set_error("SCENE PRESENT: nessun frame (manca VIEW o RENDER)");
        return GMB_3D_E_ARG;
    }
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

const uint32_t* gmb_scene_frame(int* w, int* h)
{
    if (g_scene_target.argb == NULL) return NULL;
    if (w) *w = g_scene_target.w;
    if (h) *h = g_scene_target.h;
    return g_scene_target.argb;
}

int64_t gmb_scene_present(void)
{
    int64_t rc = gmb_scene_present_check();
    if (rc != GMB_3D_OK) return rc;
    /* La copia nella pagina vive UNA volta sola, nel modulo grafico che possiede le
     * pagine (`gmb_gfx_present_argb`). Qui si sceglie solo il frame: il ritaglio, il
     * formato e la pagina attiva sono affare di quella funzione. */
    int w = 0, h = 0;
    const uint32_t* px = gmb_scene_frame(&w, &h);
    if (px == NULL) return GMB_3D_E_ARG;
    if (gmb_gfx_present_argb(px, w, h) < 0)
    {
        gmb_3d_set_error("SCENE PRESENT: nessuna pagina 2D attiva (SCREEN o CANVAS USE prima)");
        return GMB_3D_E_ARG;
    }
    gmb_3d_stats_note_present();   /* il frame si chiude qui (M-STATS) */
    return GMB_3D_OK;
}

/* Chiamata CONTROLLATA delle superfici SCENE (una per tutte le gambe): se la
 * facciata fallisce, il messaggio canonico diventa un errore BASIC catturabile
 * (ON ERROR / TRY) invece di un no-op silenzioso. `line` e' la riga BASIC che
 * C99Native passa al raise; le altre gambe usano i propri raise con lo stesso
 * testo. */
void gmb_scene_clear_error(void)
{
    gmb_3d_set_error("");
}

int64_t gmb_scene_checked(int64_t rc, int64_t line)
{
    if (rc != GMB_3D_OK)
    {
        const char* msg = gmb_3d_last_error();
        gmb_raise_error(5, (msg != NULL && *msg != '\0') ? msg : "SCENE: comando fallito",
                        (long)line);
    }
    return rc;
}

gmb_3d_target_t* gmb_scene_target_ref(void) { return &g_scene_target; }

const gmb_3d_draw_stats_t* gmb_3d_scene_draw_stats(void) { return &g_scene_stats; }

int64_t gmb_scene_pixel(int64_t x, int64_t y)
{
    if (g_scene_target.argb == NULL || x < 0 || y < 0
        || x >= g_scene_target.w || y >= g_scene_target.h) return -1;
    return (int64_t)(g_scene_target.argb[(size_t)y * (size_t)g_scene_target.w + (size_t)x] & 0xFFFFFFu);
}
