/* ============================================================================
 * gmi_array.c — array dinamici: REDIM/REDIM PRESERVE (gmi_array_resize) e
 *              JOIN (gmi_array_join_strings / gmi_array_join).
 * Parte di gmi_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmi_runtime.h"

/* ---------------------------------------------------------------------------
 * gmi_array_resize: ridimensiona un array dinamico (gmi_<tipo>*).
 *
 * ALLOCA SEMPRE un nuovo buffer (mai realloc in-place): il puntatore in ingresso
 * può riferire un array NATIVO C (es. parametro array ricevuto da un array
 * statico del chiamante) su cui realloc sarebbe illegale (SIGABRT). Se preserve=1
 * copia i dati esistenti (min(old,new) elementi), altrimenti parte azzerato.
 * Il vecchio buffer NON viene liberato (leak controllato — runtime a vita breve).
 *
 * Tipi supportati: gmb_int_t, gmb_double_t, gmi_string_t (elemento 1 scalar o
 * struct di pari dimensione). Per i tipi non interi/double l'elemento è
 * azzerato con memset (gmi_string_t zero-inizializzata = {NULL,0,0}).
 * --------------------------------------------------------------------------- */
void gmi_array_resize(void** arr, gmb_int_t* len, gmb_int_t new_len, int elem_size, int preserve) {
    if (!arr || !len || elem_size <= 0) return;
    gmb_int_t old = *len;
    if (new_len <= 0) {
        *arr = NULL;
        *len = 0;
        return;
    }
    void* na = calloc((size_t)new_len, (size_t)elem_size);
    if (!na) return;
    if (preserve && old > 0 && *arr) {
        size_t copy = (size_t)(old < new_len ? old : new_len) * (size_t)elem_size;
        memcpy(na, *arr, copy);
    }
    *arr = na;
    *len = new_len;
}

/* ---------------------------------------------------------------------------
 * gmi_array_join: JOIN di un array di gmi_string_t (str_arr con n elementi)
 * con separatore sep (NULL = ""), restituisce gmi_string_t allocata.
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * gmi_array_join_doubles: JOIN di un array di double formattati con
 * "%.15g" (via gmb_double_to_string) e separatore sep. Usato per
 * `DIM x() = {..}; PRINT JOIN(x, ", ")`.
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * gmi_array_find_index: ASCAN — ritorna l'indice 0-based del primo elemento per
 * cui la closure predicate ritorna true, o -1 se nessuno. L'elemento passato
 * alla callback è del tipo C dell'array (gmi_string_t / gmb_double_t / gmb_int_t)
 * secondo il flag type (0=stringa, 1=double, 2=int). Semantica allineata alla
 * VM (opcode ASCAN → indice 0-based o -1).
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * ArrayUtils Chunk/Zip/Pad/Shuffle — parità con common/CommonLib/ArrayUtils.cs
 * Chunk: suddivide in blocchi di chunkSize (ultimo può essere più corto).
 * Zip:   combina N liste in una lista di tuple (minLength).
 * Pad:   ridimensiona con padValue (tronca se newLen < src.count).
 * Shuffle: Fisher-Yates deterministico con seed xorshift64 (seed==0 → 42).
 * --------------------------------------------------------------------------- */



 uint64_t gmi_xorshift64(uint64_t* s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}




/* ============================================================================
 * Array Clipper su array NATIVI — parità IL (common/IlBackendRuntime/ArrayHelpers.cs).
 * Convenzioni: indici 0-based, -1 = non trovato, AMAP/AFILTER/AREVERSE/ACLONE/AUNIQUE
 * ritornano NUOVA copia (l'input non è mai mutato). Callback ABI come gmi_list.c:
 * map/filter/count/scan-pred → gmb_double_t(*)(void*, gmb_double_t)
 * reduce                    → gmb_double_t(*)(void*, gmb_double_t, gmb_double_t)
 * ============================================================================ */

/* --- numerici (gmb_double_t*) --- */


/* --- boxed (gmi_val_t* — array di UDT/classe) ---------------------------------
 * Le varianti per primitivi (gmb_double_t) non leggono correttamente una struct
 * boxata. ABI della lambda (parità ASORT con comparatore su UDT, gmi_c99_emit.c):
 *   filter: gmb_double_t(*)(void*, gmi_val_t)          → predicato != 0
 *   map:    gmb_double_t(*)(void*, gmi_val_t)          → valore numerico
 * Il C99Native dichiara gli array di UDT come gmi_val_t* (sb_var_ctype). */





