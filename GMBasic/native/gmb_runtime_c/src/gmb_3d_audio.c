/* gmb_3d_audio.c — audio posizionale della scena (P6, M-SND, piano §4.3/§6).
 *
 * `SCENE SOUND n, freq, dur, x, y, z, dist, vol` dichiara una SORGENTE nello
 * slot n (1..16) e ne suona il tono. Il LISTENER è la CAMERA (`SCENE CAMERA`),
 * quindi guadagno e pan si calcolano dall'occhio e dallo yaw:
 *
 *  - attenuazione LINEARE: gain = (1 − d/dist) × vol/100, 0 oltre `dist`;
 *    `dist` 0 = nessuna attenuazione (sorgente a volume pieno);
 *  - pan = proiezione della direzione camera→sorgente sull'asse DESTRO della
 *    camera, in −1..1 (destra positiva). L'asse destro, con up +y, è
 *    (−cos yaw, 0, sin yaw): a yaw 0 il +x è a SINISTRA dell'occhio, la stessa
 *    convenzione dello schermo (SCENE.PIXEL). Il pitch non entra nel pan;
 *  - il pan si applica al PCM che sintetizziamo (`gmb_audio_sound_panned`): la
 *    tabella dlopen del mixer NON cambia — nessun Mix_SetPanning, nessun
 *    simbolo in più. Il pan di un tono è quindi uno SCATTO: `RENDER` aggiorna i
 *    valori della sorgente (quelli che leggono SOUNDGAIN/SOUNDPAN) ma non
 *    risuona il tono già avviato (M-SND-SNAP);
 *  - senza device (headless) non si sente nulla, ma gain e pan si calcolano lo
 *    stesso: è esattamente ciò che rende la superficie verificabile.
 *
 * Letture: `SCENE.SOUNDGAIN(n)` = gain 0..1, `SCENE.SOUNDPAN(n)` = pan −1..1.
 * Sentinelle (C-SENTINEL): −1 per il gain e −2 per il pan quando lo slot è
 * vuoto o fuori 1..16 (il pan valido include −1, quindi non può fare da
 * sentinella). Sono letture: non alzano errori di superficie.
 *
 * Perché un modulo a parte: la facciata dei comandi (`gmb_3d_cmd.c`) è già al
 * limite delle 1200 righe (§0quater).
 */
#include "gmb_3d_internal.h"
#include "gmb_audio.h"

#include <math.h>
#include <string.h>

#define GMB_3D_SOUND_SLOTS 16
#define GMB_3D_PI 3.14159265358979323846

typedef struct {
    int    used;
    double x, y, z;
    double dist, vol;
    double gain, pan;
} gmb_3d_sound_t;

static gmb_3d_sound_t g_sounds[GMB_3D_SOUND_SLOTS];

/* Listener = camera: occhio e asse destro (il pitch non entra nel pan). */
static void sound_listener(double eye[3], double* rx, double* rz)
{
    float e[3];
    float yaw = 0.0f, pitch = 0.0f;
    gmb_3d_scene_camera_get(e, &yaw, &pitch);
    (void)pitch;
    eye[0] = (double)e[0];
    eye[1] = (double)e[1];
    eye[2] = (double)e[2];
    /* cross(forward, up) con forward = (sin yaw, 0, cos yaw) e up = +y. */
    *rx = -cos((double)yaw * GMB_3D_PI / 180.0);
    *rz =  sin((double)yaw * GMB_3D_PI / 180.0);
}

/* Ricalcola gain e pan di uno slot dalla camera corrente. */
static void sound_place(gmb_3d_sound_t* s)
{
    double eye[3], rx, rz;
    sound_listener(eye, &rx, &rz);
    double dx = s->x - eye[0];
    double dy = s->y - eye[1];
    double dz = s->z - eye[2];
    double d = sqrt(dx * dx + dy * dy + dz * dz);

    double att = 1.0;
    if (s->dist > 0.0) att = (d >= s->dist) ? 0.0 : 1.0 - d / s->dist;
    double gain = att * s->vol / 100.0;
    if (gain < 0.0) gain = 0.0;
    if (gain > 1.0) gain = 1.0;

    double pan = 0.0;
    if (d > 1e-9) pan = (dx * rx + dz * rz) / d;
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;

    s->gain = gain;
    s->pan = pan;
}

void gmb_3d_sound_reset(void)
{
    memset(g_sounds, 0, sizeof g_sounds);
}

void gmb_3d_sound_update(void)
{
    for (int i = 0; i < GMB_3D_SOUND_SLOTS; i++)
        if (g_sounds[i].used) sound_place(&g_sounds[i]);
}

int64_t gmb_scene_sound(int64_t n, double freq, double dur, double x, double y, double z,
                        double dist, double vol)
{
    if (n < 1 || n > GMB_3D_SOUND_SLOTS)
    {
        gmb_3d_set_error("SCENE SOUND: slot fuori 1..16");
        return GMB_3D_E_ARG;
    }
    if (!isfinite(freq) || freq <= 0.0 || freq > 20000.0)
    {
        gmb_3d_set_error("SCENE SOUND: frequenza fuori 1..20000 Hz");
        return GMB_3D_E_ARG;
    }
    if (!isfinite(dur) || dur <= 0.0 || dur > 30.0)
    {
        gmb_3d_set_error("SCENE SOUND: durata fuori 0..30 s");
        return GMB_3D_E_ARG;
    }
    if (!isfinite(x) || !isfinite(y) || !isfinite(z))
    {
        gmb_3d_set_error("SCENE SOUND: posizione non finita");
        return GMB_3D_E_ARG;
    }
    if (!isfinite(dist) || dist < 0.0)
    {
        gmb_3d_set_error("SCENE SOUND: distanza negativa");
        return GMB_3D_E_ARG;
    }
    if (!isfinite(vol) || vol < 0.0 || vol > 100.0)
    {
        gmb_3d_set_error("SCENE SOUND: volume fuori 0..100");
        return GMB_3D_E_ARG;
    }

    gmb_3d_sound_t* s = &g_sounds[n - 1];
    s->used = 1;
    s->x = x; s->y = y; s->z = z;
    s->dist = dist;
    s->vol = vol;
    sound_place(s);

    gmb_3d_set_error("");
    /* Pan sul PCM: la sorgente suona con il guadagno e il pan di ADESSO. */
    gmb_audio_sound_panned((int)(freq + 0.5), dur, s->gain, s->pan);
    return GMB_3D_OK;
}

double gmb_scene_sound_gain(int64_t n)
{
    if (n < 1 || n > GMB_3D_SOUND_SLOTS || !g_sounds[n - 1].used) return -1.0;
    return g_sounds[n - 1].gain;
}

double gmb_scene_sound_pan(int64_t n)
{
    if (n < 1 || n > GMB_3D_SOUND_SLOTS || !g_sounds[n - 1].used) return -2.0;
    return g_sounds[n - 1].pan;
}
