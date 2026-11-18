/* ============================================================================
 * gmb_core.c — stringhe, valori (gmb_val_t), oggetti boxed.
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include "gmb_memstats.h"
#include "gmb_trace.h"
#include <strings.h>

/* Stringhe contate (VM/C): vedi gmb_make_val_string_counted in gmb_runtime.h. */
#define GMB_STR_COUNTED_MAGIC 0x6D737472
static int32_t* gmb_str_counted_hdr(char* data) { return (int32_t*)(void*)(data - 16); }

gmb_string_t gmb_string_alloc(const char* str) {
    gmb_string_t s;
    if (!str) {
        s.data = (char*)calloc(1, 1);
        s.length = 0;
        s.ref_count = 1;
        if (s.data && gmb_mem_on()) gmb_mem_track(s.data, 1, GMB_MEM_STRING);
        return s;
    }
    s.length = strlen(str);
    s.data = (char*)malloc(s.length + 1);
    memcpy(s.data, str, s.length + 1);
    s.ref_count = 1;
    if (s.data && gmb_mem_on()) gmb_mem_track(s.data, s.length + 1, GMB_MEM_STRING);
    return s;
}

/* Stringa da (char*, len): binario-safe. len < 0 => NUL-terminated (strlen). ABI .gmblib
 * "comoda": i simboli esportati prendono const char* + len invece del gmb_string_t struct. */
gmb_string_t gmb_string_from_bytes(const char* data, gmb_int_t len) {
    gmb_string_t s;
    if (!data) { s.data = (char*)calloc(1, 1); s.length = 0; s.ref_count = 1;
                 if (s.data && gmb_mem_on()) gmb_mem_track(s.data, 1, GMB_MEM_STRING);
                 return s; }
    size_t n = (len < 0) ? strlen(data) : (size_t)len;
    s.data = (char*)malloc(n + 1);
    if (n) memcpy(s.data, data, n);
    s.data[n] = 0;
    s.length = n;
    s.ref_count = 1;
    if (s.data && gmb_mem_on()) gmb_mem_track(s.data, n + 1, GMB_MEM_STRING);
    return s;
}

gmb_string_t gmb_string_concat(gmb_string_t s1, gmb_string_t s2) {
    gmb_string_t res;
    res.length = s1.length + s2.length;
    res.data = (char*)malloc(res.length + 1);
    if (s1.data && s1.length > 0) memcpy(res.data, s1.data, s1.length);
    if (s2.data && s2.length > 0) memcpy(res.data + s1.length, s2.data, s2.length);
    res.data[res.length] = '\0';
    res.ref_count = 1;
    if (res.data && gmb_mem_on()) gmb_mem_track(res.data, res.length + 1, GMB_MEM_STRING);
    return res;
}

/* Concatenazione che LIBERA gli operandi quando sono temporanei (nessun altro proprietario).
 * `free_a`/`free_b` li decide l'emitter (C99) in base alla forma dell'espressione: un
 * letterale o il risultato di una call/concat è un buffer nuovo e usa-e-getta, una variabile
 * no. Serve a non lasciar vivo un temporaneo per ogni `+` dentro un ciclo: è la parte
 * "leak per iterazione" delle stringhe (2026-11-13). */
gmb_string_t gmb_str_concat_owned(gmb_string_t a, int free_a, gmb_string_t b, int free_b) {
    gmb_string_t r = gmb_string_concat(a, b);
    if (free_a) gmb_string_free(&a);
    if (free_b) gmb_string_free(&b);
    return r;
}

/* (2026-09-07) rappresentazione testo di un byte[] come lista decimale `[144, 1, ...]`
 * (parity IL/PCode/VM-C: PRINT di un byte[]/digest = `[b0, b1, ...]`, non hex). */
gmb_string_t gmb_bytes_dump(gmb_string_t b) {
    if (!b.data || b.length == 0) return gmb_string_alloc("[]");
    size_t cap = b.length * 5 + 3;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("[]");
    size_t o = 0; buf[o++] = '[';
    for (size_t i = 0; i < b.length; i++) {
        if (i > 0) { buf[o++] = ','; buf[o++] = ' '; }
        int c = snprintf(buf + o, cap - o, "%u", (unsigned)(unsigned char)b.data[i]);
        if (c < 0) break;
        o += (size_t)c;
    }
    buf[o++] = ']'; buf[o] = '\0';
    gmb_string_t r; r.data = buf; r.length = o; r.ref_count = 1;
    return r;
}

void gmb_string_free(gmb_string_t* s) {
    if (s && s->data && s->ref_count == GMB_STR_COUNTED_MAGIC) {
        /* stringa contata: il creatore (rc 0, fluttuante) la libera; se e' trattenuta da un contenitore
         * (rc > 0) il contenitore ne e' il proprietario e questa chiamata non fa nulla. */
        int32_t* h = gmb_str_counted_hdr(s->data);
        if (*h == 0) { if (gmb_mem_on()) gmb_mem_untrack((void*)h); free((char*)h); }
        s->data = NULL; s->length = 0;
        return;
    }
    if (s && s->data) {
        if (gmb_mem_on()) gmb_mem_untrack(s->data);
        free(s->data);
        s->data = NULL;
        s->length = 0;
    }
}

gmb_string_t gmb_int_to_string(gmb_int_t val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)val);
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_double_to_string(gmb_double_t val) {
    char buf[64];
    /* Parità col formato .NET (invariant): NaN/Infinity/-Infinity, non nan/inf di %g.
     * Precisione: %.15g (G15, 15 cifre significative) — stesso formato invariante usato da
     * IL (common/IlBackendRuntime/StringHelpers.cs: DoubleToInvariantString → "G15") e da
     * OGNI altro punto del runtime C condiviso che formatta un double (gmb_console.c,
     * gmb_dict.c, gmb_math.c, gmb_expr.c: tutti %.15g). Prima: %.14g qui, UNA cifra in meno
     * — divergeva da IL sull'ultima cifra per qualunque valore che ne richieda 15 per essere
     * rappresentato (es. una divisione non esatta: 245/6 → "40.833333333333" invece di
     * "40.8333333333333"), pur essendo identico per la maggioranza dei valori "corti". */
    if (isnan(val)) return gmb_string_alloc("NaN");
    if (isinf(val)) return gmb_string_alloc(val > 0 ? "Infinity" : "-Infinity");
    if (val == 0.0) val = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
snprintf(buf, sizeof(buf), "%.15g", val);
    return gmb_string_alloc(buf);
}

/* Semantica STR$/CSTR di PCodeVM su variabile numerica: spazio iniziale per
 * valori non negativi (parity verificata: CSTR(x)=` 10`, CSTR(z)=-3 → `-3`).
 * Stessa correzione di precisione di gmb_double_to_string sopra (%.15g, non %.14g). */
gmb_string_t gmb_str_basic(gmb_double_t val) {
    char buf[80];
    int n = 0;
    if (val >= 0) buf[n++] = ' ';
    if (val == 0.0) val = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
snprintf(buf + n, sizeof(buf) - (size_t)n, "%.15g", val);
    return gmb_string_alloc(buf);
}

gmb_int_t gmb_string_to_int(gmb_string_t s) {
    if (!s.data) return 0;
    /* TRUE/FALSE (formato WRITE # canonico dei booleani): -1/0. */
    if (strcasecmp(s.data, "TRUE") == 0) return -1;
    if (strcasecmp(s.data, "FALSE") == 0) return 0;
    return (gmb_int_t)strtoll(s.data, NULL, 10);
}

gmb_double_t gmb_string_to_double(gmb_string_t s) {
    if (!s.data) return 0.0;
    /* TRUE/FALSE (formato WRITE # canonico dei booleani): -1/0. */
    if (strcasecmp(s.data, "TRUE") == 0) return -1.0;
    if (strcasecmp(s.data, "FALSE") == 0) return 0.0;
    return strtod(s.data, NULL);
}

gmb_bool_t gmb_string_equals(gmb_string_t a, gmb_string_t b) {
    if (!a.data || !b.data) return a.data == b.data;
    return strcmp(a.data, b.data) == 0;
}

