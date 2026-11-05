/* gmb_win32.h — strato di compatibilità POSIX→Win32 per le nostre librerie FFI.
 *
 * Si compila con `-include gmb_win32.h` (forzato) e `-I GMBasic/native/win32shim/include`, dove stanno gli
 * header-stub POSIX (`netdb.h`, `sys/socket.h`, …): le `#include <…>` dei sorgenti si risolvono
 * qui, e le API vengono mappate su Winsock. Nessuna modifica ai sorgenti: la stessa base di
 * codice compila su macOS/Linux (`-include` non usato) e su Windows.
 */
#ifndef GMB_WIN32_COMPAT_H
#define GMB_WIN32_COMPAT_H
#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
/* dirent: i sorgenti usano `DIR`/`opendir`/`readdir` (67 punti). Il nostro header-stub
 * (GMBasic/native/win32shim/include/dirent.h) li mappa su FindFirstFile/FindNextFile. */
#include <dirent.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Winsock va inizializzata una volta per processo. */
static inline int gmb_win_net_init(void)
{
    static int done = 0;
    if (!done) { WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) == 0) done = 1; }
    return done;
}

/* POSIX: close/read/write su SOCKET → closesocket/recv/send.
 *
 * ATTENZIONE: questi sono MACRO di FUNZIONE, quindi espandono anche `obj.write(...)` e
 * rompono l'accesso ai membri con lo stesso nome (`gmb_text_writer.write`, `gmb_text_reader.read`).
 * Per questo si attivano SOLO compilando le librerie socket (`-DGMB_WIN32_SOCKET_IO`); il
 * runtime e le librerie non-socket restano senza. */
#ifdef GMB_WIN32_SOCKET_IO
#ifndef close
#define close closesocket
#endif
#ifndef read
#define read(fd, buf, n) recv((SOCKET)(fd), (char*)(buf), (int)(n), 0)
#endif
#ifndef write
#define write(fd, buf, n) send((SOCKET)(fd), (const char*)(buf), (int)(n), 0)
#endif
#endif /* GMB_WIN32_SOCKET_IO */

#define usleep(us) Sleep((DWORD)((us) / 1000))
#define sleep(s)   Sleep((DWORD)((s) * 1000))

/* `errno` dopo le chiamate socket: su Windows l'errore è di Winsock. */
static inline int gmb_win_sock_errno(void) { return WSAGetLastError(); }
#define GMB_EWOULDBLOCK WSAEWOULDBLOCK
#define GMB_EINPROGRESS WSAEWOULDBLOCK
#define GMB_EAGAIN      WSAEWOULDBLOCK


/* ── Funzioni POSIX non presenti su Windows ─────────────────────────────────────────────────── */
#include <time.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>     /* il VERO fcntl.h di mingw: sotto si aggiungono flock/fcntl POSIX */

/* windows.h definisce `near`/`far` vuoti (parole chiave legacy): gmb_kdtree.c li usa come
 * nomi di VARIABILI → "expected identifier". Si annullano. */
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

/* ── file locking POSIX (struct flock + fcntl) su LockFileEx ───────────────────────────────── */
#ifndef GMB_WIN32_FLOCK_SHL
#define GMB_WIN32_FLOCK_SHL
struct flock {
    short  l_type;
    short  l_whence;
    long long l_start;
    long long l_len;
    int    l_pid;
};
#ifndef F_WRLCK
#define F_WRLCK 1
#endif
#ifndef F_RDLCK
#define F_RDLCK 0
#endif
#ifndef F_UNLCK
#define F_UNLCK 2
#endif
#ifndef F_GETLK
#define F_GETLK 5
#endif
#ifndef F_SETLK
#define F_SETLK 6
#endif
#ifndef F_SETLKW
#define F_SETLKW 7
#endif
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

static inline int gmb_win_fcntl(int fd, int cmd, ...)
{
    va_list ap;
    int rc = -1;
    va_start(ap, cmd);
    if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK) {
        struct flock* fl = va_arg(ap, struct flock*);
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h == INVALID_HANDLE_VALUE || h == NULL || !fl) { va_end(ap); return -1; }
        if (cmd == F_GETLK) {                       /* "sempre concedibile": si dichiara UNLCK */
            fl->l_type = F_UNLCK;
            rc = 0;
        } else {
            OVERLAPPED ov;
            memset(&ov, 0, sizeof ov);
            unsigned long long start = (unsigned long long)fl->l_start;
            ov.Offset = (DWORD)(start & 0xFFFFFFFFull);
            ov.OffsetHigh = (DWORD)(start >> 32);
            DWORD len = (fl->l_len == 0) ? MAXDWORD : (DWORD)fl->l_len;
            if (fl->l_type == F_UNLCK) {
                rc = UnlockFileEx(h, 0, len, 0, &ov) ? 0 : -1;
            } else {
                DWORD flags = LOCKFILE_EXCLUSIVE_LOCK;
                if (cmd != F_SETLKW) flags |= LOCKFILE_FAIL_IMMEDIATELY;
                rc = LockFileEx(h, flags, 0, len, 0, &ov) ? 0 : -1;
            }
        }
    } else if (cmd == F_GETFL) {
        rc = _O_RDWR;                               /* il fd è già aperto: si dichiara lettura+scrittura */
    } else if (cmd == F_SETFL) {
        (void)va_arg(ap, int);
        rc = 0;
    }
    va_end(ap);
    return rc;
}
#define fcntl gmb_win_fcntl
#endif /* GMB_WIN32_FLOCK_SHL */

