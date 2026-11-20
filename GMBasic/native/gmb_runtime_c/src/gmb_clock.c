/* gmb_clock.c — base di tempo a passo fisso (vedi include/gmb_clock.h).
 *
 * Un solo posto decide quanto tempo è passato e quanti passi di simulazione
 * eseguire: accumulatore, clamp del frame, tetto al recupero, scala, pausa e
 * avanzamento deterministico. Nessuna attesa qui dentro: chi chiama decide come
 * dormire (il park GPM vive in gmb_form.c, dove c'è già la regola "park nel
 * task, sleep sul main thread").
 */
#include "gmb_clock.h"

#include <math.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <time.h>
#endif

#define GMB_CLOCK_EPS 1.0e-9
#define GMB_CLOCK_FPS_MIN 1.0
#define GMB_CLOCK_FPS_MAX 1000.0
#define GMB_CLOCK_SCALE_MAX 16.0
#define GMB_CLOCK_EMA 0.1

void gmb_clock_set_fps(gmb_clock_t* c, double fps)
{
    if (!c) return;
    if (!(fps >= GMB_CLOCK_FPS_MIN && fps <= GMB_CLOCK_FPS_MAX)) fps = GMB_CLOCK_DEFAULT_FPS;
    c->fps = fps;
    c->step = 1.0 / fps;
}

void gmb_clock_init(gmb_clock_t* c, double fps)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    gmb_clock_set_fps(c, fps);
    c->scale = 1.0;
    c->max_frame = GMB_CLOCK_DEFAULT_MAX_FRAME;
    c->max_ticks = GMB_CLOCK_DEFAULT_MAX_TICKS;
    c->ema_dt = c->step;
}

void gmb_clock_reset(gmb_clock_t* c)
{
    if (!c) return;
    c->accum = 0.0;
    c->time = 0.0;
    c->frames = 0;
    c->ticks = 0;
    c->clamped = 0;
    c->dropped = 0;
    c->ema_dt = c->step;
    c->started = 0;
    c->last_ns = 0;
}

void gmb_clock_set_scale(gmb_clock_t* c, double scale)
{
    if (!c) return;
    if (!(scale >= 0.0 && scale <= GMB_CLOCK_SCALE_MAX)) return;   /* fuori range: lo stato resta */
    c->scale = scale;
}

void gmb_clock_set_paused(gmb_clock_t* c, int paused)
{
    if (!c) return;
    c->paused = paused ? 1 : 0;
    if (c->paused) c->accum = 0.0;   /* il tempo in sospeso non si consuma al resume */
}

void gmb_clock_set_limits(gmb_clock_t* c, double max_frame_s, int max_ticks)
{
    if (!c) return;
    if (max_frame_s > 0.0) c->max_frame = max_frame_s;
    if (max_ticks > 0) c->max_ticks = max_ticks;
}

int gmb_clock_advance(gmb_clock_t* c, double real_seconds)
{
    if (!c || !(c->step > 0.0)) return 0;

    double dt = real_seconds;
    if (!(dt > 0.0)) dt = 0.0;

    /* Clamp: un blocco lungo non si smaltisce in un colpo. */
    if (dt > c->max_frame) {
        dt = c->max_frame;
        c->clamped++;
    }

    /* Statistiche sul tempo REALE del frame (prima della scala). */
    if (real_seconds > 0.0) {
        c->ema_dt += GMB_CLOCK_EMA * (real_seconds - c->ema_dt);
        if (c->ema_dt <= 0.0) c->ema_dt = c->step;
    }

    c->frames++;

    if (c->paused) {
        c->accum = 0.0;
        return 0;
    }

    c->accum += dt * c->scale;

    int n = (int)((c->accum + GMB_CLOCK_EPS) / c->step);
    if (n < 0) n = 0;
    if (n > c->max_ticks) {
        /* Recupero limitato: il ritardo in eccesso si scarta, non si trascina. */
        c->dropped += (unsigned long long)(n - c->max_ticks);
        n = c->max_ticks;
        c->accum = 0.0;
    } else {
        c->accum -= n * c->step;
        if (c->accum < 0.0) c->accum = 0.0;
    }

    c->ticks += (unsigned long long)n;
    c->time += (double)n * c->step;
    return n;
}

