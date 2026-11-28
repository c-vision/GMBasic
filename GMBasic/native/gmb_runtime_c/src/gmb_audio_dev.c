/* gmb_audio_dev.c — il device audio È quello di SDL2_mixer (fase 1, D-3/D-4).
 *
 * Perché il hook. `Mix_HookMusic` riceve il buffer PRIMA del mix dei chunk: il mixer lo
 * riempirebbe di silenzio, quindi ci scriviamo il nostro render e i WAV (chunk) restano
 * mescolati sopra. Un solo device, un solo orologio, nessun thread nostro: il callback
 * gira sul thread audio di SDL, che non esegue BASIC (§0ter).
 *
 * Perché la coda. Il thread BASIC produce comandi, il thread audio consuma: SPSC, un
 * produttore e un consumatore, nessun lock nel callback. In OFFLINE la coda non serve (un
 * solo thread): il comando è applicato subito e il render è esplicito.
 *
 * Offline NON è un ripiego silenzioso: se il formato non è 44100/S16/2 il device non è
 * utilizzabile per il nostro render (Mix_QuerySpec lo dice) e si passa al buffer esplicito.
 */
#include "gmb_runtime.h"
#include "gmb_audio_dev.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct gmb_mix_chunk gmb_mix_chunk;   /* opaco (era Mix_Chunk) */

typedef struct {
    int  (*OpenAudio)(int freq, uint16_t format, int channels, int chunksize);
    int  (*OpenAudioDevice)(const char* device, int freq, uint16_t format, int channels,
                            int chunksize, int allowed_changes);
    int  (*QuerySpec)(int* freq, uint16_t* format, int* channels);
    void (*CloseAudio)(void);
    void (*HookMusic)(void (*mix_func)(void* udata, uint8_t* stream, int len), void* arg);
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
#define GMB_AUDIO_S16SYS 0x8010   /* AUDIO_S16SYS di SDL2 (little-endian signed 16 bit) */
#define GMB_AUDIO_FORMAT_OK 1     /* 0 = offline */

static gmb_mix_api g_mix;
static int g_mix_state;           /* 0 = non tentato · 1 = pronto · -1 = assente/errore */
#if defined(_WIN32) || defined(_WIN64)
static HMODULE g_mix_handle;
#else
static void* g_mix_handle;
#endif

static int g_mode = -1;           /* -1 = da decidere · 0 = offline · 1 = device */

/* ── coda SPSC: produttore = thread BASIC, consumatore = thread audio ── */
#define GMB_DEV_QUEUE 256
static gmb_synth_cmd g_queue[GMB_DEV_QUEUE];
static volatile unsigned g_q_head, g_q_tail;

static void* gmb_mix_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
    return (void*)GetProcAddress(g_mix_handle, name);
#else
    return dlsym(g_mix_handle, name);
#endif
}

static void dev_drain(void)
{
    while (g_q_head != g_q_tail)
    {
        gmb_synth_apply(&g_queue[g_q_head]);
        g_q_head = (g_q_head + 1) % GMB_DEV_QUEUE;
    }
}

static void dev_hook(void* udata, uint8_t* stream, int len)
{
    (void)udata;
    dev_drain();
    gmb_synth_render((int16_t*)stream, len / 4);   /* S16 stereo: 4 byte per frame */
}

