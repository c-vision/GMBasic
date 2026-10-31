/* gmb_fetch.c — motore di download STREAMING con resume, lock e verifica (vedi gmb_fetch.h).
 *
 * Il TRASPORTO è separato dalla POLITICA: qui c'è solo la politica (dove scrivere, quando
 * riprendere, quando riprovare, cosa verificare). L'HTTP vero sta in gmb_fetch_curl.c; i test
 * usano un trasporto finto e girano senza rete. Questa è la ragione per cui il motore si può
 * provare in modo deterministico: il confine del sistema è dichiarato, non nascosto.
 */

#include "gmb_fetch.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#  include <fcntl.h>
#  include <windows.h>
#  define FETCH_PATH_MAX 4096
typedef int fetch_fd;
static fetch_fd fetch_open_write(const char* p, int truncate_it)
{ return _open(p, _O_WRONLY | _O_CREAT | _O_BINARY | (truncate_it ? _O_TRUNC : 0), _S_IREAD | _S_IWRITE); }
static long long fetch_seek(fetch_fd fd, long long off) { return (long long)_lseeki64(fd, off, SEEK_SET); }
static long long fetch_tell(fetch_fd fd) { return (long long)_lseeki64(fd, 0, SEEK_CUR); }
static int fetch_truncate(fetch_fd fd, long long len) { return _chsize_s(fd, (__int64)len) == 0 ? 0 : -1; }
static int fetch_close(fetch_fd fd) { return _close(fd); }
#else
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/file.h>
#  include <sys/statvfs.h>
#  include <time.h>
#  define FETCH_PATH_MAX 4096
typedef int fetch_fd;
static fetch_fd fetch_open_write(const char* p, int truncate_it)
{ return open(p, O_WRONLY | O_CREAT | (truncate_it ? O_TRUNC : 0), 0644); }
static long long fetch_seek(fetch_fd fd, long long off) { off_t r = lseek(fd, (off_t)off, SEEK_SET); return (long long)r; }
static long long fetch_tell(fetch_fd fd) { off_t r = lseek(fd, 0, SEEK_CUR); return (long long)r; }
static int fetch_truncate(fetch_fd fd, long long len) { return ftruncate(fd, (off_t)len) == 0 ? 0 : -1; }
static int fetch_close(fetch_fd fd) { return close(fd); }
#endif

/* ── utilità ─────────────────────────────────────────────────────────────────────────────── */

static const char* fetch_strerror(int c)
{
    switch (c) {
    case GMB_FETCH_OK:               return "ok";
    case GMB_FETCH_E_ARG:            return "argomenti non validi";
    case GMB_FETCH_E_OPEN:           return "impossibile aprire il file temporaneo";
    case GMB_FETCH_E_LOCKED:         return "download già in corso da un altro processo";
    case GMB_FETCH_E_SPACE:          return "spazio su disco insufficiente";
    case GMB_FETCH_E_NET:            return "errore di rete";
    case GMB_FETCH_E_RANGE:          return "intervallo (Range/Content-Range) non coerente";
    case GMB_FETCH_E_SIZE:           return "dimensione del file diversa da quella attesa";
    case GMB_FETCH_E_SHA:            return "sha256 diverso dal manifest";
    case GMB_FETCH_E_IO:             return "errore di scrittura/rename";
    case GMB_FETCH_E_CANCEL:         return "download annullato";
    case GMB_FETCH_E_NO_TRANSPORT:   return "nessun trasporto HTTP disponibile (libcurl assente)";
    case GMB_FETCH_E_RESOLVE:        return "impossibile risolvere l'URL dell'asset";
    default:                         return "errore sconosciuto";
    }
}

const char* gmb_fetch_strerror(int code) { return fetch_strerror(code); }

static void fetch_sleep_ms(int ms)
{
    if (ms <= 0) return;
#if defined(_WIN32) || defined(_WIN64)
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* Dimensione di un file regolare; -1 se non esiste / non è leggibile. */
static long long fetch_file_size(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode)) return -1;
    return (long long)st.st_size;
}