int gmb_clock_step(gmb_clock_t* c, int ticks)
{
    if (!c || ticks <= 0) return 0;
    c->frames++;
    c->ticks += (unsigned long long)ticks;
    c->time += (double)ticks * c->step;
    return ticks;   /* l'accumulatore resta com'è: nessun tempo reale consumato */
}

int gmb_clock_begin_frame(gmb_clock_t* c)
{
    if (!c) return 0;
    int64_t now = gmb_clock_now_ns();
    if (!c->started) {
        c->started = 1;
        c->last_ns = now;
        return gmb_clock_advance(c, c->step);   /* il primo frame vale un passo */
    }
    double dt = (double)(now - c->last_ns) / 1.0e9;
    c->last_ns = now;
    return gmb_clock_advance(c, dt);
}

double gmb_clock_rest(const gmb_clock_t* c)
{
    if (!c || !(c->step > 0.0)) return 0.0;
    double rest = c->step - c->accum;
    if (rest < 0.0) rest = 0.0;
    if (rest > c->step) rest = c->step;
    return rest;
}

double gmb_clock_dt(const gmb_clock_t* c)
{
    return (c && c->step > 0.0) ? c->step : 0.0;
}

double gmb_clock_alpha(const gmb_clock_t* c)
{
    if (!c || !(c->step > 0.0)) return 0.0;
    double a = c->accum / c->step;
    if (a < 0.0) a = 0.0;
    if (a >= 1.0) a = 0.999999;   /* mai 1: 1 appartiene al passo successivo */
    return a;
}

double gmb_clock_time(const gmb_clock_t* c)
{
    return c ? c->time : 0.0;
}

double gmb_clock_fps_measured(const gmb_clock_t* c)
{
    if (!c || !(c->ema_dt > 0.0)) return 0.0;
    return 1.0 / c->ema_dt;
}

int64_t gmb_clock_now_ns(void)
{
#if defined(_WIN32) || defined(_WIN64)
    static LARGE_INTEGER freq;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (int64_t)((double)t.QuadPart * 1.0e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

gmb_clock_t* gmb_clock_frame_clock(void)
{
    static gmb_clock_t s_clock;
    static int s_ready = 0;
    if (!s_ready) {
        gmb_clock_init(&s_clock, GMB_CLOCK_DEFAULT_FPS);
        s_ready = 1;
    }
    return &s_clock;
}

/* ── Facciata BASIC del dominio CLOCK (vedi gmb_clock.h) ────────────────
 * Nessuna logica nuova: letture e scritture sull'orologio globale. Sono le
 * funzioni che l'emitter C99 chiama, che la VM/C chiama dalle sue syscall e che
 * il C# importa da libgmb_clock (P/Invoke). */
double gmb_clock_frame_dt(void)
{
    return gmb_clock_dt(gmb_clock_frame_clock());
}

double gmb_clock_frame_alpha(void)
{
    return gmb_clock_alpha(gmb_clock_frame_clock());
}

double gmb_clock_frame_fps(void)
{
    const gmb_clock_t* c = gmb_clock_frame_clock();
    return (c && c->step > 0.0) ? 1.0 / c->step : 0.0;
}

double gmb_clock_frame_scale(void)
{
    const gmb_clock_t* c = gmb_clock_frame_clock();
    return c ? c->scale : 0.0;
}

void gmb_clock_frame_pause(int paused)
{
    gmb_clock_set_paused(gmb_clock_frame_clock(), paused);
}

void gmb_clock_frame_set_scale(double scale)
{
    gmb_clock_set_scale(gmb_clock_frame_clock(), scale);
}

void gmb_clock_frame_set_fps(double fps)
{
    gmb_clock_set_fps(gmb_clock_frame_clock(), fps);
}
