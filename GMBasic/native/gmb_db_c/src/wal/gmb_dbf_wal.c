/* FILE: gmb_dbf_wal.c — WAL for DBF/ISAM (common/Databases/WriteAheadLog.cs opcodes) */
#include "gmb_db_internal.h"

int64_t gmb_db_wal_open(const char* path)
{
    int64_t h;
    gmb_handle_t* hh;
    if (!path) return -1;
    h = gmb_db_alloc(GMB_H_WAL);
    if (h < 0) return -1;
    hh = &g_handles[h];
    strncpy(hh->u.wal.path, path, sizeof(hh->u.wal.path) - 1);
    hh->u.wal.fp = fopen(path, "a+b");
    if (!hh->u.wal.fp)
    {
        hh->kind = GMB_H_FREE;
        return -1;
    }
    hh->u.wal.count = 0;
    {
        unsigned char op;
        fseek(hh->u.wal.fp, 0, SEEK_SET);
        while (fread(&op, 1, 1, hh->u.wal.fp) == 1)
        {
            unsigned char lenb[4];
            unsigned int len;
            hh->u.wal.count++;
            if (op == 10 || op == 11 || op == 8) continue; /* begin/commit/checkpoint */
            fseek(hh->u.wal.fp, 8, SEEK_CUR); /* offset */
            if (fread(lenb, 1, 4, hh->u.wal.fp) != 4) break;
            len = gmb_db_read_u32(lenb);
            fseek(hh->u.wal.fp, (long)len, SEEK_CUR);
        }
        fseek(hh->u.wal.fp, 0, SEEK_END);
    }
    return h;
}

int gmb_db_wal_append(int64_t h, int op, int64_t offset, const void* data, int len)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_WAL);
    unsigned char b;
    if (!hh) return GMB_DB_ERR;
    b = (unsigned char)op;
    fwrite(&b, 1, 1, hh->u.wal.fp);
    {
        int i;
        for (i = 0; i < 8; i++)
            fputc((int)((offset >> (8 * i)) & 0xFF), hh->u.wal.fp);
    }
    gmb_db_write_u32(hh->u.wal.fp, (unsigned int)len);
    if (len > 0 && data) fwrite(data, 1, (size_t)len, hh->u.wal.fp);
    fflush(hh->u.wal.fp);
    hh->u.wal.count++;
    return GMB_DB_OK;
}

int gmb_db_wal_checkpoint(int64_t h)
{
    /* DirectCheckpoint = 8 */
    return gmb_db_wal_append(h, 8, 0, NULL, 0);
}

int gmb_db_wal_replay_count(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_WAL);
    return hh ? hh->u.wal.count : 0;
}

int gmb_db_wal_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_WAL);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.wal.fp) fclose(hh->u.wal.fp);
    hh->kind = GMB_H_FREE;
    return GMB_DB_OK;
}
