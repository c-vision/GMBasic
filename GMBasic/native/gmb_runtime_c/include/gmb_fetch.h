/* ============================================================================
 * gmb_fetch.h — motore di download STREAMING con resume, lock e verifica.
 *
 * PERCHÉ. Un cervello (`contentType: brain`) pesa 1,4 GB e arriva da una release GitHub: il
 * download NON può stare in RAM (gmb_pkg_install.c::http_download teneva tutto il corpo in
 * memoria) e non può ricominciare da capo se la rete cade al 90%. Il motore è UNO per cervelli,
 * librerie native e pacchetti: stessa politica, un solo posto dove sbagliare.
 *
 * MODELLO (portato da FastLLM, `src/hf/fmlx_hf_fetch.c`, che a sua volta segue
 * huggingface_hub): separazione netta fra TRASPORTO (HTTP, il mondo esterno) e POLITICA
 * (dove/quando/cosa scrivere). Il trasporto è iniettabile: i test girano offline con un finto
 * trasporto che dichiara intestazioni e byte prestabiliti. Non è uno stub — è il confine del
 * sistema, ed è l'unico modo di dimostrare che un file interrotto viene davvero ripreso.
 *
 * REGOLE (ognuna con il suo perché):
 *  - si scrive in `<dest>.incomplete`, MAI nel nome finale; il nome è DETERMINISTICO così il
 *    processo successivo (o un altro processo) RIPRENDE invece di riscaricare;
 *  - lock `<dir>/.locks/<nome>.lock` con flock(2) NON bloccante: il file di lock esiste anche
 *    da libero, "c'è" non vuol dire "è preso" — lo dice solo il lock;
 *  - `from = size(.incomplete)`; `from > size_attesa` → si riparte da zero; `from == size`
 *    → NESSUN GET (il server risponderebbe 416);
 *  - se il server IGNORA `Range` (200 invece di 206) si tronca e si riscarica — un 200
 *    silenzioso duplicherebbe i byte;
 *  - `Content-Range` incoerente → errore, nessun rename;
 *  - il trasferimento si ferma alla dimensione DICHIARATA (un server che promette 1 KB e ne
 *    manda 1 TB va fermato alla promessa);
 *  - retry SOLO su stallo/rete/5xx (404, 401, hash sbagliato non migliorano riprovando), con
 *    backoff e ripresa dall'ultimo byte scritto;
 *  - verifica sui BYTE del file (dimensione + sha256): un contatore interno può mentire;
 *  - `rename()` atomico nella stessa directory, solo dopo la verifica;
 *  - spazio su disco controllato PRIMA di iniziare (evita 1,4 GB di download a disco pieno);
 *  - il token va SOLO all'autority a cui è destinato: libcurl non ripropaga `Authorization`
 *    cross-host (dal 7.58), e nei messaggi d'errore l'URL si stampa senza query string (le URL
 *    firmate del CDN sono credenziali a scadenza).
 *
 * PARAMETRI: nessuna variabile d'ambiente — tutto in `gmb_fetch_opts` (regola AGENTS §0bis).
 * ============================================================================ */
#ifndef GMB_FETCH_H
#define GMB_FETCH_H

/* Nessuna dipendenza dai tipi del runtime: questo header è autocontenuto (così può essere
 * incluso anche da un driver C esterno, come gmb_libs_fetch_cli.c, senza i 2700 tipi di
 * gmb_runtime.h e senza il doppio parse che ne deriva). */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── codici di ritorno (0 = successo) ─────────────────────────────────────────────────────── */
enum {
    GMB_FETCH_OK = 0,
    GMB_FETCH_E_ARG = 1,           /* argomenti incoerenti                      */
    GMB_FETCH_E_OPEN = 2,          /* .incomplete non apribile in scrittura     */
    GMB_FETCH_E_LOCKED = 3,        /* un altro processo sta scaricando l'asset  */
    GMB_FETCH_E_SPACE = 4,         /* spazio su disco insufficiente (prima)     */
    GMB_FETCH_E_NET = 5,           /* errore di rete dopo i retry               */
    GMB_FETCH_E_RANGE = 6,         /* Content-Range incoerente / Range rifiutato */
    GMB_FETCH_E_SIZE = 7,          /* dimensione finale != attesa               */
    GMB_FETCH_E_SHA = 8,           /* sha256 != manifest                        */
    GMB_FETCH_E_IO = 9,            /* scrittura/rename                          */
    GMB_FETCH_E_CANCEL = 10,       /* annullato dalla callback                  */
    GMB_FETCH_E_NO_TRANSPORT = 11, /* runtime compilato senza libcurl           */
    GMB_FETCH_E_RESOLVE = 12       /* il re-resolver dell'URL ha fallito        */
};

/* ── trasporto (il confine del sistema: HTTP reale o finto nei test) ──────────────────────── */
#define GMB_FETCH_TRANSPORT_ABI 1