/* Confronto relazionale ORDINALE tra stringhe (parity IL/PCodeVM
 * string.CompareOrdinal): -1/0/+1 come strcmp. NULL si tratta come "".
 * Unica sorgente per VM/C, C99Native (§0bis); gli emettitori
 * la CHIAMANO, mai logica inline (bug 2026-10-01: `<`/`>` tra stringhe
 * emetteva l'operatore C nativo su gmb_string_t → C invalido). */
int gmb_string_compare(gmb_string_t a, gmb_string_t b) {
    const char* sa = a.data ? a.data : "";
    const char* sb = b.data ? b.data : "";
    return strcmp(sa, sb);
}

/* ---- Stringhe contate (VM/C) ---- header di 16 byte prima di data: [int32 rc][pad]. Marcatore in ref_count. */

gmb_val_t gmb_make_val_string_counted_n(const char* s, size_t n) {
    gmb_val_t v; v.kind = GMB_TYPE_STRING;
    char* base = (char*)malloc(16 + n + 1);
    if (!base) { v.v_string = gmb_string_alloc(""); return v; }
    *(int32_t*)(void*)base = 0;
    char* d = base + 16;
    if (s && n) memcpy(d, s, n);
    d[n] = '\0';
    v.v_string.data = d; v.v_string.length = n; v.v_string.ref_count = GMB_STR_COUNTED_MAGIC;
    if (gmb_mem_on()) gmb_mem_track(base, 16 + n + 1, GMB_MEM_STRING);
    return v;
}
gmb_val_t gmb_make_val_string_counted(const char* s) { return gmb_make_val_string_counted_n(s, s ? strlen(s) : 0); }

void gmb_val_discard(gmb_val_t v) {
    /* Scarto del CREATORE di un valore temporaneo: libera solo una stringa contata ancora fluttuante (rc 0).
     * Qualunque altro valore (stringhe normali, oggetti, stringhe gia' trattenute da un contenitore) non e' toccato. */
    if (v.kind == GMB_TYPE_STRING && v.v_string.ref_count == GMB_STR_COUNTED_MAGIC && v.v_string.data) {
        int32_t* h = gmb_str_counted_hdr(v.v_string.data);
        if (*h == 0) { if (gmb_mem_on()) gmb_mem_untrack((void*)h); free((char*)h); }
    }
}

void gmb_val_retain(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING) {
        if (v.v_string.ref_count == GMB_STR_COUNTED_MAGIC && v.v_string.data) (*gmb_str_counted_hdr(v.v_string.data))++;
        return;
    }
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (o->release_fn) o->refcount++;
}

void gmb_val_release(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING) {
        if (v.v_string.ref_count == GMB_STR_COUNTED_MAGIC && v.v_string.data) {
            int32_t* h = gmb_str_counted_hdr(v.v_string.data);
            if (*h > 0 && --(*h) == 0) { if (gmb_mem_on()) gmb_mem_untrack((void*)h); free((char*)h); }
        }
        return;
    }
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (o->release_fn && o->refcount > 0 && --o->refcount == 0) o->release_fn(o);
}

gmb_val_t gmb_val_nil(void) {
    gmb_val_t v;
    v.kind = GMB_TYPE_NIL;
    v.v_int = 0;
    return v;
}

gmb_val_t gmb_make_val_int(gmb_int_t i) { gmb_val_t v; v.kind = GMB_TYPE_INT; v.v_int = i; return v; }
gmb_val_t gmb_make_val_double(gmb_double_t d) { gmb_val_t v; v.kind = GMB_TYPE_DOUBLE; v.v_double = d; return v; }
/* CURRENCY: numerico (v_double) con `scale` cifre decimali per la resa (campo DBF N/F). */
gmb_val_t gmb_make_val_currency(gmb_double_t d, int scale) {
    gmb_val_t v; v.kind = GMB_TYPE_CURRENCY; v.v_double = d;
    if (scale < 0) scale = 0; if (scale > 15) scale = 15;
    v.scale = (int8_t)scale;
    return v;
}
/* DATETIME: OADate nel campo v_double, ma kind distinto (PRINT leggibile, JSON ISO-8601). */
gmb_val_t gmb_make_val_datetime(gmb_double_t oa) { gmb_val_t v; v.kind = GMB_TYPE_DATETIME; v.v_double = oa; return v; }
gmb_val_t gmb_make_val_bool(gmb_bool_t b) { gmb_val_t v; v.kind = GMB_TYPE_BOOL; v.v_bool = b; return v; }
gmb_val_t gmb_make_val_string(const char* s) { gmb_val_t v; v.kind = GMB_TYPE_STRING; v.v_string = gmb_string_alloc(s); return v; }

gmb_obj_t* gmb_obj_new(void* data) {
    gmb_obj_t* o = (gmb_obj_t*)malloc(sizeof(gmb_obj_t));
    if (!o) return NULL;
    o->refcount = 0;    /* prima spazzatura di malloc: la VM testa kind/refcount */
    o->kind = 0;
    o->release_fn = NULL;
    o->data = data;
    o->type_name = "";
    o->dtor = NULL;
    return o;
}

gmb_obj_t* gmb_obj_new_typed(void* data, const char* type_name) {
    gmb_obj_t* o = gmb_obj_new(data);
    if (o) o->type_name = type_name;
    return o;
}

gmb_val_t gmb_make_val_obj(gmb_obj_t* o) {
    gmb_val_t v;
    v.kind = GMB_TYPE_OBJECT;
    v.v_ptr = o;
    return v;
}

/* Box di un PUNTATORE GREZZO (non-gmb_obj_t) in un val OBJECT (v_ptr, senza refcount/
 * dtore). Usato per mettere in una variabile ANY un valore di tipo non-oggetto gestito,
 * es. un gmb_stringbuilder_t* (DIM obj AS ANY : obj = sb). */
gmb_val_t gmb_make_val_ptr(void* p) {
    gmb_val_t v;
    v.kind = GMB_TYPE_OBJECT;
    v.v_ptr = p;
    return v;
}

/* Puntatore grezzo dentro un val OBJECT (NULL se non è un OBJECT). */
void* gmb_val_as_ptr(gmb_val_t v) {
    return v.kind == GMB_TYPE_OBJECT ? v.v_ptr : NULL;
}

void* gmb_obj_data(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return o ? o->data : NULL;
}

const char* gmb_obj_type(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT) return "";
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return o ? o->type_name : "";
}

// Copia VALORE di un oggetto boxed tipizzato (semantica UDT: l'assegnamento copia,
// non aliasa). Duplica il blocco data (sizeof noto al chiamante) in un nuovo oggetto
// con lo stesso type_name; se v non è un oggetto con data, la restituisce invariata.
gmb_val_t gmb_obj_copy_typed(gmb_val_t v, size_t sz) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr || sz == 0) return v;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o->data) return v;
    void* d = malloc(sz);
    if (!d) return v;
    memcpy(d, o->data, sz);
    return gmb_make_val_obj(gmb_obj_new_typed(d, o->type_name));
}

// ── Distruttore istanze transpiler (2026-09-03) ──────────────────────────────
// La callback (generata dal transpiler C per ogni classe con SUB DESTRUCTOR) riceve
// data = puntatore alla struct dei campi e invoca il metodo BASIC; qui registriamo/
// azzeriamo lo slot e lo invochiamo una sola volta (nessuna doppia chiamata).
void gmb_obj_set_dtor(gmb_val_t v, void (*dtor)(void*)) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o) return;
    o->dtor = dtor;
}
void gmb_obj_run_dtor(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o || !o->dtor || !o->data) return;
    void (*dtor)(void*) = o->dtor;
    o->dtor = NULL;          /* niente doppia chiamata */
    dtor(o->data);
}
gmb_bool_t gmb_obj_has_dtor(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return 0;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    return o && o->dtor ? 1 : 0;
}

