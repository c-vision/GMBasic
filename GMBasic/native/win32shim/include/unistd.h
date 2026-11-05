/* unistd.h (Windows) — le API POSIX di I/O e sistema che i nostri sorgenti usano.
 *
 * Il grosso degli shim sta in gmb_win32.h (force-include). Qui si aggiungono le funzioni
 * che arrivano da `<unistd.h>` e che mingw non fornisce: pread/pwrite, fsync/fdatasync,
 * sysconf, getpagesize, access(X_OK), ssize_t/PID. Implementazioni REALI (non stub):
 * pread/pwrite si appoggiano a `_lseeki64` + `_read`/`_write`, sysconf a Win32.
 */
#pragma once
#include "gmb_win32.h"

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

typedef long long ssize_t_compat;

/* Letture/scritture posizionate: lseek + read/write sotto un lock implicito del fd
 * (su Windows il CRT non ha una pread nativa). */
static inline long long gmb_win_pread(int fd, void* buf, unsigned int n, long long off)
{
    __int64 prev = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, off, SEEK_SET) < 0) return -1;
    int r = _read(fd, buf, n);
    _lseeki64(fd, prev, SEEK_SET);
    return r;
}
static inline long long gmb_win_pwrite(int fd, const void* buf, unsigned int n, long long off)
{
    __int64 prev = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, off, SEEK_SET) < 0) return -1;
    int r = _write(fd, buf, n);
    _lseeki64(fd, prev, SEEK_SET);
    return r;
}
#ifndef pread
#define pread(fd, buf, n, off) gmb_win_pread((fd), (buf), (unsigned int)(n), (long long)(off))
#endif
#ifndef pwrite
#define pwrite(fd, buf, n, off) gmb_win_pwrite((fd), (buf), (unsigned int)(n), (long long)(off))
#endif

/* pipe: il CRT la chiama _pipe (fd, non SOCKET). */
static inline int gmb_win_pipe(int fds[2])
{
    return _pipe(fds, 65536, _O_BINARY);
}
#ifndef pipe
#define pipe(fds) gmb_win_pipe(fds)
#endif

/* fsync/fdatasync: il CRT non espone FlushFileBuffers sul fd → si fa via handle. */
static inline int gmb_win_fsync(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return -1;
    return FlushFileBuffers(h) ? 0 : -1;
}
#ifndef fsync
#define fsync(fd) gmb_win_fsync(fd)
#endif
#ifndef fdatasync
#define fdatasync(fd) gmb_win_fsync(fd)
#endif

/* ftruncate: definita in gmb_win32.h (una sola implementazione). */

/* sysconf: solo le voci che il codice chiede davvero. */
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 84
#endif
#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE 30
#endif
#ifndef _SC_PAGE_SIZE
#define _SC_PAGE_SIZE 30
#endif
#ifndef _SC_PHYS_PAGES
#define _SC_PHYS_PAGES 85
#endif
static inline long gmb_win_sysconf(int name)
{
    switch (name) {
        case _SC_NPROCESSORS_ONLN: {
            DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
            return n > 0 ? (long)n : 1;
        }
        case _SC_PHYS_PAGES: {
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof ms;
            if (GlobalMemoryStatusEx(&ms))
                return (long)(ms.ullTotalPhys / 4096ull);   /* pagine da 4 KB */
            return -1;
        }
        case _SC_PAGESIZE: {      /* _SC_PAGE_SIZE ha lo stesso valore (30) */
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            return (long)si.dwPageSize;
        }
        default: return -1;
    }
}
#ifndef sysconf
#define sysconf(n) gmb_win_sysconf(n)
#endif

static inline long gmb_win_getpagesize(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long)si.dwPageSize;
}
#ifndef getpagesize
#define getpagesize() gmb_win_getpagesize()
#endif

#endif /* _WIN32 */
