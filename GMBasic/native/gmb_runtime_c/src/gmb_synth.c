/* gmb_synth.c — synth a 8 voci, render per campione, nessun device (fase 1 ADR).
 *
 * Il render è una funzione pura di (stato, n): 1×1000 campioni e 10×100 devono dare byte
 * identici (M-AUD-BLOCK), perché fase, filtro e inviluppo vivono nello stato della voce e
 * non nella dimensione del blocco. I comandi arrivano già applicati (offline) o vengono
 * applicati dal device all'inizio del blocco (gmb_audio_dev.c): qui non c'è né lock né
 * allocazione.
 */
#include "gmb_synth.h"

#include <math.h>
#include <string.h>

#define SYN_PI  3.14159265358979323846
#define SYN_AMP 16000.0            /* ampiezza di UNA voce (M-AUD-AMP) */

typedef struct {
    int      active;
    int      paused;
    int      wave;
    int      slot;                 /* 0 = non posizionale (P6 usa 1..16) */
    double   freq, vol, pan, pw;   /* vol 0..1, pan -1..1 */
    double   a, d, s, r;           /* frazioni della durata nominale */
    int64_t  total, remaining;
    double   phase;                /* [0,1): continua fra i blocchi */
    uint64_t noise;
    /* filtro (una sola legge: RBJ con q = 0,5 + 9,5·res) */
    double   b0, b1, b2, a1, a2;
    double   x1, x2, y1, y2;
    /* statistiche dell'ultimo blocco */
    double   peak, sumsq;
    int64_t  nsum;
} syn_voice_t;

typedef struct {
    int      kind;      /* 0 nota · 1 pausa */
    int      voice;
    int      vol;
    double   freq;
    int64_t  dur, start;
} syn_event_t;

#define SYN_EVENTS_MAX 512

static syn_voice_t   g_v[GMB_SYNTH_VOICES];
static int           g_filt_type[GMB_SYNTH_VOICES];
static double        g_filt_cut[GMB_SYNTH_VOICES];
static double        g_filt_res[GMB_SYNTH_VOICES];

static double        g_master = 1.0;
static int64_t       g_clock = 0;
static int64_t       g_clips = 0;
static double        g_mix_peak = 0.0;

static syn_event_t   g_ev[SYN_EVENTS_MAX];
static int           g_ev_n = 0;
static int           g_ev_next = 0;

/* Snapshot delle letture: double-buffer con contatore di sequenza (seqlock, D-7). */
typedef struct {
    volatile int seq;
    int64_t clock, clips;
    int     events;
    double  mix_peak;
    double  peak[GMB_SYNTH_VOICES], rms[GMB_SYNTH_VOICES];
    int64_t remaining[GMB_SYNTH_VOICES];
    double  freq[GMB_SYNTH_VOICES];
    int     busy[GMB_SYNTH_VOICES];
} syn_snap_t;

static syn_snap_t g_snap;

static void syn_barrier(void)
{
#if defined(__GNUC__) || defined(__clang__)
    __sync_synchronize();
#endif
}

