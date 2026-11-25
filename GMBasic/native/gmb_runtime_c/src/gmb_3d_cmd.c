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

#define GMB_SCENE_MAX_VERTS 32768
#define GMB_SCENE_MAX_TRIS  16384
#define GMB_SCENE_MAX_OBJS  256
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

typedef struct gmb_scene_obj {
    int first_vert, nvert, first_tri, ntri;
    float x, y, z, yaw;
    unsigned char r, g, b;
} gmb_scene_obj_t;

static gmb_3d_target_t g_scene_target;   /* argb != NULL = target allocato */
static double  g_scene_fov = 60.0;
static float   g_scene_eye[3] = { 0.0f, 1.5f, 6.0f };
static float   g_scene_yaw = 0.0f, g_scene_pitch = 0.0f;
static float   g_scene_pos[GMB_SCENE_MAX_VERTS * 3];
static float   g_scene_uv[GMB_SCENE_MAX_VERTS * 2];
static int     g_scene_idx[GMB_SCENE_MAX_TRIS * 3];
static unsigned char g_scene_light[GMB_SCENE_MAX_TRIS];

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
static unsigned char g_scene_lit_tri[GMB_SCENE_MAX_TRIS];

/* Flag di MATERIALE (idea #5): il kernel li ha già in `gmb_3d_material_t` (bilinear,
 * masked, two_sided) ma il BASIC non li raggiungeva. `SCENE SMOOTH|ALPHA|TWOSIDED on|off`
 * li espone senza toccare il rasterizzatore. I default sono quelli STORICI — bilineare
 * attivo, alpha-test attivo, back-face cull attivo — quindi ogni scena che non chiama i
 * nuovi verbi dà gli stessi byte di prima (hash dei MiniTest invariati). Sono stato del
 * materiale: sopravvivono a RESET, come TEXTURE e LIGHT. */
static int g_scene_smooth = 1;        /* 1 = bilineare, 0 = nearest */
static int g_scene_alpha = 1;         /* 1 = alpha 0 scartato (alpha-test) */
static int g_scene_two_sided = 0;     /* 1 = niente back-face cull */
static gmb_scene_obj_t g_scene_obj[GMB_SCENE_MAX_OBJS];
static int     g_scene_nv = 0, g_scene_nt = 0, g_scene_nobj = 0, g_scene_cur = -1;
static unsigned char* g_scene_tex = NULL;
static int     g_scene_tex_w = 0, g_scene_tex_h = 0;
static gmb_3d_draw_stats_t g_scene_stats;
static uint64_t g_scene_hash = 0;

/* --- matrici (column-major; clip z in [-1,1], come il kernel) ------------ */

static void g_scene_m4_mul(float* out, const float* a, const float* b)   /* out = a*b */
{
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
        {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    memcpy(out, r, sizeof(r));
}

static void g_scene_m4_perspective(float* m, float fovy, float aspect, float zn, float zf)
{
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovy * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = (2.0f * zf * zn) / (zn - zf);
}

static void g_scene_m4_look_at(float* m, const float eye[3], const float at[3], const float up[3])
{
    float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fl <= 0.0f) fl = 1.0f;
    f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float r[3] = { f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0] };
    float rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (rl <= 0.0f) rl = 1.0f;
    r[0] /= rl; r[1] /= rl; r[2] /= rl;
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    memset(m, 0, 16 * sizeof(float));
    m[0] = r[0]; m[4] = r[1]; m[8]  = r[2]; m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[1] = u[0]; m[5] = u[1]; m[9]  = u[2]; m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[2] = -f[0]; m[6] = -f[1]; m[10] = -f[2]; m[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    m[15] = 1.0f;
}

/* Modello di un oggetto: traslazione * rotazione attorno a Y. */
static void g_scene_m4_model(float* m, float x, float y, float z, float yaw_deg)
{
    float a = yaw_deg * (float)(GMB_SCENE_PI / 180.0);
    float c = cosf(a), s = sinf(a);
    memset(m, 0, 16 * sizeof(float));
    m[0] = c; m[2] = -s;
    m[5] = 1.0f;
    m[8] = s; m[10] = c;
    m[12] = x; m[13] = y; m[14] = z; m[15] = 1.0f;
}

