/* gmb_audio.c — superficie audio del motore C.
 *
 * Tre strati, uno per file (fase 1 dell'ADR-Audio-Confine-Synth):
 *   gmb_synth.c      il motore a 8 voci: render puro, stato, letture (nessun device)
 *   gmb_audio_dev.c  il device (mixer + Mix_HookMusic) e la coda SPSC
 *   gmb_audio.c      questa facade: comandi BASIC, parser MML, funzioni math e di parità
 *
 * La facade NON conosce né chunk né canali del mixer per i toni: un `SOUND` è un comando
 * NOTE_ON al motore, i WAV restano chunk (D-2). Con il device assente il comando si applica
 * subito e il render è esplicito (`gmb_audio_render`): la stessa strada dei test headless.
 */
#include "gmb_runtime.h"
#include "gmb_audio.h"
#include "gmb_audio_dev.h"
#include "gmb_synth.h"
#include "gmb_c99gpm.h"   /* gmb_gpm_sleep: attesa cooperativa §0ter */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Math (parità AudioUtils.cs) ──────────────────────────────────────────────────── */

double gmb_audio_envelope_adsr(int sample, int attackSamples, int decaySamples,
                               int sustainSamples, int releaseSamples, double sustainLevel)
{
    if (attackSamples > 0 && sample < attackSamples)
        return (double)sample / attackSamples;

    int afterAttack = sample - attackSamples;
    if (decaySamples > 0 && afterAttack < decaySamples)
        return 1.0 - (1.0 - sustainLevel) * afterAttack / decaySamples;

    int afterDecay = sample - attackSamples - decaySamples;
    if (sustainSamples > 0 && afterDecay < sustainSamples)
        return sustainLevel;

    int afterSustain = sample - attackSamples - decaySamples - sustainSamples;
    if (releaseSamples > 0 && afterSustain < releaseSamples && afterSustain >= 0)
        return sustainLevel * (1.0 - (double)afterSustain / releaseSamples);

    return 0.0;
}

double gmb_audio_envelope(int sample, int totalSamples, int attackSamples, int releaseSamples)
{
    if (sample < attackSamples)
        return (double)sample / attackSamples;

    if (sample > totalSamples - releaseSamples)
        return (double)(totalSamples - sample) / releaseSamples;

    return 1.0;
}

double gmb_audio_frequency(double baseFreq, int octave)
{
    return baseFreq * pow(2, octave - 4);
}

double gmb_audio_midi_to_frequency(int midiNote)
{
    return 440.0 * pow(2, (midiNote - 69) / 12.0);
}

double gmb_audio_note_duration(int noteLength, int tempo)
{
    return 240.0 / (tempo * noteLength);
}

/* ── Synth PCM mono legacy (parità AudioManager.CreateWaveformChunk) ──────────────────
 * Resta perché è una funzione PURA usata dai selfcheck di parità (gmb_gaps_selfcheck):
 * un buffer mono, nessun device, nessuna voce. Il motore nuovo è gmb_synth.c. */
#define GMB_AUDIO_RATE 44100