static int fetch_isdir(const char* path)
{
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

/* mkdir -p (stessa forma di lf_mkdir_p in gmb_libs_fetch.c). */
static int fetch_mkdir_p(const char* path)
{
    char tmp[FETCH_PATH_MAX];
    if (!path || !*path) return 0;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
#if defined(_WIN32) || defined(_WIN64)
        _mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
        *p = '/';
    }
#if defined(_WIN32) || defined(_WIN64)
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
    return fetch_isdir(path);
}

static int fetch_dirname(const char* path, char* out, size_t cap)
{
    if (!path || !out || !cap) return 0;
    snprintf(out, cap, "%s", path);
    char* slash = strrchr(out, '/');
    if (!slash) { snprintf(out, cap, "."); return 1; }
    if (slash == out) { out[1] = '\0'; return 1; }
    *slash = '\0';
    return 1;
}

static int fetch_basename(const char* path, char* out, size_t cap)
{
    if (!path || !out || !cap) return 0;
    const char* slash = strrchr(path, '/');
    snprintf(out, cap, "%s", slash ? slash + 1 : path);
    return out[0] ? 1 : 0;
}

/* Byte liberi nella filesystem che contiene `dir`; ULLONG_MAX se non determinabile. */
static unsigned long long fetch_free_bytes(const char* dir)
{
#if defined(_WIN32) || defined(_WIN64)
    ULARGE_INTEGER avail;
    if (!GetDiskFreeSpaceExA(dir, &avail, NULL, NULL)) return (unsigned long long)-1;
    return (unsigned long long)avail.QuadPart;
#else
    struct statvfs vfs;
    if (statvfs(dir, &vfs) != 0) return (unsigned long long)-1;
    return (unsigned long long)vfs.f_bavail * (unsigned long long)(vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize);
#endif
}

/* ── lock fra processi ───────────────────────────────────────────────────────────────────
 * Il file di lock esiste anche da LIBERO: "il file c'è" non significa "è preso". Lo dice solo
 * il lock. Non si cancella mai (unlink di un file lockato è la ricetta per due proprietari). */
typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    HANDLE h;
#else
    int fd;
#endif
    int held;
} fetch_lock_t;

static int fetch_lock_acquire(const char* dest, int wait_ms, fetch_lock_t* lk, char* err, size_t errcap)
{
    char dir[FETCH_PATH_MAX], base[512], ldir[FETCH_PATH_MAX + 16], lpath[FETCH_PATH_MAX + 64];
    lk->held = 0;
#if defined(_WIN32) || defined(_WIN64)
    lk->h = INVALID_HANDLE_VALUE;
#else
    lk->fd = -1;
#endif
    if (!fetch_dirname(dest, dir, sizeof dir) || !fetch_basename(dest, base, sizeof base)) return GMB_FETCH_E_ARG;
    snprintf(ldir, sizeof ldir, "%s/.locks", dir);
    if (!fetch_mkdir_p(ldir)) return GMB_FETCH_E_OPEN;
    snprintf(lpath, sizeof lpath, "%s/%s.lock", ldir, base);

#if defined(_WIN32) || defined(_WIN64)
    int waited = 0;
    for (;;) {
        /* dwShareMode = 0: accesso esclusivo. Il sistema lo rilascia anche su crash. */
        lk->h = CreateFileA(lpath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (lk->h != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_SHARING_VIOLATION) return GMB_FETCH_E_OPEN;
        if (waited >= wait_ms) return GMB_FETCH_E_LOCKED;
        fetch_sleep_ms(50);
        waited += 50;
    }
    lk->held = 1;
    return GMB_FETCH_OK;
#else
    int fd = open(lpath, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        if (err && errcap) snprintf(err, errcap, "%s: %s", lpath, strerror(errno));
        return GMB_FETCH_E_OPEN;
    }
    int waited = 0;
    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) { lk->fd = fd; lk->held = 1; return GMB_FETCH_OK; }
        if (errno != EWOULDBLOCK && errno != EINTR) {
            if (err && errcap) snprintf(err, errcap, "flock %s: %s", lpath, strerror(errno));
            close(fd);
            return GMB_FETCH_E_OPEN;
        }
        if (waited >= wait_ms) {
            if (err && errcap) snprintf(err, errcap, "%s è in uso da un altro processo", lpath);
            close(fd);
            return GMB_FETCH_E_LOCKED;
        }
        fetch_sleep_ms(50);
        waited += 50;
    }
