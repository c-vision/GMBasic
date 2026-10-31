/* gmb_file_extra.c — estensioni FileSystem isolate per non disturbare gmb_file.c monolite
 * Aggiunge gmb_fs_dir_filtered per parità con CommonLib/FileSystemUtils.cs:GetFilesWhere
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// gmb_fs_dir_filtered: come gmb_fs_dir ma con predicate gmb_fn_t (char* path -> bool)
// Se fn.fn==NULL, comportamento identico a gmb_fs_dir. Altrimenti filtra via callback.
gmb_list_t gmb_fs_dir_filtered(gmb_string_t mask, gmb_fn_t pred)
{
    gmb_list_t all = gmb_fs_dir(mask);
    if (!pred.fn) return all;
    // pred ha firma int (*)(void* ctx, const char* path) -> 1 keep, 0 skip
    // Ma gmb_fn_t è generico: usiamo gmb_string_t come arg per semplicità
    typedef int (*pred_fn_t)(void*, gmb_string_t);
    pred_fn_t fn = (pred_fn_t)pred.fn;
    gmb_list_t out = gmb_list_new();
    for (size_t i = 0; i < all.count; i++) {
        gmb_val_t v = all.items[i];
        gmb_string_t s;
        if (v.kind == GMB_TYPE_STRING) s = v.v_string;
        else {
            gmb_string_t ts = gmb_val_to_string(v);
            s = ts;
        }
        int keep = fn(pred.ctx, s);
        if (keep) gmb_list_add(&out, v);
    }
    gmb_list_free(&all);
    return out;
}

// gmb_count_lines_filtered: conta linee con predicate opzionale (NULL = tutte)
int gmb_count_lines_filtered(const char* path, gmb_fn_t pred)
{
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    int cnt = 0;
    // pred: int (*)(void* ctx, const char* line) -> 1 count
    typedef int (*pred_line_t)(void*, gmb_string_t);
    pred_line_t fn = pred.fn ? (pred_line_t)pred.fn : NULL;
    while (fgets(line, sizeof(line), f)) {
        // strip newline
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
        if (!fn) cnt++;
        else {
            gmb_string_t s = gmb_string_alloc(line);
            int keep = fn(pred.ctx, s);
            // gmb_string_alloc alloca, ma per test va bene leak controllato breve
            if (keep) cnt++;
        }
    }
    fclose(f);
    return cnt;
}

/* ---- FileSystem metadata (CommonLib/FileSystemUtils.cs:579-588) + Cwd (CoreCwd:103) ----
 * CoreGetLastWriteTime/CreationTime/AccessTime → unix secondi (double), 0 se assente (= DateTime.MinValue)
 */

#include <sys/stat.h>
#include <unistd.h>
#include "gmb_stat_compat.h"   /* timespec portabili (macOS st_*timespec / Linux st_*tim) */
#include <time.h>
#include <errno.h>

double gmb_fs_mtime(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0.0;
    return (double)GMB_ST_MTIME_SEC(st) + (double)GMB_ST_MTIME_NSEC(st) / 1e9;
}
double gmb_fs_ctime(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0.0;
    return (double)GMB_ST_BTIME_SEC(st) + (double)GMB_ST_BTIME_NSEC(st) / 1e9;
}
double gmb_fs_atime(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0.0;
    return (double)GMB_ST_ATIME_SEC(st) + (double)GMB_ST_ATIME_NSEC(st) / 1e9;
}

char* gmb_fs_cwd(void)
{
    long sz = 4096;
    char* buf = (char*)malloc((size_t)sz);
    if (!buf) return NULL;
    while (getcwd(buf, (size_t)sz) == NULL && errno == ERANGE) {
        sz *= 2;
        char* nb = (char*)realloc(buf, (size_t)sz);
        if (!nb) { free(buf); return NULL; }
        buf = nb;
    }
    return buf;
}
int gmb_fs_set_cwd(const char* path)
{
    if (!path) return 0;
    return chdir(path) == 0;
}