gmb_bool_t gmb_val_equals(gmb_val_t a, gmb_val_t b) {
    /* Confronto NUMERICO cross-kind (INT vs DOUBLE): in BASIC `n = 42` è vero anche se un
     * lato è un INT boxed e l'altro un DOUBLE (parità PCode/VM-C, che confrontano numericamente).
     * Prima `a.kind != b.kind → 0` rendeva falsi confronti come `double(42) = int(42)`. */
    if (a.kind != b.kind) {
        /* BOOL incluso: in BASIC il booleano È -1/0 (canone TRUE=-1), quindi
         * `json("k") = -1` / `TRUE = 1` devono valere come il confronto numerico
         * (parità IL/PCode, che confrontano per valore). Senza, un BOOL proveniente da
         * JSON.PARSE era diverso da -1 solo perché di kind diverso. */
        int a_num = a.kind == GMB_TYPE_INT || a.kind == GMB_TYPE_DOUBLE || a.kind == GMB_TYPE_DATETIME || a.kind == GMB_TYPE_CURRENCY || a.kind == GMB_TYPE_BOOL;
        int b_num = b.kind == GMB_TYPE_INT || b.kind == GMB_TYPE_DOUBLE || b.kind == GMB_TYPE_DATETIME || b.kind == GMB_TYPE_CURRENCY || b.kind == GMB_TYPE_BOOL;
        if (a_num && b_num)
            return gmb_val_to_double(a) == gmb_val_to_double(b);
        return 0;
    }
    switch (a.kind) {
        case GMB_TYPE_NIL: return 1;
        case GMB_TYPE_INT: return a.v_int == b.v_int;
        case GMB_TYPE_DOUBLE: return a.v_double == b.v_double;
        case GMB_TYPE_CURRENCY: return a.v_double == b.v_double;
        case GMB_TYPE_DATETIME: return a.v_double == b.v_double;
        /* Numerico, non bit-a-bit: gmb_make_val_bool accetta 1 e -1 come VERO (i
         * chiamanti del runtime usano entrambi), quindi TRUE(1) = TRUE(-1) deve essere
         * vero come in IL/PCode. */
        case GMB_TYPE_BOOL: return gmb_val_to_double(a) == gmb_val_to_double(b);
        case GMB_TYPE_STRING: return gmb_string_equals(a.v_string, b.v_string);
        case GMB_TYPE_OBJECT: return a.v_ptr == b.v_ptr;
        default: return a.v_ptr == b.v_ptr;
    }
}

/* Confronto ORDINATO tra due gmb_val_t: -1/0/1. Se uno dei due è STRINGA confronta come
 * stringhe (gmb_string_compare, parità Clipper/IL/PCode per i confronti C), altrimenti
 * numericamente (gmb_val_to_double). Serve ai relazionali < <= > >= quando un operando è un
 * gmb_val_t (campo nudo DBF, val dinamico): senza, il C confrontava la STRUCT raw (bug
 * `BALANCE > 50` → sempre falso). */
gmb_int_t gmb_val_compare(gmb_val_t a, gmb_val_t b) {
    if (a.kind == GMB_TYPE_STRING || b.kind == GMB_TYPE_STRING) {
        gmb_string_t sa = gmb_val_to_string(a);
        gmb_string_t sb = gmb_val_to_string(b);
        return gmb_string_compare(sa, sb);
    }
    double da = gmb_val_to_double(a);
    double db = gmb_val_to_double(b);
    return da < db ? -1 : (da > db ? 1 : 0);
}

/* True se il dict boxato è "tuple-shaped": chiavi decimali sequenziali "0".."n-1"
 * nell'ordine di inserimento (esattamente come le costruisce gmb_val_tuple, gmb_dict.c).
 * Distingue una TUPLE (da stampare come lista `[e0, e1, ...]`, parità coi backend
 * gestiti) da un dict JSON genuino con chiavi stringa qualsiasi (resta `<object>`,
 * comportamento invariato). */
static int gmb_dict_is_tuple_shaped(const gmb_dict_t* d) {
    for (size_t i = 0; i < d->count; i++) {
        char expect[32];
        snprintf(expect, sizeof(expect), "%zu", i);
        if (!d->items[i].key || strcmp(d->items[i].key, expect) != 0) return 0;
    }
    return 1;
}

/* Resa di un CURRENCY (numerico con scala): `scale` cifre decimali, `lead` = spazio iniziale
 * se >= 0 (come gmb_str_basic). NON usa %.15g: perderebbe gli zeri finali ("100" invece di
 * "100.00"); %.*f li preserva. Unica sorgente per PRINT (lead=0) e CSTR (lead=1). */
static gmb_string_t gmb_currency_string(gmb_double_t val, int scale, int lead) {
    char buf[80];
    int n = 0;
    if (lead && val >= 0) buf[n++] = ' ';
    if (scale < 0) scale = 0;
    if (scale > 15) scale = 15;
    snprintf(buf + n, sizeof(buf) - (size_t)n, "%.*f", scale, val);
    return gmb_string_alloc(buf);
}

// Serializza un gmb_val_t in gmb_string_t (per PRINT # di variabili JSON/val).
gmb_string_t gmb_val_to_string(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: return gmb_int_to_string(v.v_int);
        case GMB_TYPE_DOUBLE: return gmb_double_to_string(v.v_double);
        case GMB_TYPE_CURRENCY: return gmb_currency_string(v.v_double, v.scale, 0);
        case GMB_TYPE_DATETIME: return gmb_dt_datetime_to_string(v.v_double);
        case GMB_TYPE_BOOL: return gmb_string_alloc(v.v_bool ? "TRUE" : "FALSE");
        case GMB_TYPE_STRING: return v.v_string;
        case GMB_TYPE_OBJECT: {
            /* TUPLE annidata (es. `((1,2),(3,4))`): gmb_val_tuple la boxa come dict con
             * chiavi "0","1",... — senza questo ramo cadeva nel fallback "<object>"
             * generico, divergendo dalla resa `[1, 2]` degli altri backend (bug
             * tuple/parita transpiler-native). I dict JSON genuini (chiavi stringa
             * qualsiasi) restano "<object>": nessuna modifica al loro comportamento. */
            if (v.v_ptr) {
                gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
                if (o->data && o->type_name && strstr(o->type_name, "dict")) {
                    gmb_dict_t* d = (gmb_dict_t*)o->data;
                    if (gmb_dict_is_tuple_shaped(d)) {
                        gmb_string_t out = gmb_string_alloc("[");
                        for (size_t i = 0; i < d->count; i++) {
                            if (i > 0) { gmb_string_t comma = gmb_string_alloc(", "); out = gmb_string_concat(out, comma); }
                            gmb_string_t elem = gmb_val_to_string(d->items[i].value);
                            out = gmb_string_concat(out, elem);
                        }
                        gmb_string_t close = gmb_string_alloc("]");
                        return gmb_string_concat(out, close);
                    }
                }
            }
            return gmb_string_alloc("<object>");
        }
        default: return gmb_string_alloc("<nil>");
    }
}

/* Stringa di un gmb_val_t per ASSEGNAZIONE a un campo/variabile STRING: NIL → "".
 * gmb_val_to_string(NIL) rende "<nil>" (giusto in stampa, sbagliato in un campo:
 * un JSON null assegnato a STRING deve dare stringa vuota, come nelle gambe managed). */
gmb_string_t gmb_val_to_string_or_empty(gmb_val_t v) {
    if (v.kind == GMB_TYPE_NIL) return gmb_string_alloc("");
    return gmb_val_to_string(v);
}

/* CSTR/CONVERSIONS.CSTR di un gmb_val_t (campo JSON/ANY dinamico): replica
 * CommonLib.ConversionUtils.CStr — numerico → gmb_str_basic (spazio iniziale se >= 0,
 * per gestire il segno), booleano → "True"/"False", altro (stringa/oggetto) →
 * gmb_val_to_string. Usato dagli emitter C99Native per CSTR(campo JSON):
 * senza, un numero JSON rendeva SENZA lo spazio iniziale (divergenza dal canone, che
 * CSTR(2500)=" 2500" — verificato su IL/PCode/VM-C e minitest canonico). */
gmb_string_t gmb_cstr_val(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: return gmb_str_basic((gmb_double_t)v.v_int);
        case GMB_TYPE_DOUBLE: return gmb_str_basic(v.v_double);
        case GMB_TYPE_CURRENCY: return gmb_currency_string(v.v_double, v.scale, 1);
        case GMB_TYPE_BOOL: return gmb_string_alloc(v.v_bool ? "True" : "False");
        default: return gmb_val_to_string(v);
    }
}

