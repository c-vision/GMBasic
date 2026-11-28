/* gmb_audio.c — parità CommonLib/AudioUtils.cs:86 (5 funzioni math, no SDL dependency)
 * Implementa: CalculateEnvelopeADSR, CalculateEnvelope, CalculateFrequency,
 *             MidiNoteToFrequency, CalculateNoteDuration
 * SdlAudioService/AudioManager (SDL_mixer) restano host-only: audio playback fuori
 * dal runtime C per design (vedi Knowledge/...Parity-Report.md §3.2).
 */
#include "gmb_runtime.h"
#include "gmb_audio.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

/* ---- 100% parity AudioManager.cs:398 CreateWaveformChunk + ParseMusicString ----
 * Synth PCM puro (testabile headless) + playback SDL_mixer opzionale (GMB_GFX_SDL_MIXER). */

#define GMB_AUDIO_RATE 44100

/* Genera PCM mono (16-bit) con waveform/ADSR/biquad. Ritorna il numero di sample. */
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

/* MML parse (AudioManager.cs:839 ParseMusicString): O/L/T/P/V/N/>/< + note.
 * Ritorna numero di comandi; out_freq/out_dur coppie, out_type 0=nota 1=pausa.
 *
 * `out_vol` (puo' essere NULL) riceve il volume V corrente del comando (0..100): vale per
 * le note SUCCESSIVE. Prima V veniva letto e poi buttato — il volume MML non aveva effetto.
 * `N<n>` e' una nota MIDI assoluta 0..127 (ignora l'ottava, come AudioUtils.MidiNoteToFrequency
 * nel C#); `L` accetta PIU' cifre: prima si leggeva una sola cifra e "L16" suonava come L1. */
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
            if (i == 0) basef = 261.63; else if (i == 1) basef = 293.66; else if (i == 2) basef = 329.63;
            else if (i == 3) basef = 349.23; else if (i == 4) basef = 392.00; else if (i == 5) basef = 440.00;
            else basef = 493.88;
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

/* ── SDL2_mixer CARICATO A RUNTIME (dlopen), non linkato ────────────────────────
 * Prima `gmb_runtime_deps` (INTERFACE) linkava SDL2_mixer: OGNI dylib costruita dal
 * runtime ereditava mixer + la sua catena (FLAC, ogg/opus/vorbis, sndfile, fluidsynth,
 * mpg123, wavpack, xmp, glib, portaudio, …): ~18 librerie e ~15 MB per un programma che
 * non fa audio. Ora il mixer è opzionale: si apre al PRIMO uso e, se manca, l'audio è
 * disabilitato con un avviso ESPLICITO (una volta), mai in silenzio. */
#include <stdint.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct gmb_mix_chunk gmb_mix_chunk;   /* opaco (era Mix_Chunk) */
typedef struct {
    int  (*OpenAudio)(int freq, uint16_t format, int channels, int chunksize);
    int  (*AllocateChannels)(int num);
    gmb_mix_chunk* (*QuickLoadRAW)(unsigned char* mem, unsigned int len);
    int  (*PlayChannel)(int channel, gmb_mix_chunk* chunk, int loops);
    int  (*Playing)(int channel);
    void (*FreeChunk)(gmb_mix_chunk* chunk);
    gmb_mix_chunk* (*LoadWAV)(const char* file);
    int  (*HaltChannel)(int channel);
    int  (*Pause)(int channel);
    int  (*Resume)(int channel);
    int  (*Volume)(int channel, int volume);
} gmb_mix_api;

#define GMB_MIX_MAX_VOLUME 128
#define GMB_AUDIO_S16SYS 0x8010   /* = AUDIO_S16SYS di SDL2 (little-endian signed 16 bit) */

static gmb_mix_api g_mix;
static int g_mix_state;           /* 0 = non tentato · 1 = pronto · -1 = assente/errore */
#if defined(_WIN32) || defined(_WIN64)
static HMODULE g_mix_handle;
#else
static void* g_mix_handle;
#endif

static void* gmb_mix_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
    return (void*)GetProcAddress(g_mix_handle, name);
#else
    return dlsym(g_mix_handle, name);
#endif
}