/* Carica il mixer e apre il device. 1 = device pronto, 0 = offline. */
static int dev_open(void)
{
    if (g_mix_state == 1)
    {
        /* "pronto" solo se il mixer risponde davvero: uno stato vecchio non deve far
         * dire "device" con il mixer chiuso (sarebbe silenzio, non audio). */
        int freq = 0, channels = 0;
        uint16_t format = 0;
        if (g_mix.QuerySpec && g_mix.QuerySpec(&freq, &format, &channels)) return 1;
        g_mix_state = 0;   /* chiuso da altri: si riapre sotto */
    }
    if (g_mix_state == -1) return 0;

#if defined(_WIN32) || defined(_WIN64)
    static const char* names[] = { "SDL2_mixer.dll", "SDL2_mixer-2.0.dll" };
#elif defined(__APPLE__)
    static const char* names[] = { "libSDL2_mixer.dylib", "libSDL2_mixer-2.0.0.dylib",
                                   "/usr/local/lib/libSDL2_mixer.dylib" };
#else
    static const char* names[] = { "libSDL2_mixer.so.0", "libSDL2_mixer.so", "libSDL2_mixer-2.0.so.0" };
#endif
    char cache[2048];
    char in_cache[2400];
    cache[0] = '\0';
    gmb_paths_libs_cache_dir(cache, sizeof(cache));
    in_cache[0] = '\0';
    if (cache[0])
    {
#if defined(_WIN32) || defined(_WIN64)
        snprintf(in_cache, sizeof(in_cache), "%s/win-x64/SDL2_mixer.dll", cache);
#elif defined(__APPLE__)
        snprintf(in_cache, sizeof(in_cache), "%s/osx-arm64/libSDL2_mixer.dylib", cache);
#else
        snprintf(in_cache, sizeof(in_cache), "%s/linux-x64/libSDL2_mixer.so.0", cache);
#endif
    }

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) + 1; i++)
    {
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
    if (!g_mix_handle)
    {
        fprintf(stderr, "[audio] SDL2_mixer non disponibile: audio OFFLINE "
                        "(render esplicito, nessun device).\n");
        g_mix_state = -1;
        return 0;
    }

    g_mix.OpenAudio       = (int (*)(int, uint16_t, int, int))gmb_mix_sym("Mix_OpenAudio");
    g_mix.OpenAudioDevice = (int (*)(const char*, int, uint16_t, int, int, int))gmb_mix_sym("Mix_OpenAudioDevice");
    g_mix.QuerySpec       = (int (*)(int*, uint16_t*, int*))gmb_mix_sym("Mix_QuerySpec");
    g_mix.CloseAudio      = (void (*)(void))gmb_mix_sym("Mix_CloseAudio");
    g_mix.HookMusic       = (void (*)(void (*)(void*, uint8_t*, int), void*))gmb_mix_sym("Mix_HookMusic");
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

    /* Il render è nostro: senza HookMusic il device non può funzionare (fase 1). */
    if (!g_mix.HookMusic || !g_mix.QuerySpec || !g_mix.PlayChannel)
    {
        fprintf(stderr, "[audio] SDL2_mixer incompleta (HookMusic/QuerySpec): audio OFFLINE.\n");
        g_mix_state = -1;
        return 0;
    }

    int rc = -1;
    if (g_mix.OpenAudioDevice)
    {
        /* 512 sample è il blocco che il hook vuole, ma non ogni device lo concede senza
         * cambi di formato: si riprova con 1024 prima di dichiarare l'audio offline. */
        rc = g_mix.OpenAudioDevice(NULL, GMB_SYNTH_RATE, GMB_AUDIO_S16SYS, 2, 512, 0);
        if (rc != 0)
            rc = g_mix.OpenAudioDevice(NULL, GMB_SYNTH_RATE, GMB_AUDIO_S16SYS, 2, 1024, 0);
    }
    if (rc != 0 && g_mix.OpenAudio)
        rc = g_mix.OpenAudio(GMB_SYNTH_RATE, GMB_AUDIO_S16SYS, 2, 1024);
    if (rc != 0)
    {
        fprintf(stderr, "[audio] apertura device fallita: audio OFFLINE.\n");
        g_mix_state = -1;
        return 0;
    }

    int freq = 0, channels = 0;
    uint16_t format = 0;
    if (!g_mix.QuerySpec(&freq, &format, &channels) ||
        freq != GMB_SYNTH_RATE || format != GMB_AUDIO_S16SYS || channels != 2)
    {
        /* Formato diverso: il nostro render scrive S16 stereo a 44100. Niente conversione
         * nel callback (non è il lavoro di questo passo): si va offline. */
        fprintf(stderr, "[audio] device %d Hz fmt %04x ch %d ≠ 44100/S16/2: audio OFFLINE.\n",
                freq, (unsigned)format, channels);
        if (g_mix.CloseAudio) g_mix.CloseAudio();
        g_mix_state = -1;
        return 0;
    }

    if (g_mix.AllocateChannels) g_mix.AllocateChannels(16);
    g_mix.HookMusic(dev_hook, NULL);
    g_mix_state = 1;
    return 1;
}

/* Chiude il device E azzera lo stato del mixer: dopo la chiusura non esiste più un
 * "device pronto", altrimenti il set_mode(1) successivo crederebbe di essere aperto
 * (hook staccato, mixer chiuso, modalità device: silenzio). */
