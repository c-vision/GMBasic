/* gmb_3d_time.c — orologio e animazioni del mondo (P2, specifica §2.4).
 *
 *  - l'orologio lo avanza il programma (TICK): i secondi si accodano e il mondo
 *    procede a passi FISSI di 1/60 (T-FIXED). Il passo è l'unica unità di
 *    integrazione, quindi stesso seed e stessi TICK danno la stessa sequenza di
 *    frame (T-CLOCK) e le golden restano riproducibili;
 *  - SECMOVE porta pavimento e (se richiesto) soffitto alla quota data a
 *    velocità costante; il mover si dichiara fermo quando arriva, HEIGHT lo
 *    ferma. Porte, ascensori e trituratori sono tutti questo;
 *  - ANIM cicla le picture del settore, SCROLL integra gli offset di piani e
 *    pareti, LIGHTFX calcola la luce del settore per l'ultimo frame: il flicker
 *    usa un PRNG a seed fisso per settore, così è casuale all'occhio e
 *    ripetibile al bit;
 *  - LAMP aggiunge una lampada puntiforme (W-LIGHT): schiarisce e basta.
 *
 * Nessuna allocazione, nessun accesso a file, nessuna attesa: tutto vive nel
 * mondo e avanza nel task che chiama TICK.
 */
#include "gmb_3d_internal.h"

#include <math.h>

#define TM_MAX_SECS 10.0            /* un solo TICK non accoda più di questo */
#define TM_EPS      1.0e-9
#define TM_TAU      (2.0 * 3.14159265358979323846)

static int bad(const char* msg)
{
    gmb_3d_set_error(msg);
    return GMB_3D_E_ARG;
}

static gm_real approach(gm_real cur, gm_real to, gm_real maxstep)
{
    gm_real d = to - cur;
    if (d > maxstep) return cur + maxstep;
    if (d < -maxstep) return cur - maxstep;
    return to;
}