/* ASCAN su array di UDT (gmi_val_t*): indice del primo elemento che soddisfa il
 * predicato, -1 se nessuno. ABI lambda: gmb_double_t(*)(void*, gmi_val_t). */


gmb_int_t gmi_array_scan_value(const gmb_double_t* a, gmb_int_t n, gmb_double_t v, gmb_int_t start) {
    if (n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    for (gmb_int_t i = start; i < n; i++) if (a[i] == v) return i;
    return -1;
}

gmb_double_t* gmi_array_reverse_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) out[i] = a[n - 1 - i];
    if (out_n) *out_n = n;
    return out;
}

gmb_double_t* gmi_array_unique(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    gmb_int_t k = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        int dup = 0;
        for (gmb_int_t j = 0; j < k; j++) if (out[j] == a[i]) { dup = 1; break; }
        if (!dup) out[k++] = a[i];
    }
    if (out_n) *out_n = k;
    if (k == 0) { free(out); return NULL; }
    return out;
}

gmb_double_t* gmi_array_clone(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    memcpy(out, a, (size_t)n * sizeof(gmb_double_t));
    if (out_n) *out_n = n;
    return out;
}

gmb_int_t gmi_array_binary_search(const gmb_double_t* a, gmb_int_t n, gmb_double_t v) {
    /* Vera ricerca binaria (parità IL ArrayUtils.BinarySearch): array ordinato, 0-based, -1. */
    if (n <= 0 || !a) return -1;
    gmb_int_t lo = 0, hi = n - 1;
    while (lo <= hi) {
        gmb_int_t mid = lo + (hi - lo) / 2;
        if (a[mid] == v) return mid;
        else if (a[mid] < v) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* --- stringhe (gmi_string_t*): deep copy; uguaglianza byte-level (gmi_string_equals) --- */





/* ARRAYS.ASORT (funzione): ordina in place l'array numerico (parity VM: ordina il buffer
 * sorgente e ritorna la lista; il target DIM vi si aliasing con la propria <x>_len). */
static int gmi_cmp_dbl(const void* x, const void* y) {
    double a = *(const double*)x, b = *(const double*)y;
    return (a > b) - (a < b);
}
void gmi_array_sort_inplace(gmb_double_t* a, gmb_int_t n) {
    if (n > 1 && a) qsort(a, (size_t)n, sizeof(gmb_double_t), gmi_cmp_dbl);
}

/* ASORT con comparatore custom su array di ELEMENTI BOXED (gmi_val_t — array di UDT/classe).
 * `cmp` è la closure (gmi_fn_t) con firma (void*, gmi_val_t, gmi_val_t) -> gmb_double_t.
 * Semantica (parity C99Backend qsort_r / PCodeVM Array.Sort / VM/C): il SEGNO del valore
 * ritornato determina l'ordine — < 0 = a prima di b, > 0 = a dopo b, 0 = uguali. Un
 * comparatore BASIC booleano (`RETURN a.Fatturato > b.Fatturato`) ritorna -1 (TRUE) / 0:
 * TRUE(-1) = "a prima di b", quindi un predicato `a > b` ordina DISCENDENTE.
 * Ordinamento stabile (insertion sort), mai no-op silenzioso. */

/* ASORT con comparatore custom su array NUMERICO (gmb_double_t): stessa semantica del
 * precedente, closure (void*, gmb_double_t, gmb_double_t) -> gmb_double_t. */

/* ARRAYS.APAD: copia in un buffer di lunghezza `want` (pad a destra con `pad` se piu'
 * lungo; tronca se want < n). Ritorna sempre allocazione di `want` elementi (want>=0). */
gmb_double_t* gmi_array_pad_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t want, double pad, gmb_int_t* out_n) {
    if (want < 0) want = 0;
    gmb_double_t* o = (gmb_double_t*)malloc((size_t)want * sizeof(gmb_double_t) + 1);
    if (!o) { *out_n = 0; return NULL; }
    gmb_int_t cp = want < n ? want : n;
    for (gmb_int_t i = 0; i < cp; i++) o[i] = a ? a[i] : pad;
    for (gmb_int_t i = cp; i < want; i++) o[i] = pad;
    *out_n = want;
    return o;
}