#endif
}

static void fetch_lock_release(fetch_lock_t* lk)
{
    if (!lk || !lk->held) return;
#if defined(_WIN32) || defined(_WIN64)
    if (lk->h != INVALID_HANDLE_VALUE) CloseHandle(lk->h);
    lk->h = INVALID_HANDLE_VALUE;
#else
    if (lk->fd >= 0) { flock(lk->fd, LOCK_UN); close(lk->fd); }
    lk->fd = -1;
#endif
    lk->held = 0;
}

/* ── verifica ────────────────────────────────────────────────────────────────────────────── */

static int fetch_hex_eq(const char* a, const char* b)
{
    if (!a || !b) return 0;
    for (int i = 0; i < 64; i++) {
        char x = a[i], y = b[i];
        if (!x || !y) return 0;
        if (x >= 'A' && x <= 'F') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'F') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return a[64] == '\0' && b[64] == '\0';
}

int gmb_fetch_verify_file(const char* path, const char* sha256, long long size)
{
    if (!path || !*path) return 0;
    long long got = fetch_file_size(path);
    if (got < 0) return 0;
    if (size > 0 && got != size) return 0;
    if (sha256 && sha256[0]) {
        char hex[65];
        if (!gmb_sha256_file_hex(path, hex, sizeof hex)) return 0;
        if (!fetch_hex_eq(hex, sha256)) return 0;
    }
    return 1;
}

int gmb_fetch_incomplete_path(const char* dest, char* out, size_t cap)
{
    if (!dest || !out || !cap) return 0;
    int n = snprintf(out, cap, "%s.incomplete", dest);
    return (n > 0 && (size_t)n < cap) ? 1 : 0;
}

/* ── default ─────────────────────────────────────────────────────────────────────────────── */

void gmb_fetch_opts_default(gmb_fetch_opts* o)
{
    if (!o) return;
    memset(o, 0, sizeof *o);
    o->lock_wait_ms    = 2000;
    o->stall_retries   = 3;
    o->low_speed_limit = 1024;
    o->low_speed_time  = 60;
    o->connect_timeout = 20;
    o->allow_resume    = 1;
    o->keep_incomplete = 1;
    o->quiet           = 0;
    o->disk_margin     = 64LL * 1024 * 1024;
}

/* ── motore ──────────────────────────────────────────────────────────────────────────────── */

static long long fetch_now_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (long long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L;
#endif
}

