/* ============================================================================
 * gmb_array.c — array dinamici: REDIM/REDIM PRESERVE (gmb_array_resize) e
 *              JOIN (gmb_array_join_strings / gmb_array_join).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"

/* ---------------------------------------------------------------------------
 * gmb_array_resize: ridimensiona un array dinamico (gmb_<tipo>*).
 *
 * ALLOCA SEMPRE un nuovo buffer (mai realloc in-place): il puntatore in ingresso
 * può riferire un array NATIVO C (es. parametro array ricevuto da un array
 * statico del chiamante) su cui realloc sarebbe illegale (SIGABRT). Se preserve=1
 * copia i dati esistenti (min(old,new) elementi), altrimenti parte azzerato.
 * Il vecchio buffer NON viene liberato (leak controllato — runtime a vita breve).
 *
 * Tipi supportati: gmb_int_t, gmb_double_t, gmb_string_t (elemento 1 scalar o
 * struct di pari dimensione). Per i tipi non interi/double l'elemento è
 * azzerato con memset (gmb_string_t zero-inizializzata = {NULL,0,0}).
 * --------------------------------------------------------------------------- */
void gmb_array_resize(void** arr, gmb_int_t* len, gmb_int_t new_len, int elem_size, int preserve) {
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
 * gmb_array_join: JOIN di un array di gmb_string_t (str_arr con n elementi)
 * con separatore sep (NULL = ""), restituisce gmb_string_t allocata.
 * --------------------------------------------------------------------------- */
gmb_string_t gmb_array_join(gmb_string_t* str_arr, gmb_int_t n, const char* sep) {
    if (!str_arr || n <= 0) return gmb_string_alloc("");
    const char* s = sep ? sep : "";
    size_t sep_len = strlen(s);
    size_t total = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        total += (str_arr[i].data ? str_arr[i].length : 0);
        if (i > 0) total += sep_len;
    }
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    size_t off = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        if (i > 0 && sep_len > 0) { memcpy(buf + off, s, sep_len); off += sep_len; }
        size_t ln = str_arr[i].data ? str_arr[i].length : 0;
        if (ln > 0) { memcpy(buf + off, str_arr[i].data, ln); off += ln; }
    }
    buf[off] = '\0';
    gmb_string_t res = gmb_string_alloc(buf);
    free(buf);
    return res;
}

/* ---------------------------------------------------------------------------
 * gmb_array_join_doubles: JOIN di un array di double formattati con
 * "%.15g" (via gmb_double_to_string) e separatore sep. Usato per
 * `DIM x() = {..}; PRINT JOIN(x, ", ")`.
 * --------------------------------------------------------------------------- */
gmb_string_t gmb_array_join_doubles(gmb_double_t* arr, gmb_int_t n, const char* sep) {
    if (!arr || n <= 0) return gmb_string_alloc("");
    const char* s = sep ? sep : "";
    size_t sep_len = strlen(s);
    gmb_string_t tmp;
    size_t total = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        tmp = gmb_double_to_string(arr[i]);
        total += tmp.length;
        if (i > 0) total += sep_len;
    }
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    size_t off = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        if (i > 0 && sep_len > 0) { memcpy(buf + off, s, sep_len); off += sep_len; }
        tmp = gmb_double_to_string(arr[i]);
        if (tmp.length > 0) { memcpy(buf + off, tmp.data, tmp.length); off += tmp.length; }
    }
    buf[off] = '\0';
    gmb_string_t res = gmb_string_alloc(buf);
    free(buf);
    return res;
}

/* ---------------------------------------------------------------------------
 * gmb_array_find_index: ASCAN — ritorna l'indice 0-based del primo elemento per
 * cui la closure predicate ritorna true, o -1 se nessuno. L'elemento passato
 * alla callback è del tipo C dell'array (gmb_string_t / gmb_double_t / gmb_int_t)
 * secondo il flag type (0=stringa, 1=double, 2=int). Semantica allineata alla
 * VM (opcode ASCAN → indice 0-based o -1).
 * --------------------------------------------------------------------------- */
