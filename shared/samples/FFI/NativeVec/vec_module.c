/* ============================================================================
 * vec_module.c — modulo nativo di esempio per gmb_ffi.h
 * File: samples/FFI/NativeVec/vec_module.c
 *
 * Mostra ENTRAMBI i livelli FFI:
 *   1) semplice `c-abi` — funzioni C piatte esposte come simboli normali
 *      (chiamabili OGGI con `DECLARE FUNCTION ... LIB` in BASIC);
 *   2) `gmb-native` — `gmb_ffi_module_entry()` che espone la tabella dei
 *      descriptor (name/fn/min_args/max_args/signature) per il futuro
 *      dispatcher del PackageManager/backend.
 *
 * Il modulo è SELF-CONTAINED: usa solo i TIPI di gmb_ffi.h/gmb_runtime.h
 * (nessun link a libgmb_runtime) — per i wrapper gmb-native costruisce
 * direttamente le gmb_ffi_val_t. Compilazione: Makefile.
 * ============================================================================ */

#include "gmb_ffi.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Livello 1 — funzioni C piatte (c-abi, usate dal .bas con DECLARE)
 * ============================================================================ */

long vec_add(long a, long b) { return a + b; }
double vec_scale(double a, double b) { return a * b; }

/* SUB void con parametri BYVAL (asse "DECLARE SUB"): hit counter osservabile. */
static long g_ticks = 0;
void vec_bump(double v) { (void)v; g_ticks++; }
long vec_tick(void) { return g_ticks; }
/* Reset del contatore: rende i test auto-contenuti quando piu' run girano nello
 * stesso processo (runner di parita IL/PCode/C99) con la dylib gia' mappata. */
void vec_reset(void) { g_ticks = 0; }

long vec_isprime(long n)
{
    if (n < 2) return 0;
    for (long i = 2; i * i <= n; i++)
        if (n % i == 0) return 0;
    return 1;
}

/* Arity: copertura multi-argomento (parità FFI 4 backend). 4 argomenti DOUBLE, 8 LONG. */
double vec_avg4(double a, double b, double c, double d) { return (a + b + c + d) / 4.0; }
long vec_sum8(long a, long b, long c, long d, long e, long f, long g, long h)
{ return a + b + c + d + e + f + g + h; }

/* Ritorna un puntatore a buffer STATICO: GMBasic lo COPIA subito (contratto
 * copy-out di gmb_ffi.h), quindi il buffer può essere riusato. */
static char g_label[256];
const char* vec_label(long id, const char* name)
{
    snprintf(g_label, sizeof(g_label), "%ld:%s", id, name != NULL ? name : "?");
    return g_label;
}

/* BYREF multi-out: lo=floor(in), hi=in+1. */
void vec_split(double in, double* lo, double* hi)
{
    if (lo != NULL) *lo = (double)(long)in;
    if (hi != NULL) *hi = in + 1.0;
}

/* BYREF scalare elemento LONG: quoziente come ritorno, resto come OUT — pattern C
 * classico (div/mod). Firma tutta-LONG (asse "BYREF scalare, elemento intero"). */
long vec_divmod(long a, long b, long* rem)
{
    if (b == 0) { if (rem != NULL) *rem = 0; return 0; }
    if (rem != NULL) *rem = a % b;
    return a / b;
}

/* BYREF scalare elemento DOUBLE, IN/OUT (il valore iniziale viene letto E
 * riscritto) — asse complementare a vec_divmod (qui il puntatore punta a un
 * DOUBLE, non a un LONG; lo scalare "n" resta LONG perché il puntatore forza
 * comunque la firma a INT64, indipendentemente dal tipo dell'elemento puntato,
 * stessa regola degli array BYREF). */
void vec_scale_ref(long n, double* pv)
{
    if (pv != NULL) *pv = (*pv) * (double)n;
}

/* Struct/UDT per valore (2026-09-28): un vero struct C passato/ritornato per VALORE (non
 * puntatore) — 1-2 campi, tutti dello stesso kind (qui DOUBLE). Un UDT-PARAMETRO deve avere
 * kind uniforme; un UDT-RITORNO può avere campi di kind misto (DOUBLE e intero, vedi
 * vec_labeled_dn_t/vec_labeled_nd_t più sotto) — limite del dispatch nativo VM/C, non
 * un vincolo ABI reale. */