/* xorshift32: deterministico, nessuno stato condiviso fra settori. */
static unsigned int rng_next(unsigned int* s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static gm_real rng01(unsigned int* s)
{
    return (gm_real)(rng_next(s) >> 8) * (1.0 / 16777216.0);
}

/* Un passo di simulazione per un settore: mover, animazione, scroll, luce. */
static void step_sector(gmb_3d_world_t* w, int i, gm_real dt)
{
    gm_sector* s = &w->sw.sectors[i];
    gmb_3d_sector_rt* r = &w->rt[i];

    if (r->moving)
    {
        gm_real d = r->speed * dt;
        s->floor_z = approach(s->floor_z, r->floor_to, d);
        if (r->has_ceil) s->ceil_z = approach(s->ceil_z, r->ceil_to, d);
        if (fabs(s->floor_z - r->floor_to) <= TM_EPS &&
            (!r->has_ceil || fabs(s->ceil_z - r->ceil_to) <= TM_EPS))
            r->moving = 0;
    }

    if (r->anim_count > 0)
    {
        r->anim_phase += dt;
        int frame = (int)(r->anim_phase * r->anim_fps) % r->anim_count;
        if (frame < 0) frame = 0;
        s->tex_floor = r->anim_first + frame;
        if (r->anim_ceil) s->tex_ceil = r->anim_first + frame;
    }

    if (r->scroll_planes)
    {
        s->scroll_x += r->scroll_u * dt;
        s->scroll_y += r->scroll_v * dt;
    }
    if (r->scroll_walls)
    {
        for (int k = 0; k < s->nwalls; k++)
        {
            gm_sector_wall* wl = &w->sw.walls[s->walls[k]];
            wl->scroll_u += r->scroll_u * dt;
            wl->scroll_v += r->scroll_v * dt;
        }
    }

    if (r->fx_mode)
    {
        /* Fase sempre dentro un periodo: il costo non dipende dalla `rate`
         * (niente cicli proporzionali a 1/rate) e lo stato non cresce. */
        gm_real period = 1.0 / r->fx_rate;
        r->fx_phase += dt;
        if (r->fx_phase >= period)
        {
            r->fx_phase = fmod(r->fx_phase, period);
            if (r->fx_mode == 1) r->fx_value = rng01(&r->fx_rng);
        }
        gm_real ph = r->fx_phase / period;   /* 0..1 dentro il periodo */
        gm_real k;
        if (r->fx_mode == 1)      k = 1.0 - r->fx_amount * r->fx_value;      /* flicker */
        else if (r->fx_mode == 2) k = 1.0 + r->fx_amount * sin(TM_TAU * ph); /* pulse */
        else                      k = (ph < 0.5) ? 1.0 : 1.0 - r->fx_amount; /* strobe */
        if (k < 0.0) k = 0.0;
        s->light = k;
    }
}

int gmb_3d_tick(gmb_3d_world_t* w, double seconds)
{
    if (!w) return bad("tick: mondo mancante");
    if (!(seconds > 0.0)) { gmb_3d_set_error(""); return GMB_3D_OK; }
    if (seconds > TM_MAX_SECS) seconds = TM_MAX_SECS;   /* niente spirale */
    if (!(w->step > 0.0)) w->step = GMB_3D_DEFAULT_STEP;

    w->accum += seconds;
    int cap = (int)(TM_MAX_SECS / w->step) + 1;
    int steps = 0;
    while (w->accum + TM_EPS >= w->step && steps < cap)
    {
        for (int i = 0; i < w->sw.nsectors; i++) step_sector(w, i, w->step);
        w->accum -= w->step;
        w->time += w->step;
        w->ticks++;
        steps++;
    }
    if (steps == cap) w->accum = 0.0;   /* limite di catch-up raggiunto */
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

double gmb_3d_time(const gmb_3d_world_t* w)
{
    return w ? w->time : 0.0;
}

unsigned long gmb_3d_ticks(const gmb_3d_world_t* w)
{
    return w ? w->ticks : 0;
}

static int secmove(gmb_3d_world_t* w, int sec, float speed, float floor_z,
                   int has_ceil, float ceil_z)
{
    if (!w) return bad("secmove: mondo mancante");
    if (sec < 0 || sec >= w->sw.nsectors) return bad("secmove: settore inesistente");
    if (!(speed > 0.0f)) return bad("secmove: velocità non positiva");
    if (has_ceil && !(ceil_z > floor_z)) return bad("secmove: soffitto non sopra il pavimento");
    gmb_3d_sector_rt* r = &w->rt[sec];
    r->floor_to = floor_z;
    r->ceil_to = ceil_z;
    r->speed = speed;
    r->has_ceil = has_ceil;
    r->moving = 1;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_secmove(gmb_3d_world_t* w, int sec, float speed, float floor_z)
{
    return secmove(w, sec, speed, floor_z, 0, 0.0f);
}

int gmb_3d_secmove_ceil(gmb_3d_world_t* w, int sec, float speed,
                        float floor_z, float ceil_z)
{
    return secmove(w, sec, speed, floor_z, 1, ceil_z);
}

int gmb_3d_moving(const gmb_3d_world_t* w, int sec)
{
    if (!w || !w->rt || sec < 0 || sec >= w->sw.nsectors) return 0;
    return w->rt[sec].moving ? -1 : 0;
}

int gmb_3d_sector_z(const gmb_3d_world_t* w, int sec, float* floor_z, float* ceil_z)
{
    if (!w) return bad("sector_z: mondo mancante");
    if (sec < 0 || sec >= w->sw.nsectors) return bad("sector_z: settore inesistente");
    const gm_sector* s = &w->sw.sectors[sec];
    if (floor_z) *floor_z = (float)s->floor_z;
    if (ceil_z) *ceil_z = (float)s->ceil_z;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_sector_height(gmb_3d_world_t* w, int sec, float* floor_z, float* ceil_z)
{
    int rc = gmb_3d_sector_z(w, sec, floor_z, ceil_z);
    if (rc != GMB_3D_OK) return rc;
    w->rt[sec].moving = 0;      /* HEIGHT ferma un mover (T-SECMOVE) */
    return GMB_3D_OK;
}

int gmb_3d_anim(gmb_3d_world_t* w, int sec, int first, int count, float fps)
{
    if (!w) return bad("anim: mondo mancante");
    if (sec < 0 || sec >= w->sw.nsectors) return bad("anim: settore inesistente");
    if (first < 0 || count <= 0 || !(fps > 0.0f) || first + count > w->sw.ntex)
        return bad("anim: picture o fps non validi");
    gmb_3d_sector_rt* r = &w->rt[sec];
    gm_sector* s = &w->sw.sectors[sec];
    /* Solo la prima ANIM guarda il settore: l'animazione non crea un soffitto
     * a chi non ne aveva uno. */
    if (r->anim_count == 0) r->anim_ceil = (s->tex_ceil >= 0);
    r->anim_first = first;
    r->anim_count = count;
    r->anim_fps = fps;
    r->anim_phase = 0.0;
    s->tex_floor = first;
    if (r->anim_ceil) s->tex_ceil = first;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_scroll(gmb_3d_world_t* w, int sec, int what, float u_per_s, float v_per_s)
{
    if (!w) return bad("scroll: mondo mancante");
    if (sec < 0 || sec >= w->sw.nsectors) return bad("scroll: settore inesistente");
    if (what != 0 && what != 1) return bad("scroll: what 0 = piani, 1 = pareti");
    gmb_3d_sector_rt* r = &w->rt[sec];
    r->scroll_u = u_per_s;
    r->scroll_v = v_per_s;
    int active = (u_per_s != 0.0f || v_per_s != 0.0f);
    if (what == 0) r->scroll_planes = active;
    else           r->scroll_walls = active;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

int gmb_3d_lightfx(gmb_3d_world_t* w, int sec, int mode, float amount, float rate)
{
    if (!w) return bad("lightfx: mondo mancante");
    if (sec < 0 || sec >= w->sw.nsectors) return bad("lightfx: settore inesistente");
    if (mode < 0 || mode > 3) return bad("lightfx: modo 0..3");
    if (mode > 0 && !(rate > 0.0f)) return bad("lightfx: rate non positiva");
    gmb_3d_sector_rt* r = &w->rt[sec];
    r->fx_mode = mode;
    r->fx_amount = amount < 0.0f ? 0.0 : (amount > 1.0f ? 1.0 : (gm_real)amount);
    r->fx_rate = rate;
    r->fx_phase = 0.0;
    r->fx_value = 0.0;
    if (mode == 0) w->sw.sectors[sec].light = 1.0;   /* LIGHTFX 0 spegne l'effetto */
    gmb_3d_set_error("");
    return GMB_3D_OK;
}

float gmb_3d_light_now(const gmb_3d_world_t* w, int sec)
{
    if (!w || sec < 0 || sec >= w->sw.nsectors) return 1.0f;
    return (float)w->sw.sectors[sec].light;
}

int gmb_3d_lamp(gmb_3d_world_t* w, float x, float y, float z, float radius, float level)
{
    if (!w) return bad("lamp: mondo mancante");
    if (!(radius > 0.0f) || !(level >= 0.0f)) return bad("lamp: radius o livello non validi");
    if (w->sw.nlamps >= GM_SECTOR_MAX_LAMPS)
        return bad("lamp: massimo 8 lampade per mondo");
    gm_sector_lamp* l = &w->sw.lamps[w->sw.nlamps++];
    l->x = x;
    l->y = y;
    l->z = z;
    l->radius = radius;
    l->level = level;
    gmb_3d_set_error("");
    return GMB_3D_OK;
}