int gmb_fetch_to_file(const gmb_fetch_request* req, const gmb_fetch_opts* opts,
                      const gmb_fetch_transport* tr, char* err, size_t errcap)
{
    gmb_fetch_opts def;
    if (err && errcap) err[0] = '\0';
    if (!opts) { gmb_fetch_opts_default(&def); opts = &def; }
    if (!req || !req->dest_path || !*req->dest_path) return GMB_FETCH_E_ARG;
    if (!tr || !tr->get || tr->abi != GMB_FETCH_TRANSPORT_ABI) return GMB_FETCH_E_NO_TRANSPORT;
    if ((!req->url || !*req->url) && !req->resolve) return GMB_FETCH_E_ARG;

    /* Idempotenza: se il file finale è già lì e valido, non si tocca nulla. */
    if ((req->sha256 && req->sha256[0]) || req->size > 0) {
        if (gmb_fetch_verify_file(req->dest_path, req->sha256, req->size)) return GMB_FETCH_OK;
    }

    char tmp[FETCH_PATH_MAX];
    if (!gmb_fetch_incomplete_path(req->dest_path, tmp, sizeof tmp)) return GMB_FETCH_E_ARG;

    char dir[FETCH_PATH_MAX];
    if (!fetch_dirname(req->dest_path, dir, sizeof dir) || !fetch_mkdir_p(dir)) {
        if (err && errcap) snprintf(err, errcap, "impossibile creare la cartella %s", dir);
        return GMB_FETCH_E_OPEN;
    }

    fetch_lock_t lk;
    int rc = fetch_lock_acquire(req->dest_path, opts->lock_wait_ms, &lk, err, errcap);
    if (rc != GMB_FETCH_OK) return rc;

    /* Ripartenza: l'.incomplete è lavoro già pagato. Più lungo dell'atteso = non è il nostro. */
    long long from = opts->allow_resume ? fetch_file_size(tmp) : 0;
    if (from < 0) from = 0;
    if (req->size > 0 && from > req->size) from = 0;

    /* Spazio PRIMA di iniziare: 1,4 GB non si scaricano per scoprire il disco pieno. */
    if (req->size > 0) {
        unsigned long long freeb = fetch_free_bytes(dir);
        if (freeb != (unsigned long long)-1) {
            unsigned long long need = (unsigned long long)(req->size - from);
            if (opts->disk_margin > 0) need += (unsigned long long)opts->disk_margin;
            if (freeb < need) {
                if (err && errcap)
                    snprintf(err, errcap, "servono %.1f MB liberi in %s, disponibili %.1f MB",
                             (double)need / (1024.0 * 1024.0), dir, (double)freeb / (1024.0 * 1024.0));
                fetch_lock_release(&lk);
                return GMB_FETCH_E_SPACE;
            }
        }
    }

    fetch_fd fd = -1;
    rc = GMB_FETCH_OK;
    int attempts = 0, stalls = 0, range_reset = 0;
    long long started = fetch_now_ms();

    while (rc == GMB_FETCH_OK) {
        /* Già completo: nessun GET (il server risponderebbe 416). Si passa alla verifica. */
        if (req->size > 0 && from == req->size) break;

        if (fd < 0) {
            fd = fetch_open_write(tmp, from == 0);
            if (fd < 0) {
                if (err && errcap) snprintf(err, errcap, "%s: %s", tmp, strerror(errno));
                rc = GMB_FETCH_E_OPEN;
                break;
            }
            if (fetch_seek(fd, from) != from) {
                if (err && errcap) snprintf(err, errcap, "%s: seek a %lld fallito", tmp, from);
                rc = GMB_FETCH_E_IO;
                break;
            }
        }

        char url[FETCH_PATH_MAX];
        snprintf(url, sizeof url, "%s", req->url ? req->url : "");
        if (req->resolve) {
            char rerr[256] = "";
            if (req->resolve(req->resolve_user, url, sizeof url, rerr, sizeof rerr) != 0) {
                if (err && errcap) snprintf(err, errcap, "URL non risolto%s%s", rerr[0] ? ": " : "", rerr);
                rc = GMB_FETCH_E_RESOLVE;
                break;
            }
        }

        gmb_fetch_transfer t;
        memset(&t, 0, sizeof t);
        t.from = from;
        t.expected_total = req->size;
        t.connect_timeout = opts->connect_timeout;
        t.low_speed_limit = opts->low_speed_limit;
        t.low_speed_time = opts->low_speed_time;
        t.progress = opts->progress;
        t.progress_user = opts->progress_user;
        t.cancel = opts->cancel;
        t.cancel_user = opts->cancel_user;
        t.received_from = -1;
        t.received_total = -1;

        char terr[512] = "";
        long long from_req = from;
        int trc = tr->get(tr->ctx, url, req->token, from, req->size, fd, &t, terr, sizeof terr);
        attempts++;

        /* Il corpo di una RISPOSTA DI ERRORE non è un pezzo di asset: se il server ha risposto
         * >= 300, i byte scritti in questo tentativo vanno buttati. Senza questo, i 2 byte di
         * "401 Unauthorized" (`no`) restavano nell'.incomplete e la ripresa successiva
         * costruiva un file lungo giusto ma corrotto in testa. */
        if (trc != 0 && t.http_status >= 300) {
            if (fetch_truncate(fd, from_req) == 0 && fetch_seek(fd, from_req) == from_req) from = from_req;
        }

        /* Il file è la verità: riallinea `from` a quello che c'è davvero su disco. */
        long long pos = fetch_tell(fd);
        if (pos >= 0) from = pos;

        if (t.cancelled) {
            if (err && errcap) snprintf(err, errcap, "download annullato");
            rc = GMB_FETCH_E_CANCEL;
            break;
        }
        if (trc != 0) {
            if (terr[0] && err && errcap) snprintf(err, errcap, "%s", terr);
            else if (err && errcap) snprintf(err, errcap, "errore di trasporto (tentativo %d)", attempts);
            if (!t.retryable) { rc = GMB_FETCH_E_NET; break; }
            if (stalls >= opts->stall_retries) { rc = GMB_FETCH_E_NET; break; }
            int backoff = 250 << (stalls > 3 ? 3 : stalls);   /* 250/500/1000/2000 ms */
            stalls++;
            if (!opts->quiet)
                fprintf(stderr, "[fetch] %s: %s — riprovo fra %d ms (%lld byte già presi)\n",
                        req->dest_path, terr, backoff, from);
            fetch_sleep_ms(backoff);
            continue;
        }

        /* Range ignorato (200 mentre chiedevamo un resume): si tronca e si ricomincia. */
        if (from_req > 0 && !t.ranged && t.http_status == 200) {
            if (range_reset) {
                if (err && errcap) snprintf(err, errcap, "il server ignora Range anche dopo il riavvio");
                rc = GMB_FETCH_E_RANGE;
                break;
            }
            range_reset = 1;
            if (fetch_truncate(fd, 0) != 0 || fetch_seek(fd, 0) != 0) {
                if (err && errcap) snprintf(err, errcap, "%s: troncamento fallito", tmp);
                rc = GMB_FETCH_E_IO;
                break;
            }
            from = 0;
            if (!opts->quiet) fprintf(stderr, "[fetch] il server ignora Range: riparto da zero\n");
            continue;
        }

        if (t.received_from >= 0 && t.received_from != from_req) {
            if (err && errcap) snprintf(err, errcap, "Content-Range inizia a %lld, atteso %lld",
                                        t.received_from, from_req);
            rc = GMB_FETCH_E_RANGE;
            break;
        }
        if (req->size > 0 && t.received_total > 0 && t.received_total != req->size) {
            if (err && errcap) snprintf(err, errcap, "Content-Range dichiara %lld byte, il manifest %lld",
                                        t.received_total, req->size);
            rc = GMB_FETCH_E_SIZE;
            break;
        }
        break;   /* trasferimento concluso: si passa alla verifica */
    }

    if (fd >= 0) { fetch_close(fd); fd = -1; }

    if (rc == GMB_FETCH_OK) {
        long long got = fetch_file_size(tmp);
        if (req->size > 0 && got != req->size) {
            if (err && errcap) snprintf(err, errcap, "dimensione finale %lld, attesa %lld", got, req->size);
            rc = GMB_FETCH_E_SIZE;
        } else if (req->sha256 && req->sha256[0]) {
            char hex[65] = "";
            if (!gmb_sha256_file_hex(tmp, hex, sizeof hex)) {
                if (err && errcap) snprintf(err, errcap, "%s: sha256 non calcolabile", tmp);
                rc = GMB_FETCH_E_IO;
            } else if (!fetch_hex_eq(hex, req->sha256)) {
                if (err && errcap) snprintf(err, errcap, "sha256 %s, atteso %s (file conservato per diagnosi)",
                                            hex, req->sha256);
                rc = GMB_FETCH_E_SHA;
            }
        }
    }

    if (rc == GMB_FETCH_OK) {
        remove(req->dest_path);   /* Windows: rename non sovrascrive */
        if (rename(tmp, req->dest_path) != 0) {
            if (err && errcap) snprintf(err, errcap, "rename %s: %s", req->dest_path, strerror(errno));
            rc = GMB_FETCH_E_IO;
        } else if (opts->progress) {
            double secs = (double)(fetch_now_ms() - started) / 1000.0;
            long long total = req->size > 0 ? req->size : fetch_file_size(req->dest_path);
            if (total < 0) total = 0;
            opts->progress(opts->progress_user, total, total,
                           secs > 0.0 ? (double)total / secs : 0.0, 0);
        }
    } else if (!opts->keep_incomplete) {
        remove(tmp);
    }

    fetch_lock_release(&lk);
    return rc;
}
