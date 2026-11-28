/* gmb_audio_dev.h — device audio (fase 1): il device È quello di SDL2_mixer.
 *
 * `Mix_HookMusic` gira per primo su un buffer che il mixer avrebbe riempito di silenzio:
 * il nostro motore ci scrive i campioni e `Mix_SetPostMix` resta per gli effetti. Se il
 * formato non è 44100/S16/2 (o il mixer non c'è) la modalità è OFFLINE: stesso motore,
 * buffer esplicito, nessun thread audio — così i test headless non aprono device.
 */
#ifndef GMB_AUDIO_DEV_H
#define GMB_AUDIO_DEV_H

#include <stdint.h>

#include "gmb_synth.h"

typedef struct gmb_mix_chunk gmb_mix_chunk;

int     gmb_audio_dev_mode(void);        /* 0 offline · 1 device (apre al primo uso) */
void    gmb_audio_dev_set_mode(int mode); /* 0 forza offline · 1 prova ad aprire */
int     gmb_audio_dev_is_open(void);

/* Comando verso il motore: device = coda SPSC drenata nel hook; offline = applicato subito. */
void    gmb_audio_dev_command(const gmb_synth_cmd* c);
/* Render esplicito: 0 in device (ci pensa il hook), `frames` in offline. */
int     gmb_audio_dev_render(int16_t* out, int frames);
/* Svuota la coda sul thread chiamante (offline: non serve, è già applicata). */
void    gmb_audio_dev_flush(void);

/* WAV = chunk del mixer (fase 1, D-2): con il device assente restano non disponibili. */
gmb_mix_chunk* gmb_audio_dev_load_wav(const char* path);
void    gmb_audio_dev_free_chunk(gmb_mix_chunk* chunk);
int     gmb_audio_dev_play_chunk(gmb_mix_chunk* chunk, int channel, int loops);
int     gmb_audio_dev_mix_playing(void);
void    gmb_audio_dev_set_channels_volume(int channel, int sdl_volume);
/* action: 0 halt · 1 pause · 2 resume; channel < 0 = tutti */
void    gmb_audio_dev_control(int action, int channel);
int     gmb_audio_dev_channel_limit(void);

#endif /* GMB_AUDIO_DEV_H */