typedef struct { double x, y; } vec_point_t;

/* Return per valore: costruisce un punto dai due scalari. */
vec_point_t vec_make_point(double x, double y)
{
    vec_point_t p; p.x = x; p.y = y; return p;
}

/* Parametro per valore: nessuna scrittura possibile sul chiamante (a differenza di BYREF) —
 * la lunghezza euclidea del punto. */
double vec_point_len(vec_point_t p)
{
    return sqrt(p.x * p.x + p.y * p.y);
}

/* Parametro E ritorno per valore, nella stessa firma: somma due punti componente per
 * componente — copre lo "stesso kind in tutta la firma" con PIÙ di un parametro struct. */
vec_point_t vec_point_add(vec_point_t a, vec_point_t b)
{
    vec_point_t r; r.x = a.x + b.x; r.y = a.y + b.y; return r;
}

/* Array BYREF (asse "DECLARE SUB ... (buf() AS DOUBLE, n AS LONG)"): riempie il
 * buffer del chiamante con buf[i] = i*10 — pattern CopyBuffer-style (output array,
 * nessun bound auto-passato: il chiamante BASIC dichiara n separatamente). */
void vec_fill(double* buf, long n)
{
    if (buf == NULL) return;
    for (long i = 0; i < n; i++) buf[i] = (double)(i * 10);
}

/* Array BYREF elemento INTERO, array come INPUT (sola lettura) a una FUNCTION con
 * ritorno — asse complementare a vec_fill (SUB, DOUBLE, output): qui LONG, FUNCTION,
 * input. Copre sia la variante di tipo elemento sia il ramo "array letto ma non
 * riscritto" del marshalling (deve comunque sopravvivere al copy-out innocuo). */
long vec_isum(long* buf, long n)
{
    long s = 0;
    if (buf == NULL) return 0;
    for (long i = 0; i < n; i++) s += buf[i];
    return s;
}

/* Array BYREF multiplo (3 parametri array nella STESSA chiamata, non tutti in
 * posizione 0): out[i] = a[i] + b[i]. Copre arrayMask multi-bit con array non
 * all'indice 0 (n e' il primo parametro, scalare) e piu' array attivi insieme. */
void vec_combine(double* a, int64_t na, double* b, int64_t nb, double* out, int64_t nout)
{
    int64_t n = na < nb ? na : nb;
    if (nout < n) n = nout;
    if (a == NULL || b == NULL || out == NULL) return;
    for (int64_t i = 0; i < n; i++) out[i] = a[i] + b[i];
}

/* Array + STRING nella STESSA firma (entrambi puntatori, entrambi forzano firma
 * INT64): somma il buffer e la lunghezza del prefisso — copre la combinazione
 * che nessuno dei minitest precedenti esercitava (array E stringa insieme). */
long vec_label_len(const char* prefix, long* buf, long n)
{
    long s = 0;
    if (buf != NULL) for (long i = 0; i < n; i++) s += buf[i];
    return s + (long)(prefix != NULL ? strlen(prefix) : 0);
}

/* ============================================================================
 * Livello 2 — gmb-native: wrapper gmb_ffi_val_t + tabella descriptor.
 * Self-contained: costruisce le gmb_ffi_val_t a mano (GMB_TYPE_* di
 * gmb_runtime.h). Una volta linkato gmb_ffi.c+libgmb_runtime si possono usare
 * i helper gmb_ffi_make_* e gmb_ffi_get_* al posto dei campi diretti.
 * ============================================================================ */

static gmb_ffi_val_t make_int(gmb_int_t v)
{
    gmb_ffi_val_t r; r.kind = GMB_TYPE_INT; r.v_int = v; return r;
}
static gmb_ffi_val_t make_double(gmb_double_t v)
{
    gmb_ffi_val_t r; r.kind = GMB_TYPE_DOUBLE; r.v_double = v; return r;
}
static gmb_ffi_val_t make_nil(void)
{
    gmb_ffi_val_t r; r.kind = GMB_TYPE_NIL; r.v_int = 0; return r;
}
static gmb_ffi_val_t make_string_copy(const char* s)
{
    gmb_ffi_val_t r;
    size_t n = s != NULL ? strlen(s) : 0;
    r.kind = GMB_TYPE_STRING;
    r.v_string.data = (char*)malloc(n + 1);
    if (r.v_string.data == NULL) return make_nil();
    if (n > 0) memcpy(r.v_string.data, s, n);
    r.v_string.data[n] = '\0';
    r.v_string.length = n;
    r.v_string.ref_count = 1;
    return r;
}

