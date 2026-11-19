/* gmb_3d_oracle_world.c — oracolo P2: settori, portali e load che parca.
 *
 * Le scene usano gm_sector_render direttamente (la suite è la sorgente della
 * rasterizzazione): il target e la cattura sono quelli di P1, quindi le golden
 * passano per la stessa pipeline verificata.
 *
 * Le prove R11 usano `gmb_3d_world_load` (loader vero, JSON v1): dentro un task
 * il file viene letto da un thread esterno e il task parca; il conteggio dei
 * park lo dimostra senza dipendere dai tempi. Fuori da un task la lettura è
 * sincrona e il conteggio non cresce.
 */
#include "gmb_3d_oracle_world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmb_c99gpm.h"
#include "gm_gfx_sector.h"

#define W_ROOM 128.0

static gm_tex g_tex[2];
static gm_sector_wall g_walls[16];
static int g_awalls[8], g_bwalls[8];
static gm_sector g_sectors[2];
static gm_sector_world g_world;

static void mk_tex(gm_tex* t, unsigned int a, unsigned int b, int checker)
{
    gm_tex_alloc(t, 32, 32);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
        {
            unsigned int c = (((x / checker) + (y / checker)) & 1) ? b : a;
            gm_tex_set(t, x, y, (unsigned char)(c >> 16), (unsigned char)(c >> 8),
                       (unsigned char)c, 255);
        }
}

/* Le scene sono due: quella di rendering scrive in g_walls, quella di
 * collisione in c_walls. `g_wdst` evita di duplicare l'helper per trenta
 * chiamate. */
static gm_sector_wall* g_wdst = g_walls;

static void wall(int i, double x0, double y0, double x1, double y1, int to, int tex,
                 double bottom, double top)
{
    g_wdst[i].x0 = (gm_real)x0; g_wdst[i].y0 = (gm_real)y0;
    g_wdst[i].x1 = (gm_real)x1; g_wdst[i].y1 = (gm_real)y1;
    g_wdst[i].to = to;
    g_wdst[i].tex = tex;
    g_wdst[i].bottom = (gm_real)bottom; g_wdst[i].top = (gm_real)top;
}

/* Una stanza chiusa (door = 0) o due stanze con una porta nella parete x=128
 * (door = 1). Pareti in senso antiorario visto dall'alto. */
static int g_tex_ready = 0;

static void ensure_textures(void)
{
    if (g_tex_ready) return;
    mk_tex(&g_tex[0], 0x204060u, 0xE0C020u, 8);
    mk_tex(&g_tex[1], 0x1E4A28u, 0x2E6A38u, 16);
    g_tex_ready = 1;
}