int gmb_audio_synth(int frequency, double duration, int waveform,
                    double attack, double decay, double sustain, double release,
                    double pulse_width, int filter_type, double cutoff, double resonance,
                    double* out, int max_samples)
{
    if (frequency <= 0 || duration <= 0 || !out || max_samples <= 0) return 0;
    int samples = (int)(GMB_AUDIO_RATE * duration);
    if (samples > max_samples) samples = max_samples;
    int attackS = (int)(attack * samples);
    int decayS = (int)(decay * samples);
    int releaseS = (int)(release * samples);
    int sustainS = samples - attackS - decayS - releaseS;
    if (sustainS < 0) sustainS = 0;

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    if (filter_type != 0 && cutoff > 0) {
        double w0 = 2.0 * 3.141592653589793 * cutoff / GMB_AUDIO_RATE;
        double alpha = sin(w0) * resonance;
        double cosW0 = cos(w0);
        double a0 = 1.0 + alpha;
        switch (filter_type) {
            case 1: b0 = (1.0 - cosW0) / 2.0 / a0; b1 = (1.0 - cosW0) / a0; b2 = (1.0 - cosW0) / 2.0 / a0; a1 = (-2.0 * cosW0) / a0; a2 = (1.0 - alpha) / a0; break;
            case 2: b0 = (1.0 + cosW0) / 2.0 / a0; b1 = -(1.0 + cosW0) / a0; b2 = (1.0 + cosW0) / 2.0 / a0; a1 = (-2.0 * cosW0) / a0; a2 = (1.0 - alpha) / a0; break;
            case 3: b0 = alpha / a0; b1 = 0; b2 = -alpha / a0; a1 = (-2.0 * cosW0) / a0; a2 = (1.0 - alpha) / a0; break;
        }
    }

    static uint64_t noise_seed = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < samples; i++) {
        double t = (double)i / GMB_AUDIO_RATE;
        double env = gmb_audio_envelope_adsr(i, attackS, decayS, sustainS, releaseS, sustain);
        double value;
        switch (waveform) {
            case 0: value = sin(2 * 3.141592653589793 * frequency * t) + 0.1 * sin(4 * 3.141592653589793 * frequency * t); break;
            case 1: value = sin(2 * 3.141592653589793 * frequency * t) >= 0 ? 1.0 : -1.0; break;
            case 2: value = 2.0 * (frequency * t - floor(frequency * t + 0.5)); break;
            case 3: { double ph = frequency * t - floor(frequency * t); value = 4.0 * fabs(ph - 0.5) - 1.0; break; }
            case 4: { noise_seed ^= noise_seed << 13; noise_seed ^= noise_seed >> 7; noise_seed ^= noise_seed << 17; value = ((double)(noise_seed >> 33) / 2147483648.0) * 2.0 - 1.0; break; }
            case 5: value = (frequency * t - floor(frequency * t)) < pulse_width ? 1.0 : -1.0; break;
            default: value = sin(2 * 3.141592653589793 * frequency * t); break;
        }
        if (filter_type != 0) {
            double x0 = value;
            double y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            if (y0 > 2.0) y0 = 2.0; if (y0 < -2.0) y0 = -2.0;
            x2 = x1; x1 = x0; y2 = y1; y1 = y0;
            value = y0;
        }
        if (value > 1.0) value = 1.0; if (value < -1.0) value = -1.0;
        out[i] = 16000.0 * env * value;
    }
    return samples;
}

