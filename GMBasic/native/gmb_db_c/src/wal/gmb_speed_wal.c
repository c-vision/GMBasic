/* FILE: gmb_speed_wal.c — SPEED WAL (Set=1 Delete=2), NOT fused with DBF WAL */
#include "gmb_db_internal.h"

#ifdef _WIN32
#include <direct.h>
#define gmb_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define gmb_mkdir(p) mkdir(p, 0755)
#endif

int64_t gmb_db_speed_wal_open(const char* dir)
{
    int64_t h;
    gmb_handle_t* hh;
    char waldir[1100], path[1100];
    if (!dir) return -1;
    gmb_mkdir(dir);
    snprintf(waldir, sizeof(waldir), "%s/wal", dir);
    gmb_mkdir(waldir);
    snprintf(path, sizeof(path), "%s/000001.wal", waldir);
    h = gmb_db_alloc(GMB_H_SPEED_WAL);
    if (h < 0) return -1;
    hh = &g_handles[h];
    strncpy(hh->u.wal.path, path, sizeof(hh->u.wal.path) - 1);
    hh->u.wal.fp = fopen(path, "ab");
    if (!hh->u.wal.fp)
    {
        hh->kind = GMB_H_FREE;
        return -1;
    }
    hh->u.wal.count = 0;
    return h;
}

int gmb_db_speed_wal_log_set(int64_t h, int table_id, const void* key, int klen,
                             const void* val, int vlen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED_WAL);
    unsigned char op = 1;
    if (!hh) return GMB_DB_ERR;
    fwrite(&op, 1, 1, hh->u.wal.fp);
    fwrite(&table_id, 4, 1, hh->u.wal.fp);
    fwrite(&klen, 4, 1, hh->u.wal.fp);
    fwrite(key, 1, (size_t)klen, hh->u.wal.fp);
    fwrite(&vlen, 4, 1, hh->u.wal.fp);
    if (vlen > 0) fwrite(val, 1, (size_t)vlen, hh->u.wal.fp);
    fflush(hh->u.wal.fp);
    hh->u.wal.count++;
    return GMB_DB_OK;
}

int gmb_db_speed_wal_log_delete(int64_t h, int table_id, const void* key, int klen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED_WAL);
    unsigned char op = 2;
    if (!hh) return GMB_DB_ERR;
    fwrite(&op, 1, 1, hh->u.wal.fp);
    fwrite(&table_id, 4, 1, hh->u.wal.fp);
    fwrite(&klen, 4, 1, hh->u.wal.fp);
    fwrite(key, 1, (size_t)klen, hh->u.wal.fp);
    fflush(hh->u.wal.fp);
    hh->u.wal.count++;
    return GMB_DB_OK;
}

int gmb_db_speed_wal_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED_WAL);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.wal.fp) fclose(hh->u.wal.fp);
    hh->kind = GMB_H_FREE;
    return GMB_DB_OK;
}