/* Nomi provati, in ordine: cache on-demand (nostra), poi le dir di sistema. */
static int gmb_mix_ensure(void)
{
    if (g_mix_state != 0) return g_mix_state;

#if defined(_WIN32) || defined(_WIN64)
    static const char* names[] = { "SDL2_mixer.dll", "SDL2_mixer-2.0.dll" };
#elif defined(__APPLE__)
    static const char* names[] = { "libSDL2_mixer.dylib", "libSDL2_mixer-2.0.0.dylib",

                                   "/usr/local/lib/libSDL2_mixer.dylib" };
#else
    static const char* names[] = { "libSDL2_mixer.so.0", "libSDL2_mixer.so", "libSDL2_mixer-2.0.so.0" };
#endif
    char cache[2048];
    gmb_paths_libs_cache_dir(cache, sizeof(cache));
    char in_cache[2400];
    in_cache[0] = '\0';
    if (cache[0]) {
#if defined(_WIN32) || defined(_WIN64)
        snprintf(in_cache, sizeof(in_cache), "%s/win-x64/SDL2_mixer.dll", cache);
#elif defined(__APPLE__)
        snprintf(in_cache, sizeof(in_cache), "%s/osx-arm64/libSDL2_mixer.dylib", cache);
#else
        snprintf(in_cache, sizeof(in_cache), "%s/linux-x64/libSDL2_mixer.so.0", cache);
#endif
    }

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) + 1; i++) {
        const char* candidate = (i == 0 && in_cache[0]) ? in_cache : names[i - 1];
        if (!candidate || !*candidate) continue;
#if defined(_WIN32) || defined(_WIN64)
        g_mix_handle = LoadLibraryA(candidate);
        if (g_mix_handle) break;
#else
        g_mix_handle = dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
        if (g_mix_handle) break;
#endif
    }
    if (!g_mix_handle) {
        fprintf(stderr, "[audio] SDL2_mixer non disponibile: playback disabilitato "
                        "(synth PCM e funzioni matematiche attive).\n");
        g_mix_state = -1;
        return g_mix_state;
    }

    g_mix.OpenAudio       = (int (*)(int, uint16_t, int, int))gmb_mix_sym("Mix_OpenAudio");
    g_mix.AllocateChannels = (int (*)(int))gmb_mix_sym("Mix_AllocateChannels");
    g_mix.QuickLoadRAW    = (gmb_mix_chunk* (*)(unsigned char*, unsigned int))gmb_mix_sym("Mix_QuickLoad_RAW");
    g_mix.PlayChannel     = (int (*)(int, gmb_mix_chunk*, int))gmb_mix_sym("Mix_PlayChannel");
    g_mix.Playing         = (int (*)(int))gmb_mix_sym("Mix_Playing");
    g_mix.FreeChunk       = (void (*)(gmb_mix_chunk*))gmb_mix_sym("Mix_FreeChunk");
    g_mix.LoadWAV         = (gmb_mix_chunk* (*)(const char*))gmb_mix_sym("Mix_LoadWAV");
    g_mix.HaltChannel     = (int (*)(int))gmb_mix_sym("Mix_HaltChannel");
    g_mix.Pause           = (int (*)(int))gmb_mix_sym("Mix_Pause");
    g_mix.Resume          = (int (*)(int))gmb_mix_sym("Mix_Resume");
    g_mix.Volume          = (int (*)(int, int))gmb_mix_sym("Mix_Volume");

    if (!g_mix.OpenAudio || !g_mix.PlayChannel || !g_mix.Playing) {
        fprintf(stderr, "[audio] SDL2_mixer incompleta (simboli mancanti): playback disabilitato.\n");
        g_mix_state = -1;
        return g_mix_state;
    }
    if (g_mix.OpenAudio(44100, GMB_AUDIO_S16SYS, 2, 1024) == 0) {
        if (g_mix.AllocateChannels) g_mix.AllocateChannels(16);
        g_mix_state = 1;
    } else {
        fprintf(stderr, "[audio] Mix_OpenAudio fallita: playback disabilitato.\n");
        g_mix_state = -1;
    }
    return g_mix_state;
}