// Tipo BASIC di un gmb_val_t (per TYPENAME). Nota: il C99 collassa i tipi numerici
// (int64/double) — INT → "LONG" (tipo C gmb_int_t), parità col valore IL boxed long.
gmb_string_t gmb_val_typename(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_NIL: return gmb_string_alloc("NIL");
        case GMB_TYPE_INT: return gmb_string_alloc("LONG");
        case GMB_TYPE_DOUBLE: return gmb_string_alloc("DOUBLE");
        case GMB_TYPE_CURRENCY: return gmb_string_alloc("CURRENCY");
        case GMB_TYPE_DATETIME: return gmb_string_alloc("DATETIME");
        case GMB_TYPE_BOOL: return gmb_string_alloc("BIT");
        case GMB_TYPE_STRING: return gmb_string_alloc("STRING");
        case GMB_TYPE_OBJECT: {
            const char* tn = gmb_obj_type(v);
            return gmb_string_alloc(tn && *tn ? tn : "OBJECT");
        }
        default: return gmb_string_alloc("OBJECT");
    }
}

/* TYPEINFO(value): nome del tipo (parità IlRuntime.TypeInfo). Come gmb_val_typename, ma per un
 * UDT boxato (`gmb_udt_<Nome>`) ritorna il nome del TYPE senza il prefisso (C# → "Person").
 * TYPEINFO.HAS(value): 1 se non NIL (parità IlRuntime.TypeInfoHas).
 * METHODINFO/PROPERTYINFO: il runtime C non ha il TypeSchemaRegistry (reflection) del C# →
 * nessuno schema → "{}" come IlRuntime.TypeInfoJson quando il tipo non è trovato. */
gmb_string_t gmb_typeinfo(gmb_val_t v) {
    gmb_string_t t = gmb_val_typename(v);
    if (t.data && strncmp(t.data, "gmb_udt_", 8) == 0) return gmb_string_alloc(t.data + 8);
    return t;
}
gmb_int_t gmb_typeinfo_has(gmb_val_t v) { return v.kind == GMB_TYPE_NIL ? 0 : 1; }

/* TYPEINFO(value) con la semantica di IlRuntime.TypeInfo (NON TypeName): un dict boxed →
 * "DICT" (o il nome del TYPE se è un UDT con __class__.Name), una lista (anche array boxato)
 * → "LIST OF <TypeInfo(elem)>" ("LIST OF ANY" se vuota). Diverso da gmb_val_typename
 * (che dà "CURRENCY"/"LONG"/"DICTIONARY…"), usato da vmc/c99/c99n per SYSTEM.TYPEINFO. */
gmb_string_t gmb_typeinfo_val(gmb_val_t v)
{
    switch (v.kind) {
        case GMB_TYPE_NIL: return gmb_string_alloc("NIL");
        case GMB_TYPE_BOOL: return gmb_string_alloc("BIT");
        case GMB_TYPE_CURRENCY: return gmb_string_alloc("CURRENCY");
        case GMB_TYPE_DATETIME: return gmb_string_alloc("DATETIME");
        case GMB_TYPE_INT:
        case GMB_TYPE_DOUBLE: return gmb_string_alloc("DOUBLE");
        case GMB_TYPE_STRING: return gmb_string_alloc("STRING");
        case GMB_TYPE_OBJECT: {
            const char* tn = gmb_obj_type(v);
            if (tn && strcmp(tn, "gmb_list") == 0) {
                gmb_list_t l = gmb_val_as_list(v);
                if (gmb_list_count(l) <= 0) return gmb_string_alloc("LIST OF ANY");
                gmb_string_t e = gmb_typeinfo_val(gmb_list_get(l, 0));
                char buf[512];
                snprintf(buf, sizeof(buf), "LIST OF %s", e.data ? e.data : "ANY");
                gmb_string_free(&e);
                return gmb_string_alloc(buf);
            }
            if (tn && strcmp(tn, "gmb_dict") == 0) {
                gmb_dict_t d = gmb_val_as_dict(v);
                gmb_val_t cv = gmb_dict_get(d, "__class__");
                if (cv.kind == GMB_TYPE_OBJECT && cv.v_ptr) {
                    gmb_dict_t cd = gmb_val_as_dict(cv);
                    gmb_val_t nv = gmb_dict_get(cd, "Name");
                    if (nv.kind == GMB_TYPE_STRING && nv.v_string.data && nv.v_string.data[0])
                        return gmb_string_alloc(nv.v_string.data);
                }
                return gmb_string_alloc("DICT");
            }
            return gmb_string_alloc(tn && *tn ? tn : "OBJECT");
        }
        default: return gmb_string_alloc("OBJECT");
    }
}

/* VALTYPE(expr) — lettera di tipo Clipper: C/N/L/U/A/O (parità ClipperStrings.ValType).
 * Il C non ha DateTime first-class (le date sono stringhe/oggetti → C/O). Un dict è una
 * ICollection nel C# → "A"; qui un OBJECT con type-name list/dict mappa "A". */
const char* gmb_valtype_letter(int kind);

const char* gmb_valtype_letter(int kind) {
    switch (kind) {
        case GMB_TYPE_NIL: return "U";
        case GMB_TYPE_BOOL: return "L";
        case GMB_TYPE_INT: case GMB_TYPE_DOUBLE: case GMB_TYPE_CURRENCY: return "N";
        case GMB_TYPE_STRING: return "C";
        case GMB_TYPE_ARRAY: return "A";
        case GMB_TYPE_DATETIME: return "D";   /* parita ClipperStrings.ValType */
        default: return "O";
    }
}

gmb_string_t gmb_val_valtype(gmb_val_t v) {
    if (v.kind == GMB_TYPE_OBJECT) {
        const char* tn = gmb_obj_type(v);
        if (tn && (strstr(tn, "list") || strstr(tn, "List") || strstr(tn, "dict") || strstr(tn, "Dict")))
            return gmb_string_alloc("A");
        return gmb_string_alloc("O");
    }
    return gmb_string_alloc(gmb_valtype_letter((int)v.kind));
}

// Unboxing numerico di gmb_val_t (per CHOOSE/IIF/ritorni val): int/double/bool → double,
// stringa → parsing numerico, altrimenti 0.
gmb_double_t gmb_val_to_double(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: return (gmb_double_t)v.v_int;
        case GMB_TYPE_DOUBLE: return v.v_double;
        case GMB_TYPE_CURRENCY: return v.v_double;
        case GMB_TYPE_DATETIME: return v.v_double;   /* OADate */
        case GMB_TYPE_BOOL: return v.v_bool ? -1.0 : 0.0;   /* convenzione VMBASIC */
        case GMB_TYPE_STRING: return gmb_string_to_double(v.v_string);
        default: return 0.0;
    }
}

gmb_int_t gmb_val_to_int(gmb_val_t v) {
    switch (v.kind) {
        case GMB_TYPE_INT: return v.v_int;
        case GMB_TYPE_DOUBLE: return (gmb_int_t)v.v_double;
        case GMB_TYPE_CURRENCY: return (gmb_int_t)v.v_double;
        case GMB_TYPE_DATETIME: return (gmb_int_t)v.v_double;   /* OADate */
        case GMB_TYPE_BOOL: return v.v_bool ? -1 : 0;   /* convenzione VMBASIC */
        case GMB_TYPE_STRING: return gmb_string_to_int(v.v_string);
        default: return 0;
    }
}

// Boxing di un UDT struct (gmb_udt_*) in gmb_val_t: copia la struct su heap (dimensione
// nota dal chiamante via sizeof) e la avvolge come oggetto type_name "gmb_udt_<Nome>".
void* gmb_udt_box(const void* src, size_t size, const char* type_name) {
    if (!src || size == 0) return NULL;
    void* copy = malloc(size);
    if (!copy) return NULL;
    memcpy(copy, src, size);
    return gmb_obj_new_typed(copy, type_name);
}

// MATRIX.CREATE(r, c) / VECTOR.CREATE(...): restituisce un handle LONG progressivo.
// Il backend C99 non modella le matrici 4x4 (comandi MATRIX.* sono no-op).
static gmb_int_t g_matrix_handle_counter = 0;

/* ----------------------------------------------------------------------------
 * Error handling (ON ERROR GOTO/RESUME, TRY/CATCH) — stato errore per-thread.
 * gmb_raise_error fa longjmp all'handler registrato (se attivo), altrimenti termina.
 * ---------------------------------------------------------------------------- */