gmb_int_t gmb_array_find_index(void* arr, gmb_int_t n, gmb_fn_t fn, int type) {
    if (!arr || n <= 0 || !fn.fn) return -1;
    if (type == 0) {
        gmb_string_t* a = (gmb_string_t*)arr;
        gmb_int_t (*cb)(void*, gmb_string_t) = (gmb_int_t(*)(void*, gmb_string_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i])) return i;
    } else if (type == 1) {
        gmb_double_t* a = (gmb_double_t*)arr;
        gmb_int_t (*cb)(void*, gmb_double_t) = (gmb_int_t(*)(void*, gmb_double_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i])) return i;
    } else {
        gmb_int_t* a = (gmb_int_t*)arr;
        gmb_int_t (*cb)(void*, gmb_int_t) = (gmb_int_t(*)(void*, gmb_int_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i])) return i;
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * ArrayUtils Chunk/Zip/Pad/Shuffle — parità con common/CommonLib/ArrayUtils.cs
 * Chunk: suddivide in blocchi di chunkSize (ultimo può essere più corto).
 * Zip:   combina N liste in una lista di tuple (minLength).
 * Pad:   ridimensiona con padValue (tronca se newLen < src.count).
 * Shuffle: Fisher-Yates deterministico con seed xorshift64 (seed==0 → 42).
 * --------------------------------------------------------------------------- */
gmb_list_t gmb_list_chunk(gmb_list_t src, gmb_int_t chunkSize) {
    gmb_list_t out = gmb_list_new();
    if (chunkSize <= 0) return out;
    for (gmb_int_t i = 0; i < (gmb_int_t)src.count; i += chunkSize) {
        gmb_int_t sz = chunkSize;
        if (i + sz > (gmb_int_t)src.count) sz = (gmb_int_t)src.count - i;
        gmb_list_t chunk = gmb_list_new();
        for (gmb_int_t j = 0; j < sz; j++) gmb_list_add(&chunk, src.items[i + j]);
        gmb_list_add(&out, gmb_make_val_list_take(&chunk));
        gmb_list_free(&chunk);
    }
    return out;
}

gmb_list_t gmb_list_zip_multi(const gmb_list_t* lists, gmb_int_t nLists) {
    gmb_list_t out = gmb_list_new();
    if (!lists || nLists <= 0) return out;
    gmb_int_t minLen = (gmb_int_t)lists[0].count;
    for (gmb_int_t k = 1; k < nLists; k++) if ((gmb_int_t)lists[k].count < minLen) minLen = (gmb_int_t)lists[k].count;
    for (gmb_int_t i = 0; i < minLen; i++) {
        gmb_list_t tuple = gmb_list_new();
        for (gmb_int_t k = 0; k < nLists; k++) gmb_list_add(&tuple, lists[k].items[i]);
        gmb_list_add(&out, gmb_make_val_list_take(&tuple));
        gmb_list_free(&tuple);
    }
    return out;
}

gmb_list_t gmb_list_pad_copy(gmb_list_t src, gmb_int_t newLen, gmb_val_t pad) {
    gmb_list_t out = gmb_list_new();
    if (newLen < 0) return out;
    for (gmb_int_t i = 0; i < newLen; i++) {
        if (i < (gmb_int_t)src.count) gmb_list_add(&out, src.items[i]);
        else gmb_list_add(&out, pad);
    }
    return out;
}

static uint64_t gmb_xorshift64(uint64_t* s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

void gmb_list_shuffle(gmb_list_t* l, uint64_t seed) {
    if (!l || l->count < 2) return;
    if (seed == 0) seed = 42;
    uint64_t s = seed;
    for (size_t i = l->count - 1; i > 0; i--) {
        uint64_t r = gmb_xorshift64(&s);
        size_t j = (size_t)(r % (i + 1));
        gmb_val_t tmp = l->items[i]; l->items[i] = l->items[j]; l->items[j] = tmp;
    }
}

gmb_list_t gmb_array_chunk_doubles(const gmb_double_t* arr, gmb_int_t n, gmb_int_t chunkSize) {
    gmb_list_t src = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++) gmb_list_add(&src, gmb_make_val_double(arr[i]));
    gmb_list_t out = gmb_list_chunk(src, chunkSize);
    gmb_list_free(&src);
    return out;
}