typedef struct gmb_fetch_transfer {
    /* --- INGRESSO: li imposta il motore PRIMA della chiamata --- */
    long long  from;              /* offset da cui riprendere (0 = dall'inizio)   */
    long long  expected_total;    /* dimensione attesa, 0 = ignota                */
    int        connect_timeout;   /* secondi                                      */
    int        low_speed_limit;   /* B/s: sotto questa soglia è uno stallo        */
    int        low_speed_time;    /* secondi di stallo prima di dichiararlo       */
    void     (*progress)(void* user, long long done, long long total,
                         double bytes_per_sec, long long eta_sec);
    void*      progress_user;
    int      (*cancel)(void* user);   /* 1 = l'utente ha annullato                */
    void*      cancel_user;

    /* --- USCITA: li imposta il trasporto --- */
    long long  written;           /* byte scritti in QUESTO tentativo             */
    long long  http_status;       /* 0 = nessuna risposta                         */
    int        ranged;            /* 1 = il server ha risposto 206                */
    long long  received_from;     /* Content-Range start, -1 se assente           */
    long long  received_total;    /* Content-Range total, -1 se assente/`*`       */
    int        retryable;         /* 1 = ha senso riprovare (rete/5xx)            */
    int        cancelled;         /* 1 = fermato dalla callback di annullamento   */
} gmb_fetch_transfer;

typedef struct gmb_fetch_transport {
    int  abi;                     /* GMB_FETCH_TRANSPORT_ABI                       */
    /* GET url → byte [from, …) scritti in `fd` (già posizionato su `from`).
     * 0 = ok (anche quando il server ha ignorato il Range: lo dice `t.ranged`/`http_status`),
     * !=0 = errore; `err` = messaggio per l'utente (MAI con il token). */
    int (*get)(void* ctx, const char* url, const char* token,
               long long from, long long expected_total, int fd,
               gmb_fetch_transfer* t, char* err, size_t errcap);
    void* ctx;
    void (*ctx_free)(void* ctx);  /* opzionale */
} gmb_fetch_transport;

/* ── opzioni (default: gmb_fetch_opts_default) ────────────────────────────────────────────── */
typedef struct gmb_fetch_opts {
    int       lock_wait_ms;     /* 0 = non attendere (E_LOCKED subito); default 2000 */
    int       stall_retries;    /* tentativi extra dopo uno stallo; default 3        */
    int       low_speed_limit;  /* B/s; default 1024                                 */
    int       low_speed_time;   /* secondi; default 60                               */
    int       connect_timeout;  /* secondi; default 20                               */
    int       allow_resume;     /* 1 (default) = riprende dall'.incomplete           */
    int       keep_incomplete;  /* 1 (default) = su errore lascia l'.incomplete      */
    int       quiet;            /* 1 = nessun output su stderr                       */
    long long disk_margin;      /* byte liberi extra richiesti; default 64 MiB       */
    void     (*progress)(void* user, long long done, long long total,
                         double bytes_per_sec, long long eta_sec);
    void*     progress_user;
    int      (*cancel)(void* user);
    void*     cancel_user;
} gmb_fetch_opts;

/* ── richiesta ────────────────────────────────────────────────────────────────────────────── */
typedef struct gmb_fetch_request {
    const char* url;            /* URL del primo tentativo (può essere NULL se c'è `resolve`) */
    const char* token;          /* Bearer per l'API; il trasporto lo manda solo all'autority  */
    const char* dest_path;      /* percorso FINALE (rename atomico dopo la verifica)          */
    const char* sha256;         /* atteso, 64 hex (case-insensitive); NULL = non verificare   */
    long long   size;           /* dimensione attesa; 0 = ignota                              */
    /* Re-resolver invocato PRIMA di OGNI tentativo: l'URL di un asset GitHub scade (il 302
     * punta a un CDN firmato) e non va memorizzato. 0 = ok, !=0 = errore. */
    int (*resolve)(void* user, char* url, size_t cap, char* err, size_t errcap);
    void* resolve_user;
} gmb_fetch_request;

/* ── API ──────────────────────────────────────────────────────────────────────────────────── */

/* Riempie `o` con i default. */
void gmb_fetch_opts_default(gmb_fetch_opts* o);

/* Scarica `req->dest_path`. Ritorna GMB_FETCH_OK (file presente e verificato) o un E_*.
 * `opts` NULL = default; `transport` obbligatorio. `err` (opzionale) riceve una spiegazione. */
int gmb_fetch_to_file(const gmb_fetch_request* req, const gmb_fetch_opts* opts,
                      const gmb_fetch_transport* transport, char* err, size_t errcap);

/* Trasporto libcurl di sistema. NULL se il runtime è stato compilato senza libcurl. */
const gmb_fetch_transport* gmb_fetch_transport_curl(void);

/* Verifica un file GIÀ presente: dimensione (`size` > 0) e sha256 (`sha256` non NULL).
 * 1 = valido, 0 = no (o file assente). */
int gmb_fetch_verify_file(const char* path, const char* sha256, long long size);

/* `<dest>.incomplete` — il nome del temporaneo (per CLI, pulizie, diagnostica). 1 = scritto. */
int gmb_fetch_incomplete_path(const char* dest, char* out, size_t cap);

/* Messaggio leggibile per un codice E_*. */
const char* gmb_fetch_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FETCH_H */
