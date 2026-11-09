/* signal_core.c — nuclei di base: FFT, finestre, convoluzione/correlazione,
 * densita' spettrale (welch) e ricampionamento (resample).
 *
 * Codice spostato dal runtime C di GMBasic (gmb_math.c: gmb_fft,
 * gmb_signal_convolve/correlate/window; gmb_filters.c: gmb_welch, gmb_resample)
 * con il solo cambio di prefisso (gmb_ -> gs_): formule e costanti INVARIATE,
 * perche' queste funzioni sono l'oracolo di parita' delle 4 gambe.
 */
#include "signal_internal.h"
#include "gmbsignal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── FFT ─────────────────────────────────────────────────────────────────────── */

/* FFT radix-2 iterativa in-place su array complessi (re, im separati).
 * n non potenza di 2 -> Bluestein (chirp-z), O(n log n), corretto per ogni n
 * (il radix-2 puro scriverebbe fuori dai limiti). */
void gs_fft(double* re, double* im, int n, int inverse)
{
    if ((n & (n - 1)) != 0 && n > 1)
    {
        int m = 1;
        while (m < 2 * n - 1) m <<= 1;
        double* br = (double*)calloc((size_t)m, sizeof(double));
        double* bi = (double*)calloc((size_t)m, sizeof(double));
        double* cr = (double*)calloc((size_t)m, sizeof(double));
        double* ci = (double*)calloc((size_t)m, sizeof(double));
        if (!br || !bi || !cr || !ci) { free(br); free(bi); free(cr); free(ci); return; }
        /* chirp: w[j] = exp(-i*pi*j^2/n) (inverse: +) */
        double wsign = inverse ? 1.0 : -1.0;
        for (int j = 0; j < n; j++)
        {
            double ang = wsign * 3.141592653589793 * ((double)(j * j % (2 * n))) / (double)n;
            double w = cos(ang), w2 = sin(ang);
            br[j] = re[j] * w - im[j] * w2;
            bi[j] = re[j] * w2 + im[j] * w;
            cr[j] = w; ci[j] = -w2;
            if (j > 0) { cr[m - j] = w; ci[m - j] = -w2; }
        }
        gs_fft(br, bi, m, 0);
        gs_fft(cr, ci, m, 0);
        for (int k = 0; k < m; k++)
        {
            double tr = br[k] * cr[k] - bi[k] * ci[k];
            bi[k] = br[k] * ci[k] + bi[k] * cr[k];
            br[k] = tr;
        }
        gs_fft(br, bi, m, 1);
        /* l'IFFT ha gia' diviso per m: qui NON si divide di nuovo. */
        for (int k = 0; k < n; k++)
        {
            double ang = wsign * 3.141592653589793 * ((double)(k * k % (2 * n))) / (double)n;
            double a = cos(ang), a2 = sin(ang);
            double r = br[k], ii = bi[k];
            re[k] = r * a - ii * a2;
            im[k] = r * a2 + ii * a;
        }
        /* la normalizzazione 1/n della DFT inversa va aggiunta qui */
        if (inverse) for (int k = 0; k < n; k++) { re[k] /= n; im[k] /= n; }
        free(br); free(bi); free(cr); free(ci);
        return;
    }
    /* bit-reversal */
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        double ang = 2.0 * 3.141592653589793 / len * (inverse ? 1 : -1);
        double wr = cos(ang), wi = sin(ang);
        for (int i = 0; i < n; i += len)
        {
            double cur_r = 1, cur_i = 0;
            for (int k = 0; k < len / 2; k++)
            {
                int ia = i + k, ib = i + k + len / 2;
                double tr = cur_r * re[ib] - cur_i * im[ib];
                double ti = cur_r * im[ib] + cur_i * re[ib];
                re[ib] = re[ia] - tr; im[ib] = im[ia] - ti;
                re[ia] += tr; im[ia] += ti;
                double nr = cur_r * wr - cur_i * wi;
                cur_i = cur_r * wi + cur_i * wr; cur_r = nr;
            }
        }
    }
    if (inverse) for (int i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}

/* ── convoluzione / correlazione ────────────────────────────────────────────── */

double* gs_signal_convolve(const double* x, long nx, const double* k, long nk, long* outn)
{
    int n = (int)nx + (int)nk - 1;
    double* r = (double*)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (int)nk; j++)
        {
            int xi = i - j;
            if (xi >= 0 && xi < (int)nx) r[i] += x[xi] * k[j];
        }
    return r;
}