static double syn_clamp(double x, double lo, double hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int syn_clamp_int(int x, int lo, int hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int16_t syn_quant(double x)
{
    long v = lround(x * 32767.0);
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

/* Coefficienti del filtro del voice dalle impostazioni correnti. */
static void syn_filter_coeffs(syn_voice_t* v, int voice)
{
    int type = g_filt_type[voice];
    if (type <= 0 || g_filt_cut[voice] <= 0.0)
    {
        v->b0 = 1.0; v->b1 = 0.0; v->b2 = 0.0; v->a1 = 0.0; v->a2 = 0.0;
        return;
    }
    double w0 = 2.0 * SYN_PI * g_filt_cut[voice] / (double)GMB_SYNTH_RATE;
    double cosw = cos(w0), sinw = sin(w0);
    double q = 0.5 + 9.5 * g_filt_res[voice];
    double alpha = sinw / (2.0 * q);
    double a0 = 1.0 + alpha;
    double b0, b1, b2;
    if (type == 1)      { b0 = (1.0 - cosw) / 2.0; b1 = 1.0 - cosw;       b2 = (1.0 - cosw) / 2.0; }
    else if (type == 2) { b0 = (1.0 + cosw) / 2.0; b1 = -(1.0 + cosw);    b2 = (1.0 + cosw) / 2.0; }
    else                { b0 = sinw / 2.0;         b1 = 0.0;               b2 = -sinw / 2.0; }
    v->b0 = b0 / a0; v->b1 = b1 / a0; v->b2 = b2 / a0;
    v->a1 = (-2.0 * cosw) / a0; v->a2 = (1.0 - alpha) / a0;
}

/* Inviluppo ADSR in frazioni di N, release DENTRO la durata (M-AUD-ENV). */
static double syn_env(const syn_voice_t* v, int64_t i)
{
    int64_t A = (int64_t)(v->a * (double)v->total);
    int64_t D = (int64_t)(v->d * (double)v->total);
    int64_t R = (int64_t)(v->r * (double)v->total);
    int64_t S = v->total - A - D - R;
    if (S < 0) S = 0;
    if (A > 0 && i < A) return (double)i / (double)A;
    int64_t after_a = i - A;
    if (D > 0 && after_a < D) return 1.0 - (1.0 - v->s) * (double)after_a / (double)D;
    int64_t after_d = after_a - D;
    if (S > 0 && after_d < S) return v->s;
    int64_t k = after_d - S;
    if (R > 0 && k >= 0 && k < R) return v->s * (1.0 - (double)k / (double)R);
    return 0.0;
}

/* Sei forme d'onda su una fase in [0,1) (M-AUD-WAVE). */
static double syn_wave(syn_voice_t* v)
{
    double ph = v->phase;
    switch (v->wave)
    {
    case 0: return sin(2.0 * SYN_PI * ph) + 0.1 * sin(4.0 * SYN_PI * ph);
    case 1: return sin(2.0 * SYN_PI * ph) >= 0.0 ? 1.0 : -1.0;
    case 2: return 2.0 * (ph - floor(ph + 0.5));
    case 3: return 4.0 * fabs(ph - 0.5) - 1.0;
    case 4:
        v->noise ^= v->noise << 13;
        v->noise ^= v->noise >> 7;
        v->noise ^= v->noise << 17;
        return ((double)(v->noise >> 33) / 2147483648.0) * 2.0 - 1.0;
    case 5: return ph < v->pw ? 1.0 : -1.0;
    default: return sin(2.0 * SYN_PI * ph);
    }
}

static void syn_publish(void)
{
    g_snap.seq++;
    syn_barrier();
    g_snap.clock = g_clock;
    g_snap.clips = g_clips;
    g_snap.events = g_ev_n;
    g_snap.mix_peak = g_mix_peak;
    for (int i = 0; i < GMB_SYNTH_VOICES; i++)
    {
        int64_t n = g_v[i].nsum;
        g_snap.peak[i] = n > 0 ? g_v[i].peak : -1.0;
        g_snap.rms[i] = n > 0 ? sqrt(g_v[i].sumsq / (double)n) : -1.0;
        g_snap.busy[i] = g_v[i].active ? 1 : 0;
        g_snap.freq[i] = g_v[i].active ? g_v[i].freq : -1.0;
        g_snap.remaining[i] = g_v[i].active ? g_v[i].remaining : -1;
    }
    syn_barrier();
    g_snap.seq++;
}

void gmb_synth_reset(void)
{
    memset(g_v, 0, sizeof g_v);
    memset(g_filt_type, 0, sizeof g_filt_type);
    memset(g_filt_cut, 0, sizeof g_filt_cut);
    memset(g_filt_res, 0, sizeof g_filt_res);
    memset(&g_snap, 0, sizeof g_snap);
    g_master = 1.0;
    g_clock = 0;
    g_clips = 0;
    g_mix_peak = 0.0;
    g_ev_n = 0;
    g_ev_next = 0;
    for (int i = 0; i < GMB_SYNTH_VOICES; i++)
        g_v[i].noise = 0x9E3779B97F4A7C15ULL + (uint64_t)i * 0x2545F4914F6CDD1DULL;
}

static void syn_note_on(const gmb_synth_cmd* c)
{
    int voice = syn_clamp_int(c->voice, 0, GMB_SYNTH_VOICES - 1);
    double freq = c->f0, dur = c->f1;
    if (!(freq > 0.0) || !(dur > 0.0)) return;          /* M-AUD-SOUND-QUIET */
    int64_t total = (int64_t)lround((double)GMB_SYNTH_RATE * dur);
    if (total < 1) total = 1;

    syn_voice_t* v = &g_v[voice];
    v->active = 1;
    v->paused = 0;
    v->freq = freq;
    v->total = total;
    v->remaining = total;
    v->phase = 0.0;                                       /* retrigger: fase da zero */
    v->vol = syn_clamp(c->f2 / 100.0, 0.0, 1.0);
    v->wave = syn_clamp_int((int)c->f3, 0, 5);
    v->a = syn_clamp(c->f4, 0.0, 1.0);
    v->d = syn_clamp(c->f5, 0.0, 1.0);
    v->s = syn_clamp(c->f6, 0.0, 1.0);
    v->r = syn_clamp(c->f7, 0.0, 1.0);
    v->pw = syn_clamp(c->f8, 0.01, 0.99);
    v->pan = 0.0;
    v->slot = c->slot;
    v->peak = 0.0; v->sumsq = 0.0; v->nsum = 0;
    v->x1 = v->x2 = v->y1 = v->y2 = 0.0;
    syn_filter_coeffs(v, voice);
}

void gmb_synth_apply(const gmb_synth_cmd* c)
{
    if (!c) return;
    switch (c->kind)
    {
    case GMB_SYN_NOTE_ON:  syn_note_on(c); break;
    case GMB_SYN_STOP:
        if (c->voice < 0) { for (int i = 0; i < GMB_SYNTH_VOICES; i++) g_v[i].active = 0; }
        else if (c->voice < GMB_SYNTH_VOICES) g_v[c->voice].active = 0;
        break;
    case GMB_SYN_PAUSE:
        if (c->voice < 0) { for (int i = 0; i < GMB_SYNTH_VOICES; i++) g_v[i].paused = 1; }
        else if (c->voice < GMB_SYNTH_VOICES) g_v[c->voice].paused = 1;
        break;
    case GMB_SYN_RESUME:
        if (c->voice < 0) { for (int i = 0; i < GMB_SYNTH_VOICES; i++) g_v[i].paused = 0; }
        else if (c->voice < GMB_SYNTH_VOICES) g_v[c->voice].paused = 0;
        break;
    case GMB_SYN_VOL_VOICE:
        if (c->voice >= 0 && c->voice < GMB_SYNTH_VOICES)
            g_v[c->voice].vol = syn_clamp(c->f0 / 100.0, 0.0, 1.0);
        break;
    case GMB_SYN_VOL_MASTER:
        g_master = syn_clamp(c->f0 / 100.0, 0.0, 1.0);
        break;
    case GMB_SYN_PAN:
        if (c->voice >= 0 && c->voice < GMB_SYNTH_VOICES)
            g_v[c->voice].pan = syn_clamp(c->f0, -1.0, 1.0);
        break;
    case GMB_SYN_FILTER:
        if (c->voice >= 0 && c->voice < GMB_SYNTH_VOICES)
        {
            int voice = c->voice;
            g_filt_type[voice] = syn_clamp_int((int)c->f0, 0, 3);
            g_filt_cut[voice] = syn_clamp(c->f1, 20.0, 20000.0);
            g_filt_res[voice] = syn_clamp(c->f2, 0.0, 95.0) / 100.0;
            if (g_v[voice].active) syn_filter_coeffs(&g_v[voice], voice);   /* live */
        }
        break;
    case GMB_SYN_RESET: gmb_synth_reset(); break;
    case GMB_SYN_SLOT_PAN:
        for (int i = 0; i < GMB_SYNTH_VOICES; i++)
        {
            if (!g_v[i].active || g_v[i].slot != c->slot) continue;
            g_v[i].vol = syn_clamp(c->f0, 0.0, 1.0);
            g_v[i].pan = syn_clamp(c->f1, -1.0, 1.0);
        }
        break;
    default: break;
    }
}

/* Eventi MML il cui start è già passato: partono all'inizio del blocco (§D-7). */
static void syn_dispatch_events(void)
{
    while (g_ev_next < g_ev_n && g_ev[g_ev_next].start <= g_clock)
    {
        syn_event_t* e = &g_ev[g_ev_next++];
        if (e->kind == 0 && e->freq > 0.0)
        {
            gmb_synth_cmd c;
            memset(&c, 0, sizeof c);
            c.kind = GMB_SYN_NOTE_ON;
            c.voice = e->voice;
            c.f0 = e->freq;
            c.f1 = (double)e->dur / (double)GMB_SYNTH_RATE;
            c.f2 = (double)e->vol;
            c.f3 = 0.0;                      /* timbro MML: sine */
            c.f4 = 0.10; c.f5 = 0.0; c.f6 = 1.0; c.f7 = 0.20; c.f8 = 0.25;
            syn_note_on(&c);
        }
    }
}

void gmb_synth_render(int16_t* out, int frames)
{
    if (!out || frames <= 0) return;
    syn_dispatch_events();

    for (int i = 0; i < GMB_SYNTH_VOICES; i++)
    {
        g_v[i].peak = 0.0;
        g_v[i].sumsq = 0.0;
        g_v[i].nsum = 0;
    }
    double mix_peak = 0.0;

    for (int n = 0; n < frames; n++)
    {
        double sum_l = 0.0, sum_r = 0.0;
        for (int i = 0; i < GMB_SYNTH_VOICES; i++)
        {
            syn_voice_t* v = &g_v[i];
            if (!v->active || v->paused) continue;

            int64_t idx = v->total - v->remaining;
            double env = syn_env(v, idx);
            double w = syn_wave(v);
            double s = SYN_AMP * env * w * v->vol;

            if (g_filt_type[i] > 0 || v->b1 != 0.0 || v->b2 != 0.0)
            {
                double y = v->b0 * s + v->b1 * v->x1 + v->b2 * v->x2
                         - v->a1 * v->y1 - v->a2 * v->y2;
                v->x2 = v->x1; v->x1 = s;
                v->y2 = v->y1; v->y1 = y;
                if (y > 2.0) y = 2.0;
                if (y < -2.0) y = -2.0;
                s = y;
            }

            double norm = s / 32767.0;
            double an = fabs(norm);
            if (an > v->peak) v->peak = an;
            v->sumsq += norm * norm;
            v->nsum++;

            double gl = v->pan > 0.0 ? 1.0 - v->pan : 1.0;
            double gr = v->pan < 0.0 ? 1.0 + v->pan : 1.0;
            sum_l += s * gl;
            sum_r += s * gr;

            v->phase += v->freq / (double)GMB_SYNTH_RATE;
            if (v->phase >= 1.0) v->phase -= floor(v->phase);
            v->remaining--;
            if (v->remaining <= 0) { v->remaining = 0; v->active = 0; }
        }

        double l = sum_l * g_master;
        double r = sum_r * g_master;
        double ln = l / 32767.0, rn = r / 32767.0;
        if (ln > 1.0)  { g_clips++; ln = 1.0; }
        if (ln < -1.0) { g_clips++; ln = -1.0; }
        if (rn > 1.0)  { g_clips++; rn = 1.0; }
        if (rn < -1.0) { g_clips++; rn = -1.0; }
        double a = fabs(ln) > fabs(rn) ? fabs(ln) : fabs(rn);
        if (a > mix_peak) mix_peak = a;

        out[n * 2] = syn_quant(ln);
        out[n * 2 + 1] = syn_quant(rn);
    }

    g_clock += frames;
    g_mix_peak = mix_peak;
    syn_publish();
}

int gmb_synth_busy(int voice)
{
    if (voice < 0 || voice >= GMB_SYNTH_VOICES) return 0;
    return g_v[voice].active ? 1 : 0;
}

int gmb_synth_any_busy(void)
{
    for (int i = 0; i < GMB_SYNTH_VOICES; i++)
        if (g_v[i].active) return 1;
    return 0;
}

int64_t gmb_synth_clock(void) { return g_clock; }

int64_t gmb_synth_voice_remaining(int voice)
{
    if (voice < 0 || voice >= GMB_SYNTH_VOICES) return -1;
    return g_v[voice].active ? g_v[voice].remaining : -1;
}

void gmb_synth_schedule_mml(const double* freq, const double* dur, const int* type,
                            const int* vol, int n, int voice, int64_t* out_total_samples)
{
    int64_t offset = 0;
    if (out_total_samples) *out_total_samples = 0;
    if (!freq || !dur || !type || n <= 0) return;
    int v = syn_clamp_int(voice, 0, GMB_SYNTH_VOICES - 1);

    for (int i = 0; i < n && g_ev_n < SYN_EVENTS_MAX; i++)
    {
        int64_t d = (int64_t)lround((double)GMB_SYNTH_RATE * dur[i]);
        if (d < 1) d = 1;
        syn_event_t* e = &g_ev[g_ev_n++];
        e->kind = type[i];
        e->voice = v;
        e->vol = vol ? vol[i] : 100;
        e->freq = freq[i];
        e->dur = d;
        e->start = g_clock + offset;
        offset += d;
    }
    if (out_total_samples) *out_total_samples = offset;
}

int gmb_synth_voice_read(int voice, int what, double* out)
{
    if (!out || voice < 0 || voice >= GMB_SYNTH_VOICES || what < 0 || what > 4) return 0;
    int seq = g_snap.seq;
    if (seq & 1) { syn_barrier(); seq = g_snap.seq; }
    double peak = g_snap.peak[voice];
    double rms = g_snap.rms[voice];
    int busy = g_snap.busy[voice];
    double fq = g_snap.freq[voice];
    int64_t rem = g_snap.remaining[voice];
    syn_barrier();
    if (g_snap.seq != seq) return 0;

    switch (what)
    {
    case 0: *out = peak; return peak >= 0.0 ? 1 : 0;
    case 1: *out = rms;  return rms >= 0.0 ? 1 : 0;
    case 2: *out = (double)busy; return 1;
    case 3: *out = fq; return fq >= 0.0 ? 1 : 0;
    case 4: *out = (double)rem; return rem >= 0 ? 1 : 0;
    default: return 0;
    }
}

int gmb_synth_status_read(int what, double* out)
{
    if (!out || what < 0 || what > 3) return 0;
    int seq = g_snap.seq;
    if (seq & 1) { syn_barrier(); seq = g_snap.seq; }
    double clock = (double)g_snap.clock;
    double peak = g_snap.mix_peak;
    double clips = (double)g_snap.clips;
    double events = (double)g_snap.events;
    syn_barrier();
    if (g_snap.seq != seq) return 0;
    switch (what)
    {
    case 0: *out = clock; return 1;
    case 1: *out = peak; return 1;
    case 2: *out = clips; return 1;
    case 3: *out = events; return 1;
    default: return 0;
    }
}

int gmb_synth_event_read(int index, int what, double* out)
{
    if (!out || index < 0 || index >= g_ev_n || what < 0 || what > 5) return 0;
    const syn_event_t* e = &g_ev[index];
    switch (what)
    {
    case 0: *out = (double)e->start; return 1;
    case 1: *out = e->freq; return 1;
    case 2: *out = (double)e->dur; return 1;
    case 3: *out = (double)e->vol; return 1;
    case 4: *out = (double)e->kind; return 1;
    case 5: *out = (double)e->voice; return 1;
    default: return 0;
    }
}