/* ── MML (AudioManager.cs ParseMusicString): O/L/T/V/N/>/< + note ─────────────────── */
int gmb_audio_mml_parse(const char* mml, double* out_freq, double* out_dur, int* out_type,
                        int* out_vol, int max_cmds)
{
    if (!mml || !out_freq || max_cmds <= 0) return 0;
    int current_octave = 4, note_len = 4, tempo = 120, vol = 100;
    int n = 0;
    const char* p = mml;
    while (*p && n < max_cmds) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        char c = (char)toupper((unsigned char)*p);
        int dotted = 0;
        if (c == '.') { p++; continue; }
        if (c == 'O' && p[1] >= '0' && p[1] <= '9') { current_octave = p[1] - '0'; if (current_octave > 6) current_octave = 6; p += 2; continue; }
        /* L a PIU' cifre (parita' C#: L[1-9]\d* — L16 = 1/16, non L1). */
        if (c == 'L' && p[1] >= '0' && p[1] <= '9') {
            note_len = atoi(p + 1);
            if (note_len < 1) note_len = 1;
            if (note_len > 64) note_len = 64;
            p++; while (*p >= '0' && *p <= '9') p++;
            continue;
        }
        if (c == 'T' && p[1]) { tempo = atoi(p + 1); if (tempo < 32) tempo = 32; if (tempo > 255) tempo = 255; p++; while (*p >= '0' && *p <= '9') p++; continue; }
        if (c == 'V' && p[1]) { vol = atoi(p + 1); if (vol > 100) vol = 100; p++; while (*p >= '0' && *p <= '9') p++; continue; }
        if (c == 'P' && p[1]) {
            int pl = atoi(p + 1); if (pl < 1) pl = 1;
            p++; while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { dotted = 1; p++; }
            double dur = gmb_audio_note_duration(pl, tempo); if (dotted) dur *= 1.5;
            out_freq[n] = 0; out_dur[n] = dur; out_type[n] = 1;
            if (out_vol) out_vol[n] = vol;
            n++;
            continue;
        }
        /* N<midi>: nota MIDI ASSOLUTA 0..127 (ignora l'ottava), come il C#. */
        if (c == 'N' && p[1] >= '0' && p[1] <= '9') {
            int midi = atoi(p + 1);
            if (midi < 0) midi = 0;
            if (midi > 127) midi = 127;
            p++; while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { dotted = 1; p++; }
            double dur = gmb_audio_note_duration(note_len, tempo); if (dotted) dur *= 1.5;
            out_freq[n] = gmb_audio_midi_to_frequency(midi); out_dur[n] = dur; out_type[n] = 0;
            if (out_vol) out_vol[n] = vol;
            n++;
            continue;
        }
        if (c == '>') { if (current_octave < 6) current_octave++; p++; continue; }
        if (c == '<') { if (current_octave > 0) current_octave--; p++; continue; }
        /* nota: C D E F G A B (con #/+) */
        static const char* names = "CDEFGAB";
        const char* idx = strchr(names, c);
        if (idx) {
            p++;
            int sharp = 0;
            if (*p == '#' || *p == '+') { sharp = 1; p++; }
            if (*p == '.') { dotted = 1; p++; }
            int i = (int)(idx - names);
            double basef = 261.63;
            if (i == 1) basef = 293.66; else if (i == 2) basef = 329.63;
            else if (i == 3) basef = 349.23; else if (i == 4) basef = 392.00; else if (i == 5) basef = 440.00;
            else if (i == 6) basef = 493.88;
            if (sharp) basef *= 1.0594630943592953;
            double freq = gmb_audio_frequency(basef, current_octave);
            double dur = gmb_audio_note_duration(note_len, tempo); if (dotted) dur *= 1.5;
            out_freq[n] = freq; out_dur[n] = dur; out_type[n] = 0;
            if (out_vol) out_vol[n] = vol;
            n++;
            continue;
        }
        p++;
    }
    return n;
}

/* ── Comandi verso il motore ──────────────────────────────────────────────────────── */