/* --- accumulo della geometria -------------------------------------------- */

static int g_scene_push_vert(float x, float y, float z, float u, float v)
{
    if (g_scene_cur < 0 || g_scene_nv >= GMB_SCENE_MAX_VERTS) return GMB_3D_E_ARG;
    g_scene_pos[g_scene_nv * 3 + 0] = x;
    g_scene_pos[g_scene_nv * 3 + 1] = y;
    g_scene_pos[g_scene_nv * 3 + 2] = z;
    g_scene_uv[g_scene_nv * 2 + 0] = u;
    g_scene_uv[g_scene_nv * 2 + 1] = v;
    g_scene_nv++;
    g_scene_obj[g_scene_cur].nvert++;
    return GMB_3D_OK;
}

static int g_scene_push_tri(int a, int b, int c, unsigned char light)
{
    if (g_scene_cur < 0 || g_scene_nt >= GMB_SCENE_MAX_TRIS) return GMB_3D_E_ARG;
    if (a < 0 || b < 0 || c < 0) return GMB_3D_E_ARG;
    if (a >= g_scene_obj[g_scene_cur].nvert || b >= g_scene_obj[g_scene_cur].nvert
        || c >= g_scene_obj[g_scene_cur].nvert)
        return GMB_3D_E_ARG;
    g_scene_idx[g_scene_nt * 3 + 0] = a;
    g_scene_idx[g_scene_nt * 3 + 1] = b;
    g_scene_idx[g_scene_nt * 3 + 2] = c;
    g_scene_light[g_scene_nt] = light;
    g_scene_nt++;
    g_scene_obj[g_scene_cur].ntri++;
    return GMB_3D_OK;
}

static int g_scene_clampi(int64_t v)
{
    return (v < 0) ? 0 : (v > 255 ? 255 : (int)v);
}

/* --- comandi -------------------------------------------------------------- */