static void build_world(int door)
{
    static int built_door = -1;
    if (built_door == door) return;
    ensure_textures();
    g_wdst = g_walls;
    built_door = door;

    int nw = 0;
    int* a = g_awalls;
    wall(nw, 0, 0, W_ROOM, 0, -1, 0, 0, 64);            a[0] = nw++;
    wall(nw, W_ROOM, 0, W_ROOM, 32, -1, 0, 0, 64);      a[1] = nw++;
    if (door) wall(nw, W_ROOM, 32, W_ROOM, 96, 1, 0, 0, 48);
    else      wall(nw, W_ROOM, 32, W_ROOM, 96, -1, 0, 0, 64);
    a[2] = nw++;
    wall(nw, W_ROOM, 96, W_ROOM, W_ROOM, -1, 0, 0, 64); a[3] = nw++;
    wall(nw, W_ROOM, W_ROOM, 0, W_ROOM, -1, 0, 0, 64);  a[4] = nw++;
    wall(nw, 0, W_ROOM, 0, 0, -1, 0, 0, 64);            a[5] = nw++;

    g_sectors[0].floor_z = 0; g_sectors[0].ceil_z = 64;
    g_sectors[0].tex_floor = 1; g_sectors[0].tex_ceil = 0;
    g_sectors[0].flags = 0; g_sectors[0].walls = a; g_sectors[0].nwalls = 6;

    int ns = 1;
    if (door)
    {
        int* b = g_bwalls;
        wall(nw, W_ROOM, 0, 256, 0, -1, 0, 0, 64);       b[0] = nw++;
        wall(nw, 256, 0, 256, W_ROOM, -1, 0, 0, 64);     b[1] = nw++;
        wall(nw, 256, W_ROOM, W_ROOM, W_ROOM, -1, 0, 0, 64); b[2] = nw++;
        wall(nw, W_ROOM, W_ROOM, W_ROOM, 96, -1, 0, 0, 64);  b[3] = nw++;
        wall(nw, W_ROOM, 96, W_ROOM, 32, 0, 0, 0, 48);   b[4] = nw++;   /* portale: lato B */
        wall(nw, W_ROOM, 32, W_ROOM, 0, -1, 0, 0, 64);   b[5] = nw++;

        g_sectors[1].floor_z = 0; g_sectors[1].ceil_z = 64;
        g_sectors[1].tex_floor = 1; g_sectors[1].tex_ceil = 1;
        g_sectors[1].flags = 0; g_sectors[1].walls = b; g_sectors[1].nwalls = 6;
        ns = 2;
    }

    g_world.sectors = g_sectors; g_world.nsectors = ns;
    g_world.walls = g_walls; g_world.nwalls = nw;
    g_world.tex = g_tex; g_world.ntex = 2;
    g_world.tex_sky = -1;
    g_world.fog = 0; g_world.fog_rgb = 0x101418u;
}

static int shoot_world(gmb_3d_target_t* t, int door, double x, double y, double angle, double pitch)
{
    build_world(door);
    gm_sector_camera cam;
    cam.x = (gm_real)x; cam.y = (gm_real)y; cam.z = 46;
    cam.angle = (gm_real)angle; cam.pitch = (gm_real)pitch;
    cam.fov = (gm_real)1.5707963; cam.far = 4096;
    return gm_sector_render(&g_world, &cam, t->argb, t->depth, t->w, t->h) == GM_OK ? 0 : -1;
}

int w_scene_room(gmb_3d_target_t* t)  { return shoot_world(t, 0, 64, 64, 0.0, 0.0); }
int w_scene_door(gmb_3d_target_t* t)  { return shoot_world(t, 1, 32, 64, 0.0, 0.0); }
/* Pitch NEGATIVO: porta in vista il pavimento (texture diversa dalle pareti), così
 * la scena prova lo shear e l'ancoraggio dei piani con più di due colori. */
int w_scene_pitch(gmb_3d_target_t* t) { return shoot_world(t, 0, 64, 64, 0.0, -0.35); }

/* --- documenti di prova per il load -------------------------------------- */

static char g_good[1024], g_bad[1024];
/* Mondi di collisione (W-MOVE): due stanze affacciate sul portale x=128. Le
 * sei varianti cambiano solo quota/soffitto/apertura, cioè i parametri che il
 * movimento deve rispettare. */
static char g_rooms[6][1024];

/* Mondo buono: le due stanze della scena door, portale elencato da entrambi i
 * lati (W-INV-CCW) con apertura dentro le quote di entrambi (W-INV-HEIGHT). */
