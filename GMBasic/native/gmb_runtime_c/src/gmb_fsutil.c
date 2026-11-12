/* gmb_fsutil.c — mkdir -p, rmtree, isdir/isfile, spazio libero (una sola implementazione).
 * Usate da gmb_brains.c, gmb_fixtures.c e dal motore di download.
 */
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "gmb_compat_win.h"   /* lstat (solo su Windows) */

#if defined(_WIN32) || defined(_WIN64)
#  include <direct.h>
#  include <io.h>
#  include <windows.h>
#  define FS_MKDIR(p) _mkdir(p)
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  define FS_MKDIR(p) mkdir((p), 0755)
#endif

int gmb_fs_isdir(const char* path)
{
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

int gmb_fs_isfile(const char* path)
{
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

int gmb_fs_mkdir_p(const char* path)
{
    char tmp[4096];
    if (!path || !*path) return 0;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        FS_MKDIR(tmp);
        *p = '/';
    }
    FS_MKDIR(tmp);
    return gmb_fs_isdir(path);
}

void gmb_fs_rmtree(const char* path)
{
    if (!path || !*path) return;
    struct stat st;
    /* lstat, non stat: un symlink si RIMUOVE come link (mai il contenuto del target). */
    if (lstat(path, &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { remove(path); return; }
    DIR* d = opendir(path);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char child[4096];
            snprintf(child, sizeof child, "%s/%s", path, e->d_name);
            gmb_fs_rmtree(child);
        }
        closedir(d);
    }
#if defined(_WIN32) || defined(_WIN64)
    _rmdir(path);
#else
    rmdir(path);
#endif
}

unsigned long long gmb_fs_free_bytes(const char* dir)
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