int64_t gmb_scene_reset(void)
{
    g_scene_nv = g_scene_nt = g_scene_nobj = 0;
    g_scene_cur = -1;
    memset(&g_scene_stats, 0, sizeof(g_scene_stats));
    g_scene_hash = 0;
    g_scene_eye[0] = 0.0f; g_scene_eye[1] = 1.5f; g_scene_eye[2] = 6.0f;
    g_scene_yaw = 0.0f;
    g_scene_pitch = 0.0f;
    g_scene_fov = 60.0;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_view(int64_t w, int64_t h, double fov_deg)
{
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return GMB_3D_E_ARG;
    if (fov_deg <= 1.0 || fov_deg >= 179.0) return GMB_3D_E_ARG;
    int rc = gmb_3d_target_alloc(&g_scene_target, (int)w, (int)h);
    if (rc != GMB_3D_OK) return rc;
    g_scene_fov = fov_deg;
    return gmb_3d_target_clear(&g_scene_target, GMB_SCENE_BG, 0.0f);
}

int64_t gmb_scene_checker(int64_t cells)
{
    if (cells < 1 || cells > 64) return GMB_3D_E_ARG;
    const int cell = 8;
    int size = (int)cells * cell;
    unsigned char* buf = (unsigned char*)malloc((size_t)size * (size_t)size * 4u);
    if (buf == NULL) return GMB_3D_E_NOMEM;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            unsigned char v = (((x / cell) + (y / cell)) & 1) ? 255u : 96u;
            unsigned char* p = buf + ((size_t)y * (size_t)size + (size_t)x) * 4u;
            p[0] = v; p[1] = v; p[2] = v; p[3] = 255u;
        }
    free(g_scene_tex);
    g_scene_tex = buf;
    g_scene_tex_w = size;
    g_scene_tex_h = size;
    return GMB_3D_OK;
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
    {
        gmb_3d_set_error("SCENE TEXTURE: manca il percorso del PNG");
        return GMB_3D_E_ARG;
    }

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
    double len = sqrt(dx * dx + dy * dy + dz * dz);
    if (!(len > 1e-9)) return GMB_3D_E_ARG;
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

int64_t gmb_scene_object(int64_t r, int64_t g, int64_t b)
{
    if (g_scene_nobj >= GMB_SCENE_MAX_OBJS) return GMB_3D_E_ARG;
    gmb_scene_obj_t* o = &g_scene_obj[g_scene_nobj];
    o->first_vert = g_scene_nv;
    o->nvert = 0;
    o->first_tri = g_scene_nt;
    o->ntri = 0;
    o->x = o->y = o->z = 0.0f;
    o->yaw = 0.0f;
    o->r = (unsigned char)g_scene_clampi(r);
    o->g = (unsigned char)g_scene_clampi(g);
    o->b = (unsigned char)g_scene_clampi(b);
    g_scene_cur = g_scene_nobj++;
    return GMB_3D_OK;
}

int64_t gmb_scene_vertex(double x, double y, double z, double u, double v)
{
    return g_scene_push_vert((float)x, (float)y, (float)z, (float)u, (float)v);
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
    if (g_scene_cur < 0) return GMB_3D_E_ARG;
    if (g_scene_nv + 24 > GMB_SCENE_MAX_VERTS || g_scene_nt + 12 > GMB_SCENE_MAX_TRIS)
        return GMB_3D_E_ARG;
    float cx = (float)((x0 + x1) * 0.5), hx = (float)((x1 - x0) * 0.5);
    float cy = (float)((y0 + y1) * 0.5), hy = (float)((y1 - y0) * 0.5);
    float cz = (float)((z0 + z1) * 0.5), hz = (float)((z1 - z0) * 0.5);
    int base = g_scene_obj[g_scene_cur].nvert;
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
    if (g_scene_cur < 0) return GMB_3D_E_ARG;
    g_scene_obj[g_scene_cur].x = (float)x;
    g_scene_obj[g_scene_cur].y = (float)y;
    g_scene_obj[g_scene_cur].z = (float)z;
    g_scene_obj[g_scene_cur].yaw = (float)yaw_deg;
    return GMB_3D_OK;
}

int64_t gmb_scene_camera(double x, double y, double z, double yaw_deg, double pitch_deg)
{
    if (pitch_deg <= -89.0 || pitch_deg >= 89.0) return GMB_3D_E_ARG;
    g_scene_eye[0] = (float)x;
    g_scene_eye[1] = (float)y;
    g_scene_eye[2] = (float)z;
    g_scene_yaw = (float)yaw_deg;
    g_scene_pitch = (float)pitch_deg;
    return GMB_3D_OK;
}

/* --- disegno -------------------------------------------------------------- */

int64_t gmb_scene_render(void)
{
    if (g_scene_target.argb == NULL) return GMB_3D_E_ARG;
    if (g_scene_nobj <= 0 || g_scene_nt <= 0) return GMB_3D_E_ARG;

    int rc = gmb_3d_target_clear(&g_scene_target, GMB_SCENE_BG, 0.0f);
    if (rc != GMB_3D_OK) return rc;

    float yr = g_scene_yaw * (float)(GMB_SCENE_PI / 180.0);
    float pr = g_scene_pitch * (float)(GMB_SCENE_PI / 180.0);
    float cp = cosf(pr);
    float fwd[3] = { sinf(yr) * cp, sinf(pr), cosf(yr) * cp };
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    float at[3] = { g_scene_eye[0] + fwd[0], g_scene_eye[1] + fwd[1], g_scene_eye[2] + fwd[2] };
    float view[16], proj[16], vp[16];
    g_scene_m4_look_at(view, g_scene_eye, at, up);
    g_scene_m4_perspective(proj, (float)(g_scene_fov * (GMB_SCENE_PI / 180.0)),
                           (float)g_scene_target.w / (float)g_scene_target.h, 0.1f, 300.0f);
    g_scene_m4_mul(vp, proj, view);

    memset(&g_scene_stats, 0, sizeof(g_scene_stats));

    for (int oi = 0; oi < g_scene_nobj; oi++)
    {
        const gmb_scene_obj_t* o = &g_scene_obj[oi];
        if (o->ntri <= 0 || o->nvert <= 0) continue;

        float model[16], mvp[16];
        g_scene_m4_model(model, o->x, o->y, o->z, o->yaw);
        g_scene_m4_mul(mvp, vp, model);

        /* Luce per faccia: una luce per triangolo, dalle posizioni MONDO. Un piano
         * orizzontale illuminato dall'alto dà tutti i triangoli uguali (un run solo),
         * un cubo dà 6 run: il batching sotto resta quello di prima. */
        if (g_scene_lit)
        {
            for (int t = 0; t < o->ntri; t++)
            {
                const float* pa = g_scene_pos + (size_t)(o->first_vert + g_scene_idx[(size_t)(o->first_tri + t) * 3 + 0]) * 3;
                const float* pb = g_scene_pos + (size_t)(o->first_vert + g_scene_idx[(size_t)(o->first_tri + t) * 3 + 1]) * 3;
                const float* pc = g_scene_pos + (size_t)(o->first_vert + g_scene_idx[(size_t)(o->first_tri + t) * 3 + 2]) * 3;
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
                g_scene_lit_tri[t] = (unsigned char)l;
            }
        }

        gmb_3d_mesh_t m;
        m.pos = g_scene_pos + (size_t)o->first_vert * 3;
        m.uv = g_scene_uv + (size_t)o->first_vert * 2;
        m.nv = o->nvert;

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
        mat.masked = g_scene_alpha;
        mat.two_sided = g_scene_two_sided;
        mat.light = 255;
        mat.tint_r = o->r;
        mat.tint_g = o->g;
        mat.tint_b = o->b;

        /* Un draw per RUN di triangoli con la stessa luce: le 6 facce del cubo hanno
         * luci diverse, il resto è un run solo. Con LIGHT attiva la luce è quella
         * calcolata per faccia (g_scene_lit_tri), altrimenti quella dichiarata. */
        const unsigned char* lights = g_scene_lit ? g_scene_lit_tri : (g_scene_light + o->first_tri);
        int start = 0;
        while (start < o->ntri)
        {
            unsigned char light = lights[start];
            int n = 1;
            while (start + n < o->ntri && lights[start + n] == light) n++;

            mat.light = light;
            m.idx = g_scene_idx + (size_t)(o->first_tri + start) * 3;
            m.nt = n;

            gmb_3d_draw_stats_t s;
            rc = gmb_3d_mesh_draw(&g_scene_target, mvp, &m, &mat, &s);
            if (rc != GMB_3D_OK) return rc;
            g_scene_stats.tris_drawn += s.tris_drawn;
            g_scene_stats.tris_culled += s.tris_culled;
            g_scene_stats.pixels += s.pixels;
            start += n;
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
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int64_t gmb_scene_save(const char* path)
{
    if (g_scene_target.argb == NULL || path == NULL || *path == '\0') return GMB_3D_E_ARG;
    int rc = gmb_3d_target_capture_png(&g_scene_target, path);
    return (int64_t)rc;
}

/* --- letture -------------------------------------------------------------- */

int64_t gmb_scene_verts(void)  { return (int64_t)g_scene_nv; }
int64_t gmb_scene_tris(void)   { return (int64_t)g_scene_nt; }
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
    return GMB_3D_OK;
}

int64_t gmb_scene_pixel(int64_t x, int64_t y)
{
    if (g_scene_target.argb == NULL || x < 0 || y < 0
        || x >= g_scene_target.w || y >= g_scene_target.h) return -1;
    return (int64_t)(g_scene_target.argb[(size_t)y * (size_t)g_scene_target.w + (size_t)x] & 0xFFFFFFu);
}