/* Errore controllato: stringa locale + nil (in gmb_ffi.c sarebbe gmb_ffi_error). */
static char g_last_err[256] = "";
static void set_err(const char* m) { snprintf(g_last_err, sizeof(g_last_err), "%s", m); }
const char* vec_last_error(void) { return g_last_err; }

static gmb_ffi_val_t w_add(const gmb_ffi_val_t* a, size_t n)
{
    if (n < 2) { set_err("vec_add: 2 args"); return make_nil(); }
    return make_int(a[0].v_int + a[1].v_int);
}

static gmb_ffi_val_t w_scale(const gmb_ffi_val_t* a, size_t n)
{
    if (n < 2) { set_err("vec_scale: 2 args"); return make_nil(); }
    return make_double(a[0].v_double * a[1].v_double);
}

static gmb_ffi_val_t w_label(const gmb_ffi_val_t* a, size_t n)
{
    if (n < 2) { set_err("vec_label: 2 args"); return make_nil(); }
    long id = a[0].v_int;
    const char* name = (a[1].kind == GMB_TYPE_STRING && a[1].v_string.data)
        ? a[1].v_string.data : "";
    snprintf(g_label, sizeof(g_label), "%ld:%.*s", id, (int)a[1].v_string.length, name);
    return make_string_copy(g_label);
}

static gmb_ffi_val_t w_boom(const gmb_ffi_val_t* a, size_t n)
{
    (void)a; (void)n;
    set_err("vec_boom: errore controllato");
    return make_nil();
}

static const gmb_ffi_fn_descr_t g_vec_fns[] = {
    { "VecAdd",   w_add,   2, 2, "L,L" },
    { "VecScale", w_scale, 2, 2, "D,D" },
    { "VecLabel", w_label, 2, 2, "L,S" },
    { "VecBoom",  w_boom,  0, 0, ""   },
    { NULL, NULL, 0, 0, NULL }
};

static gmb_ffi_module_t g_vec_module = {
    "vec", GMB_FFI_VERSION, g_vec_fns, NULL, NULL
};

gmb_ffi_module_t* gmb_ffi_module_entry(void) { return &g_vec_module; }

typedef struct { int32_t a, b; } vec_int2_t;
vec_int2_t vec_int2_sum(vec_int2_t a, vec_int2_t b) { vec_int2_t r; r.a=a.a+b.a; r.b=a.b+b.b; return r; }

/* Kind misto (2026-09-29): un ritorno con un campo DOUBLE e uno intero — ammesso solo come
 * TIPO DI RITORNO (mai come parametro) dal marshalling FFI. */
typedef struct { double v; int32_t n; } vec_labeled_dn_t;
/* Parametri OMOGENEI (entrambi DOUBLE) — la firma mista PARAMETRI/ritorno resta vietata
 * per design (regola preesistente, indipendente dallo UDT): solo i CAMPI del ritorno
 * possono avere kind diverso tra loro. */
vec_labeled_dn_t vec_make_labeled_dn(double v, double n) { vec_labeled_dn_t r; r.v = v; r.n = (int32_t)n; return r; }
typedef struct { int32_t n; double v; } vec_labeled_nd_t;
vec_labeled_nd_t vec_make_labeled_nd(double n, double v) { vec_labeled_nd_t r; r.n = (int32_t)n; r.v = v; return r; }

/* Kind misto come PARAMETRO (2026-09-29): un UDT a kind misto passato BYVAL — il caso che
 * prima era vietato dal marshalling FFI. */
double vec_labeled_dn_total(vec_labeled_dn_t a) { return a.v + (double)a.n; }
double vec_labeled_nd_total(vec_labeled_nd_t a) { return (double)a.n + a.v; }
/* Kind misto come PARAMETRO, in posizione NON iniziale/finale (per verificare che il
 * dispatch generalizzato per posizione funzioni ovunque, non solo al primo/ultimo slot). */