/* lstat: su Windows non c'è differenza con stat (niente link simbolici POSIX). */
#ifndef lstat
#define lstat(p, st) stat((p), (st))
#endif

/* ftruncate: il CRT non la dichiara. Una sola definizione (chi include <unistd.h> o
 * <sys/mman.h> la trova comunque, entrambi includono questo header). */
static inline int gmb_win_ftruncate_impl(int fd, long long len)
{
    return _chsize_s(fd, len) == 0 ? 0 : -1;
}
#ifndef ftruncate
#define ftruncate(fd, len) gmb_win_ftruncate_impl((fd), (long long)(len))
#endif

/* fmemopen: mingw non ce l'ha e il CRT non permette stream su memoria. Implementazione
 * REALE: si scrive su un file temporaneo e al `fclose` il contenuto viene copiato nel
 * buffer del chiamante (semantica "w" di POSIX: `out` contiene ciò che è stato scritto).
 * Registro piccolo e statico: 16 stream simultanei sono più che sufficienti (un comando
 * alla volta). Gli stream non registrati passano dritti al fclose del CRT. */
#define GMB_WIN_MEMSTREAM_MAX 16
typedef struct gmb_win_memstream {
    FILE* f;
    char* buf;
    size_t cap;
    int used;
} gmb_win_memstream_t;
static gmb_win_memstream_t g_gmb_memstreams[GMB_WIN_MEMSTREAM_MAX];

static inline FILE* gmb_win_fmemopen(void* buf, size_t cap, const char* mode)
{
    FILE* f;
    int slot = -1;
    if (!buf || cap == 0 || !mode || mode[0] != 'w') return NULL;
    f = tmpfile();
    if (!f) return NULL;
    for (int i = 0; i < GMB_WIN_MEMSTREAM_MAX; i++)
        if (!g_gmb_memstreams[i].used) { slot = i; break; }
    if (slot < 0) { fclose(f); return NULL; }
    g_gmb_memstreams[slot].f = f;
    g_gmb_memstreams[slot].buf = (char*)buf;
    g_gmb_memstreams[slot].cap = cap;
    g_gmb_memstreams[slot].used = 1;
    return f;
}

static inline int gmb_win_fclose(FILE* f)
{
    for (int i = 0; i < GMB_WIN_MEMSTREAM_MAX; i++) {
        if (g_gmb_memstreams[i].used && g_gmb_memstreams[i].f == f) {
            size_t n;
            fflush(f);
            rewind(f);
            n = fread(g_gmb_memstreams[i].buf, 1, g_gmb_memstreams[i].cap - 1, f);
            g_gmb_memstreams[i].buf[n] = '\0';
            g_gmb_memstreams[i].used = 0;
            return fclose(f);
        }
    }
    return fclose(f);
}
#ifndef fmemopen
#define fmemopen(buf, cap, mode) gmb_win_fmemopen((buf), (cap), (mode))
#endif
#ifndef fclose
#define fclose(f) gmb_win_fclose(f)
#endif

/* readlink: su Windows i link sono reparse point. Si risolve il path finale del file
 * (GetFinalPathNameByHandle) e, se differisce, lo si restituisce — è ciò che serve a
 * risalire dal binario caricato alla sua directory. */