gmb_list_t gmb_array_chunk_ints(const gmb_int_t* arr, gmb_int_t n, gmb_int_t chunkSize) {
    gmb_list_t src = gmb_list_new();
    for (gmb_int_t i = 0; i < n; i++) gmb_list_add(&src, gmb_make_val_int(arr[i]));
    gmb_list_t out = gmb_list_chunk(src, chunkSize);
    gmb_list_free(&src);
    return out;
}

/* ============================================================================
 * Array Clipper su array NATIVI — parità IL (common/IlBackendRuntime/ArrayHelpers.cs).
 * Convenzioni: indici 0-based, -1 = non trovato, AMAP/AFILTER/AREVERSE/ACLONE/AUNIQUE
 * ritornano NUOVA copia (l'input non è mai mutato). Callback ABI come gmb_list.c:
 * map/filter/count/scan-pred → gmb_double_t(*)(void*, gmb_double_t)
 * reduce                    → gmb_double_t(*)(void*, gmb_double_t, gmb_double_t)
 * ============================================================================ */

/* --- numerici (gmb_double_t*) --- */
gmb_double_t* gmb_array_map(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    if (fn.fn) {
        gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) out[i] = cb(fn.ctx, a ? a[i] : 0.0);
    } else if (a) {
        memcpy(out, a, (size_t)n * sizeof(gmb_double_t));
    } else {
        memset(out, 0, (size_t)n * sizeof(gmb_double_t));
    }
    if (out_n) *out_n = n;
    return out;
}

gmb_double_t* gmb_array_filter(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    gmb_int_t k = 0;
    if (fn.fn) {
        gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) out[k++] = a[i];
    } else {
        memcpy(out, a, (size_t)n * sizeof(gmb_double_t));
        k = n;
    }
    if (out_n) *out_n = k;
    if (k == 0) { free(out); return NULL; }
    return out;
}

/* --- boxed (gmb_val_t* — array di UDT/classe) ---------------------------------
 * Le varianti per primitivi (gmb_double_t) non leggono correttamente una struct
 * boxata. ABI della lambda (parità ASORT con comparatore su UDT, gmb_c99_emit.c):
 *   filter: gmb_double_t(*)(void*, gmb_val_t)          → predicato != 0
 *   map:    gmb_double_t(*)(void*, gmb_val_t)          → valore numerico
 * Il C99Native dichiara gli array di UDT come gmb_val_t* (sb_var_ctype). */
gmb_val_t* gmb_array_filter_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_val_t* out = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!out) return NULL;
    gmb_int_t k = 0;
    if (fn.fn) {
        gmb_double_t (*cb)(void*, gmb_val_t) = (gmb_double_t(*)(void*, gmb_val_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) out[k++] = a[i];
    } else {
        memcpy(out, a, (size_t)n * sizeof(gmb_val_t));
        k = n;
    }
    if (out_n) *out_n = k;
    if (k == 0) { free(out); return NULL; }
    return out;
}

gmb_double_t* gmb_array_map_val_dbl(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    if (fn.fn) {
        gmb_double_t (*cb)(void*, gmb_val_t) = (gmb_double_t(*)(void*, gmb_val_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) out[i] = cb(fn.ctx, a[i]);
    } else {
        memset(out, 0, (size_t)n * sizeof(gmb_double_t));
    }
    if (out_n) *out_n = n;
    return out;
}

/* AMAP su array di UDT con callback UDT->UDT (val->val): preserva l'elemento boxato
 * (AMAP(Catalogo, ApplicaSconto) con ApplicaSconto(P AS Prodotto) AS Prodotto).
 * ABI lambda: gmb_val_t(*)(void*, gmb_val_t) — stessa convenzione ctx-first di
 * gmb_array_filter_val/map_val_dbl. */
gmb_val_t* gmb_array_map_val_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_val_t* out = (gmb_val_t*)malloc((size_t)n * sizeof(gmb_val_t));
    if (!out) return NULL;
    if (fn.fn) {
        gmb_val_t (*cb)(void*, gmb_val_t) = (gmb_val_t(*)(void*, gmb_val_t))fn.fn;
        for (gmb_int_t i = 0; i < n; i++) out[i] = cb(fn.ctx, a[i]);
    } else {
        memcpy(out, a, (size_t)n * sizeof(gmb_val_t));
    }
    if (out_n) *out_n = n;
    return out;
}