double* gs_signal_correlate(const double* x, long nx, const double* k, long nk, long* outn)
{
    int n = (int)nx + (int)nk - 1;
    double* r = (double*)calloc((size_t)(n > 0 ? n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (int)nk; j++)
        {
            int xi = i - ((int)nk - 1) + j;
            if (xi >= 0 && xi < (int)nx) r[i] += x[xi] * k[j];
        }
    return r;
}

/* ── finestre ───────────────────────────────────────────────────────────────── */

double* gs_signal_window(long n, int kind, long* outn)
{
    double* r = (double*)calloc((size_t)(n > 0 ? (size_t)n : 1), sizeof(double));
    if (!r) { *outn = 0; return NULL; }
    *outn = n;
    if (n <= 0) return r;
    if (n == 1) { r[0] = 1.0; return r; }
    double t = 2.0 * GS_PI / ((double)n - 1.0);
    for (int i = 0; i < (int)n; i++)
    {
        if (kind == 0) r[i] = 0.5 * (1.0 - cos(t * i));                        /* hann */
        else if (kind == 1) r[i] = 0.54 - 0.46 * cos(t * i);                   /* hamming */
        else r[i] = 0.42 - 0.5 * cos(t * i) + 0.08 * cos(2.0 * t * i);         /* blackman */
    }
    return r;
}

/* ── Welch: matrice flat [nfreqs][2] (freq, psd); *out_rows = nfreqs ────────── */

double* gs_welch(const double* x, int n, int nperseg, double fs, int* out_rows)
{
    if (nperseg <= 0 || nperseg > n) nperseg = n;
    int noverlap = nperseg / 2;
    int step = nperseg - noverlap;
    int nseg = (n - noverlap) / step;
    if (nseg < 1) nseg = 1;
    double* win = (double*)malloc(sizeof(double) * (size_t)nperseg);
    if (!win) { *out_rows = 0; return NULL; }
    for (int i = 0; i < nperseg; i++)
        win[i] = 0.5 - 0.5 * cos(2.0 * 3.141592653589793 * i / (nperseg - 1));
    double win2 = 0;
    for (int i = 0; i < nperseg; i++) win2 += win[i] * win[i];
    int nfreqs = nperseg / 2 + 1;
    double* psd = (double*)calloc((size_t)nfreqs, sizeof(double));
    double* re = (double*)malloc(sizeof(double) * (size_t)nperseg);
    double* im = (double*)malloc(sizeof(double) * (size_t)nperseg);
    if (!psd || !re || !im) { free(win); free(psd); free(re); free(im); *out_rows = 0; return NULL; }
    double scale = 1.0 / (fs * (win2 > 0 ? win2 : 1.0));
    for (int s = 0; s < nseg; s++)
    {
        int start = s * step;
        for (int i = 0; i < nperseg; i++) { re[i] = x[start + i] * win[i]; im[i] = 0; }
        gs_fft(re, im, nperseg, 0);
        for (int k = 0; k < nfreqs; k++) psd[k] += (re[k] * re[k] + im[k] * im[k]) * scale;
    }
    for (int k = 0; k < nfreqs; k++) psd[k] /= nseg;
    double* out = (double*)malloc(sizeof(double) * (size_t)nfreqs * 2);
    if (!out) { free(win); free(psd); free(re); free(im); *out_rows = 0; return NULL; }
    for (int k = 0; k < nfreqs; k++)
    {
        out[k * 2] = k * fs / nperseg;
        out[k * 2 + 1] = psd[k];
    }
    *out_rows = nfreqs;
    free(win); free(psd); free(re); free(im);
    return out;
}

/* ── Ressampling: resample via FFT (stessa procedura di scipy) ──────────────── */

double* gs_resample(const double* x, int n, int num, long* outn)
{
    if (num <= 0) { *outn = 0; return NULL; }
    double* re = (double*)calloc((size_t)n, sizeof(double));
    double* im = (double*)calloc((size_t)n, sizeof(double));
    if (!re || !im) { free(re); free(im); *outn = 0; return NULL; }
    for (int i = 0; i < n; i++) re[i] = x[i];
    gs_fft(re, im, n, 0);
    double* sre = (double*)calloc((size_t)num, sizeof(double));
    double* sim = (double*)calloc((size_t)num, sizeof(double));
    if (!sre || !sim) { free(re); free(im); free(sre); free(sim); *outn = 0; return NULL; }
    int keep = (n / 2 < num / 2) ? n / 2 : num / 2;
    sre[0] = re[0]; sim[0] = im[0];
    for (int k = 1; k < keep; k++) { sre[k] = re[k]; sim[k] = im[k]; }
    if (num % 2 == 0 && num / 2 < n)
    {
        int ny = num / 2 < n - 1 ? num / 2 : n - 1;
        sre[ny] = re[ny]; sim[ny] = im[ny];
    }
    for (int k = 1; k < keep; k++)
        if (num - k >= 0 && n - k >= 0) { sre[num - k] = re[n - k]; sim[num - k] = im[n - k]; }
    gs_fft(sre, sim, num, 1); /* divide per num */
    double* result = (double*)malloc(sizeof(double) * (size_t)num);
    if (!result) { free(re); free(im); free(sre); free(sim); *outn = 0; return NULL; }
    for (int i = 0; i < num; i++) result[i] = sre[i] * ((double)num / n);
    *outn = num;
    free(re); free(im); free(sre); free(sim);
    return result;
}