double vec_labeled_dn_between(double before, vec_labeled_dn_t a, double after) { return before + a.v + (double)a.n + after; }
/* Kind misto come PARAMETRO, in ULTIMA posizione con 3 scalari prima (NB=3,NA=0) — verifica
 * che il dispatch generalizzato copra anche gli estremi della finestra 0-6. */
double vec_labeled_dn_last(double a, double b, double c, vec_labeled_dn_t lab) { return a + b + c + lab.v + (double)lab.n; }
/* Kind misto come PARAMETRO, combinato con un array BYREF nella STESSA firma (l'array forza
 * INT64 sugli ALTRI slot, il parametro misto ne resta esente — verifica l'interazione). */
long vec_labeled_dn_with_array(long* buf, long n, vec_labeled_dn_t lab) { long s = 0; if (buf) for (long i = 0; i < n; i++) s += buf[i]; return s + (long)lab.v + lab.n; }

/* Callback come PARAMETRO (2026-09-29): il caso classico "il nativo richiama indietro nel
 * chiamante" (comparator-style/apply-style) — il trampolino BASIC->nativo->BASIC e' quello
 * costruito da PCodeProcessor.FfiCallback.cs/gmb_vm_core.c. */
long vec_cb_apply(long (*cb)(long), long x) { return cb ? cb(x) : 0; }
long vec_cb_combine2(long (*cb)(long, long), long a, long b) { return cb ? cb(a, b) : 0; }
long vec_cb_sum_transformed(long* buf, long n, long (*cb)(long)) { long s = 0; if (buf && cb) for (long i = 0; i < n; i++) s += cb(buf[i]); return s; }
/* vec_cb_apply_i2d: callback a kind INTERNO double (cb prende/ritorna DOUBLE), ma la firma
 * ESTERNA (parametri/ritorno di vec_cb_apply_i2d stessa, esclusa la firma del callback) resta
 * tutta LONG — un parametro callback forza Int64 sull'INTERA dichiarazione ESTERNA (stesso
 * 'kind' unico condiviso con array/STRING/BYREF), ma il kind INTERNO del callback (i suoi
 * propri parametri/ritorno, marshallati separatamente al momento del richiamo nativo->BASIC)
 * resta libero di essere DOUBLE — verifica che i due kind non vengano confusi. */
long vec_cb_apply_i2d(double (*cb)(double), long unused) { (void)unused; return cb ? (long)cb(3.5) : -1; }

/* Struct annidate (2026-09-29): un campo di uno UDT-per-valore FFI il cui tipo è A SUA VOLTA
 * un altro struct semplice — dopo flattening ricorsivo (i.x + y = 2 foglie totali) è
 * ABI-identica a vec_point_t (2 campi diretti), nessuna nuova convenzione lato C richiesta:
 * qui il nome/layout dello struct C non ha significato semantico per GMBasic (che costruisce
 * un compound literal della "shape" pre-dichiarata gmb_ffi_udtd2_t e lo passa castato), ma un
 * vero struct annidato rende il test end-to-end realistico. Parametro E ritorno nella stessa
 * firma (roundtrip), come vec_point_add. */
typedef struct { double x; } vec_inner_t;
typedef struct { vec_inner_t i; int32_t y; } vec_outer_t;
vec_outer_t vec_nested_roundtrip(vec_outer_t o) { o.i.x += 1.0; o.y += 1; return o; }

/* Firma mista LONG/DOUBLE fra scalari semplici (2026-10): nessun array/STRING/BYREF/UDT
 * coinvolto, solo scalari intercalati — id/qty sono LONG, price e' DOUBLE, il ritorno e'
 * DOUBLE (kind diverso da due dei tre parametri). */
double vec_mixed(long id, double price, long qty) { return (double)id * 0.0 + price * (double)qty; }
/* SUB a firma mista (nessun ritorno da interpretare, nessun BYREF — BYREF+mista resta fuori
 * scope v1, forza sempre Int64 su tutta la firma): l'effetto si osserva via un getter. */