/* AREDUCE su array di UDT (gmb_val_t* boxed): accumulatore NUMERICO (gmb_double_t) +
 * elemento boxed. ABI lambda: gmb_double_t(*)(void*, gmb_double_t, gmb_val_t) — parità
 * IL/PCode/VM-C (accumulatore double; senza init → seed = val_to_double(a[0])). */
gmb_double_t gmb_array_reduce_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, int has_init, gmb_double_t init) {
    if (!fn.fn || n <= 0 || !a) return has_init ? init : 0.0;
    gmb_double_t (*cb)(void*, gmb_double_t, gmb_val_t) =
        (gmb_double_t(*)(void*, gmb_double_t, gmb_val_t))fn.fn;
    gmb_double_t acc;
    gmb_int_t i;
    if (has_init) { acc = init; i = 0; }
    else { acc = gmb_val_to_double(a[0]); i = 1; }
    for (; i < n; i++) acc = cb(fn.ctx, acc, a[i]);
    return acc;
}

/* ACOUNT su array di UDT (gmb_val_t* boxed): conta gli elementi che soddisfano il
 * predicato. ABI lambda: gmb_double_t(*)(void*, gmb_val_t). */
gmb_int_t gmb_array_count_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn) {
    if (!fn.fn || n <= 0 || !a) return 0;
    gmb_double_t (*cb)(void*, gmb_val_t) = (gmb_double_t(*)(void*, gmb_val_t))fn.fn;
    gmb_int_t c = 0;
    for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) c++;
    return c;
}

gmb_int_t gmb_array_count(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn) {
    if (!fn.fn || n <= 0 || !a) return 0;
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    gmb_int_t c = 0;
    for (gmb_int_t i = 0; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) c++;
    return c;
}

gmb_double_t gmb_array_reduce(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, int has_init, gmb_double_t init) {
    /* AREDUCE: con init → acc=init, fold su tutti gli elementi (anche n==0 → init).
     * senza init → seed = a[0], fold da i=1; array vuoto → default 0 (parità IL). */
    if (!fn.fn || n <= 0 || !a) return has_init ? init : 0.0;
    gmb_double_t (*cb)(void*, gmb_double_t, gmb_double_t) =
        (gmb_double_t(*)(void*, gmb_double_t, gmb_double_t))fn.fn;
    gmb_double_t acc;
    gmb_int_t i;
    if (has_init) { acc = init; i = 0; }
    else { acc = a[0]; i = 1; }
    for (; i < n; i++) acc = cb(fn.ctx, acc, a[i]);
    return acc;
}