static __thread int g_err_number = 0;
static __thread const char* g_err_description = "";
static __thread long g_err_line = 0;
static __thread jmp_buf* g_err_jmp = NULL;
static __thread jmp_buf* g_err_resume = NULL;
/* Modalità ON ERROR RESUME NEXT (1 = attiva). I guard per-istruzione installano solo il
 * jmp_buf di ripresa; questa flag dice a gmb_raise_error se usarlo. ON ERROR RESUME NEXT
 * la attiva, ON ERROR GOTO 0 la disattiva. */
static __thread int g_err_resume_mode = 0;
/* Eccezione UTENTE (THROW NEW <Classe>): valore boxed che viaggia con l'errore. Lo slot è
 * "owned" dallo stato finché il CATCH non lo preleva (gmb_err_take_value, move). */
static __thread gmb_val_t g_err_value;
static __thread int g_err_has_value = 0;

int gmb_err_number(void) { return g_err_number; }
const char* gmb_err_description(void) { return g_err_description; }
long gmb_err_line(void) { return g_err_line; }

/* ERR.SourceLineText / ex.SourceLineText: testo della riga che ha sollevato l'errore, letto
 * dal blob del sorgente registrato dal codice generato (gmb_src_text_register). "" quando il
 * sorgente non è stato incorporato (es. eseguibile compilato senza il blob). */
const char* gmb_err_source_line_text(void) { return gmb_src_line_text(g_err_line); }

int gmb_err_has_value(void) { return g_err_has_value; }
/* TRASFERISCE il valore dell'eccezione utente al chiamante: dopo la chiamata lo slot è vuoto
 * (il chiamante ne diventa l'unico detentore — nessun doppio free con i dtor delle classi). */
gmb_val_t gmb_err_take_value(void) {
    gmb_val_t v = g_err_has_value ? g_err_value : gmb_val_nil();
    g_err_value = gmb_val_nil();
    g_err_has_value = 0;
    return v;
}
static void gmb_err_clear_value(void) { g_err_value = gmb_val_nil(); g_err_has_value = 0; }

/* 1 se v è un oggetto con type_name == name (case-insensitive). */
int gmb_val_is_class(gmb_val_t v, const char* name) {
    if (v.kind != GMB_TYPE_OBJECT || !name || !*name) return 0;
    const char* t = gmb_obj_type(v);
    return t && *t && strcasecmp(t, name) == 0;
}

jmp_buf* gmb_err_set_handler(jmp_buf* buf) {
    jmp_buf* prev = g_err_jmp;
    g_err_jmp = buf;
    /* ON ERROR GOTO 0: disattiva anche la modalità RESUME NEXT (altrimenti gli errori
     * successivi continuerebbero a essere ripresi dai guard per-istruzione). */
    if (buf == NULL) g_err_resume_mode = 0;
    return prev;
}

void gmb_err_set_resume(jmp_buf* buf) { g_err_resume = buf; }
jmp_buf* gmb_err_resume_jmp(void) { return g_err_resume; }

/* RESUME NEXT: ATTIVA la modalità e lascia che sia gmb_raise_error a longjmpare al
 * jmp_buf del guard dell'istruzione fallita (installato dal codegen). Prima questa
 * funzione faceva longjmp SUBITO al guard corrente: alla riga `ON ERROR RESUME NEXT`
 * nessun guard è attivo, quindi era un no-op e l'errore successivo terminava il
 * processo (C99Native) invece di riprendere. */
void gmb_err_resume_next(void) {
    /* Due casi:
     *  - ON ERROR RESUME NEXT (setup): nessun guard per-istruzione attivo → attiva la
     *    modalità e lascia che sia gmb_raise_error a longjmpare al guard della prossima
     *    istruzione fallita;
     *  - RESUME NEXT dentro un handler ON ERROR GOTO: il guard dell'istruzione fallita è
     *    ancora attivo (il longjmp all'handler non lo ha ripulito) → riprende subito dopo
     *    quell'istruzione, come prima. */
    if (g_err_resume) longjmp(*g_err_resume, 1);
    g_err_resume_mode = 1;
}

/* Descrizione canonica di un codice d'errore (catalogo condiviso dalle gambe C): serve quando
 * un errore viene sollevato SENZA descrizione (`ERROR 11`), così `ERR.Description` non è vuota
 * e combacia con `BasicException.GetDefaultMessage` lato gestito (IL/PCode). */
const char* gmb_err_default_message(int code) {
    switch (code) {
        case 1: return "NEXT without FOR";
        case 2: return "Syntax error";
        case 3: return "RETURN without GOSUB";
        case 4: return "Out of DATA";
        case 5: return "Illegal function call";
        case 6: return "Overflow";
        case 7: return "Out of memory";
        case 8: return "Undefined line number";
        case 9: return "Subscript out of range";
        case 10: return "Duplicate definition";
        case 11: return "Division by zero";
        case 12: return "Array expected";
        case 13: return "Type mismatch";
        case 14: return "Wrong number of arguments";
        case 15: return "String too long";
        case 16: return "Expression too complex";
        case 17: return "EXIT not within FOR or DO loop";
        case 18: return "Subprogram or function not defined";
        case 19: return "Variable not defined";
        case 20: return "Invalid parameter";
        case 21: return "Illegal quantity";
        case 22: return "Calculation error";
        case 23: return "Invalid logarithm argument";
        case 53: return "File not found";
        case 54: return "Bad file mode";
        case 55: return "File already open";
        case 56: return "Bad file number or handle";
        case 58: return "File already exists";
        case 59: return "Bad record length";
        case 62: return "Input past end of file";
        case 64: return "Bad file name";
        case 67: return "Too many files open";
        case 68: return "Device unavailable";
        case 70: return "Permission denied";
        case 71: return "Disk full";
        case 76: return "Path not found";
        case 91: return "Object reference not set to an instance of an object";
        case 200: return "Internal runtime error";
        /* Stesso fallback di BasicException.GetDefaultMessage lato gestito: un codice fuori
         * catalogo (es. `THROW 42`) deve riportare un testo, non una stringa vuota. */
        default: return "Unspecified runtime error";
    }
}

/* Nucleo condiviso da gmb_raise_error (errore "numerico": azzera un eventuale valore utente
 * rimasto) e gmb_raise_user_error (keep_value=1: il valore è già nello slot). */
static void gmb_raise_error_impl(int number, const char* description, long line, int keep_value) {
    gmb_print_stage_abort();   /* canone: scarta l'output PARZIALE del PRINT in corso (tutti i livelli) */
    /* Descrizione vuota (es. `ERROR 11` senza testo) → catalogo canonico: `ERR.Description`
     * deve riportare il testo dell'errore, non una stringa vuota (parità IL/PCode, dove
     * BasicException.GetDefaultMessage fa la stessa cosa). */
    if (description == NULL || *description == 0) description = gmb_err_default_message(number);
    g_err_number = number;
    g_err_description = description ? description : "";
    g_err_line = line;
    if (!keep_value) gmb_err_clear_value();
    /* STACK TRACE: fotografia della catena PRIMA di qualunque longjmp (dopo il salto i frame
     * delle procedure attraversate non esistono più). La leggono `ERR.StackTrace` e il
     * messaggio fatale qui sotto. */
    gmb_trace_snapshot();
    if (g_err_jmp) {
        longjmp(*g_err_jmp, number);
    }
    /* ON ERROR RESUME NEXT: nessun handler GOTO attivo, ma un guard per-istruzione
     * installato dal codegen → riprende dopo l'istruzione fallita. */
    if (g_err_resume_mode && g_err_resume) {
        longjmp(*g_err_resume, 1);
    }
    fprintf(stderr, "[GMBASIC] ERROR %d: %s (line %ld)\n", number,
            g_err_description, line);
    if (gmb_err_trace()[0]) fprintf(stderr, "%s", gmb_err_trace());
    /* Codice d'uscita canonico per un errore BASIC a runtime NON gestito: 6, allineato a
     * IL/PCode (test_ilrun) e VM/C (gmb_vm_host). Vedi
     * Bugs/(solved) - Semantica-Errore-Builtin-Tra-Backend.md. */
    exit(6);
}

void gmb_raise_error(int number, const char* description, long line) {
    gmb_raise_error_impl(number, description, line, 0);
}

/* THROW NEW <Classe>: il valore lanciato è l'istanza boxed; numero/descrizione/riga seguono
 * il canone degli errori utente (256 = UserDefined, descrizione vuota → "Unspecified runtime
 * error"). Il valore resta nello slot finché il CATCH non lo preleva. */