/* ── Filtro biquad per voice (parità AudioManager.SetVoiceFilter + biquad IIR) ── */
#define GMB_AUDIO_MAX_VOICES 8
typedef struct { int type; double cutoff, res; } gmb_audio_filter_t;
static gmb_audio_filter_t g_filters[GMB_AUDIO_MAX_VOICES];
static int g_filter_set[GMB_AUDIO_MAX_VOICES];

void gmb_audio_set_filter(int voice, int type, double cutoff, double resonance)
{
    if (voice < 0) voice = 0;
    if (voice >= GMB_AUDIO_MAX_VOICES) voice = GMB_AUDIO_MAX_VOICES - 1;
    if (type < 0) type = 0;
    if (type > 3) type = 3;
    if (cutoff < 20.0) cutoff = 20.0;
    if (cutoff > 20000.0) cutoff = 20000.0;
    if (resonance < 0.0) resonance = 0.0;
    if (resonance > 95.0) resonance = 95.0;
    g_filters[voice].type = type;
    g_filters[voice].cutoff = cutoff;
    g_filters[voice].res = resonance / 100.0;
    g_filter_set[voice] = 1;
}

static void apply_biquad(double* buf, int n, const gmb_audio_filter_t* f)
{
    if (!buf || n <= 0 || !f || f->type <= 0) return;
    double w0 = 2.0 * 3.14159265358979323846 * f->cutoff / (double)GMB_AUDIO_RATE;
    double cosw = cos(w0), sinw = sin(w0);
    double q = 0.5 + f->res * 9.5;
    double alpha = sinw / (2.0 * q);
    double b0, b1, b2, a0, a1, a2;
    if (f->type == 1) { b0 = (1 - cosw) / 2; b1 = 1 - cosw; b2 = (1 - cosw) / 2; }
    else if (f->type == 2) { b0 = (1 + cosw) / 2; b1 = -(1 + cosw); b2 = (1 + cosw) / 2; }
    else { b0 = sinw / 2; b1 = 0; b2 = -sinw / 2; }
    a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (int i = 0; i < n; i++)
    {
        double x0 = buf[i];
        double y0 = (b0 / a0) * x0 + (b1 / a0) * x1 + (b2 / a0) * x2
                  - (a1 / a0) * y1 - (a2 / a0) * y2;
        x2 = x1; x1 = x0; y2 = y1; y1 = y0;
        buf[i] = y0;
    }
}

/* ── Chunk caricati (SOUND LOAD) ── */
#define GMB_AUDIO_MAX_CHUNKS 64
static gmb_mix_chunk* g_chunks[GMB_AUDIO_MAX_CHUNKS]; /* handle 1..63 (0 = errore) */

/* Un tono avviato sul mixer: il CHUNK e il PCM da cui è stato costruito. Mix_QuickLoad_RAW
 * NON copia il buffer, quindi il PCM vive quanto il chunk (va liberato insieme). */
typedef struct {
    gmb_mix_chunk* chunk;
    int16_t* pcm;
    int channel;          /* canale scelto dal mixer; -1 = non avviato */
} gmb_audio_voice_t;

/* Sintetizza e AVVIA il tono: NON attende e non libera nulla (lo fa il chiamante, o il reap
 * delle varianti asincrone). Ritorna 1 se il tono è partito. */
static int gmb_audio_start_tone(int frequency, double duration, int volume, gmb_audio_voice_t* out)
{
    out->chunk = NULL; out->pcm = NULL; out->channel = -1;
    if (gmb_mix_ensure() != 1 || frequency <= 0 || duration <= 0) return 0;
    int samples = (int)(GMB_AUDIO_RATE * duration);
    if (samples <= 0) return 0;
    double* buf = (double*)malloc((size_t)samples * sizeof(double));
    if (!buf) return 0;
    int n = gmb_audio_synth(frequency, duration, 0, 0.01, 0.0, 1.0, 0.02, 0.25, 0, 0, 0, buf, samples);
    if (g_filter_set[0]) apply_biquad(buf, n, &g_filters[0]); /* SOUND usa il voice 0 */
    int16_t* pcm = (int16_t*)malloc((size_t)n * sizeof(int16_t) * 2);
    if (!pcm) { free(buf); return 0; }
    for (int i = 0; i < n; i++) { int16_t v = (int16_t)(buf[i] * (volume > 0 ? volume / 100.0 : 1.0)); if (v > 32767) v = 32767; if (v < -32768) v = -32768; pcm[i*2] = v; pcm[i*2+1] = v; }
    free(buf);
    gmb_mix_chunk* ch = g_mix.QuickLoadRAW ? g_mix.QuickLoadRAW((unsigned char*)pcm, (unsigned int)(n * 4)) : NULL;
    if (!ch) { free(pcm); return 0; }
    int channel = g_mix.PlayChannel(-1, ch, 0);
    if (channel < 0) { if (g_mix.FreeChunk) g_mix.FreeChunk(ch); free(pcm); return 0; }
    out->chunk = ch; out->pcm = pcm; out->channel = channel;
    return 1;
}

