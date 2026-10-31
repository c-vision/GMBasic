/* sys/mman.h (Windows) — `mmap`/`munmap`/`msync`/`ftruncate` su Win32.
 *
 * Serve a gmb_file.c (MEMORYMAP.*): il fd è un descrittore CRT, quindi si passa a
 * `_get_osfhandle` e si mappa la vista con CreateFileMapping/MapViewOfFile.
 */
#ifndef GMB_WIN32_MMAN_H
#define GMB_WIN32_MMAN_H
#if defined(_WIN32) || defined(_WIN64)
#include "gmb_win32.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif
#ifndef PROT_READ
#define PROT_READ  1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 2
#endif
#ifndef MAP_SHARED
#define MAP_SHARED  1
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 2
#endif
#ifndef MS_SYNC
#define MS_SYNC 4
#endif
#ifndef MS_ASYNC
#define MS_ASYNC 1
#endif

static inline void* mmap(void* addr, size_t len, int prot, int flags, int fd, long long off)
{
    (void)addr; (void)flags;
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return MAP_FAILED;
    DWORD hi = (DWORD)((unsigned long long)off >> 32);
    DWORD lo = (DWORD)((unsigned long long)off & 0xFFFFFFFFull);
    DWORD map_prot = (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
    HANDLE sec = CreateFileMappingA(h, NULL, map_prot, 0, 0, NULL);
    if (!sec) return MAP_FAILED;
    DWORD access = FILE_MAP_READ | ((prot & PROT_WRITE) ? FILE_MAP_WRITE : 0);
    void* p = MapViewOfFile(sec, access, hi, lo, len);
    CloseHandle(sec);
    return p ? p : MAP_FAILED;
}

static inline int munmap(void* p, size_t len)
{
    (void)len;
    return UnmapViewOfFile(p) ? 0 : -1;
}

static inline int msync(void* p, size_t len, int flags)
{
    (void)flags;
    return FlushViewOfFile(p, len) ? 0 : -1;
}

/* ftruncate: definita in gmb_win32.h (una sola implementazione). */

/* madvise: MADV_DONTNEED (buttare le pagine) → VirtualAlloc(MEM_RESET), che le marca
 * "non più necessarie" lasciandole riutilizzabili; MADV_WILLNEED è solo un hint (0). */
#ifndef MADV_NORMAL
#define MADV_NORMAL   0
#endif
#ifndef MADV_RANDOM
#define MADV_RANDOM   1
#endif
#ifndef MADV_SEQUENTIAL
#define MADV_SEQUENTIAL 2
#endif
#ifndef MADV_WILLNEED
#define MADV_WILLNEED 3
#endif
#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif
#ifndef MADV_FREE
#define MADV_FREE 8
#endif

static inline int madvise(void* addr, size_t len, int advice)
{
    if (!addr || len == 0) return -1;
    switch (advice) {
        case MADV_DONTNEED:
        case MADV_FREE:
            return VirtualAlloc(addr, len, MEM_RESET, PAGE_READWRITE) ? 0 : -1;
        default:
            return 0;                 /* hint: nessuna azione necessaria */
    }
}
#endif /* _WIN32 */
#endif /* GMB_WIN32_MMAN_H */