static const char* GOOD_JSON =
"{ \"format\": \"gmb-3d\", \"version\": 1, \"kind\": \"sectors\",\n"
"  \"environment\": { \"fog\": 0, \"fogColor\": \"#101418\" },\n"
"  \"textures\": [ { \"w\": 32, \"h\": 32, \"a\": \"#204060\", \"b\": \"#e0c020\", \"checker\": 8 } ],\n"
"  \"sectors\": [\n"
"    { \"floor\": 0, \"ceil\": 64, \"floorTex\": 0, \"ceilTex\": 0, \"flags\": 0, \"walls\": [\n"
"      { \"x0\": 0, \"y0\": 0, \"x1\": 128, \"y1\": 0, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 128, \"y0\": 0, \"x1\": 128, \"y1\": 32, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 128, \"y0\": 32, \"x1\": 128, \"y1\": 96, \"to\": 1, \"tex\": 0, \"bottom\": 0, \"top\": 48 },\n"
"      { \"x0\": 128, \"y0\": 96, \"x1\": 128, \"y1\": 128, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 128, \"y0\": 128, \"x1\": 0, \"y1\": 128, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 0, \"y0\": 128, \"x1\": 0, \"y1\": 0, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 } ] },\n"
"    { \"floor\": 0, \"ceil\": 64, \"floorTex\": 0, \"ceilTex\": 0, \"flags\": 0, \"walls\": [\n"
"      { \"x0\": 128, \"y0\": 0, \"x1\": 256, \"y1\": 0, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 256, \"y0\": 0, \"x1\": 256, \"y1\": 128, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 256, \"y0\": 128, \"x1\": 128, \"y1\": 128, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 128, \"y0\": 128, \"x1\": 128, \"y1\": 96, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 },\n"
"      { \"x0\": 128, \"y0\": 96, \"x1\": 128, \"y1\": 32, \"to\": 0, \"tex\": 0, \"bottom\": 0, \"top\": 48 },\n"
"      { \"x0\": 128, \"y0\": 32, \"x1\": 128, \"y1\": 0, \"to\": -1, \"tex\": 0, \"bottom\": 0, \"top\": 64 } ] } ] }\n";

/* Mondo invalido: soffitto (0) <= pavimento (0) → W-INV-FLOOR, nominando
 * sectors[0]. */
static const char* BAD_JSON =
"{ \"format\": \"gmb-3d\", \"version\": 1, \"kind\": \"sectors\",\n"
"  \"sectors\": [ { \"floor\": 0, \"ceil\": 0, \"walls\": [\n"
"    { \"x0\": 0, \"y0\": 0, \"x1\": 64, \"y1\": 0 },\n"
"    { \"x0\": 64, \"y0\": 0, \"x1\": 0, \"y1\": 64 },\n"
"    { \"x0\": 0, \"y0\": 64, \"x1\": 0, \"y1\": 0 } ] } ] }\n";