void gmb_audio_sound(int frequency, double duration, int volume)
{
    gmb_audio_voice_t v;
    if (!gmb_audio_start_tone(frequency, duration, volume, &v)) return;
    /* §0ter: attesa cooperativa — dentro un task GPM il worker viene liberato (C99: park della
     * coroutine). Chi non sa parcheggiare usa gmb_audio_sound_async + park del task. */
    while (g_mix.Playing(-1) > 0) gmb_gpm_sleep(10);
    if (g_mix.FreeChunk) g_mix.FreeChunk(v.chunk);
    free(v.pcm);
}

void gmb_audio_play_mml(const char* mml)
{
    if (!mml) return;
    double freq[256], dur[256]; int type[256], vol[256];
    int n = gmb_audio_mml_parse(mml, freq, dur, type, vol, 256);
    for (int i = 0; i < n; i++) {
        /* Timbro delle note MML: il preset di AudioManager.SoundOnVoiceAsync
         * (voice 0, wave 0, A 10%, D 0, S 100%, R 20%, pw 25%) e il volume V della
         * sequenza — prima V era ignorato e le note suonavano sempre a 100. */
        if (type[i] == 0) gmb_audio_sound_voice(0, freq[i], dur[i], vol[i], 0, 0.10, 0.0, 1.0, 0.20, 0.25);
        else gmb_gpm_sleep((int)(dur[i] * 1000));   /* §0ter: park, non worker bloccato */
    }
}

int gmb_audio_load_sound(const char* path)
{
    if (gmb_mix_ensure() != 1 || !path || !*path || !g_mix.LoadWAV) return 0;
    gmb_mix_chunk* ch = g_mix.LoadWAV(path);
    if (!ch) return 0;
    for (int i = 1; i < GMB_AUDIO_MAX_CHUNKS; i++)
    {
        if (!g_chunks[i]) { g_chunks[i] = ch; return i; }
    }
    if (g_mix.FreeChunk) g_mix.FreeChunk(ch);
    return 0;
}

void gmb_audio_play_sound(int handle, int channel, int loops)
{
    if (handle <= 0 || handle >= GMB_AUDIO_MAX_CHUNKS || !g_chunks[handle]) return;
    if (gmb_mix_ensure() != 1 || !g_mix.PlayChannel) return;
    if (loops < 0) loops = -1;
    g_mix.PlayChannel(channel < 0 ? -1 : channel, g_chunks[handle], loops);
}

void gmb_audio_set_volume(int volume, int channel)
{
    if (gmb_mix_ensure() != 1 || !g_mix.Volume) return;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    int sdl_vol = volume * GMB_MIX_MAX_VOLUME / 100;
    if (channel >= 0) { g_mix.Volume(channel, sdl_vol); return; }
    int nch = g_mix.AllocateChannels ? g_mix.AllocateChannels(-1) : 16;
    for (int ch = 0; ch < nch; ch++) g_mix.Volume(ch, sdl_vol);
}

void gmb_audio_control(int action, int channel)
{
    if (gmb_mix_ensure() != 1) return;
    int ch = channel < 0 ? -1 : channel;
    switch (action)
    {
        case 0: if (g_mix.HaltChannel) g_mix.HaltChannel(ch); break;
        case 1: if (g_mix.Pause) g_mix.Pause(ch); break;
        case 2: if (g_mix.Resume) g_mix.Resume(ch); break;
        default: break; /* 3 = clear queue: il motore C non ha coda (PLAY bloccante) */
    }
}

