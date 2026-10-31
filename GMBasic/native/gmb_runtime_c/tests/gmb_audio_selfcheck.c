/* gmb_audio_selfcheck.c — playback audio C (SDL_mixer, driver dummy).
 * Copre: WAV load/play/stop, volume, filtro biquad, MML e stato. Headless. */
#include "gmb_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

/* WAV minimale: PCM 16-bit mono 44100 Hz, 200 campioni (onda quadra 440Hz). */
static int write_wav(const char* path)
{
    const int rate = 44100, samples = 200;
    const int data_size = samples * 2;
    unsigned char hdr[44];
    memcpy(hdr, "RIFF", 4);
    int chunk = 36 + data_size;
    hdr[4] = chunk & 0xFF; hdr[5] = (chunk >> 8) & 0xFF; hdr[6] = (chunk >> 16) & 0xFF; hdr[7] = (chunk >> 24) & 0xFF;
    memcpy(hdr + 8, "WAVEfmt ", 8);
    int fmt = 16; memcpy(hdr + 16, &fmt, 4);
    short audio_fmt = 1, channels = 1;
    memcpy(hdr + 20, &audio_fmt, 2); memcpy(hdr + 22, &channels, 2);
    memcpy(hdr + 24, &rate, 4);
    int byte_rate = rate * 2; memcpy(hdr + 28, &byte_rate, 4);
    short block_align = 2; memcpy(hdr + 32, &block_align, 2);
    short bits = 16; memcpy(hdr + 34, &bits, 2);
    memcpy(hdr + 36, "data", 4); memcpy(hdr + 40, &data_size, 4);
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(hdr, 1, 44, f);
    for (int i = 0; i < samples; i++)
    {
        short v = ((i / 25) % 2) ? 12000 : -12000;
        fwrite(&v, 2, 1, f);
    }
    fclose(f);
    return 0;
}

int main(void)
{
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    const char* wav = "/tmp/gmb_audio_selfcheck.wav";
    CHECK(write_wav(wav) == 0, "crea WAV");

    /* File inesistente → 0 (nessun crash). */
    CHECK(gmb_audio_load_sound("/tmp/gmb_audio_non_esiste.wav") == 0, "load file assente");

    int h = gmb_audio_load_sound(wav);
    CHECK(h > 0, "load WAV valido");
    int h2 = gmb_audio_load_sound(wav);
    CHECK(h2 > 0 && h2 != h, "handle distinto");

    gmb_audio_play_sound(h, -1, 0);
    gmb_audio_set_volume(50, -1);
    gmb_audio_set_volume(80, 0);
    gmb_audio_control(0, -1); /* stop */
    CHECK(gmb_audio_is_playing() == 0, "stop azzera il playback");

    /* Handle non valido → no-op senza crash. */
    gmb_audio_play_sound(0, -1, 0);
    gmb_audio_play_sound(63, -1, 0);

    /* Filtro + SOUND (bloccante, ~50ms) con filtro lowpass attivo. */
    gmb_audio_set_filter(0, 1, 1000, 50);
    gmb_audio_sound(440, 0.05, 50);
    gmb_audio_set_filter(0, 0, 1000, 0);
    gmb_audio_sound(440, 0.02, 50);

    /* MML breve (bloccante). */
    gmb_audio_play_mml("T400 L16 C D E");

    remove(wav);
    if (fails == 0) { printf("gmb_audio_selfcheck: OK\n"); return 0; }
    printf("gmb_audio_selfcheck: %d FAIL\n", fails);
    return 1;
}