static void write_text(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

/* Documento di due stanze: A 0..128 × 0..128 con pavimento 0, B 128..256 ×
 * 0..128 con pavimento `raise`; il portale sta nel muro x=128 fra py0 e py1,
 * con apertura [bottom=raise, top=ptop] (W-INV-HEIGHT: top ≤ ceil di B,
 * bottom ≥ pavimento di B). */
static void room_doc(char* out, size_t cap, double a_ceil, double raise, double bceil,
                     double py0, double py1, double ptop)
{
    snprintf(out, cap,
        "{ \"format\": \"gmb-3d\", \"version\": 1, \"kind\": \"sectors\",\n"
        "  \"sectors\": [\n"
        "    { \"floor\": 0, \"ceil\": %.1f, \"walls\": [\n"
        "      { \"x0\": 0, \"y0\": 0, \"x1\": 128, \"y1\": 0, \"to\": -1 },\n"
        "      { \"x0\": 128, \"y0\": 0, \"x1\": 128, \"y1\": %.1f, \"to\": -1 },\n"
        "      { \"x0\": 128, \"y0\": %.1f, \"x1\": 128, \"y1\": %.1f, \"to\": 1, \"bottom\": %.1f, \"top\": %.1f },\n"
        "      { \"x0\": 128, \"y0\": %.1f, \"x1\": 128, \"y1\": 128, \"to\": -1 },\n"
        "      { \"x0\": 128, \"y0\": 128, \"x1\": 0, \"y1\": 128, \"to\": -1 },\n"
        "      { \"x0\": 0, \"y0\": 128, \"x1\": 0, \"y1\": 0, \"to\": -1 } ] },\n"
        "    { \"floor\": %.1f, \"ceil\": %.1f, \"walls\": [\n"
        "      { \"x0\": 128, \"y0\": 0, \"x1\": 256, \"y1\": 0, \"to\": -1 },\n"
        "      { \"x0\": 256, \"y0\": 0, \"x1\": 256, \"y1\": 128, \"to\": -1 },\n"
        "      { \"x0\": 256, \"y0\": 128, \"x1\": 128, \"y1\": 128, \"to\": -1 },\n"
        "      { \"x0\": 128, \"y0\": 128, \"x1\": 128, \"y1\": %.1f, \"to\": -1 },\n"
        "      { \"x0\": 128, \"y0\": %.1f, \"x1\": 128, \"y1\": %.1f, \"to\": 0, \"bottom\": %.1f, \"top\": %.1f },\n"
        "      { \"x0\": 128, \"y0\": %.1f, \"x1\": 128, \"y1\": 0, \"to\": -1 } ] } ] }\n",
        a_ceil, py0, py0, py1, raise, ptop, py1,
        raise, bceil, py1, py1, py0, raise, ptop, py0);
}

void w_prepare(const char* dir)
{
    snprintf(g_good, sizeof(g_good), "%s/world_good.gmbscene", dir);
    snprintf(g_bad, sizeof(g_bad), "%s/world_bad_floor.gmbscene", dir);
    write_text(g_good, GOOD_JSON);
    write_text(g_bad, BAD_JSON);

    /* a_ceil, raise, bceil, py0, py1, ptop */
    static const struct { const char* name; double a_ceil, raise, bceil, py0, py1, ptop; }
    docs[6] = {
        { "rooms_base.gmbscene",    128,  0, 128, 32, 96, 96 },   /* porta alta */
        { "rooms_step24.gmbscene",  128, 24, 128, 32, 96, 96 },   /* gradino 24 */
        { "rooms_step40.gmbscene",  128, 40, 128, 32, 96, 96 },   /* gradino 40 */
        { "rooms_low.gmbscene",     128,  0, 128, 32, 96, 48 },   /* apertura 48 < 56 */
        { "rooms_narrow.gmbscene",  128,  0, 128, 56, 80, 96 },   /* larga 24 < 2×14 */
        { "rooms_ceil40.gmbscene",   40,  0,  40, 32, 96, 40 }    /* W-EYE-CLAMP */
    };
    char text[2048];
    for (int i = 0; i < 6; i++)
    {
        snprintf(g_rooms[i], sizeof(g_rooms[i]), "%s/%s", dir, docs[i].name);
        room_doc(text, sizeof(text), docs[i].a_ceil, docs[i].raise, docs[i].bceil,
                 docs[i].py0, docs[i].py1, docs[i].ptop);
        write_text(g_rooms[i], text);
    }
}

#define WCHECK(cond, what) do { \
    if (cond) { (*ok)++; } else { (*fail)++; printf("FAIL %s (world)\n", (what)); } } while (0)

