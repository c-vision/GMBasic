/* gmb_writebytes.c — WRITEBYTES: scrittura BYTE-ESATTA su file.
 *
 * Complementare di gmb_read_all_bytes (gmb_file.c): FWrite passa da fputs/UTF-8 e si
 * ferma al primo 0x00, quindi non può produrre formati binari. Le dichiarazioni sono in
 * gmb_runtime.h; qui ci sono le tre implementazioni usate dai backend (buffer, lista
 * del runtime, array double del C99Native).
 */
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gmb_write_all_bytes(const char* path, const uint8_t* data, size_t len)
{
    FILE* f;
    size_t w;
    if (path == NULL || *path == '\0') return -1;
    if (data == NULL && len > 0) return -1;
    f = fopen(path, "wb");
    if (f == NULL) return -1;
    w = (len > 0) ? fwrite(data, 1, len, f) : 0;
    if (fclose(f) != 0) return -1;
    return (int)w;
}

static int wr_byte_value(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_DOUBLE) return (int)v.v_double & 0xFF;
    if (v.kind == GMB_TYPE_INT) return (int)v.v_int & 0xFF;
    if (v.kind == GMB_TYPE_BOOL) return v.v_int ? 1 : 0;
    return 0;
}

int gmb_write_all_bytes_list(const char* path, gmb_list_t list)
{
    uint8_t* buf;
    size_t i, n = list.count;
    int rc;
    if (path == NULL || *path == '\0') return -1;
    if (n == 0) return gmb_write_all_bytes(path, NULL, 0);
    buf = (uint8_t*)malloc(n);
    if (buf == NULL) return -1;
    for (i = 0; i < n; i++) buf[i] = (uint8_t)wr_byte_value(list.items[i]);
    rc = gmb_write_all_bytes(path, buf, n);
    free(buf);
    return rc;
}

int gmb_write_all_bytes_f64(const char* path, const double* src, int count)
{
    uint8_t* buf;
    int i, rc;
    if (path == NULL || *path == '\0' || count < 0) return -1;
    if (count == 0) return gmb_write_all_bytes(path, NULL, 0);
    if (src == NULL) return -1;
    buf = (uint8_t*)malloc((size_t)count);
    if (buf == NULL) return -1;
    for (i = 0; i < count; i++) buf[i] = (uint8_t)((int)src[i] & 0xFF);
    rc = gmb_write_all_bytes(path, buf, (size_t)count);
    free(buf);
    return rc;
}
