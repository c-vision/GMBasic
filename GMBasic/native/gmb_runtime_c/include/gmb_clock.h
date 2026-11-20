/* ============================================================================
 * gmb_clock.h — base di tempo a PASSO FISSO, condivisa da tutto il runtime.
 *
 * Perché esiste. Un ciclo grafico non deve misurare il tempo a mano: se ogni
 * fotogramma fa `SLEEP 33` il ritmo dipende da quanto è veloce il disegno, un
 * blocco di 400 ms (swap, I/O, un load) fa "teletrasportare" il mondo e non
 * esiste modo di avanzare l'animazione senza orologio, quindi di registrarla o
 * confrontarla a immagine. Questo modulo è l'unico posto dove si decide
 * "quanto tempo è passato": un accumulatore con passo fisso, un limite al
 * singolo frame, un tetto al recupero, scala e pausa.
 *
 * Contratto (numeri verificati in tests/gmb_clock_selfcheck.c):
 *  - la simulazione avanza SOLO in multipli interi di `step` = 1/fps; il tempo
 *    reale che avanza non basta a un passo resta in `accum` e diventa `alpha`;
 *  - un singolo frame non accoda più di `max_frame` secondi (default 250 ms):
 *    un blocco lungo viene TAGLIATO, non smaltito tutto insieme;
 *  - al massimo `max_ticks` passi per frame (default 8): il ritardo che resta
 *    viene scartato (`dropped`) e NON si trascina nei frame successivi;
 *  - `scale` moltiplica il tempo che entra (0.5 = slow motion, 0 = fermo);
 *    `paused` azzera l'accumulatore e non consuma tempo;
 *  - `gmb_clock_step` avanza di N passi SENZA orologio: è la via deterministica
 *    per registrare un filmato o confrontare due esecuzioni;
 *  - nessuna allocazione, nessun accesso a file, nessun thread: la struttura è
 *    del chiamante e si tocca solo dal thread che esegue il ciclo.
 *
 * Alimenta `gmb_form_frame(0)` (DOEVENTS) con l'orologio globale
 * `gmb_clock_frame_clock()`; il ciclo 3D (`gmb_3d_tick`) ha ancora il suo
 * accumulatore per-mondo e la convergenza su questo modulo è il passo dopo.
 *
 * Il dominio BASIC `CLOCK` (2026-11-20) non aggiunge stato: le sue quattro
 * letture e i quattro comandi sono la facciata `gmb_clock_frame_*` in fondo a
 * questo header, sullo stesso orologio globale.
 * ============================================================================ */
#ifndef GMB_CLOCK_H
#define GMB_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_CLOCK_DEFAULT_FPS      60.0
#define GMB_CLOCK_DEFAULT_MAX_FRAME 0.250   /* secondi accodabili in un frame */
#define GMB_CLOCK_DEFAULT_MAX_TICKS 8       /* passi massimi per frame */

typedef struct gmb_clock {
    double fps;                        /* passo nominale */
    double step;                       /* 1/fps, secondi per passo */
    double accum;                      /* tempo (scalato) che non basta a un passo */
    double time;                       /* tempo simulato consumato, secondi */
    double scale;                      /* moltiplicatore del tempo reale */
    double max_frame;                  /* clamp del singolo frame, secondi */
    int    max_ticks;                  /* recupero massimo per frame */
    int    paused;                     /* 1 = fermo: nessun passo, accum azzerato */
    unsigned long long frames;         /* frame chiusi */
    unsigned long long ticks;           /* passi di simulazione eseguiti */
    unsigned long long clamped;        /* frame in cui il clamp ha tagliato */
    unsigned long long dropped;        /* passi scartati dal tetto di recupero */
    double ema_dt;                     /* media mobile del frame reale (fps misurati) */
    int    started;                    /* primo frame già visto */
    int64_t last_ns;                   /* ultimo campione monotono */
} gmb_clock_t;

/* Inizializza a `fps` (default GMB_CLOCK_DEFAULT_FPS se fuori da 1..1000). */
void gmb_clock_init(gmb_clock_t* c, double fps);

/* Riporta il tempo a zero (fps, scala e limiti restano). */
void gmb_clock_reset(gmb_clock_t* c);

void gmb_clock_set_fps(gmb_clock_t* c, double fps);
void gmb_clock_set_scale(gmb_clock_t* c, double scale);   /* 0..16, fuori range ignorato */
void gmb_clock_set_paused(gmb_clock_t* c, int paused);
void gmb_clock_set_limits(gmb_clock_t* c, double max_frame_s, int max_ticks);

/* Avanza con `real_seconds` di tempo reale; ritorna i passi da eseguire (0..max_ticks). */
int gmb_clock_advance(gmb_clock_t* c, double real_seconds);

/* Avanza di N passi senza orologio (deterministico, per filmati e test). */
int gmb_clock_step(gmb_clock_t* c, int ticks);

/* Un frame vero: misura il tempo trascorso dall'ultima chiamata (la prima vale
 * un passo) e chiama gmb_clock_advance. Ritorna i passi da eseguire. */
int gmb_clock_begin_frame(gmb_clock_t* c);

/* Quanto manca al confine del prossimo passo, in secondi (>= 0). */
double gmb_clock_rest(const gmb_clock_t* c);

double gmb_clock_dt(const gmb_clock_t* c);      /* passo fisso, secondi */
double gmb_clock_alpha(const gmb_clock_t* c);   /* residuo 0..1 per interpolare */
double gmb_clock_time(const gmb_clock_t* c);    /* tempo simulato, secondi */
double gmb_clock_fps_measured(const gmb_clock_t* c); /* 1/media del frame reale */

/* Orologio monotono in nanosecondi (CLOCK_MONOTONIC / QueryPerformanceCounter). */
int64_t gmb_clock_now_ns(void);

/* Orologio globale del ciclo grafico, usato da DOEVENTS (`gmb_form_frame(0)`). */
gmb_clock_t* gmb_clock_frame_clock(void);

/* ── SUPERFICIE BASIC del dominio CLOCK (2026-11-20) ─────────────────────────
 * Un programma legge e governa lo STESSO orologio del ciclo grafico: queste
 * funzioni sono la facciata unica di C99Native e VM/C (chiamata diretta) e delle
 * gambe gestite IL/PCodeVM (P/Invoke verso la libreria condivisa `gmb_clock`,
 * che compila QUESTO sorgente). Un solo orologio per processo, nessuna copia
 * dello stato (§0bis).
 *
 * Letture (funzioni BASIC `CLOCK.DT/ALPHA/FPS/SCALE`): double.
 * Scritture (comandi BASIC `CLOCK PAUSE|RESUME|SCALE|FPS`): void — un comando
 * non ha valore di ritorno (§1). */
double gmb_clock_frame_dt(void);        /* CLOCK.DT()    = 1/fps, secondi per passo */
double gmb_clock_frame_alpha(void);     /* CLOCK.ALPHA() residuo 0..1 per interpolare */
double gmb_clock_frame_fps(void);       /* CLOCK.FPS()   passi nominali al secondo */
double gmb_clock_frame_scale(void);     /* CLOCK.SCALE() moltiplicatore del tempo */
void   gmb_clock_frame_pause(int paused);        /* CLOCK PAUSE (1) / CLOCK RESUME (0) */
void   gmb_clock_frame_set_scale(double scale);  /* CLOCK SCALE secondi (0..16) */
void   gmb_clock_frame_set_fps(double fps);      /* CLOCK FPS frequenza (1..1000) */

#ifdef __cplusplus
}
#endif
#endif /* GMB_CLOCK_H */