/* Dentro un task GPM (chiamata da sched_main, dopo la prova R3). */
void w_world_checks(int* ok, int* fail)
{
    gmb_3d_target_t t;
    memset(&t, 0, sizeof(t));
    WCHECK(gmb_3d_target_alloc(&t, 160, 120) == GMB_3D_OK, "world: target del test");

    unsigned long p0 = gmb_3d_load_park_count();
    gmb_3d_world_t* w = NULL;
    int rc = gmb_3d_world_load(g_good, &w);
    WCHECK(rc == GMB_3D_OK && w != NULL, "R11: load del mondo dentro un task");
    WCHECK(gmb_3d_load_park_count() == p0 + 1, "R11: il load ha parcheggiato il task (non sincrono)");

    gmb_3d_camera_t cam;
    cam.x = 32.0f; cam.y = 64.0f; cam.z = 46.0f;
    cam.angle = 0.0f; cam.pitch = 0.0f; cam.fov = 1.5707963f; cam.far = 4096.0f;
    WCHECK(gmb_3d_world_render(w, &cam, &t) == GMB_3D_OK, "world: render del mondo caricato");

    gmb_3d_world_t* keep = w;
    gmb_3d_world_t* w2 = NULL;
    rc = gmb_3d_world_load(g_bad, &w2);
    WCHECK(rc == GMB_3D_E_INVARIANT, "R11: mondo invalido rifiutato");
    WCHECK(strstr(gmb_3d_last_error(), "sectors[0]") != NULL,
           "F-ERRORS: l'errore nomina l'elemento (sectors[0])");
    WCHECK(w2 == NULL, "R11: load atomico, *out non viene toccato");
    WCHECK(keep != NULL && gmb_3d_world_render(keep, &cam, &t) == GMB_3D_OK,
           "R11: dopo il load fallito il mondo precedente resta valido");

    gmb_3d_world_free(w);
    gmb_3d_target_free(&t);
}

/* --- W-MOVE: 10 casi headless di collisione ------------------------------ */

#define MOVE_CHECK(cond, what) do { \
    if (cond) { (*ok)++; } else { (*fail)++; printf("FAIL %s (move)\n", (what)); } } while (0)

static int nearf(float a, float b, float tol) { return fabsf(a - b) <= tol; }

static gmb_3d_world_t* load_rooms(const char* path)
{
    gmb_3d_world_t* w = NULL;
    if (gmb_3d_world_load(path, &w) != GMB_3D_OK) return NULL;
    return w;
}

/* Tutti i casi partono dai documenti scritti da w_prepare e passano dal loader
 * vero: se il movimento e il formato divergessero, si vedrebbe qui. */
