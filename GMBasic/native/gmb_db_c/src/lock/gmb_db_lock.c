/* FILE: gmb_db_lock.c — flock / LockFileEx */
#include "gmb_db_internal.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

int gmb_db_lock_file(const char* path, int exclusive)
{
#ifdef _WIN32
    HANDLE hf;
    OVERLAPPED ov;
    DWORD flags;
    hf = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return -1;
    memset(&ov, 0, sizeof(ov));
    flags = exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(hf, flags, 0, 1, 0, &ov))
    {
        CloseHandle(hf);
        return -1;
    }
    return (int)(intptr_t)hf;
#else
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;
    if (flock(fd, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0)
    {
        close(fd);
        return -1;
    }
    return fd;
#endif
}

int gmb_db_unlock(int fd)
{
#ifdef _WIN32
    HANDLE hf = (HANDLE)(intptr_t)fd;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    UnlockFileEx(hf, 0, 1, 0, &ov);
    CloseHandle(hf);
    return GMB_DB_OK;
#else
    if (fd < 0) return GMB_DB_ERR;
    flock(fd, LOCK_UN);
    close(fd);
    return GMB_DB_OK;
#endif
}