void gmb_raise_user_error(gmb_val_t value, int number, const char* description, long line) {
    g_err_value = value;
    g_err_has_value = 1;
    gmb_raise_error_impl(number, description, line, 1);
}

/* Guardia del dispatch di metodo: `DIM c AS Cls` senza NEW è un riferimento NIL;
 * invocare un metodo su nil è un errore BASIC CATTURABILE (parità IL/ON ERROR), mai
 * un SIGSEGV (C99Native) né un nil silenzioso (PCode/VM-C) — bug AiWiki
 * CLASS-MethodCall-Su-Nil-Dispatch-Divergente. Ritorna self se è un oggetto valido. */
gmb_val_t* gmb_obj_require(gmb_val_t* self, long line) {
    if (self == NULL || self->kind != GMB_TYPE_OBJECT || self->v_ptr == NULL)
        gmb_raise_error(91, "Object reference not set to an instance of an object", line);
    return self;
}

/* Divisione per zero → ERR 11 "Division by zero" SEMPRE (canone BASIC, identico alle 4 gambe).
 * Con un handler attivo (ON ERROR GOTO / TRY/CATCH, entrambi registrano g_err_jmp) fa longjmp
 * all'handler; senza handler gmb_raise_error stampa l'errore ed esce con il codice canonico 6.
 * NON esiste più il valore "legacy" (+inf/NAN): era proprio ciò che faceva divergere le gambe —
 * su IL/PCode la stessa espressione sollevava ERR 11 e su VM/C restituiva 0/Inf in silenzio.
 * Vedi Bugs/(solved) - Divisione-Per-Zero-Divergente-4-Gambe. */
/* Ramo FREDDO di gmb_arr_check (il confronto sta in linea nell'header, gmb_runtime.h).
 * Se ON ERROR RESUME NEXT riprende da qui, il chiamante in linea restituisce l'indice originale:
 * l'accesso che segue resta l'unico punto che ha bisogno di un valore. */
void gmb_arr_check_fail(gmb_int_t idx, gmb_int_t lo, gmb_int_t hi, long line) {
    (void)idx; (void)lo; (void)hi;
    gmb_raise_error(9, "", line);
}

double gmb_safe_div(double a, double b, long line) {
    if (b == 0) gmb_raise_error(11, "Division by zero", line);
    return a / b;
}

/* MOD su zero → ERR 11 (stesso canone della divisione). */
double gmb_safe_fmod(double a, double b, long line) {
    if (b == 0) gmb_raise_error(11, "Division by zero", line);
    return fmod(a, b);
}

/* Divisione INTERA (`\`) su zero → ERR 11; altrimenti trunc(a/b) come virgola mobile
 * (stessa semantica di OP_INT_DIVIDE della VM e di Math.Truncate su IL/PCode). */
double gmb_safe_int_div(double a, double b, long line) {
    if (b == 0) gmb_raise_error(11, "Division by zero", line);
    return trunc(a / b);
}

/* Subscript fuori range → ERR 9 se handler attivo (longjmp); senza handler ritorna -1
 * e il chiamante applica il no-op legacy (guardia bounds). In range → idx. */
gmb_int_t gmb_safe_index(gmb_int_t idx, gmb_int_t len, long line) {
    if (idx >= 0 && idx < len) return idx;
    if (g_err_jmp) gmb_raise_error(9, "Subscript out of range", line);
    return -1;
}

/* ── TYPEINFO.METHODINFO/PROPERTYINFO: registro schema tipi (parità TypeSchemaRegistry) ── */
#define GMB_TYPE_SCHEMA_MAX 64
typedef struct { char name[128]; char fields[4096]; char methods[4096]; } gmb_type_schema_t;
static gmb_type_schema_t g_type_schemas[GMB_TYPE_SCHEMA_MAX];
static int g_type_schema_count = 0;

static gmb_type_schema_t* type_schema_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_type_schema_count; i++)
        if (strcmp(g_type_schemas[i].name, name) == 0) return &g_type_schemas[i];
    return NULL;
}

int gmb_type_register(const char* name, const char* fields_json, const char* methods_json) {
    if (!name || !*name) return 0;
    gmb_type_schema_t* s = type_schema_find(name);
    if (!s) {
        if (g_type_schema_count >= GMB_TYPE_SCHEMA_MAX) return 0;
        s = &g_type_schemas[g_type_schema_count++];
        snprintf(s->name, sizeof(s->name), "%s", name);
        s->fields[0] = 0; s->methods[0] = 0;
    }
    if (fields_json) snprintf(s->fields, sizeof(s->fields), "%s", fields_json);
    if (methods_json) snprintf(s->methods, sizeof(s->methods), "%s", methods_json);
    return 1;
}

char* gmb_type_properties(const char* name) {
    gmb_type_schema_t* s = type_schema_find(name);
    return strdup(s && s->fields[0] ? s->fields : "{}");
}

char* gmb_type_methods(const char* name) {
    gmb_type_schema_t* s = type_schema_find(name);
    return strdup(s && s->methods[0] ? s->methods : "{}");
}

/* gmb_type_member(name, member, is_method): {"member":"type"} o "{}" — cerca nel JSON schema. */
char* gmb_type_member(const char* name, const char* member, int is_method) {
    gmb_type_schema_t* s = type_schema_find(name);
    const char* json = s ? (is_method ? s->methods : s->fields) : "";
    if (!member || !*member || !json) return strdup("{}");
    char pat[512];
    snprintf(pat, sizeof(pat), "\"%s\":", member);
    const char* p = strstr(json, pat);
    if (!p) return strdup("{}");
    p += strlen(pat);
    while (*p == ' ' || *p == '"') p++;
    char val[256]; int vi = 0;
    while (*p && *p != '"' && *p != ',' && *p != '}' && vi < 255) val[vi++] = *p++;
    val[vi] = 0;
    char out[512];
    snprintf(out, sizeof(out), "{\"%s\":\"%s\"}", member, val);
    return strdup(out);
}

/* ── UDT layout (per unboxing generic): "field:offset:type,..." t:0=int 1=dbl 2=str 3=bool ── */
#define GMB_UDT_LAYOUT_MAX 8
typedef struct { char name[128]; char layout[4096]; } gmb_udt_layout_t;
static gmb_udt_layout_t g_udt_layouts[GMB_UDT_LAYOUT_MAX];
static int g_udt_layout_count = 0;

static gmb_udt_layout_t* udt_layout_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_udt_layout_count; i++)
        if (strcmp(g_udt_layouts[i].name, name) == 0) return &g_udt_layouts[i];
    return NULL;
}

int gmb_type_register_udt(const char* name, int count) {
    if (!name || !*name) return 0;
    gmb_udt_layout_t* l = udt_layout_find(name);
    if (!l) {
        if (g_udt_layout_count >= GMB_UDT_LAYOUT_MAX) return 0;
        l = &g_udt_layouts[g_udt_layout_count++];
        snprintf(l->name, sizeof(l->name), "%s", name);
        l->layout[0] = 0;
    }
    (void)count;
    return 1;
}

/* Aggiunge un campo al layout (offset = espressione C offsetof valutata dal compilatore). */
int gmb_type_udt_field(const char* name, const char* field, long offset, int type) {
    gmb_udt_layout_t* l = udt_layout_find(name);
    if (!l || !field) return 0;
    size_t used = strlen(l->layout);
    int w = snprintf(l->layout + used, sizeof(l->layout) - used, "%s%s:%ld:%d",
                     used ? "," : "", field, offset, type);
    return w > 0 ? 1 : 0;
}

/* Aggiunge un campo con tipo elemento (es. LIST: type=4, elem="Leg") → "field:off:type:elem". */
int gmb_type_udt_field_ex(const char* name, const char* field, long offset, int type, const char* elem) {
    gmb_udt_layout_t* l = udt_layout_find(name);
    if (!l || !field) return 0;
    if (elem && elem[0])
        return snprintf(l->layout + strlen(l->layout), sizeof(l->layout) - strlen(l->layout),
                        "%s%s:%ld:%d:%s", l->layout[0] ? "," : "", field, offset, type, elem) > 0 ? 1 : 0;
    return gmb_type_udt_field(name, field, offset, type);
}

