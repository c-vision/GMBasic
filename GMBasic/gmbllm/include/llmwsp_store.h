/* llmwsp_store.h — W1 del piano Weight-Space Programming: Weight Store.
 *
 * Apertura uniforme di una directory safetensors (shard multipli o file singolo), indice dei tensori
 * (nome, dtype, forma, file, offset, byte), SLICE-READ senza leggere l'intero tensore, sha256 per tensore
 * e verifica di copertura dei byte. Sola lettura: la base è immutabile. C99 puro, nessuna dipendenza.
 * Il C# lo usa solo via P/Invoke. */
#ifndef LLMWSP_STORE_H
#define LLMWSP_STORE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLMWSP_MAX_DIMS 8

typedef struct llmwsp_tensor {
    char    *name;
    char     dtype[16];             /* "BF16", "F16", "F32", "U32", ... come nell'header safetensors */
    int      ndim;
    int64_t  shape[LLMWSP_MAX_DIMS];
    int      file;                  /* indice dello shard */
    uint64_t offset;                /* offset ASSOLUTO nel file */
    uint64_t nbytes;
} llmwsp_tensor;

typedef struct llmwsp_store llmwsp_store;

/* Apre `dir` (o un singolo .safetensors). 0 = ok; <0 = errore, descritto in err. */
int  llmwsp_store_open(const char *dir, llmwsp_store **out, char *err, size_t errcap);
void llmwsp_store_close(llmwsp_store *s);

size_t               llmwsp_store_count(const llmwsp_store *s);          /* tensori, ordinati per nome */
const llmwsp_tensor *llmwsp_store_at(const llmwsp_store *s, size_t i);
const llmwsp_tensor *llmwsp_store_find(const llmwsp_store *s, const char *name);
uint64_t             llmwsp_store_total_bytes(const llmwsp_store *s);    /* somma dei nbytes */
int                  llmwsp_store_file_count(const llmwsp_store *s);
const char          *llmwsp_store_file_name(const llmwsp_store *s, int i);

/* Dimensione in byte di un elemento; 0 se dtype sconosciuto. */
size_t   llmwsp_dtype_size(const char *dtype);
int64_t  llmwsp_tensor_elems(const llmwsp_tensor *t);

/* Legge le righe [r0,r1) (asse 0) — byte contigui, UNA pread. buf deve avere (r1-r0)*riga byte.
 * Restituisce i byte scritti, <0 in errore. */
int64_t llmwsp_read_rows(const llmwsp_store *s, const llmwsp_tensor *t, int64_t r0, int64_t r1,
                         void *buf, size_t cap);
/* Per tensori 2D: colonne [c0,c1) di tutte le righe, uscita row-major [righe x (c1-c0)] (letture a passo). */
int64_t llmwsp_read_cols(const llmwsp_store *s, const llmwsp_tensor *t, int64_t c0, int64_t c1,
                         void *buf, size_t cap);
/* Intervallo di byte grezzo [off,off+n) relativo all'inizio del tensore. */
int64_t llmwsp_read_bytes(const llmwsp_store *s, const llmwsp_tensor *t, uint64_t off, uint64_t n,
                          void *buf);

/* sha256 esadecimale (65 byte incl. NUL), a blocchi: memoria costante. */
int llmwsp_tensor_sha256(const llmwsp_store *s, const llmwsp_tensor *t, char hex[65]);

/* Copertura: per ogni shard i tensori devono tassellare esattamente la zona dati (nessun buco, nessuna
 * sovrapposizione). Restituisce il numero di violazioni (0 = copertura completa); descrive la prima in err. */
int llmwsp_store_check_coverage(const llmwsp_store *s, char *err, size_t errcap);

/* ---- Writer a streaming (safetensors) -----------------------------------------------------------
 * L'header è calcolato in anticipo da nome/dtype/forma: i tensori si scrivono poi in ordine, a blocchi,
 * con memoria costante. Chiusura: verifica che ogni tensore abbia ricevuto esattamente i suoi byte. */
typedef struct llmwsp_wspec {
    const char *name;
    const char *dtype;
    int         ndim;
    int64_t     shape[LLMWSP_MAX_DIMS];
} llmwsp_wspec;

typedef struct llmwsp_writer llmwsp_writer;

/* Crea `path` con l'header per `n` tensori (ordine dato; nomi unici; dtype noto). */
int llmwsp_writer_open(const char *path, const llmwsp_wspec *spec, size_t n, llmwsp_writer **out,
                       char *err, size_t errcap);
/* Accoda byte al tensore corrente; passa al successivo quando ha ricevuto i suoi nbytes. */
int llmwsp_writer_append(llmwsp_writer *w, const void *buf, size_t n);
/* Copia a blocchi un tensore dallo store: deve essere il tensore corrente (stesso nome/dtype/forma). */
int llmwsp_writer_copy(llmwsp_writer *w, const llmwsp_store *s, const llmwsp_tensor *t);
/* Chiude; 0 solo se TUTTI i tensori sono completi. In ogni caso libera `w`. */
int llmwsp_writer_close(llmwsp_writer *w, char *err, size_t errcap);

#ifdef __cplusplus
}
#endif
#endif
