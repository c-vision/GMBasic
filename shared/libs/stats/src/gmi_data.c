/* gmb_data.c — estrattori bar/tick e aggregazione tick->bar. */
#include "gmi_data.h"

void gmb_bars_open(const gmb_bar *b, int n, double *out)   { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].o; }
void gmb_bars_high(const gmb_bar *b, int n, double *out)   { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].h; }
void gmb_bars_low(const gmb_bar *b, int n, double *out)    { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].l; }
void gmb_bars_close(const gmb_bar *b, int n, double *out)  { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].c; }
void gmb_bars_volume(const gmb_bar *b, int n, double *out) { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].v; }
void gmb_bars_ts(const gmb_bar *b, int n, int64_t *out)    { int i; if (!b || !out) return; for (i = 0; i < n; i++) out[i] = b[i].ts; }
void gmb_bars_hlc(const gmb_bar *b, int n, double *oh, double *ol, double *oc)
{
    int i; if (!b) return;
    for (i = 0; i < n; i++) { if (oh) oh[i] = b[i].h; if (ol) ol[i] = b[i].l; if (oc) oc[i] = b[i].c; }
}
void gmb_bars_ohlcv(const gmb_bar *b, int n, double *out5)
{
    int i; if (!b || !out5) return;
    for (i = 0; i < n; i++) { out5[i*5]=b[i].o; out5[i*5+1]=b[i].h; out5[i*5+2]=b[i].l; out5[i*5+3]=b[i].c; out5[i*5+4]=b[i].v; }
}
void gmb_ticks_price(const gmb_tick *t, int n, double *out)  { int i; if (!t || !out) return; for (i = 0; i < n; i++) out[i] = t[i].price; }
void gmb_ticks_volume(const gmb_tick *t, int n, double *out) { int i; if (!t || !out) return; for (i = 0; i < n; i++) out[i] = t[i].volume; }
void gmb_ticks_ts(const gmb_tick *t, int n, int64_t *out)    { int i; if (!t || !out) return; for (i = 0; i < n; i++) out[i] = t[i].ts; }

int gmb_ticks_to_bars(const gmb_tick *ticks, int n, int64_t dt, gmb_bar *bars, int max_bars)
{
    int i, nb = 0;
    int64_t t0;
    if (!ticks || !bars || n < 1 || dt <= 0 || max_bars < 1) return -1;
    t0 = ticks[0].ts;
    for (i = 0; i < n; i++) {
        int idx = (int)((ticks[i].ts - t0) / dt);
        if (idx < 0) continue;
        if (idx >= max_bars) idx = max_bars - 1;
        if (idx >= nb) {
            int k;
            for (k = nb; k <= idx; k++) {
                bars[k].ts = t0 + (int64_t)k * dt;
                bars[k].o = bars[k].h = bars[k].l = bars[k].c = ticks[i].price;
                bars[k].v = 0.0;
            }
            nb = idx + 1;
        }
        { double p = ticks[i].price;
          if (p > bars[idx].h) bars[idx].h = p;
          if (p < bars[idx].l) bars[idx].l = p;
          bars[idx].c = p;
          bars[idx].v += ticks[i].volume; }
    }
    return nb;
}