/* Cerca "field:offset:type[:elem]" nel layout; ritorna offset (out), type code, elem. */
static int udt_layout_lookup(const gmb_udt_layout_t* l, const char* field, long* out_offset, int* out_type) {
    const char* p = l->layout;
    while (p && *p) {
        const char* col1 = strchr(p, ':');
        if (!col1) break;
        size_t fn = (size_t)(col1 - p);
        if (fn == strlen(field) && strncasecmp(p, field, fn) == 0) {
            *out_offset = atol(col1 + 1);
            const char* col2 = strchr(col1 + 1, ':');
            *out_type = col2 ? atoi(col2 + 1) : 0;
            return 1;
        }
        const char* comma = strchr(p, ',');
        if (!comma) break;
        p = comma + 1;
    }
    return 0;
}

/* Estrae il valore di un campo UDT boxato (blob) dato offset+type → stringa CSV. */
static void udt_layout_read_field(const uint8_t* base, long off, int type, char* out, size_t cap) {
    out[0] = 0;
    switch (type) {
        case 2: { /* stringa gmb_string_t */
            gmb_string_t* s = (gmb_string_t*)(base + off);
            if (s && s->data) snprintf(out, cap, "%s", s->data);
            break;
        }
        case 1: snprintf(out, cap, "%.15g", *(gmb_double_t*)(base + off)); break;
        case 3: snprintf(out, cap, "%d", *(gmb_bool_t*)(base + off) ? 1 : 0); break;
        default: snprintf(out, cap, "%lld", (long long)(*(gmb_int_t*)(base + off))); break;
    }
}

/* API pubbliche per il motore DB (gmb_db_c): lookup layout UDT + lettura campo. */
int gmb_type_udt_lookup(const char* udt, const char* field, long* out_offset, int* out_type) {
    gmb_udt_layout_t* l = udt_layout_find(udt);
    if (!l) return 0;
    return udt_layout_lookup(l, field, out_offset, out_type);
}
void gmb_type_udt_read_field(const uint8_t* base, long off, int type, char* out, size_t cap) {
    udt_layout_read_field(base, off, type, out, cap);
}

/* Primo campo del layout UDT (PK implicito): ritorna nome/offset/type. */
int gmb_type_udt_first_field(const char* udt, const char** out_name, long* out_offset, int* out_type) {
    gmb_udt_layout_t* l = udt_layout_find(udt);
    if (!l || !l->layout[0]) return 0;
    const char* col1 = strchr(l->layout, ':');
    if (!col1) return 0;
    if (out_name) *out_name = l->layout;
    if (out_offset) *out_offset = atol(col1 + 1);
    const char* col2 = strchr(col1 + 1, ':');
    if (out_type) *out_type = col2 ? atoi(col2 + 1) : 0;
    return 1;
}

/* Enumerazione dei campi del layout: idx-esimo campo (name copiato in out_name). */
int gmb_type_udt_field_at(const char* udt, int idx, char* out_name, int namecap, long* out_offset, int* out_type, char* out_elem, int elemcap) {
    gmb_udt_layout_t* l = udt_layout_find(udt);
    const char* p; int i = 0;
    if (out_elem && elemcap > 0) out_elem[0] = 0;
    if (!l || !l->layout[0] || idx < 0) return 0;
    p = l->layout;
    while (p && *p) {
        const char* col1 = strchr(p, ':');
        const char* comma;
        if (!col1) break;
        if (i == idx) {
            size_t fl = (size_t)(col1 - p);
            if (namecap > 0) { size_t c = fl < (size_t)(namecap - 1) ? fl : (size_t)(namecap - 1); memcpy(out_name, p, c); out_name[c] = 0; }
            *out_offset = atol(col1 + 1);
            { const char* col2 = strchr(col1 + 1, ':');
              *out_type = col2 ? atoi(col2 + 1) : 0;
              if (col2 && out_elem && elemcap > 0) {
                  const char* e = col2 + 1;
                  while (*e >= '0' && *e <= '9') e++;           /* salta cifre del type */
                  if (*e == ':') { e++; const char* ce = strchr(e, ','); size_t el = ce ? (size_t)(ce - e) : strlen(e);
                      size_t c = el < (size_t)(elemcap - 1) ? el : (size_t)(elemcap - 1); memcpy(out_elem, e, c); out_elem[c] = 0; }
              }
            }
            return 1;
        }
        comma = strchr(p, ',');
        if (!comma) break;
        p = comma + 1; i++;
    }
    return 0;
}

/* Costruisce un gmb_dict_t dai campi del layout UDT (per serializzazione JSON/CSV). */
gmb_dict_t* gmb_type_udt_to_dict(const char* udt, const uint8_t* base) {
    gmb_dict_t* d = (gmb_dict_t*)malloc(sizeof(gmb_dict_t));
    if (!d) return NULL;
    *d = gmb_dict_new();
    gmb_udt_layout_t* l = udt_layout_find(udt);
    if (!l) return d;
    const char* p = l->layout;
    while (p && *p) {
        const char* col1 = strchr(p, ':');
        if (!col1) break;
        size_t fl = (size_t)(col1 - p);
        char field[64];
        if (fl > 63) fl = 63;
        memcpy(field, p, fl); field[fl] = 0;
        long off = atol(col1 + 1);
        const char* col2 = strchr(col1 + 1, ':');
        int t = col2 ? atoi(col2 + 1) : 0;
        /* 4ª colonna (elem): numero di slot per i campi-array (code 5=double[], 6=string[]). */
        char elem[24]; elem[0] = 0;
        const char* col3 = col2 ? strchr(col2 + 1, ':') : NULL;
        if (col3) {
            const char* ce = strchr(col3 + 1, ',');
            size_t el = ce ? (size_t)(ce - (col3 + 1)) : strlen(col3 + 1);
            if (el > 23) el = 23;
            memcpy(elem, col3 + 1, el); elem[el] = 0;
        }
        if (t == 4) {
            /* Campo LIST OF <Elem>: il layout memorizza il gmb_list_t inline (non un blob).
             * Serializza la lista boxata (gli elementi UDT vengono a loro volta convertiti). */
            gmb_list_t* lst = (gmb_list_t*)(base + off);
            gmb_dict_set(d, field, gmb_make_val_list(*lst));
        } else if (t == 5 || t == 6) {
            /* Campo ARRAY (INTEGER[N]/STRING[N]): nel layout c'è il PUNTATORE del buffer,
             * elem = numero di slot → serializza come array JSON (parità IL/C#). */
            int n = atoi(elem);
            gmb_list_t lst = gmb_list_new();
            if (t == 6) {
                gmb_string_t* sa = *(gmb_string_t**)(base + off);
                for (int i = 0; i < n; i++)
                    gmb_list_add(&lst, gmb_make_val_string((sa && sa[i].data) ? sa[i].data : ""));
            } else {
                gmb_double_t* da = *(gmb_double_t**)(base + off);
                for (int i = 0; i < n; i++)
                    gmb_list_add(&lst, gmb_make_val_double(da ? da[i] : 0));
            }
            gmb_dict_set(d, field, gmb_make_val_list(lst));
        } else {
            char val[1024]; udt_layout_read_field(base, off, t, val, sizeof(val));
            if (t == 2)
                gmb_dict_set(d, field, gmb_make_val_string(val));
            else if (t == 1)
                gmb_dict_set(d, field, gmb_make_val_double(atof(val)));
            else if (t == 3)
                gmb_dict_set(d, field, gmb_make_val_bool(atoi(val) != 0));
            else
                gmb_dict_set(d, field, gmb_make_val_int(atoll(val)));
        }
        const char* comma = strchr(p, ',');
        if (!comma) break;
        p = comma + 1;
    }
    return d;
}

/* Converte un array C double* (allocato dal runtime) in gmb_list_t — una sola
 * implementazione della conversione array→lista (§0bis #2), usata dai builtin
 * array-returning (SIGNAL/ANALYSIS/FFT/...) invece del loop inline nell'emitter. */
gmb_list_t gmb_list_from_double_array(const double* arr, gmb_int_t n) {
    gmb_list_t l = gmb_list_new();
    if (arr) { for (gmb_int_t i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_double(arr[i])); free((void*)arr); }
    return l;
}

/* Converte un array char** (allocato, con liberazione) in gmb_list_t di stringhe —
 * una sola implementazione (§0bis #2), usata da ABI decode e affini. */