static long g_mixed_sub_last = 0;
void vec_mixed_sub(long a, double b) { g_mixed_sub_last = a + (long)b; }
long vec_mixed_sub_last(void) { return g_mixed_sub_last; }
/* Caso limite: 8 parametri alternati LONG/DOUBLE, ritorno LONG — esercita il dispatcher
 * generato oltre la shape minima (shapeBits=0b01010101, argc=8). */
long vec_mixed8(long a, double b, long c, double d, long e, double f, long g, double h)
{
    return a + (long)b + c + (long)d + e + (long)f + g + (long)h;
}

/* Array BYREF + scalari DOUBLE nella STESSA firma (2026-11-02): il puntatore viaggia come
 * intero, gli scalari come double — il kind per-slot deve restare distinto. Somma
 * buf[i]*scale + offset. */
double vec_array_scale(const double* buf, long n, double scale, double offset)
{
    double s = offset;
    long i;
    if (!buf || n <= 0) return offset;
    for (i = 0; i < n; i++) s += buf[i] * scale;
    return s;
}

/* Callback scalare + limiti double (forma OPTIMIZE.BRENTQ): bisezione 60 passi. */
double vec_cb_rootq(double (*cb)(double), double a, double b)
{
    double fa;
    int i;
    if (!cb) return 0.0;
    fa = cb(a);
    if (fa * cb(b) > 0.0) return 0.0;
    for (i = 0; i < 60; i++) {
        double m = 0.5 * (a + b);
        double fm = cb(m);
        if (fa * fm <= 0.0) b = m;
        else { a = m; fa = fm; }
    }
    return 0.5 * (a + b);
}

/* Callback VETTORIALE (2026-11-02): f(v[], n) -> double. Il nativo riceve (double*, n) e il
 * trampolino ricostruisce l'array BASIC. Somma dei quadrati (25 per v={3,4}). */
double vec_cb_vecd(double (*cb)(double*, int64_t), const double* v, int64_t n)
{
    return cb ? cb((double*)v, n) : 0.0;
}

/* Callback ARRAY + SCALARE (2026-11-02): forma dei modelli LM/CurveFit f(p[], x). */
double vec_cb_arr_scalar(double (*cb)(double*, int64_t, double), const double* v, int64_t n, double s)
{
    return cb ? cb((double*)v, n, s) : 0.0;
}

/* Callback con RITORNO ARRAY (2026-11-02): f(v[]) AS ARRAY (Fsolve/Broyden/LSQF). */
void vec_cb_arr_out(void (*cb)(double*, int64_t, double*), const double* v, int64_t n, double* out, int64_t nout)
{
    (void)nout;
    if (cb) cb((double*)v, n, out);
}
/* Due array in ingresso + ritorno array (LMVec): f(p[], x[]) AS ARRAY. */
void vec_cb_vv_out(void (*cb)(double*, int64_t, double*, int64_t, double*),
                   const double* p, int64_t np, const double* x, int64_t nx, double* out, int64_t nout)
{
    (void)nout;
    if (cb) cb((double*)p, np, (double*)x, nx, out);
}

/* Due array -> scalare (2026-11-02): f(a[], b[]) AS DOUBLE (descriptor 129). */
double vec_cb_vv_scalar(double (*cb)(double*, int64_t, double*, int64_t), const double* a, int64_t na, const double* b, int64_t nb)
{
    return cb ? cb((double*)a, na, (double*)b, nb) : 0.0;
}

/* Test marshalling: 5 array in una sola chiamata. out(i) = a(i)+b(i)+c(i)+d(i). */
void vec_five(const double* a, int64_t na, const double* b, int64_t nb, const double* c, int64_t nc, const double* d, int64_t nd, double* out, int64_t nout)
{
    int64_t i, n = na; (void)nout;
    if (nb < n) n = nb; if (nc < n) n = nc; if (nd < n) n = nd;
    for (i = 0; i < n; i++) out[i] = a[i] + b[i] + c[i] + d[i];
}

/* 5 array + 1 scalare (forma di LINPROG: c, A, b, bounds, out). */
void vec_five2(const double* a, int64_t na, const double* b, int64_t nb, const double* c, int64_t nc, const double* d, int64_t nd, double* out, int64_t nout)
{
    int64_t i, n = na;
    (void)nb; (void)nc; (void)nd; (void)nout;
    for (i = 0; i < n; i++) out[i] = a[i] + b[i] + c[i] + d[i];
}
