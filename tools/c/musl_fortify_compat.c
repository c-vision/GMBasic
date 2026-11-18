/* musl_fortify_compat.c — simboli "fortify" di glibc per il link STATICO (musl).
 *
 * PERCHÉ: gli archivi delle terze parti in `~/.gmbasic/libs/<rid>/lib/*.a` (SDL2, curl, …)
 * sono costruiti contro glibc, che con `_FORTIFY_SOURCE` sostituisce le funzioni di I/O con
 * varianti `__*_chk` che portano un argomento in più (la dimensione del buffer). musl non le
 * ha, quindi il link statico di un programma che tira dentro quegli oggetti fallisce con
 * "undefined symbol: __fprintf_chk".
 *
 * Qui le si fornisce come semplici inoltri alle funzioni C standard: il controllo di
 * dimensione è già stato fatto a monte dal codice che le chiama (è una ridondanza di
 * sicurezza del libc, non una funzionalità). Si linka SOLO con `--static` (musl): su
 * glibc/Darwin/Windows questi simboli o esistono già o non servono, e il file non entra.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int __printf_chk(int flag, const char *fmt, ...)
{
    (void)flag;
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int __fprintf_chk(FILE *f, int flag, const char *fmt, ...)
{
    (void)flag;
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int __vfprintf_chk(FILE *f, int flag, const char *fmt, va_list ap)
{
    (void)flag;
    return vfprintf(f, fmt, ap);
}

int __sprintf_chk(char *s, int flag, size_t slen, const char *fmt, ...)
{
    (void)flag; (void)slen;
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

int __snprintf_chk(char *s, size_t n, int flag, size_t slen, const char *fmt, ...)
{
    (void)flag; (void)slen;
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}

int __vsnprintf_chk(char *s, size_t n, int flag, size_t slen, const char *fmt, va_list ap)
{
    (void)flag; (void)slen;
    return vsnprintf(s, n, fmt, ap);
}

int __fputs_chk(const char *s, FILE *f)
{
    return fputs(s, f);
}

char *__strcpy_chk(char *d, const char *s, size_t dlen)
{
    (void)dlen;
    return strcpy(d, s);
}

char *__strncpy_chk(char *d, const char *s, size_t n, size_t dlen)
{
    (void)dlen;
    return strncpy(d, s, n);
}

char *__strcat_chk(char *d, const char *s, size_t dlen)
{
    (void)dlen;
    return strcat(d, s);
}

void *__memcpy_chk(void *d, const void *s, size_t n, size_t dlen)
{
    (void)dlen;
    return memcpy(d, s, n);
}

void *__memmove_chk(void *d, const void *s, size_t n, size_t dlen)
{
    (void)dlen;
    return memmove(d, s, n);
}

void *__memset_chk(void *d, int c, size_t n, size_t dlen)
{
    (void)dlen;
    return memset(d, c, n);
}

/* ── varianti C23 di glibc 2.38+ (`__isoc23_*`) ────────────────────────────────────────
 * glibc recente espone strtol/strtoll/strtoul/strtoull anche con il prefisso `__isoc23_`
 * (stessa semantica, cambia solo il riconoscimento del prefisso 0b). musl non le ha e gli
 * archivi delle terze parti compilati su glibc recente le referenziano. */
#include <stdlib.h>

long __isoc23_strtol(const char *s, char **end, int base) { return strtol(s, end, base); }
unsigned long __isoc23_strtoul(const char *s, char **end, int base) { return strtoul(s, end, base); }
long long __isoc23_strtoll(const char *s, char **end, int base) { return strtoll(s, end, base); }
unsigned long long __isoc23_strtoull(const char *s, char **end, int base) { return strtoull(s, end, base); }

/* ── GBM/DRM (backend video KMSDRM di SDL2) ────────────────────────────────────────────
 * L'archivio SDL2 statico referenzia il backend KMSDRM, che richiede libgbm (non presente
 * nella cache e non necessaria a un binario console/servizio). Queste tre funzioni sono
 * l'UNICA superficie usata: restituiscono NULL/0 — il backend non viene mai inizializzato
 * se non si chiede esplicitamente il video DRM. */
void *gbm_surface_destroy(void *surface) { (void)surface; return 0; }
void *gbm_surface_lock_front_buffer(void *surface) { (void)surface; return 0; }
void *gbm_surface_release_buffer(void *surface, void *bo) { (void)surface; (void)bo; return 0; }

/* ── dipendenze opzionali di SDL2 statico non presenti come archivi ─────────────────────
 * SDL2_ttf (freetype) usa bzip2 per le tabelle font compresse; SDL2 audio usa libsamplerate.
 * Nessuno dei due serve a un binario console/servizio, e nella cache non esistono in forma
 * statica (solo .so). Qui si forniscono stub che FALLISCONO in modo ESPLICITO (una riga su
 * stderr, una volta sola, + codice d'errore): mai un successo silenzioso. Se un programma
 * statico prova davvero a usare font compressi o audio, lo dice.
 */
static int g_gmb_static_missing_warned = 0;
static void gmb_static_missing(const char *what)
{
    if (g_gmb_static_missing_warned) return;
    g_gmb_static_missing_warned = 1;
    fprintf(stderr, "[GMBASIC] avviso: %s non disponibile nel binario STATICO (dipendenza "
                    "senza archivio statico nella cache del RID)\n", what);
}

/* bzip2 (freetype/SDL2_ttf) — BZ_OK = 0, BZ_DATA_ERROR = -4 */
int BZ2_bzDecompressInit(void *strm, int verbosity, int small) { (void)strm; (void)verbosity; (void)small; gmb_static_missing("bzip2"); return 0; }
int BZ2_bzDecompress(void *strm) { (void)strm; gmb_static_missing("bzip2"); return -4; }
int BZ2_bzDecompressEnd(void *strm) { (void)strm; return 0; }

/* libsamplerate (SDL2 audio) — SRC_ERR_* negativi */
typedef struct { int dummy; } SRC_STATE;
typedef struct { int data_in, data_out, input_frames, output_frames, end_of_input; double src_ratio; } SRC_DATA;
SRC_STATE *src_new(int converter_type, int channels, int *error) { (void)converter_type; (void)channels; if (error) *error = -1; gmb_static_missing("libsamplerate"); return 0; }
SRC_STATE *src_delete(SRC_STATE *s) { (void)s; return 0; }
int src_simple(SRC_DATA *d, int converter, int channels) { (void)d; (void)converter; (void)channels; gmb_static_missing("libsamplerate"); return -1; }
const char *src_strerror(int e) { (void)e; return "not available in static build"; }
