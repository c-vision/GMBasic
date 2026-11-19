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
 */
#include "gmb_3d_internal.h"

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