gmb_int_t gmb_array_scan_pred(const gmb_double_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start) {
    if (!fn.fn || n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    gmb_double_t (*cb)(void*, gmb_double_t) = (gmb_double_t(*)(void*, gmb_double_t))fn.fn;
    for (gmb_int_t i = start; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) return i;
    return -1;
}

/* ASCAN su array di UDT (gmb_val_t*): indice del primo elemento che soddisfa il
 * predicato, -1 se nessuno. ABI lambda: gmb_double_t(*)(void*, gmb_val_t). */
gmb_int_t gmb_array_scan_pred_val(const gmb_val_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start) {
    if (!fn.fn || n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    gmb_double_t (*cb)(void*, gmb_val_t) = (gmb_double_t(*)(void*, gmb_val_t))fn.fn;
    for (gmb_int_t i = start; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) return i;
    return -1;
}

gmb_int_t gmb_array_scan_pred_str(const gmb_string_t* a, gmb_int_t n, gmb_fn_t fn, gmb_int_t start) {
    if (!fn.fn || n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    gmb_double_t (*cb)(void*, gmb_string_t) = (gmb_double_t(*)(void*, gmb_string_t))fn.fn;
    for (gmb_int_t i = start; i < n; i++) if (cb(fn.ctx, a[i]) != 0.0) return i;
    return -1;
}

gmb_int_t gmb_array_scan_value(const gmb_double_t* a, gmb_int_t n, gmb_double_t v, gmb_int_t start) {
    if (n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    for (gmb_int_t i = start; i < n; i++) if (a[i] == v) return i;
    return -1;
}

gmb_double_t* gmb_array_reverse_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) out[i] = a[n - 1 - i];
    if (out_n) *out_n = n;
    return out;
}

gmb_double_t* gmb_array_unique(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
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

gmb_double_t* gmb_array_clone(const gmb_double_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_double_t* out = (gmb_double_t*)malloc((size_t)n * sizeof(gmb_double_t));
    if (!out) return NULL;
    memcpy(out, a, (size_t)n * sizeof(gmb_double_t));
    if (out_n) *out_n = n;
    return out;
}

gmb_int_t gmb_array_binary_search(const gmb_double_t* a, gmb_int_t n, gmb_double_t v) {
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

/* --- stringhe (gmb_string_t*): deep copy; uguaglianza byte-level (gmb_string_equals) --- */
static gmb_string_t gmb_ar_str_dup(gmb_string_t s) {
    return gmb_string_alloc(s.data ? s.data : "");
}

gmb_string_t* gmb_array_reverse_copy_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)n, sizeof(gmb_string_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) out[i] = gmb_ar_str_dup(a[n - 1 - i]);
    if (out_n) *out_n = n;
    return out;
}

gmb_string_t* gmb_array_unique_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)n, sizeof(gmb_string_t));
    if (!out) return NULL;
    gmb_int_t k = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        int dup = 0;
        for (gmb_int_t j = 0; j < k; j++) if (gmb_string_equals(out[j], a[i])) { dup = 1; break; }
        if (!dup) out[k++] = gmb_ar_str_dup(a[i]);
    }
    if (out_n) *out_n = k;
    if (k == 0) { free(out); return NULL; }
    return out;
}

gmb_string_t* gmb_array_clone_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t* out_n) {
    if (out_n) *out_n = 0;
    if (n <= 0 || !a) return NULL;
    gmb_string_t* out = (gmb_string_t*)calloc((size_t)n, sizeof(gmb_string_t));
    if (!out) return NULL;
    for (gmb_int_t i = 0; i < n; i++) out[i] = gmb_ar_str_dup(a[i]);
    if (out_n) *out_n = n;
    return out;
}

gmb_int_t gmb_array_scan_value_str(const gmb_string_t* a, gmb_int_t n, const char* v, gmb_int_t start) {
    if (n <= 0 || !a) return -1;
    if (start < 0) start = 0;
    gmb_string_t t = gmb_string_alloc(v ? v : "");
    gmb_int_t res = -1;
    for (gmb_int_t i = start; i < n; i++) if (gmb_string_equals(a[i], t)) { res = i; break; }
    gmb_string_free(&t);
    return res;
}

/* ARRAYS.ASORT (funzione): ordina in place l'array numerico (parity VM: ordina il buffer
 * sorgente e ritorna la lista; il target DIM vi si aliasing con la propria <x>_len). */
static int gmb_cmp_dbl(const void* x, const void* y) {
    double a = *(const double*)x, b = *(const double*)y;
    return (a > b) - (a < b);
}
void gmb_array_sort_inplace(gmb_double_t* a, gmb_int_t n) {
    if (n > 1 && a) qsort(a, (size_t)n, sizeof(gmb_double_t), gmb_cmp_dbl);
}

static int gmb_cmp_str(const void* pa, const void* pb) {
    const gmb_string_t* x = (const gmb_string_t*)pa;
    const gmb_string_t* y = (const gmb_string_t*)pb;
    return strcmp(x->data ? x->data : "", y->data ? y->data : "");
}