void w_collision_checks(int* ok, int* fail)
{
    gmb_3d_player_t p;
    gmb_3d_world_t* base = load_rooms(g_rooms[0]);
    MOVE_CHECK(base != NULL, "W-MOVE: mondo a due stanze caricato");
    if (!base) return;

    /* (1) muro frontale: si ferma a un raggio dalla parete. Il punto è fuori
     *     dall'apertura del portale (y 32..96), altrimenti attraverserebbe. */
    gmb_3d_player_defaults(&p);
    MOVE_CHECK(gmb_3d_player_place(base, &p, 64, 112) == GMB_3D_OK, "W-MOVE: collocazione in stanza A");
    MOVE_CHECK(gmb_3d_player_move(base, &p, 200, 0) == GMB_3D_OK, "W-MOVE: frontale eseguito");
    MOVE_CHECK(!(p.hit & GMB_3D_HIT_SECTOR) && nearf(p.x, 114, 0.2f) && nearf(p.y, 112, 0.2f),
               "W-MOVE: muro frontale, fermata a un raggio");
    MOVE_CHECK((p.hit & GMB_3D_HIT_WALL) != 0, "W-MOVE: HIT bit 1 sul muro frontale");

    /* (2) addosso al muro: la richiesta di moto non sposta nulla (HIT bit 2). */
    gmb_3d_player_defaults(&p);
    MOVE_CHECK(gmb_3d_player_place(base, &p, 114, 112) == GMB_3D_OK, "W-MOVE: collocazione addosso");
    MOVE_CHECK(gmb_3d_player_move(base, &p, 5, 0) == GMB_3D_OK, "W-MOVE: spinta contro il muro");
    MOVE_CHECK(nearf(p.x, 114, 0.01f) && (p.hit & GMB_3D_HIT_STUCK) != 0,
               "W-MOVE: HIT bit 2 quando non ci si muove");

    /* (3) muro obliquo: scivola lungo il muro y=128, la componente
     *     tangenziale resta (forward 40, right -40 → +x,+y). */
    gmb_3d_player_defaults(&p);
    MOVE_CHECK(gmb_3d_player_place(base, &p, 64, 100) == GMB_3D_OK, "W-MOVE: collocazione obliqua");
    MOVE_CHECK(gmb_3d_player_move(base, &p, 40, -40) == GMB_3D_OK, "W-MOVE: diagonale eseguita");
    MOVE_CHECK(nearf(p.x, 104, 0.3f) && nearf(p.y, 114, 0.2f) && !(p.hit & GMB_3D_HIT_STUCK) &&
               (p.hit & GMB_3D_HIT_WALL) != 0, "W-MOVE: scivolamento lungo la parete");

    /* (4) portale alto: cambio di settore (HIT bit 4). */
    gmb_3d_player_defaults(&p);
    MOVE_CHECK(gmb_3d_player_place(base, &p, 64, 64) == GMB_3D_OK, "W-MOVE: collocazione davanti al portale");
    MOVE_CHECK(gmb_3d_player_move(base, &p, 100, 0) == GMB_3D_OK, "W-MOVE: attraversamento eseguito");
    MOVE_CHECK(p.sector == 1 && (p.hit & GMB_3D_HIT_SECTOR) != 0 && nearf(p.z, 0, 0.01f),
               "W-MOVE: portale alto, cambio di settore");
    MOVE_CHECK(nearf(p.x, 164, 0.5f) && nearf(p.y, 64, 0.3f), "W-MOVE: posizione oltre il portale");

    /* (5) gradino 24: si sale (pavimento del settore dietro = 24). */
    gmb_3d_world_t* w24 = load_rooms(g_rooms[1]);
    MOVE_CHECK(w24 != NULL, "W-MOVE: mondo con gradino 24 caricato");
    if (w24)
    {
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(w24, &p, 64, 64);
        gmb_3d_player_move(w24, &p, 100, 0);
        MOVE_CHECK(p.sector == 1 && nearf(p.z, 24, 0.01f) && (p.hit & GMB_3D_HIT_SECTOR) != 0,
                   "W-MOVE: gradino 24 salito");
    }

    /* (6) gradino 40: più alto dello step, il corpo resta di qua. */
    gmb_3d_world_t* w40 = load_rooms(g_rooms[2]);
    MOVE_CHECK(w40 != NULL, "W-MOVE: mondo con gradino 40 caricato");
    if (w40)
    {
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(w40, &p, 64, 64);
        gmb_3d_player_move(w40, &p, 100, 0);
        MOVE_CHECK(p.sector == 0 && nearf(p.x, 114, 0.2f) && nearf(p.z, 0, 0.01f) &&
                   !(p.hit & GMB_3D_HIT_SECTOR), "W-MOVE: gradino 40 rifiutato");
    }

    /* (7) apertura bassa (48 < altezza 56): il portale è un muro. */
    gmb_3d_world_t* wlow = load_rooms(g_rooms[3]);
    MOVE_CHECK(wlow != NULL, "W-MOVE: mondo con apertura bassa caricato");
    if (wlow)
    {
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(wlow, &p, 64, 64);
        gmb_3d_player_move(wlow, &p, 100, 0);
        MOVE_CHECK(p.sector == 0 && nearf(p.x, 114, 0.2f) && !(p.hit & GMB_3D_HIT_SECTOR),
                   "W-MOVE: portale basso non attraversato");
    }

    /* (8) apertura stretta (24 < 2×raggio 28): non ci passa il corpo. */
    gmb_3d_world_t* wnar = load_rooms(g_rooms[4]);
    MOVE_CHECK(wnar != NULL, "W-MOVE: mondo con apertura stretta caricato");
    if (wnar)
    {
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(wnar, &p, 64, 64);
        gmb_3d_player_move(wnar, &p, 100, 0);
        MOVE_CHECK(p.sector == 0 && nearf(p.x, 114, 0.2f) && !(p.hit & GMB_3D_HIT_SECTOR),
                   "W-MOVE: portale più stretto del corpo non attraversato");
    }

    /* (9) angolo: due pareti insieme, nessuno spostamento (bit 1 e 2). */
    gmb_3d_player_defaults(&p);
    MOVE_CHECK(gmb_3d_player_place(base, &p, 114, 114) == GMB_3D_OK, "W-MOVE: collocazione nell'angolo");
    MOVE_CHECK(gmb_3d_player_move(base, &p, 20, -20) == GMB_3D_OK, "W-MOVE: spinta nell'angolo");
    MOVE_CHECK(nearf(p.x, 114, 0.01f) && nearf(p.y, 114, 0.01f) &&
               (p.hit & GMB_3D_HIT_STUCK) != 0 && (p.hit & GMB_3D_HIT_WALL) != 0,
               "W-MOVE: angolo blocca su entrambi gli assi");

    /* (10) W-EYE-CLAMP: soffitto a 40, occhio non oltre 38. */
    gmb_3d_world_t* wceil = load_rooms(g_rooms[5]);
    MOVE_CHECK(wceil != NULL, "W-EYE-CLAMP: mondo col soffitto basso caricato");
    if (wceil)
    {
        gmb_3d_camera_t cam;
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(wceil, &p, 64, 64);
        MOVE_CHECK(gmb_3d_player_view(wceil, &p, &cam) == GMB_3D_OK && nearf(cam.z, 38, 0.01f),
                   "W-EYE-CLAMP: occhio fermo a 2 unità dal soffitto");
    }

    /* Integrazione: dopo un MOVE la vista del giocatore rende il mondo vero
     * (nessun colore inventato, profondità scritta su quasi tutto il target). */
    {
        gmb_3d_target_t t;
        gmb_3d_camera_t cam;
        memset(&t, 0, sizeof(t));
        MOVE_CHECK(gmb_3d_target_alloc(&t, 160, 120) == GMB_3D_OK, "W-MOVE: target per la vista");
        gmb_3d_player_defaults(&p);
        gmb_3d_player_place(base, &p, 48, 64);
        gmb_3d_player_move(base, &p, 120, 0);
        p.angle = 3.1415927f;                 /* di là dal portale, si guarda indietro */
        gmb_3d_present_mode_set(GMB_3D_MODE_TARGET);
        MOVE_CHECK(gmb_3d_player_view(base, &p, &cam) == GMB_3D_OK,
                   "W-MOVE: camera dal giocatore dopo il movimento");
        MOVE_CHECK(gmb_3d_world_render(base, &cam, &t) == GMB_3D_OK,
                   "W-MOVE: render dalla vista del giocatore");
        int lit = 0;
        for (int i = 0; i < t.w * t.h; i++) if (t.depth[i] > 0.0f) lit++;
        MOVE_CHECK(lit > (t.w * t.h) / 2, "W-MOVE: il mondo è coperto dalla vista (depth)");
        gmb_3d_target_free(&t);
    }

    gmb_3d_world_free(base);
    if (w24) gmb_3d_world_free(w24);
    if (w40) gmb_3d_world_free(w40);
    if (wlow) gmb_3d_world_free(wlow);
    if (wnar) gmb_3d_world_free(wnar);
    if (wceil) gmb_3d_world_free(wceil);
}

#undef MOVE_CHECK

/* Fuori da un task: stesso loader, strada sincrona (tool e oracolo, T3). */
void w_world_t3_checks(int* ok, int* fail)
{
    unsigned long p0 = gmb_3d_load_park_count();
    gmb_3d_world_t* w = NULL;
    int rc = gmb_3d_world_load(g_good, &w);
    WCHECK(rc == GMB_3D_OK && w != NULL, "R11/T3: load sincrono fuori da un task");
    WCHECK(gmb_3d_load_park_count() == p0, "R11/T3: nessun park senza scheduler");
    gmb_3d_world_free(w);
}