gmb_list_t gmb_list_from_string_array(char** arr, int n) {
    gmb_list_t l = gmb_list_new();
    if (arr) {
        for (int i = 0; i < n; i++) {
            gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(arr[i] ? arr[i] : "").data));
            free(arr[i]);
        }
        free(arr);
    }
    return l;
}

/* ATOKENS(text, sep): tokenizza la stringa per il separatore → lista di stringhe
 * (parità C# string.Split). Una sola implementazione. */
gmb_list_t gmb_string_split(const char* text, const char* sep) {
    gmb_list_t l = gmb_list_new();
    if (!text) return l;
    size_t sl = sep ? strlen(sep) : 0;
    if (!sl) { gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(text).data)); return l; }
    const char* p = text;
    while (p && *p) {
        const char* n = strstr(p, sep);
        size_t len = n ? (size_t)(n - p) : strlen(p);
        char* tok = (char*)malloc(len + 1);
        if (!tok) break;
        memcpy(tok, p, len); tok[len] = 0;
        gmb_list_add(&l, gmb_make_val_string(gmb_string_alloc(tok).data));
        free(tok);
        if (!n) break;
        p = n + sl;
    }
    return l;
}

/* PCA → dict {"EigenValues":[...],"EigenVectors":[[...],...]} (parità C# Pca). */
gmb_val_t gmb_pca_dict(const double* data, int rows, int cols) {
    gmb_int_t n = 0, m = 0;
    double eigs[64];
    gmb_double_t** eigv = gmb_pca(data, rows, cols, &n, &m, eigs);
    gmb_dict_t d = gmb_dict_new();
    gmb_list_t ev = gmb_list_new();
    for (gmb_int_t i = 0; i < n && i < 64; i++) gmb_list_add(&ev, gmb_make_val_double(eigs[i]));
    gmb_dict_set(&d, "EigenValues", gmb_make_val_list_take(&ev));
    gmb_list_t vecs = gmb_list_new();
    if (eigv) {
        for (gmb_int_t i = 0; i < n; i++) {
            gmb_list_t row = gmb_list_new();
            for (gmb_int_t j = 0; j < m; j++) gmb_list_add(&row, gmb_make_val_double(eigv[i][j]));
            gmb_list_add(&vecs, gmb_make_val_list_take(&row));
            free(eigv[i]);
        }
        free(eigv);
    }
    gmb_dict_set(&d, "EigenVectors", gmb_make_val_list_take(&vecs));
    return gmb_make_val_dict_take(&d);
}

/* ACLONE/ACOPY: copia allocata di un array double (parità .Clone()). */
gmb_double_t* gmb_clone_double_array(const double* arr, gmb_int_t n) {
    gmb_double_t* c = (gmb_double_t*)calloc((size_t)n + 1, sizeof(gmb_double_t));
    if (c && arr) memcpy(c, arr, (size_t)n * sizeof(gmb_double_t));
    return c;
}

/* ISLONGONLY: 1 se tutti gli elementi >= 0 (parità long-only check). */
int gmb_all_non_negative(const double* arr, gmb_int_t n) {
    if (!arr) return 1;
    for (gmb_int_t i = 0; i < n; i++) if (arr[i] < 0) return 0;
    return 1;
}

/* DATA/READ/RESTORE: tabella globale con cursore (parità IlRuntime.BeginData/
 * RestoreData/TakeData — errore BASIC 200 "OUT OF DATA" a pool esaurito). */
static const gmb_val_t* __gmb_data_items = NULL;
static gmb_int_t __gmb_data_n = 0;
static gmb_int_t __gmb_data_ptr = 0;

void gmb_data_begin(const gmb_val_t* items, gmb_int_t n) {
    __gmb_data_items = items;
    __gmb_data_n = n;
    __gmb_data_ptr = 0;
}
void gmb_data_restore(gmb_int_t pos) {
    if (pos < 0) pos = 0;
    if (pos > __gmb_data_n) pos = __gmb_data_n;
    __gmb_data_ptr = pos;
}
static gmb_val_t gmb_data_take(void) {
    if (__gmb_data_ptr >= __gmb_data_n)
        gmb_raise_error(200, "OUT OF DATA", 0);
    return __gmb_data_items[__gmb_data_ptr++];
}
gmb_double_t gmb_data_read_num(void) { return gmb_val_to_double(gmb_data_take()); }
gmb_string_t gmb_data_read_str(void) { return gmb_val_to_string(gmb_data_take()); }

/* Bound degli array per le lambda C99 (2026-11-02): la lambda riceve `double*` senza
 * lunghezza; il chiamante pubblica gli indici massimi (uno per parametro array, in ordine)
 * prima della chiamata. Thread-local. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define GMB_LAMBDA_TLS _Thread_local
#elif defined(__GNUC__)
#define GMB_LAMBDA_TLS __thread
#else
#define GMB_LAMBDA_TLS
#endif
static GMB_LAMBDA_TLS gmb_int_t g_lambda_bounds[GMB_LAMBDA_MAX_BOUNDS] = { -1, -1, -1, -1 };
/* Lower bound degli array per le lambda/fn-value C99: il chiamante lo pubblica prima
 * dell'invocazione (come il bound); default 0 = 0-based. */
static GMB_LAMBDA_TLS gmb_int_t g_lambda_lowers[GMB_LAMBDA_MAX_BOUNDS] = { 0, 0, 0, 0 };
/* Lunghezza dell'ultimo array restituito da una lambda AS ARRAY (2026-11-03): il chiamante
 * (es. LEASTSQUARESFIT) la usa quando il numero di residui non e' noto a compile-time. */
static GMB_LAMBDA_TLS gmb_int_t g_lambda_ret_len = 0;
/* Puntatore dell'array restituito da una funzione `AS ARRAY` quando il chiamante passa
 * `__out == NULL` (2026-11-03): l'ABI `void (..., double* __out)` non può restituire un
 * array composto, quindi la funzione alloca e pubblica qui puntatore + lunghezza; il call
 * site BASIC legge subito i due valori. Thread-local (nessuna interferenza fra esecuzioni). */
static GMB_LAMBDA_TLS gmb_double_t* g_lambda_ret_ptr = NULL;
/* Dimensioni (rank-2) dell'ultimo valore restituito da una funzione con rank esplicito
 * (`FUNCTION F() AS DOUBLE(,)`): stessa ABI del canale ptr/len, usata dal chiamante per
 * ricostruire i bound della matrice. */
static GMB_LAMBDA_TLS gmb_int_t g_lambda_ret_dims[8] = { 0 };
gmb_int_t gmb_lambda_bound(int idx) { return (idx >= 0 && idx < GMB_LAMBDA_MAX_BOUNDS) ? g_lambda_bounds[idx] : -1; }
void gmb_lambda_bound_set(int idx, gmb_int_t b0) { if (idx >= 0 && idx < GMB_LAMBDA_MAX_BOUNDS) g_lambda_bounds[idx] = b0; }
gmb_int_t gmb_lambda_lower(int idx) { return (idx >= 0 && idx < GMB_LAMBDA_MAX_BOUNDS) ? g_lambda_lowers[idx] : 0; }
void gmb_lambda_lower_set(int idx, gmb_int_t lo) { if (idx >= 0 && idx < GMB_LAMBDA_MAX_BOUNDS) g_lambda_lowers[idx] = lo; }
gmb_int_t gmb_lambda_ret_len_get(void) { return g_lambda_ret_len; }
void gmb_lambda_ret_len_set(gmb_int_t n) { g_lambda_ret_len = n; }
gmb_double_t* gmb_lambda_ret_ptr_get(void) { return g_lambda_ret_ptr; }
void gmb_lambda_ret_ptr_set(gmb_double_t* p) { g_lambda_ret_ptr = p; }
gmb_int_t gmb_lambda_ret_dim_get(int axis) { return (axis >= 0 && axis < 8) ? g_lambda_ret_dims[axis] : 0; }
void gmb_lambda_ret_dims_set(gmb_int_t d0, gmb_int_t d1) { g_lambda_ret_dims[0] = d0; g_lambda_ret_dims[1] = d1; }
void gmb_lambda_ret_dim_set(int axis, gmb_int_t v) { if (axis >= 0 && axis < 8) g_lambda_ret_dims[axis] = v; }