void gmb_string_array_sort_inplace(gmb_string_t* a, gmb_int_t n) {
    if (n > 1 && a) qsort(a, (size_t)n, sizeof(gmb_string_t), gmb_cmp_str);
}

/* ASORT con comparatore custom su array di ELEMENTI BOXED (gmb_val_t — array di UDT/classe).
 * `cmp` è la closure (gmb_fn_t) con firma (void*, gmb_val_t, gmb_val_t) -> gmb_double_t.
 * Semantica (parity emitter C# qsort_r / PCodeVM Array.Sort / VM/C): il SEGNO del valore
 * ritornato determina l'ordine — < 0 = a prima di b, > 0 = a dopo b, 0 = uguali. Un
 * comparatore BASIC booleano (`RETURN a.Fatturato > b.Fatturato`) ritorna -1 (TRUE) / 0:
 * TRUE(-1) = "a prima di b", quindi un predicato `a > b` ordina DISCENDENTE.
 * Ordinamento stabile (insertion sort), mai no-op silenzioso. */
void gmb_array_sort_val_cmp(gmb_val_t* a, gmb_int_t n, gmb_fn_t cmp) {
    if (!a || n < 2 || !cmp.fn) return;
    for (gmb_int_t i = 1; i < n; i++) {
        gmb_val_t key = a[i];
        gmb_int_t j = i - 1;
        while (j >= 0) {
            gmb_double_t r = ((gmb_double_t (*)(void*, gmb_val_t, gmb_val_t))cmp.fn)(cmp.ctx, key, a[j]);
            if (!(r < 0)) break;
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

/* ASORT con comparatore custom su array NUMERICO (gmb_double_t): stessa semantica del
 * precedente, closure (void*, gmb_double_t, gmb_double_t) -> gmb_double_t. */
void gmb_array_sort_dbl_cmp(gmb_double_t* a, gmb_int_t n, gmb_fn_t cmp) {
    if (!a || n < 2 || !cmp.fn) return;
    for (gmb_int_t i = 1; i < n; i++) {
        gmb_double_t key = a[i];
        gmb_int_t j = i - 1;
        while (j >= 0) {
            gmb_double_t r = ((gmb_double_t (*)(void*, gmb_double_t, gmb_double_t))cmp.fn)(cmp.ctx, key, a[j]);
            if (!(r < 0)) break;
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

/* ARRAYS.APAD: copia in un buffer di lunghezza `want` (pad a destra con `pad` se piu'
 * lungo; tronca se want < n). Ritorna sempre allocazione di `want` elementi (want>=0). */
gmb_double_t* gmb_array_pad_copy(const gmb_double_t* a, gmb_int_t n, gmb_int_t want, double pad, gmb_int_t* out_n) {
    if (want < 0) want = 0;
    gmb_double_t* o = (gmb_double_t*)malloc((size_t)want * sizeof(gmb_double_t) + 1);
    if (!o) { *out_n = 0; return NULL; }
    gmb_int_t cp = want < n ? want : n;
    for (gmb_int_t i = 0; i < cp; i++) o[i] = a ? a[i] : pad;
    for (gmb_int_t i = cp; i < want; i++) o[i] = pad;
    *out_n = want;
    return o;
}
gmb_string_t* gmb_array_pad_copy_strings(const gmb_string_t* a, gmb_int_t n, gmb_int_t want, const char* pad, gmb_int_t* out_n) {
    if (want < 0) want = 0;
    gmb_string_t* o = (gmb_string_t*)malloc((size_t)want * sizeof(gmb_string_t) + 1);
    if (!o) { *out_n = 0; return NULL; }
    gmb_int_t cp = want < n ? want : n;
    for (gmb_int_t i = 0; i < cp; i++) o[i] = a ? gmb_string_alloc(a[i].data) : gmb_string_alloc(pad ? pad : "");
    for (gmb_int_t i = cp; i < want; i++) o[i] = gmb_string_alloc(pad ? pad : "");
    *out_n = want;
    return o;
}