int gmb_audio_is_playing(void)
{
    if (gmb_mix_ensure() != 1 || !g_mix.Playing) return 0;
    return g_mix.Playing(-1) > 0 ? 1 : 0;
}

/* ── SOUND posizionale (P6/M-SND) ───────────────────────────────────────────────────────
 * Il pan NON passa dal mixer (nessun Mix_SetPanning nella tabella dlopen dei simboli): si
 * applica ai campioni che sintetizziamo, con un pan LINEARE — centro a piena ampiezza, gli
 * estremi lasciano un solo canale. `gain` 0..1 porta gia' l'attenuazione e il volume.
 *
 * NON attende: e' un effetto one-shot, non uno statement bloccante. La §0ter vieta di
 * dormire dentro un worker GPM (VM/C: nessuna coroutine per il frame C del syscall), e un
 * effetto posizionale non deve comunque fermare il gioco. Il chunk si libera al primo
 * SOUND posizionale successivo in cui il canale ha finito (spazi finiti: 8 toni
 * sovrapposti; oltre, si perde la sola referenza al buffer di quel tono, come fa gia' il
 * reap delle varianti asincrone). Senza device (headless) ritorna subito. */
#define GMB_AUDIO_PAN_SLOTS 8
static struct { gmb_mix_chunk* chunk; int16_t* pcm; int channel; } g_pan_play[GMB_AUDIO_PAN_SLOTS];

/* Libera i toni posizionali gia' finiti: nessuna attesa, nessun thread di servizio. */
static void gmb_audio_pan_reap(void)
{
    if (!g_mix.Playing) return;
    for (int i = 0; i < GMB_AUDIO_PAN_SLOTS; i++)
    {
        if (!g_pan_play[i].chunk) continue;
        if (g_mix.Playing(g_pan_play[i].channel) > 0) continue;
        if (g_mix.FreeChunk) g_mix.FreeChunk(g_pan_play[i].chunk);
        free(g_pan_play[i].pcm);
        g_pan_play[i].chunk = NULL; g_pan_play[i].pcm = NULL; g_pan_play[i].channel = -1;
    }
}

void gmb_audio_sound_panned(int frequency, double duration, double gain, double pan)
{
    if (gmb_mix_ensure() != 1 || frequency <= 0 || duration <= 0) return;
    if (!(gain > 0.0)) return;
    gmb_audio_pan_reap();
    if (gain > 1.0) gain = 1.0;
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;
    int samples = (int)(GMB_AUDIO_RATE * duration);
    if (samples <= 0) return;
    double* buf = (double*)malloc((size_t)samples * sizeof(double));
    if (!buf) return;
    int n = gmb_audio_synth(frequency, duration, 0, 0.01, 0.0, 1.0, 0.02, 0.25, 0, 0, 0, buf, samples);
    if (n <= 0) { free(buf); return; }
    if (g_filter_set[0]) apply_biquad(buf, n, &g_filters[0]);   /* il voice 0 di SOUND */
    int16_t* pcm = (int16_t*)malloc((size_t)n * sizeof(int16_t) * 2);
    if (!pcm) { free(buf); return; }
    /* Pan lineare: a pan 0 entrambi i canali a piena ampiezza (nessuna perdita al centro). */
    double gl = pan > 0.0 ? 1.0 - pan : 1.0;
    double gr = pan < 0.0 ? 1.0 + pan : 1.0;
    for (int i = 0; i < n; i++)
    {
        double v = buf[i] * gain;
        double l = v * gl * 32767.0, r = v * gr * 32767.0;
        if (l > 32767.0) l = 32767.0; if (l < -32768.0) l = -32768.0;
        if (r > 32767.0) r = 32767.0; if (r < -32768.0) r = -32768.0;
        pcm[i * 2] = (int16_t)l;
        pcm[i * 2 + 1] = (int16_t)r;
    }
    free(buf);
    gmb_mix_chunk* ch = g_mix.QuickLoadRAW
        ? g_mix.QuickLoadRAW((unsigned char*)pcm, (unsigned int)(n * 4)) : NULL;
    if (!ch) { free(pcm); return; }
    int channel = g_mix.PlayChannel(-1, ch, 0);
    if (channel < 0)
    {
        if (g_mix.FreeChunk) g_mix.FreeChunk(ch);
        free(pcm);
        return;
    }
    for (int i = 0; i < GMB_AUDIO_PAN_SLOTS; i++)
    {
        if (g_pan_play[i].chunk) continue;
        g_pan_play[i].chunk = ch;
        g_pan_play[i].pcm = pcm;
        g_pan_play[i].channel = channel;
        return;
    }
    /* Tutti i posti occupati: il chunk e' IN RIPRODUZIONE, liberarlo ora sarebbe un
     * use-after-free nel mixer. Si perde la referenza (nessun leak di canali). */
}

