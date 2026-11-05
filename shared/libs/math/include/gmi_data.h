/* gmb_data.h — modello dati canonico: BAR (OHLCV) e TICK (prezzo+volume).
 *
 * Convenzione dell'intera suite gmbmaths:
 *  - Le funzioni generali (indicatori, stats, strategie) ricevono SERIE DI BARS.
 *  - Le funzioni HFT (gmbhft) ricevono SERIE DI TICKS (o bars).
 *
 * gmb_bar  : o,h,l,c,v (+timestamp) — una barra.
 * gmb_tick : price, volume (opzionale, 0 se assente) (+timestamp).
 *
 * Gli estrattori copiano una serie di bars/ticks nei vettori "flat" attesi
 * dalle funzioni esistenti (close, high, low, open, volume / price, volume). */
#ifndef GMBCOMPUTE_GMB_DATA_H
#define GMBCOMPUTE_GMB_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t ts;      /* timestamp (ns/us/ms a scelta) */
    double o, h, l, c;
    double v;        /* volume */
} gmb_bar;

typedef struct {
    int64_t ts;      /* timestamp */
    double price;    /* prezzo del tick */
    double volume;   /* volume (0 se non disponibile) */
} gmb_tick;

/* Estrattori da serie di bars (n barre) verso vettori flat (n elementi).
 * out puo' essere NULL per saltare quel campo. */
void gmb_bars_open(const gmb_bar *bars, int n, double *out);
void gmb_bars_high(const gmb_bar *bars, int n, double *out);
void gmb_bars_low(const gmb_bar *bars, int n, double *out);
void gmb_bars_close(const gmb_bar *bars, int n, double *out);
void gmb_bars_volume(const gmb_bar *bars, int n, double *out);
void gmb_bars_ts(const gmb_bar *bars, int n, int64_t *out);
/* tipico: hlc in un vettore n x 3 row-major */
void gmb_bars_hlc(const gmb_bar *bars, int n, double *out_h, double *out_l, double *out_c);
/* OHLCV in un vettore n x 5 row-major */
void gmb_bars_ohlcv(const gmb_bar *bars, int n, double *out5);

/* Estrattori da serie di ticks. */
void gmb_ticks_price(const gmb_tick *ticks, int n, double *out);
void gmb_ticks_volume(const gmb_tick *ticks, int n, double *out);
void gmb_ticks_ts(const gmb_tick *ticks, int n, int64_t *out);

/* Costruzione barre da tick (aggregazione per tempo): riempie bars fino a
 * max_bars, un bucket ogni dt (in unita' del timestamp). Ritorna n barre. */
int gmb_ticks_to_bars(const gmb_tick *ticks, int n, int64_t dt, gmb_bar *bars,
                      int max_bars);

#ifdef __cplusplus
}
#endif
#endif /* GMBCOMPUTE_GMB_DATA_H */