static void dev_close(void)
{
    if (g_mix_state != 1) return;
    if (g_mix.HookMusic) g_mix.HookMusic(NULL, NULL);
    if (g_mix.CloseAudio) g_mix.CloseAudio();
    g_mix_state = 0;   /* riaperto alla prossima richiesta di device */
}

int gmb_audio_dev_is_open(void) { return g_mix_state == 1 && g_mode == 1; }

int gmb_audio_dev_mode(void)
{
    if (g_mode < 0) g_mode = dev_open();
    return g_mode;
}

void gmb_audio_dev_set_mode(int mode)
{
    if (mode == 0)
    {
        dev_close();
        g_mode = 0;
        return;
    }
    /* Riapertura vera: via lo stato del mixer prima, così non si ricade in un
     * "pronto" che non corrisponde a nessun device aperto. */
    dev_close();
    g_mode = dev_open();
}

void gmb_audio_dev_command(const gmb_synth_cmd* c)
{
    if (!c) return;
    /* Il device si apre quando c'è qualcosa da SUONARE, non per un comando di stato:
     * `FILTER 0, 0, …` non deve caricare il mixer (i MiniTest muti restano muti). */
    if (gmb_audio_dev_is_open())
    {
        unsigned next = (g_q_tail + 1) % GMB_DEV_QUEUE;
        if (next == g_q_head) { gmb_synth_apply(c); return; }   /* coda piena: mai perdere il comando */
        g_queue[g_q_tail] = *c;
        g_q_tail = next;
        return;
    }
    gmb_synth_apply(c);
}

int gmb_audio_dev_render(int16_t* out, int frames)
{
    if (gmb_audio_dev_is_open()) return 0;   /* col device rende il hook */
    gmb_synth_render(out, frames);
    return frames;
}

void gmb_audio_dev_flush(void) { dev_drain(); }

/* ── WAV: chunk del mixer (fase 1, D-2) ── */
gmb_mix_chunk* gmb_audio_dev_load_wav(const char* path)
{
    if (gmb_audio_dev_mode() != 1 || !g_mix.LoadWAV || !path || !*path) return NULL;
    return g_mix.LoadWAV(path);
}

void gmb_audio_dev_free_chunk(gmb_mix_chunk* chunk)
{
    if (chunk && g_mix.FreeChunk) g_mix.FreeChunk(chunk);
}

int gmb_audio_dev_play_chunk(gmb_mix_chunk* chunk, int channel, int loops)
{
    if (!chunk || gmb_audio_dev_mode() != 1 || !g_mix.PlayChannel) return -1;
    if (loops < 0) loops = -1;
    return g_mix.PlayChannel(channel < 0 ? -1 : channel, chunk, loops);
}

int gmb_audio_dev_mix_playing(void)
{
    if (!gmb_audio_dev_is_open() || !g_mix.Playing) return 0;
    return g_mix.Playing(-1) > 0 ? 1 : 0;
}

void gmb_audio_dev_set_channels_volume(int channel, int sdl_volume)
{
    if (!gmb_audio_dev_is_open() || !g_mix.Volume) return;
    if (sdl_volume < 0) sdl_volume = 0;
    if (sdl_volume > GMB_MIX_MAX_VOLUME) sdl_volume = GMB_MIX_MAX_VOLUME;
    if (channel >= 0) { g_mix.Volume(channel, sdl_volume); return; }
    int nch = g_mix.AllocateChannels ? g_mix.AllocateChannels(-1) : 16;
    for (int ch = 0; ch < nch; ch++) g_mix.Volume(ch, sdl_volume);
}

void gmb_audio_dev_control(int action, int channel)
{
    if (!gmb_audio_dev_is_open()) return;
    int ch = channel < 0 ? -1 : channel;
    switch (action)
    {
    case 0: if (g_mix.HaltChannel) g_mix.HaltChannel(ch); break;
    case 1: if (g_mix.Pause) g_mix.Pause(ch); break;
    case 2: if (g_mix.Resume) g_mix.Resume(ch); break;
    default: break;   /* 3 = clear queue: il motore C non ha coda di playback */
    }
}

int gmb_audio_dev_channel_limit(void)
{
    if (gmb_audio_dev_mode() != 1 || !g_mix.AllocateChannels) return 0;
    return g_mix.AllocateChannels(-1);
}