/* ── SOUND con voice, timbro e ADSR (parità AudioManager.SoundOnVoiceAsync) ─────────────
 * Sintassi: SOUND [ON voice,] freq, dur[, vol[, wave[, att, dec, sus, rel[, pw]]]].
 * Prima la leg C conosceva solo il preset (wave 0, A 1%, R 2%) e il voice era un'etichetta
 * ignorata: il canale lo sceglieva il mixer (Mix_PlayChannel(-1,…)). Qui il canale E' il
 * voice (0..7), come nel C#, quindi due SOUND sullo stesso voice si sovrascrivono invece di
 * accumularsi, e il filtro impostato con FILTER su quel voice viene applicato al tono.
 *
 * Attesa cooperativa (§0ter): il worker GPM viene parcheggiato mentre il tono suona. Senza
 * device (headless) ritorna SUBITO: un programma di test non paga la durata del tono. */
void gmb_audio_sound_voice(double voice_d, double frequency, double duration, double volume_d,
                           double waveform_d, double attack, double decay, double sustain,
                           double release, double pulse_width)
{
    int voice = (int)voice_d;
    int volume = (int)volume_d;
    int waveform = (int)waveform_d;
    if (voice < 0) voice = 0;
    if (voice >= GMB_AUDIO_MAX_VOICES) voice = GMB_AUDIO_MAX_VOICES - 1;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (waveform < 0) waveform = 0;
    if (waveform > 5) waveform = 5;
    if (attack < 0.0) attack = 0.0;   if (attack > 1.0) attack = 1.0;
    if (decay < 0.0) decay = 0.0;     if (decay > 1.0) decay = 1.0;
    if (sustain < 0.0) sustain = 0.0; if (sustain > 1.0) sustain = 1.0;
    if (release < 0.0) release = 0.0; if (release > 1.0) release = 1.0;
    if (pulse_width < 0.01) pulse_width = 0.01;
    if (pulse_width > 0.99) pulse_width = 0.99;
    if (frequency <= 0 || duration <= 0) return;
    if (gmb_mix_ensure() != 1) return;   /* headless: nessun device, nessuna attesa */

    int samples = (int)(GMB_AUDIO_RATE * duration);
    if (samples <= 0) return;
    double* buf = (double*)malloc((size_t)samples * sizeof(double));
    if (!buf) return;
    int n = gmb_audio_synth((int)frequency, duration, waveform, attack, decay, sustain, release,
                            pulse_width, 0, 0, 0, buf, samples);
    if (n <= 0) { free(buf); return; }
    if (g_filter_set[voice]) apply_biquad(buf, n, &g_filters[voice]);

    int16_t* pcm = (int16_t*)malloc((size_t)n * sizeof(int16_t) * 2);
    if (!pcm) { free(buf); return; }
    double scale = volume / 100.0;
    for (int i = 0; i < n; i++)
    {
        double v = buf[i] * scale;
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        pcm[i * 2] = (int16_t)v;
        pcm[i * 2 + 1] = (int16_t)v;
    }
    free(buf);

    gmb_mix_chunk* ch = g_mix.QuickLoadRAW
        ? g_mix.QuickLoadRAW((unsigned char*)pcm, (unsigned int)(n * 4)) : NULL;
    if (!ch) { free(pcm); return; }
    int channel = g_mix.PlayChannel(voice, ch, 0);   /* canale stabile = voice */
    if (channel < 0)
    {
        if (g_mix.FreeChunk) g_mix.FreeChunk(ch);
        free(pcm);
        return;
    }
    while (g_mix.Playing(channel) > 0) gmb_gpm_sleep(10);
    if (g_mix.FreeChunk) g_mix.FreeChunk(ch);
    free(pcm);
}