static inline long gmb_win_readlink(const char* path, char* buf, unsigned long cap)
{
    HANDLE h;
    char full[MAX_PATH * 2];
    DWORD n;
    if (!path || !buf || cap == 0) return -1;
    h = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    n = GetFinalPathNameByHandleA(h, full, (DWORD)sizeof full, FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (n == 0 || n >= sizeof full) return -1;
    if (_stricmp(full, path) == 0) return 0;          /* non è un link */
    if (n > cap - 1) n = cap - 1;
    memcpy(buf, full, n);
    buf[n] = '\0';
    return (long)n;
}
#ifndef readlink
#define readlink(p, buf, cap) gmb_win_readlink((p), (buf), (unsigned long)(cap))
#endif

/* getline: POSIX, assente su mingw. Legge una riga intera allocando/estendendo il buffer. */
static inline long gmb_win_getline(char** lineptr, size_t* n, FILE* stream)
{
    size_t cap, len = 0;
    int ch;
    if (!lineptr || !n || !stream) return -1;
    cap = *lineptr ? *n : 0;
    if (cap < 128) {
        char* nb = (char*)realloc(*lineptr, 128);
        if (!nb) return -1;
        *lineptr = nb; cap = 128; *n = cap;
    }
    while ((ch = fgetc(stream)) != EOF) {
        if (len + 2 > cap) {
            size_t nc = cap * 2;
            char* nb = (char*)realloc(*lineptr, nc);
            if (!nb) return -1;
            *lineptr = nb; cap = nc; *n = cap;
        }
        (*lineptr)[len++] = (char)ch;
        if (ch == '\n') break;
    }
    if (len == 0 && ch == EOF) return -1;
    (*lineptr)[len] = '\0';
    return (long)len;
}
#ifndef getline
#define getline(lp, n, st) gmb_win_getline((lp), (n), (st))
#endif

/* setenv/unsetenv: su Windows si passa da _putenv_s (CRT). */
static inline int gmb_win_setenv(const char* name, const char* value, int overwrite)
{
    (void)overwrite;
    return _putenv_s(name, value ? value : "");
}
static inline int gmb_win_unsetenv(const char* name)
{
    return _putenv_s(name, "");
}
#ifndef setenv
#define setenv(n, v, ow) gmb_win_setenv((n), (v), (ow))
#endif
#ifndef unsetenv
#define unsetenv(n) gmb_win_unsetenv(n)
#endif

/* link: hard link → CreateHardLinkA(argomenti invertiti rispetto a POSIX). */
static inline int gmb_win_link(const char* oldpath, const char* newpath)
{
    return CreateHardLinkA(newpath, oldpath, NULL) ? 0 : -1;
}
#ifndef link
#define link gmb_win_link
#endif

/* O_NONBLOCK: mingw non lo definisce (su socket si usa ioctlsocket/FIONBIO). */
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004
#endif

#ifndef gmtime_r
#define gmtime_r(t, out) (gmtime_s((out), (t)) == 0 ? (out) : NULL)
#endif
#ifndef localtime_r
#define localtime_r(t, out) (localtime_s((out), (t)) == 0 ? (out) : NULL)
#endif
/* strcasestr: ricerca case-insensitive. Non si usa StrStrIA (richiede <shlwapi.h> e -lshlwapi):
 * bastano tolower/strlen, così non aggiungiamo dipendenze di link. */
static inline char* gmb_win_strcasestr(const char* h, const char* n)
{
    if (!h || !n) return NULL;
    size_t ln = strlen(n);
    if (!ln) return (char*)h;
    for (; *h; h++) {
        size_t i = 0;
        while (i < ln && h[i] &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) i++;
        if (i == ln) return (char*)h;
    }
    return NULL;
}
#ifndef strcasestr
#define strcasestr gmb_win_strcasestr
#endif
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strdup
#define strdup _strdup
#endif
#ifndef realpath
#define realpath(p, out) _fullpath((out), (p), _MAX_PATH)
#endif
#ifndef symlink
static inline int gmb_win_no_symlink(const char* a, const char* b) { (void)a; (void)b; return -1; }
#define symlink gmb_win_no_symlink
#endif
#ifndef mkdir
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

/* gettimeofday: orologio di sistema (microsecondi dal 1970). */
struct timeval;
static inline int gmb_win_gettimeofday(struct timeval* tv, void* tz)
{
    (void)tz;
    if (tv) {
        FILETIME ft; ULARGE_INTEGER u;
        GetSystemTimeAsFileTime(&ft);
        u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
        unsigned long long us = (u.QuadPart / 10ULL) - 11644473600000000ULL;
        tv->tv_sec = (long)(us / 1000000ULL);
        tv->tv_usec = (long)(us % 1000000ULL);
    }
    return 0;
}
#define gettimeofday gmb_win_gettimeofday

/* timegm: interpreta `struct tm` come UTC (mingw non ce l'ha: c'è _mkgmtime64). */
static inline time_t gmb_win_timegm(struct tm* tm)
{
    return (time_t)_mkgmtime64(tm);
}
#ifndef timegm
#define timegm gmb_win_timegm
#endif

/* mkstemps / mkstemp: mingw non li fornisce. `_mktemp_s` sostituisce la parte XXXXXX. */
static inline int gmb_win_mkstemps(char* tmpl, int suffixlen)
{
    (void)suffixlen;
    if (!tmpl || _mktemp_s(tmpl, strlen(tmpl) + 1) != 0) return -1;
    return _open(tmpl, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
}
#ifndef mkstemps
#define mkstemps gmb_win_mkstemps
#endif
#ifndef mkstemp
static inline int gmb_win_mkstemp(char* tmpl) { return gmb_win_mkstemps(tmpl, 0); }
#define mkstemp gmb_win_mkstemp
#endif

/* NOTA: la mappatura st_mtim/st_ctim/st_atim vive in gmb_runtime_c/src/gmb_stat_compat.h
 * (unico punto: macOS st_*timespec, Linux st_*tim, Windows st_*time a secondi). */

#ifdef __cplusplus
}
#endif
#endif /* _WIN32 */
#endif /* GMB_WIN32_COMPAT_H */
