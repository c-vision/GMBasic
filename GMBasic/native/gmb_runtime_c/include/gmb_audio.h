/* ============================================================================
 * gmb_audio.h — playback audio del motore C (SDL_mixer) + filtro biquad.
 *
 * Le funzioni di sintesi/math sono in gmb_audio.c; qui la superficie usata dal
 * bridge C# (CGfxAudioService) in modalità motore C (GMB_GFX_C=1). Headless:
 * senza GMB_GFX_SDL_MIXER le funzioni sono no-op sicure (nessun device).
 * ============================================================================ */
#ifndef GMB_AUDIO_H
#define GMB_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* SOUND freq, dur [, vol]: tono sul voice 0 (bloccante fino a fine tono). */
void gmb_audio_sound(int frequency, double duration, int volume);
/* PLAY "MML": sequenza musicale (bloccante). */
void gmb_audio_play_mml(const char* mml);

/* SOUND posizionale (P6/M-SND): un tono con guadagno e pan applicati ai CAMPIONI che
 * sintetizziamo — la tabella dlopen del mixer non cambia (nessun Mix_SetPanning).
 * `gain` 0..1 porta gia' attenuazione e volume; `pan` −1 sinistra .. +1 destra, lineare:
 * centro a piena ampiezza, gli estremi lasciano un solo canale. NON attende (effetto
 * one-shot): un effetto posizionale non blocca un worker GPM (§0ter); il chunk si libera
 * al SOUND posizionale successivo. Headless: nessun device, ritorna subito. */
void gmb_audio_sound_panned(int frequency, double duration, double gain, double pan);

/* SOUND LOAD "file" AS #h: carica un WAV. Ritorna l'handle (>=1) o 0. */
int  gmb_audio_load_sound(const char* path);
/* SOUND PLAY #h [, canale] [, loop]: channel < 0 = canale libero. */
void gmb_audio_play_sound(int handle, int channel, int loops);
/* SOUND VOLUME n [, canale]: channel < 0 = tutti i canali. volume 0..100. */
void gmb_audio_set_volume(int volume, int channel);
/* AUDIO action [, arg]: 0=stop, 1=pausa, 2=riprendi (channel < 0 = tutti).
 * action 3 (clear queue) non ha equivalente nel motore C (PLAY è bloccante). */
void gmb_audio_control(int action, int channel);
/* SETFILTER voice, type, cutoff, resonance (0=Off,1=LowPass,2=HighPass,3=BandPass).
 * Il filtro è applicato al prossimo SOUND sul voice (biquad IIR). */
void gmb_audio_set_filter(int voice, int type, double cutoff, double resonance);
/* True se un canale sta suonando. */
int  gmb_audio_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_AUDIO_H */
