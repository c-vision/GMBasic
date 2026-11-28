/* gmb_synth.h — motore di sintesi a 8 voci (fase 1 dell'ADR-Audio-Confine-Synth).
 *
 * Perché un modulo a parte: il render è una FUNZIONE PURA di (stato, n) — la stessa che
 * i MiniTest chiamano offline e che il hook del device chiama a blocchi. Nessun device,
 * nessuna allocazione per campione, nessun lock: qui c'è solo lo stato delle voci.
 *
 * Contratto (ID della specifica `docs/Audio-Specifica-Comportamenti.md`):
 *   M-AUD-RATE/AMP/QUANT/BLOCK/DET  render 44100 stereo int16, quantizzazione lround
 *   M-AUD-VOICE/RETRIG/ALLOC        voce 0..7 = canale, retrigger uniforme
 *   M-AUD-ENV/WAVE/PW/NOISE/PHASE  inviluppo, 6 forme, rumore per voce, fase continua
 *   M-AUD-MIX/MASTER/PAN/CLIP      somma in double, master, pan lineare, contatore clip
 *   M-AUD-VOICE-READ/STATUS/EVENT  letture headless (snapshot seqlock + lista eventi)
 */
#ifndef GMB_SYNTH_H
#define GMB_SYNTH_H

#include <stdint.h>

#define GMB_SYNTH_VOICES 8
#define GMB_SYNTH_RATE   44100

/* Comandi applicati dal thread del motore (offline: subito; device: all'inizio del blocco). */
enum {
    GMB_SYN_NOTE_ON = 1,   /* f0 freq, f1 durata s, f2 volume 0..100, f3 wave, f4..f7 A D S R,
                            * f8 pulse width; `slot` 0 = non posizionale */
    GMB_SYN_STOP,          /* voice */
    GMB_SYN_PAUSE,         /* voice < 0 = tutte */
    GMB_SYN_RESUME,        /* voice < 0 = tutte */
    GMB_SYN_VOL_VOICE,     /* voice, f0 = volume 0..100 */
    GMB_SYN_VOL_MASTER,    /* f0 = volume 0..100 (master) */
    GMB_SYN_PAN,           /* voice, f0 = pan -1..1 */
    GMB_SYN_FILTER,        /* voice, f0 type, f1 cutoff, f2 resonance % */
    GMB_SYN_RESET,         /* azzera voci, clock, clip, eventi */
    GMB_SYN_SLOT_PAN       /* slot, f0 = gain 0..1, f1 = pan -1..1 (P6 live) */
};

typedef struct {
    int kind;
    int voice;
    int slot;
    double f0, f1, f2, f3, f4, f5, f6, f7, f8;
} gmb_synth_cmd;

void    gmb_synth_reset(void);
void    gmb_synth_apply(const gmb_synth_cmd* c);
/* Rende `frames` frame stereo int16 interleaved e avanza il clock. */
void    gmb_synth_render(int16_t* out, int frames);

int     gmb_synth_busy(int voice);        /* 1 = campioni rimanenti > 0 */
int     gmb_synth_any_busy(void);
int64_t gmb_synth_clock(void);
int64_t gmb_synth_voice_remaining(int voice);

/* Accoda una sequenza MML già parsata: gli eventi partono su `voice` ai campioni
 * `start` assoluti; `out_total_samples` riceve la durata totale. */
void    gmb_synth_schedule_mml(const double* freq, const double* dur, const int* type,
                               const int* vol, int n, int voice, int64_t* out_total_samples);

/* Letture: what 0..4 per VOICE (picco, RMS, occupata, freq, rimanenti),
 * 0..3 per STATUS (clock, picco mix, clip, eventi), 0..5 per EVENT. 1 = ok, 0 = non valido. */
int     gmb_synth_voice_read(int voice, int what, double* out);
int     gmb_synth_status_read(int what, double* out);
int     gmb_synth_event_read(int index, int what, double* out);

#endif /* GMB_SYNTH_H */
