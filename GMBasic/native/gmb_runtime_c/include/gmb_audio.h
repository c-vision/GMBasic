/* ============================================================================
 * gmb_audio.h — playback audio del motore C (SDL_mixer) + filtro biquad.
 *
 * Le funzioni di sintesi/math sono in gmb_audio.c; qui la superficie usata dal
 * bridge C# (CGfxAudioService) in modalità motore C (GMB_GFX_C=1). Headless:
 * senza GMB_GFX_SDL_MIXER le funzioni sono no-op sicure (nessun device).
 * ============================================================================ */
#ifndef GMB_AUDIO_H
#define GMB_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SOUND freq, dur [, vol]: tono sul voice 0 (bloccante fino a fine tono). */
void gmb_audio_sound(int frequency, double duration, int volume);
/* SOUND [ON voice,] freq, dur[, vol[, wave[, att,dec,sus,rel[, pw]]]] — parità
 * AudioManager.SoundOnVoiceAsync: voice 0..7 (canale STABILE = voice), 6 forme d'onda,
 * ADSR in frazioni della durata, pulse width, filtro del voice. Bloccante con attesa
 * cooperativa (§0ter); headless ritorna subito (nessun device, nessuna attesa). */
void gmb_audio_sound_voice(double voice, double frequency, double duration, double volume,
                           double waveform, double attack, double decay, double sustain,
                           double release, double pulse_width);
/* PLAY "MML": sequenza musicale (bloccante). */
void gmb_audio_play_mml(const char* mml);

/* §0ter — varianti ASINCRONE, per i backend che parcheggiano al confine di un syscall (VM/C:
 * nessuna coroutine per il frame C). Avviano la riproduzione sul thread audio di SDL_mixer
 * (che non esegue BASIC) e ritornano SUBITO la durata da attendere in ms; chi chiama
 * parcheggia il task (sleep_until_ms + timer) e il worker resta libero. Semantica identica
 * alla variante bloccante: il task non prosegue prima che il suono sia finito.
 * Ritorno 0 = niente da attendere. I chunk del SOUND vengono liberati quando il canale
 * finisce (reap alla chiamata successiva: nessuna attesa, nessun thread in più).
 * gmb_audio_mml_async accoda la sequenza a UN sequencer dedicato e ritorna la durata
 * TOTALE da attendere (coda compresa), così il chiamante parcheggia una volta sola. */
int  gmb_audio_sound_async(int frequency, double duration, int volume);
int  gmb_audio_mml_async(const char* mml);

/* SOUND posizionale (P6/M-SND): un tono con guadagno e pan applicati ai CAMPIONI che
 * sintetizziamo — la tabella dlopen del mixer non cambia (nessun Mix_SetPanning).
 * `gain` 0..1 porta gia' attenuazione e volume; `pan` −1 sinistra .. +1 destra, lineare:
 * centro a piena ampiezza, gli estremi lasciano un solo canale. NON attende (effetto
 * one-shot): un effetto posizionale non blocca un worker GPM (§0ter); il chunk si libera
 * al SOUND posizionale successivo. Headless: nessun device, ritorna subito. */
void gmb_audio_sound_panned(int frequency, double duration, double gain, double pan);
/* Stessa cosa con lo slot P6 1..16: la voce è (slot-1) mod 8 (M-SND-VOICE), così due
 * sorgenti sugli slot 1 e 9 non si sovrascrivono per caso. */
void gmb_audio_sound_panned_slot(int slot, double frequency, double duration, double gain, double pan);
/* P6 live: aggiorna gain/pan della voce che lo slot POSSIEDE ancora (D-9). Non reclama
 * una voce libera e non migra: se lo slot ha perso la voce il comando non fa nulla. */
void gmb_audio_update_panned(int slot, double gain, double pan);

/* ── Fase 1: device + letture headless ────────────────────────────────────────────────
 * `gmb_audio_set_mode(0)` forza OFFLINE (buffer esplicito, nessun device): è ciò che
 * sostituisce il vecchio `setenv("SDL_AUDIODRIVER","dummy")` (D-6). Con il device il
 * motore suona sul hook del mixer; senza, i test chiamano `gmb_audio_render`. */
void gmb_audio_set_mode(int mode);       /* 0 = offline · 1 = prova il device */
int  gmb_audio_mode(void);               /* 0 = offline · 1 = device pronto */
/* Rende `frames` frame stereo S16 interleaved; ritorna i frame resi (0 col device:
 * ci pensa il hook). Avanza il clock e pubblica le letture. */
int  gmb_audio_render(int16_t* out, int frames);
/* AUDIO.VOICE(v, what): 0 picco 0..1, 1 RMS 0..1, 2 occupata 0/1, 3 freq, 4 campioni
 * rimanenti. -1 (e ritorno 0) per voce vuota o what non valido. */
int  gmb_audio_voice_read(int voice, int what, double* out);
/* AUDIO.STATUS(what): 0 clock campioni, 1 picco mix 0..1, 2 clip (per canale), 3 eventi
 * dall'ultimo RESET, 4 modalità (0 offline, 1 device). */
int  gmb_audio_status(int what, double* out);
/* AUDIO.EVENT(i, what): 0 start campioni, 1 freq, 2 durata campioni, 3 volume, 4 tipo
 * (0 nota, 1 pausa), 5 voce. */
int  gmb_audio_event(int index, int what, double* out);

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