static double clampd(double x, double lo, double hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int voice_of_slot(int slot)
{
    int v = (slot - 1) % GMB_SYNTH_VOICES;   /* P6: voce = (slot-1) mod 8 */
    if (v < 0) v += GMB_SYNTH_VOICES;
    return v;
}

static void send_note(int voice, int slot, double freq, double dur, double vol100,
                      int wave, double a, double d, double s, double r, double pw,
                      double pan)
{
    gmb_synth_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = GMB_SYN_NOTE_ON;
    c.voice = voice;
    c.slot = slot;
    c.f0 = freq;
    c.f1 = dur;
    c.f2 = vol100;
    c.f3 = (double)wave;
    c.f4 = a; c.f5 = d; c.f6 = s; c.f7 = r; c.f8 = pw;
    gmb_audio_dev_command(&c);
    if (pan != 0.0)
    {
        memset(&c, 0, sizeof c);
        c.kind = GMB_SYN_PAN;
        c.voice = voice;
        c.f0 = clampd(pan, -1.0, 1.0);
        gmb_audio_dev_command(&c);
    }
}

/* Attesa cooperativa §0ter: solo col device (in offline il render è di chi chiama). */
static void wait_voice(int voice, double duration)
{
    if (gmb_audio_dev_mode() != 1) return;
    int64_t limit = (int64_t)(duration * 500.0) + 4000;   /* guardia: 2 ms per giro */
    int64_t spins = 0;
    while (gmb_synth_busy(voice) && spins++ < limit) gmb_gpm_sleep(2);
}

void gmb_audio_sound_voice(double voice_d, double frequency, double duration, double volume_d,
                           double waveform_d, double attack, double decay, double sustain,
                           double release, double pulse_width)
{
    int voice = (int)voice_d;
    if (voice < 0) voice = 0;
    if (voice >= GMB_SYNTH_VOICES) voice = GMB_SYNTH_VOICES - 1;
    if (!(frequency > 0.0) || !(duration > 0.0)) return;
    double vol = clampd(volume_d, 0.0, 100.0);
    int wave = (int)waveform_d;
    if (wave < 0) wave = 0;
    if (wave > 5) wave = 5;
    gmb_audio_dev_mode();   /* apre il device al primo uso (D-6) */
    send_note(voice, 0, frequency, duration, vol, wave,
              clampd(attack, 0.0, 1.0), clampd(decay, 0.0, 1.0),
              clampd(sustain, 0.0, 1.0), clampd(release, 0.0, 1.0),
              clampd(pulse_width, 0.01, 0.99), 0.0);
    wait_voice(voice, duration);
}

void gmb_audio_sound(int frequency, double duration, int volume)
{
    gmb_audio_sound_voice(0, frequency, duration, volume, 0, 0.10, 0.0, 1.0, 0.20, 0.25);
}

void gmb_audio_play_mml(const char* mml)
{
    if (!mml || !*mml) return;
    double freq[256], dur[256];
    int type[256], vol[256];
    int n = gmb_audio_mml_parse(mml, freq, dur, type, vol, 256);
    if (n <= 0) return;
    gmb_audio_dev_mode();
    int64_t total = 0;
    gmb_synth_schedule_mml(freq, dur, type, vol, n, 0, &total);
    if (gmb_audio_dev_mode() != 1) return;      /* offline: suona il render esplicito */
    int64_t end = gmb_synth_clock() + total;
    int64_t limit = total / 882 + 4000;         /* 2 ms per giro, guardia */
    int64_t spins = 0;
    while (gmb_synth_clock() < end && spins++ < limit) gmb_gpm_sleep(2);
}

int gmb_audio_sound_async(int frequency, double duration, int volume)
{
    if (duration <= 0) return 0;
    int ms = (int)(duration * 1000.0 + 0.999);
    if (ms < 1) ms = 1;
    double vol = clampd(volume, 0.0, 100.0);
    gmb_audio_dev_mode();
    if (frequency > 0) send_note(0, 0, frequency, duration, vol, 0, 0.10, 0.0, 1.0, 0.20, 0.25, 0.0);
    return ms;
}

int gmb_audio_mml_async(const char* mml)
{
    if (!mml || !*mml) return 0;
    double freq[256], dur[256];
    int type[256], vol[256];
    int n = gmb_audio_mml_parse(mml, freq, dur, type, vol, 256);
    if (n <= 0) return 0;
    int64_t total = 0;
    gmb_synth_schedule_mml(freq, dur, type, vol, n, 0, &total);
    int ms = (int)((double)total * 1000.0 / (double)GMB_SYNTH_RATE + 0.5);
    return ms > 0 ? ms : 1;
}

void gmb_audio_sound_panned(int frequency, double duration, double gain, double pan)
{
    gmb_audio_sound_panned_slot(0, frequency, duration, gain, pan);
}

void gmb_audio_sound_panned_slot(int slot, double frequency, double duration, double gain, double pan)
{
    if (!(frequency > 0.0) || !(duration > 0.0)) return;
    if (!(gain > 0.0)) return;
    int voice = slot >= 1 ? voice_of_slot(slot) : 0;
    gmb_audio_dev_mode();
    send_note(voice, slot >= 1 ? slot : 0, frequency, duration, clampd(gain, 0.0, 1.0) * 100.0,
              0, 0.01, 0.0, 1.0, 0.02, 0.25, clampd(pan, -1.0, 1.0));
}

void gmb_audio_update_panned(int slot, double gain, double pan)
{
    if (slot < 1) return;
    gmb_synth_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = GMB_SYN_SLOT_PAN;
    c.slot = slot;
    c.f0 = clampd(gain, 0.0, 1.0);
    c.f1 = clampd(pan, -1.0, 1.0);
    gmb_audio_dev_command(&c);
}

/* Handle 1..63 dei WAV caricati (0 = errore). Il chunk vive quanto il handle. */
#define GMB_AUDIO_MAX_CHUNKS 64
static gmb_mix_chunk* g_chunks[GMB_AUDIO_MAX_CHUNKS];

int gmb_audio_load_sound(const char* path)
{
    gmb_mix_chunk* ch = gmb_audio_dev_load_wav(path);
    if (!ch) return 0;
    for (int i = 1; i < GMB_AUDIO_MAX_CHUNKS; i++)
    {
        if (!g_chunks[i]) { g_chunks[i] = ch; return i; }
    }
    gmb_audio_dev_free_chunk(ch);
    return 0;
}

void gmb_audio_play_sound(int handle, int channel, int loops)
{
    if (handle <= 0 || handle >= GMB_AUDIO_MAX_CHUNKS || !g_chunks[handle]) return;
    gmb_audio_dev_play_chunk(g_chunks[handle], channel, loops);
}

void gmb_audio_set_volume(int volume, int channel)
{
    int v = (int)clampd(volume, 0.0, 100.0);
    gmb_synth_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = channel < 0 ? GMB_SYN_VOL_MASTER : GMB_SYN_VOL_VOICE;
    c.voice = channel < 0 ? -1 : (channel >= GMB_SYNTH_VOICES ? GMB_SYNTH_VOICES - 1 : channel);
    c.f0 = (double)v;
    gmb_audio_dev_command(&c);
    gmb_audio_dev_set_channels_volume(channel, v * 128 / 100);
}

void gmb_audio_control(int action, int channel)
{
    gmb_synth_cmd c;
    memset(&c, 0, sizeof c);
    c.voice = channel < 0 ? -1 : (channel >= GMB_SYNTH_VOICES ? GMB_SYNTH_VOICES - 1 : channel);
    if (action == 0) c.kind = GMB_SYN_STOP;
    else if (action == 1) c.kind = GMB_SYN_PAUSE;
    else if (action == 2) c.kind = GMB_SYN_RESUME;
    else return;                                   /* 3 = clear queue: nessuna coda */
    gmb_audio_dev_command(&c);
    gmb_audio_dev_control(action, channel);
}

void gmb_audio_set_filter(int voice, int type, double cutoff, double resonance)
{
    gmb_synth_cmd c;
    memset(&c, 0, sizeof c);
    c.kind = GMB_SYN_FILTER;
    c.voice = voice < 0 ? 0 : (voice >= GMB_SYNTH_VOICES ? GMB_SYNTH_VOICES - 1 : voice);
    c.f0 = (double)(type < 0 ? 0 : (type > 3 ? 3 : type));
    c.f1 = cutoff;
    c.f2 = resonance;
    gmb_audio_dev_command(&c);
}

int gmb_audio_is_playing(void)
{
    if (gmb_synth_any_busy()) return 1;
    return gmb_audio_dev_mix_playing();
}

/* ── Letture headless (AUDIO.*: superficie C finché non esiste il token BASIC) ────── */

void gmb_audio_set_mode(int mode)
{
    gmb_audio_dev_set_mode(mode ? 1 : 0);
}

int gmb_audio_mode(void) { return gmb_audio_dev_mode(); }

int gmb_audio_render(int16_t* out, int frames)
{
    return gmb_audio_dev_render(out, frames);
}

int gmb_audio_voice_read(int voice, int what, double* out)
{
    return gmb_synth_voice_read(voice, what, out);
}

int gmb_audio_status(int what, double* out)
{
    if (what == 4) { *out = (double)gmb_audio_dev_mode(); return 1; }
    return gmb_synth_status_read(what, out);
}

int gmb_audio_event(int index, int what, double* out)
{
    return gmb_synth_event_read(index, what, out);
}
