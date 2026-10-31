/* ============================================================================
 * gmb_compress.c — Compressione via zlib.
 * Algo: "deflate"=raw, "gzip"=header gzip, "zlib"=formato zlib.
 * ============================================================================ */
#include "gmb_runtime.h"
#include "gmb_compress.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef __has_include
#if __has_include(<zlib.h>)
#define GMB_HAVE_ZLIB 1
#include <zlib.h>
#endif
#else
#include <zlib.h>
#define GMB_HAVE_ZLIB 1
#endif

#if defined(GMB_HAVE_ZSTD)
#include <zstd.h>
#endif
#if defined(GMB_HAVE_LZ4)
#include <lz4.h>
#include <lz4frame.h>
#endif
#if defined(GMB_HAVE_LZMA)
#include <lzma.h>
#endif

/* Algoritmi aggiuntivi (zstd / lz4-frame / xz-lzma), librerie bundlate in
 * cache utente (`~/.gmbasic/libs/<rid>`) o le dir di sistema. Compress: buffer malloc (free dal chiamante). */
#if defined(GMB_HAVE_ZSTD)
static unsigned char* gmb_zstd_compress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    size_t cap = ZSTD_compressBound(n);
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) return NULL;
    size_t got = ZSTD_compress(b, cap, d, n, 3);
    if (ZSTD_isError(got)) { free(b); return NULL; }
    *out_len = got; return b;
}
static unsigned char* gmb_zstd_decompress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    unsigned long long sz = ZSTD_getFrameContentSize(d, n);
    size_t cap = (sz == ZSTD_CONTENTSIZE_UNKNOWN || sz == ZSTD_CONTENTSIZE_ERROR || sz == 0)
                 ? (n * 8 + 65536) : (size_t)sz;
    for (int t = 0; t < 6; t++) {
        unsigned char* b = (unsigned char*)malloc(cap);
        if (!b) return NULL;
        size_t got = ZSTD_decompress(b, cap, d, n);
        if (!ZSTD_isError(got)) { *out_len = got; return b; }
        free(b); cap *= 2;
    }
    return NULL;
}
#endif

#if defined(GMB_HAVE_LZ4)
static unsigned char* gmb_lz4_compress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    size_t cap = LZ4F_compressFrameBound(n, NULL);
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) return NULL;
    size_t got = LZ4F_compressFrame(b, cap, d, n, NULL);
    if (LZ4F_isError(got)) { free(b); return NULL; }
    *out_len = got; return b;
}
static unsigned char* gmb_lz4_decompress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    LZ4F_dctx* dctx = NULL;
    if (LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION)) || !dctx) return NULL;
    size_t cap = n * 4 + 65536, used = 0, src = 0;
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) { LZ4F_freeDecompressionContext(dctx); return NULL; }
    size_t r;
    do {
        size_t dstSz = cap - used, srcSz = n - src;
        r = LZ4F_decompress(dctx, b + used, &dstSz, d + src, &srcSz, NULL);
        if (LZ4F_isError(r)) { free(b); LZ4F_freeDecompressionContext(dctx); return NULL; }
        used += dstSz; src += srcSz;
        if (r != 0 && used == cap) {
            cap *= 2; unsigned char* nb = (unsigned char*)realloc(b, cap);
            if (!nb) { free(b); LZ4F_freeDecompressionContext(dctx); return NULL; }
            b = nb;
        }
    } while (r != 0);
    LZ4F_freeDecompressionContext(dctx);
    *out_len = used; return b;
}
#endif

#if defined(GMB_HAVE_LZMA)
static unsigned char* gmb_lzma_compress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    size_t cap = lzma_stream_buffer_bound(n);
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) return NULL;
    size_t pos = 0;
    lzma_ret r = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, NULL, d, n, b, &pos, cap);
    if (r != LZMA_OK) { free(b); return NULL; }
    *out_len = pos; return b;
}
static unsigned char* gmb_lzma_decompress_buf(const unsigned char* d, size_t n, size_t* out_len) {
    size_t cap = n * 8 + 65536;
    unsigned char* b = (unsigned char*)malloc(cap);
    if (!b) return NULL;
    for (int t = 0; t < 8; t++) {
        uint64_t memlimit = UINT64_MAX;
        size_t in_pos = 0, out_pos = 0;
        lzma_ret r = lzma_stream_buffer_decode(&memlimit, 0, NULL, d, &in_pos, n, b, &out_pos, cap);
        if (r == LZMA_OK) { *out_len = out_pos; return b; }
        if (r != LZMA_BUF_ERROR) { free(b); return NULL; }
        cap *= 2; unsigned char* nb = (unsigned char*)realloc(b, cap);
        if (!nb) { free(b); return NULL; }
        b = nb;
    }
    free(b); return NULL;
}
#endif

unsigned char* gmb_compress_data(const char* algo, const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_ZLIB
    uLongf clen = compressBound((uLong)len);
    unsigned char* out = (unsigned char*)malloc(clen + 64);
    if (!out) return NULL;
    int rc = -1;
    int windowBits = 15;
    if (algo && strcmp(algo, "gzip") == 0) windowBits = 15 + 16;
    else if (algo && strcmp(algo, "zlib") == 0) windowBits = 15;
    else windowBits = -15; /* deflate raw */

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    { free(out); return NULL; }
    zs.next_in = (Bytef*)data;
    zs.avail_in = (uInt)len;
    zs.next_out = out;
    zs.avail_out = (uInt)clen;
    rc = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) { free(out); return NULL; }
    *out_len = zs.total_out;
    return out;
#else
    (void)algo; (void)data; (void)len;
    return NULL;
#endif
}

unsigned char* gmb_decompress_data(const char* algo, const unsigned char* data, size_t len, size_t* out_len)
{
    *out_len = 0;
#ifdef GMB_HAVE_ZLIB
    int windowBits = 15;
    if (algo && strcmp(algo, "gzip") == 0) windowBits = 15 + 16;
    else if (algo && strcmp(algo, "zlib") == 0) windowBits = 15;
    else windowBits = -15;

    size_t cap = len * 4 + 1024;
    unsigned char* out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, windowBits) != Z_OK) { free(out); return NULL; }
    zs.next_in = (Bytef*)data;
    zs.avail_in = (uInt)len;
    zs.next_out = out;
    zs.avail_out = (uInt)cap;
    int rc = inflate(&zs, Z_FINISH);
    if (rc == Z_OK || rc == Z_BUF_ERROR)
    {
        /* output non basta: ridimensiona e continua */
        do
        {
            size_t used = (size_t)(zs.next_out - out);
            cap *= 2;
            unsigned char* nb = (unsigned char*)realloc(out, cap);
            if (!nb) { inflateEnd(&zs); free(out); return NULL; }
            out = nb;
            zs.next_out = out + used;
            zs.avail_out = (uInt)(cap - used);
            rc = inflate(&zs, Z_FINISH);
        } while (rc == Z_OK || rc == Z_BUF_ERROR);
    }
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) { free(out); return NULL; }
    *out_len = (size_t)(zs.total_out);
    return out;
#else
    (void)algo; (void)data; (void)len;
    return NULL;
#endif
}

/* algo codes: 0=gzip 1=zlib 2=deflate 3=bzip2 4=brotli 5=zstd 6=lz4 7=xz */
unsigned char* gmb_compress_algo(int algo, const unsigned char* data, size_t len, size_t* out_len)
{
    const char* name = "deflate";
    if (algo == 0) name = "gzip";
    else if (algo == 1) name = "zlib";
    else if (algo == 2) name = "deflate";
    else if (algo == 3) return gmb_bzip2_compress(data, len, out_len);
    else if (algo == 4) return gmb_brotli_compress(data, len, out_len);
#if defined(GMB_HAVE_ZSTD)
    else if (algo == 5) return gmb_zstd_compress_buf(data, len, out_len);
#endif
#if defined(GMB_HAVE_LZ4)
    else if (algo == 6) return gmb_lz4_compress_buf(data, len, out_len);
#endif
#if defined(GMB_HAVE_LZMA)
    else if (algo == 7) return gmb_lzma_compress_buf(data, len, out_len);
#endif
    return gmb_compress_data(name, data, len, out_len);
}

unsigned char* gmb_decompress_algo(int algo, const unsigned char* data, size_t len, size_t* out_len)
{
    const char* name = "deflate";
    if (algo == 0) name = "gzip";
    else if (algo == 1) name = "zlib";
    else if (algo == 2) name = "deflate";
    else if (algo == 3) return gmb_bzip2_decompress(data, len, out_len);
    else if (algo == 4) return gmb_brotli_decompress(data, len, out_len);
#if defined(GMB_HAVE_ZSTD)
    else if (algo == 5) return gmb_zstd_decompress_buf(data, len, out_len);
#endif
#if defined(GMB_HAVE_LZ4)
    else if (algo == 6) return gmb_lz4_decompress_buf(data, len, out_len);
#endif
#if defined(GMB_HAVE_LZMA)
    else if (algo == 7) return gmb_lzma_decompress_buf(data, len, out_len);
#endif
    return gmb_decompress_data(name, data, len, out_len);
}

/* ==================== ZIP archive (STORE, formato minimale) ==================== */

static void gmb_put_le16(unsigned char* p, unsigned v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }
static void gmb_put_le32(unsigned char* p, unsigned long v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF; }

/* Crea un archivio ZIP (metodo STORE) dai file sources; scrive su dest. Ritorna 0 ok.
 * Layout corretto: prima tutti i Local File Header + data, poi Central Directory contigua,
 * poi EOCD. Il bug precedente scriveva CD interleaved con i dati — EOCD puntava a offset errato. */
/* ---------------------------------------------------------------------------
 * gmb_zip_create — IMPLEMENTAZIONE UNICA dello ZIP nel runtime condiviso.
 *
 * Semantica = CompressionUtils.CreateZip (C#), cioe' quella che eseguono IL e
 * PCode: ogni sorgente puo' essere un FILE o una CARTELLA (ricorsiva, tutte le
 * sottocartelle); una sorgente INESISTENTE e' un errore esplicito (mai silenzioso);
 * il nome dell'entry e' il percorso RELATIVO alla radice comune (il dirname piu'
 * corto fra le entry), con '/' come separatore; scrittura STORE.
 *
 * Storico dei difetti corretti qui (2026-10-06, scoperti lanciando il registry
 * reale samples/Community/PackageRegistry/PackageServer.bas, che gira su VM/C):
 *   1) le cartelle venivano IGNORATE (si usava il solo basename, e una sorgente non
 *      apribile veniva saltata) -> archivio vuoto ma RITORNO 0, cioe' successo. Il
 *      client che scaricava il pacchetto riceveva 0 byte e moriva con
 *      "Central Directory corrupt";
 *   2) i sub-path si perdevano: il layout atteso dal client (gmb-package.json piu'
 *      src/...) richiede i nomi relativi alla radice comune, non i basename.
 * Se non c'e' nessuna entry si restituisce -1 (errore), mai 0.
 *
 * Ritorno: 0 = ok, -1 = errore.
 * gmb_zip_files() e gmb_zip_files_semicolon() delegano qui: una sola
 * implementazione (runtime condiviso, AGENTS 0bis).
 * ------------------------------------------------------------------------- */
#define GMB_ZIP_MAXDEPTH 64

/* CRC-32/ISO-HDLC ACCUMULATIVO (stesso risultato di zlib crc32): si inizializza a
 * 0xFFFFFFFF e si passa il valore avanti a blocco. Serve per le entry ZIP: prima il
 * campo CRC restava 0 e i lettori che lo validano (es. zipfile di Python, o
 * ZipArchive di .NET) fallivano con "Bad CRC-32" appena si leggevano i dati.
 * NOTA: gmb_crc32_once piu' in basso NON e' accumulativo (fa gia' lui xor iniziale
 * e finale): non usarlo per lo streaming. */
static uint32_t gmb_zip_crc_update(uint32_t crc, const uint8_t* data, size_t len)
{
    static uint32_t tab[256];
    static int ready = 0;
    if (!ready) {
        for (int i = 0; i < 256; i++) {
            uint32_t c = (uint32_t)i;
            for (int j = 0; j < 8; j++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
            tab[i] = c;
        }
        ready = 1;
    }
    for (size_t i = 0; i < len; i++) crc = tab[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

static int gmb_zip_push(char*** arr, int* count, int* cap, const char* path)
{
    if (*count + 1 > *cap) {
        int ncap = *cap ? *cap * 2 : 32;
        char** n = (char**)realloc(*arr, sizeof(char*) * (size_t)ncap);
        if (!n) return -1;
        *arr = n; *cap = ncap;
    }
    (*arr)[(*count)++] = strdup(path);
    return 0;
}

/* file -> una entry; cartella -> ricorsione su tutte le sottocartelle.
 * Percorso inesistente/illeggibile -> -1 (errore esplicito, niente skip silenzioso). */
static int gmb_zip_walk(const char* path, char*** arr, int* count, int* cap, int depth)
{
    struct stat st;
    if (!path || !*path) return -1;
    if (depth > GMB_ZIP_MAXDEPTH) return -1;
    if (stat(path, &st) != 0) return -1;
    if (S_ISREG(st.st_mode)) return gmb_zip_push(arr, count, cap, path);
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(path);
        struct dirent* de;
        int rc = 0;
        if (!d) return -1;
        while ((de = readdir(d)) != NULL) {
            size_t n;
            char* child;
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            n = strlen(path) + strlen(de->d_name) + 2;
            child = (char*)malloc(n);
            if (!child) { rc = -1; break; }
            snprintf(child, n, "%s/%s", path, de->d_name);
            if (gmb_zip_walk(child, arr, count, cap, depth + 1) != 0) { rc = -1; }
            free(child);
            if (rc != 0) break;
        }
        closedir(d);
        return rc;
    }
    return -1;
}

int gmb_zip_create(const char* dest, char* const* sources, int nsources) {
    char** files = NULL;
    int count = 0, cap = 0;
    char* root = NULL;
    size_t rootlen = 0;
    long* lfh_pos_arr = NULL;
    long* sz_arr = NULL;
    uint32_t* crc_arr = NULL;
    const char** name_arr = NULL;
    FILE* z = NULL;
    int i;
    int rc = -1;

    if (!dest || !*dest || !sources || nsources <= 0) return -1;

    /* 1) raccolta: file e cartelle ricorsivamente; sorgente assente = ERRORE */
    for (i = 0; i < nsources; i++) {
        const char* s = sources[i];
        if (!s) continue;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
        if (!*s) continue;
        if (gmb_zip_walk(s, &files, &count, &cap, 0) != 0) goto cleanup;
    }
    if (count == 0) goto cleanup;   /* niente da archiviare = ERRORE (non successo vuoto) */

    /* 2) radice comune = dirname PIU' CORTO fra le entry (parita' C#) */
    for (i = 0; i < count; i++) {
        const char* slash = strrchr(files[i], '/');
        size_t dl = slash ? (size_t)(slash - files[i]) : 0;
        if (!root || dl < rootlen) {
            char* nr = slash ? strndup(files[i], dl) : strdup("");
            if (!nr) goto cleanup;
            free(root);
            root = nr;
            rootlen = dl;
        }
    }

    lfh_pos_arr = (long*)calloc((size_t)count, sizeof(long));
    sz_arr = (long*)calloc((size_t)count, sizeof(long));
    crc_arr = (uint32_t*)calloc((size_t)count, sizeof(uint32_t));
    name_arr = (const char**)calloc((size_t)count, sizeof(const char*));
    if (!lfh_pos_arr || !sz_arr || !crc_arr || !name_arr) goto cleanup;

    z = fopen(dest, "wb");
    if (!z) goto cleanup;

    /* 3) local header + dati (STORE), memorizzando l'offset per la CD */
    for (i = 0; i < count; i++) {
        const char* full = files[i];
        const char* name = full;
        FILE* f;
        unsigned char lfh[30];
        unsigned char buf[65536];
        size_t nlen, rd;
        long sz;
        uint32_t crc = 0xFFFFFFFFu;   /* CRC-32/ISO-HDLC (lo stesso di zlib) */

        if (rootlen > 0 && strncmp(full, root, rootlen) == 0 && full[rootlen] == '/')
            name = full + rootlen + 1;
        nlen = strlen(name);
        if (nlen == 0) { name = full; nlen = strlen(name); }
        name_arr[i] = name;

        f = fopen(full, "rb");
        if (!f) goto cleanup;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); goto cleanup; }
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz < 0) { fclose(f); goto cleanup; }

        lfh_pos_arr[i] = ftell(z);
        sz_arr[i] = sz;

        memset(lfh, 0, sizeof(lfh));
        lfh[0] = 'P'; lfh[1] = 'K'; lfh[2] = 3; lfh[3] = 4;
        gmb_put_le16(lfh + 4, 20);
        gmb_put_le32(lfh + 18, (unsigned long)sz);
        gmb_put_le32(lfh + 22, (unsigned long)sz);
        gmb_put_le16(lfh + 26, (unsigned)nlen);
        /* il CRC (offset 14) resta 0 qui: viene riscritto dopo lo streaming dei dati */
        if (fwrite(lfh, 1, 30, z) != 30) { fclose(f); goto cleanup; }
        if (nlen && fwrite(name, 1, nlen, z) != nlen) { fclose(f); goto cleanup; }
        while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
            crc = gmb_zip_crc_update(crc, buf, rd);
            if (fwrite(buf, 1, rd, z) != rd) { fclose(f); goto cleanup; }
        }
        fclose(f);
        crc ^= 0xFFFFFFFFu;
        crc_arr[i] = crc;
        {
            unsigned char cle[4];
            gmb_put_le32(cle, crc);
            if (fseek(z, lfh_pos_arr[i] + 14, SEEK_SET) != 0) goto cleanup;
            if (fwrite(cle, 1, 4, z) != 4) goto cleanup;
            if (fseek(z, 0, SEEK_END) != 0) goto cleanup;
        }
    }

    /* 4) central directory CONTIGUA, con l'offset reale di ogni local header */
    {
        long cd_offset = ftell(z);
        long cd_size;
        unsigned char eocd[22];
        for (i = 0; i < count; i++) {
            unsigned char cd[46];
            size_t nlen = strlen(name_arr[i]);
            memset(cd, 0, sizeof(cd));
            cd[0] = 'P'; cd[1] = 'K'; cd[2] = 1; cd[3] = 2;
            gmb_put_le16(cd + 4, 20);
            gmb_put_le16(cd + 6, 20);
            gmb_put_le32(cd + 16, crc_arr[i]);
            gmb_put_le32(cd + 20, (unsigned long)sz_arr[i]);
            gmb_put_le32(cd + 24, (unsigned long)sz_arr[i]);
            gmb_put_le16(cd + 28, (unsigned)nlen);
            gmb_put_le32(cd + 42, (unsigned long)lfh_pos_arr[i]);
            if (fwrite(cd, 1, 46, z) != 46) goto cleanup;
            if (nlen && fwrite(name_arr[i], 1, nlen, z) != nlen) goto cleanup;
        }
        cd_size = ftell(z) - cd_offset;
        memset(eocd, 0, sizeof(eocd));
        eocd[0] = 'P'; eocd[1] = 'K'; eocd[2] = 5; eocd[3] = 6;
        gmb_put_le16(eocd + 8, (unsigned)count);
        gmb_put_le16(eocd + 10, (unsigned)count);
        gmb_put_le32(eocd + 12, (unsigned long)cd_size);
        gmb_put_le32(eocd + 16, (unsigned long)cd_offset);
        if (fwrite(eocd, 1, 22, z) != 22) goto cleanup;
    }
    rc = 0;

cleanup:
    if (z) fclose(z);
    if (rc != 0 && dest) remove(dest);   /* nessun archivio a meta': fallire pulito */
    free(lfh_pos_arr);
    free(sz_arr);
    free(crc_arr);
    free(name_arr);
    free(root);
    for (i = 0; i < count; i++) free(files[i]);
    free(files);
    return rc;
}

/* Elenca i nomi dei file in un archivio ZIP (STORE). Ritorna 0 ok, *outNames malloc. */
int gmb_zip_list(const char* archive, char*** outNames, int* outCount) {
    if (!archive || !outNames || !outCount) return -1;
    *outNames = NULL; *outCount = 0;
    FILE* z = fopen(archive, "rb");
    if (!z) return -1;
    unsigned char eocd[22];
    if (fseek(z, -22, SEEK_END) != 0) { fclose(z); return -1; }
    if (fread(eocd, 1, 22, z) != 22) { fclose(z); return -1; }
    unsigned long cd_off = (unsigned long)eocd[16] | ((unsigned long)eocd[17]<<8) | ((unsigned long)eocd[18]<<16) | ((unsigned long)eocd[19]<<24);
    unsigned short nents = (unsigned short)(eocd[10] | (eocd[11]<<8));
    if (nents == 0) { fclose(z); return 0; }
    char** names = (char**)malloc(sizeof(char*) * nents);
    if (!names) { fclose(z); return -1; }
    fseek(z, cd_off, SEEK_SET);
    int ok = 0;
    for (unsigned short e = 0; e < nents; e++) {
        unsigned char cd[46];
        if (fread(cd, 1, 46, z) != 46) break;
        unsigned short nlen = (unsigned short)(cd[28] | (cd[29]<<8));
        char name[512];
        if (nlen >= sizeof(name)) nlen = (unsigned short)(sizeof(name)-1);
        if (fread(name, 1, nlen, z) != nlen) break;
        name[nlen] = '\0';
        // skip extra/comment
        unsigned short elen = (unsigned short)(cd[30] | (cd[31]<<8));
        unsigned short clen = (unsigned short)(cd[32] | (cd[33]<<8));
        if (elen) fseek(z, elen, SEEK_CUR);
        if (clen) fseek(z, clen, SEEK_CUR);
        names[ok] = strdup(name);
        if (!names[ok]) break;
        ok++;
    }
    fclose(z);
    *outNames = names;
    *outCount = ok;
    return 0;
}

void gmb_zip_list_free(char** names, int count) {
    if (!names) return;
    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
}

/* Itera le entry di un archivio ZIP in MEMORIA (STORE=0 + DEFLATE=8), senza toccare il disco.
 * Per ogni file regolare invoca cb(name, content, clen, ctx). Ritorna il numero di entry. */
int gmb_zip_foreach_mem(const unsigned char* data, size_t len,
                        int (*cb)(const char* name, const unsigned char* content, size_t clen, void* ctx),
                        void* ctx) {
    if (!data || len < 22 || !cb) return 0;
    size_t scan = len > 65557 ? len - 65557 : 0;
    long eocd = -1;
    for (long i = (long)len - 22; i >= (long)scan; i--) {
        if (data[i] == 0x50 && data[i+1] == 0x4b && data[i+2] == 0x05 && data[i+3] == 0x06) { eocd = i; break; }
    }
    if (eocd < 0) return 0;
    unsigned long cd_off = (unsigned long)data[eocd+16] | ((unsigned long)data[eocd+17]<<8)
                         | ((unsigned long)data[eocd+18]<<16) | ((unsigned long)data[eocd+19]<<24);
    unsigned short nents = (unsigned short)(data[eocd+10] | (data[eocd+11]<<8));
    size_t p = cd_off;
    int visited = 0;
    for (unsigned short e = 0; e < nents && p + 46 <= len; e++) {
        const unsigned char* cd = data + p;
        if (!(cd[0] == 0x50 && cd[1] == 0x4b && cd[2] == 0x01 && cd[3] == 0x02)) break;
        unsigned short method = (unsigned short)(cd[10] | (cd[11]<<8));
        unsigned long csize = (unsigned long)cd[20] | ((unsigned long)cd[21]<<8) | ((unsigned long)cd[22]<<16) | ((unsigned long)cd[23]<<24);
        unsigned long usize = (unsigned long)cd[24] | ((unsigned long)cd[25]<<8) | ((unsigned long)cd[26]<<16) | ((unsigned long)cd[27]<<24);
        unsigned short nlen = (unsigned short)(cd[28] | (cd[29]<<8));
        unsigned short elen = (unsigned short)(cd[30] | (cd[31]<<8));
        unsigned short clen = (unsigned short)(cd[32] | (cd[33]<<8));
        unsigned long lho = (unsigned long)cd[42] | ((unsigned long)cd[43]<<8) | ((unsigned long)cd[44]<<16) | ((unsigned long)cd[45]<<24);
        if (p + 46 + nlen > len) break;
        char name[1024];
        size_t nn = nlen < sizeof(name) - 1 ? nlen : sizeof(name) - 1;
        memcpy(name, cd + 46, nn); name[nn] = 0;
        p += 46 + nlen + elen + clen;
        if (nn > 0 && name[nn - 1] == '/') continue;   /* directory */
        if (lho + 30 > len) continue;
        const unsigned char* lh = data + lho;
        if (!(lh[0] == 0x50 && lh[1] == 0x4b && lh[2] == 0x03 && lh[3] == 0x04)) continue;
        unsigned short lnlen = (unsigned short)(lh[26] | (lh[27]<<8));
        unsigned short lelen = (unsigned short)(lh[28] | (lh[29]<<8));
        size_t dstart = lho + 30 + lnlen + lelen;
        if (dstart + csize > len) continue;
        if (method == 0) {
            cb(name, data + dstart, (size_t)csize, ctx);
        } else if (method == 8) {
            unsigned char* out = (unsigned char*)malloc((size_t)usize + 1);
            if (!out) continue;
            z_stream zs; memset(&zs, 0, sizeof(zs));
            zs.next_in = (Bytef*)(data + dstart); zs.avail_in = (uInt)csize;
            zs.next_out = out; zs.avail_out = (uInt)usize;
            if (inflateInit2(&zs, -15) == Z_OK) {
                int rc = inflate(&zs, Z_FINISH);
                inflateEnd(&zs);
                if (rc == Z_STREAM_END) cb(name, out, (size_t)zs.total_out, ctx);
            }
            free(out);
        }
        visited++;
    }
    return visited;
}

/* Estrae un archivio ZIP (STORE) in dir. Ritorna 0 ok. */
int gmb_zip_extract(const char* archive, const char* dir) {
    FILE* z = fopen(archive, "rb");
    if (!z) return -1;
    unsigned char eocd[22];
    fseek(z, -22, SEEK_END);
    if (fread(eocd, 1, 22, z) != 22) { fclose(z); return -1; }
    unsigned long cd_off = (unsigned long)eocd[16] | ((unsigned long)eocd[17]<<8) | ((unsigned long)eocd[18]<<16) | ((unsigned long)eocd[19]<<24);
    unsigned short nents = (unsigned short)(eocd[10] | (eocd[11]<<8));
    fseek(z, cd_off, SEEK_SET);
    for (unsigned short e = 0; e < nents; e++) {
        unsigned char cd[46];
        if (fread(cd, 1, 46, z) != 46) break;
        unsigned short nlen = (unsigned short)(cd[28] | (cd[29]<<8));
        unsigned long lfh_off = (unsigned long)cd[42] | ((unsigned long)cd[43]<<8) | ((unsigned long)cd[44]<<16) | ((unsigned long)cd[45]<<24);
        unsigned long csize = (unsigned long)cd[20] | ((unsigned long)cd[21]<<8) | ((unsigned long)cd[22]<<16) | ((unsigned long)cd[23]<<24);
        char name[512];
        fread(name, 1, nlen, z);
        name[nlen] = '\0';
        /* estrai in dir/name */
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s/%s", dir, name);
        FILE* out = fopen(outpath, "wb");
        if (out) {
            long cur = ftell(z);
            fseek(z, lfh_off + 30 + nlen, SEEK_SET);
            unsigned char buf[65536];
            unsigned long remaining = csize;
            while (remaining > 0) {
                size_t rd = fread(buf, 1, remaining < sizeof(buf) ? remaining : sizeof(buf), z);
                if (rd == 0) break;
                fwrite(buf, 1, rd, out);
                remaining -= (unsigned long)rd;
            }
            fclose(out);
            fseek(z, cur, SEEK_SET);
        }
    }
    fclose(z);
    return 0;
}

/* gmb_compress_string(algo, s): comprime e ritorna base64 (parità C# CompressString). */
static unsigned char* gmb_compress_by_name(const char* algo, const unsigned char* d, size_t n, size_t* out_len) {    if (!algo || !*algo) algo = "gzip";
    if (strcmp(algo, "bzip2") == 0) return gmb_bzip2_compress(d, n, out_len);
    if (strcmp(algo, "brotli") == 0) return gmb_brotli_compress(d, n, out_len);
#if defined(GMB_HAVE_ZSTD)
    if (strcmp(algo, "zstd") == 0) return gmb_zstd_compress_buf(d, n, out_len);
#endif
#if defined(GMB_HAVE_LZ4)
    if (strcmp(algo, "lz4") == 0) return gmb_lz4_compress_buf(d, n, out_len);
#endif
#if defined(GMB_HAVE_LZMA)
    if (strcmp(algo, "xz") == 0 || strcmp(algo, "lzma") == 0) return gmb_lzma_compress_buf(d, n, out_len);
#endif
    return gmb_compress_data(algo, d, n, out_len);
}


/* ── .Z (compress/LZW, formato Unix `compress`) — decoder in casa, nessuna lib esterna.
 * Header: 0x1F 0x9D, poi un byte: bit0-4 = maxbits (9..16), bit7 = block mode.
 * Codici LSB-first a larghezza variabile 9..maxbits; CLEAR=256, primo codice libero 257;
 * in block mode ogni CLEAR azzera il dizionario e la larghezza. ── */
static unsigned char* gmb_lzw_decompress(const unsigned char* d, size_t n, size_t* out_len)
{
    size_t cap = 1 << 16, olen = 0, inp = 0;
    unsigned char* out;
    uint32_t prefix[65536];
    unsigned char suffix[65536];
    unsigned char stack[65536];
    int n_bits, maxcode, free_ent, oldcode = -1, finchar = 0, block_mode, maxbits;
    uint64_t bitbuf = 0;
    int bitcnt = 0;
    if (!d || n < 3 || d[0] != 0x1F || d[1] != 0x9D) return NULL;
    maxbits = d[2] & 0x1F;
    block_mode = (d[2] & 0x80) != 0;
    if (maxbits < 9 || maxbits > 16) return NULL;
    out = (unsigned char*)malloc(cap);
    if (!out) return NULL;
    inp = 3;
    n_bits = 9;
    maxcode = (1 << n_bits) - 1;
    free_ent = block_mode ? 257 : 256;
    for (;;)
    {
        int code;
        /* riempi il bit buffer con byte finché ci sono n_bits bit */
        while (bitcnt < n_bits)
        {
            if (inp >= n) { code = -1; break; }
            bitbuf |= (uint64_t)d[inp++] << bitcnt;
            bitcnt += 8;
        }
        if (bitcnt < n_bits) break;
        code = (int)(bitbuf & (uint64_t)((1u << n_bits) - 1));
        bitbuf >>= n_bits;
        bitcnt -= n_bits;
        if (code == 256)
        {
            /* CLEAR: reset dizionario e larghezza (block mode) */
            n_bits = 9;
            maxcode = (1 << n_bits) - 1;
            free_ent = block_mode ? 257 : 256;
            oldcode = -1;
            continue;
        }
        if (oldcode < 0)
        {
            /* primo codice: byte singolo */
            if (code > 255) { free(out); return NULL; }
            if (olen + 1 > cap) { cap *= 2; unsigned char* no = realloc(out, cap); if (!no) { free(out); return NULL; } out = no; }
            out[olen++] = (unsigned char)code;
            finchar = code;
            oldcode = code;
            continue;
        }
        {
            int sp = 0, c = code;
            if (c >= free_ent)
            {
                if (c != free_ent) { free(out); return NULL; }
                /* caso KwKwK: stringa(oldcode) + primo carattere(oldcode) */
                stack[sp++] = (unsigned char)finchar;
                c = oldcode;
            }
            while (c >= 256)
            {
                if (sp >= (int)sizeof(stack)) { free(out); return NULL; }
                stack[sp++] = suffix[c];
                c = (int)prefix[c];
            }
            stack[sp++] = (unsigned char)c;   /* primo carattere */
            finchar = c;
            if (olen + (size_t)sp > cap)
            {
                while (olen + (size_t)sp > cap) cap *= 2;
                { unsigned char* no = realloc(out, cap); if (!no) { free(out); return NULL; } out = no; }
            }
            while (sp > 0) out[olen++] = stack[--sp];
        }
        /* nuova entry: prefix=oldcode, suffix=primo char della stringa corrente */
        if (free_ent < 65536)
        {
            prefix[free_ent] = (uint32_t)oldcode;
            suffix[free_ent] = (unsigned char)finchar;
            free_ent++;
        }
        oldcode = code;
        if (free_ent > maxcode && n_bits < maxbits)
        {
            n_bits++;
            maxcode = (1 << n_bits) - 1;
        }
    }
    *out_len = olen;
    return out;
}

static unsigned char* gmb_decompress_by_name(const char* algo, const unsigned char* d, size_t n, size_t* out_len) {
    if (!algo || !*algo) algo = "gzip";
    if (strcmp(algo, "lzw") == 0 || strcmp(algo, "compress") == 0) return gmb_lzw_decompress(d, n, out_len);
    if (strcmp(algo, "bzip2") == 0) return gmb_bzip2_decompress(d, n, out_len);
    if (strcmp(algo, "brotli") == 0) return gmb_brotli_decompress(d, n, out_len);
#if defined(GMB_HAVE_ZSTD)
    if (strcmp(algo, "zstd") == 0) return gmb_zstd_decompress_buf(d, n, out_len);
#endif
#if defined(GMB_HAVE_LZ4)
    if (strcmp(algo, "lz4") == 0) return gmb_lz4_decompress_buf(d, n, out_len);
#endif
#if defined(GMB_HAVE_LZMA)
    if (strcmp(algo, "xz") == 0 || strcmp(algo, "lzma") == 0) return gmb_lzma_decompress_buf(d, n, out_len);
#endif
    return gmb_decompress_data(algo, d, n, out_len);
}

gmb_string_t gmb_compress_string(const char* algo, gmb_string_t s) {
    size_t out_len = 0;
    unsigned char* cb = gmb_compress_by_name(algo, (const unsigned char*)(s.data ? s.data : ""), s.length, &out_len);
    if (!cb) return gmb_string_alloc("");
    char* b64 = gmb_base64_encode(cb, out_len);
    free(cb);
    gmb_string_t r = gmb_string_alloc(b64 ? b64 : "");
    if (b64) free(b64);
    return r;
}

/* gmb_decompress_string(algo, b64): decodifica base64 e decomprime. */
gmb_string_t gmb_decompress_string(const char* algo, gmb_string_t s) {
    size_t raw_len = 0, out_len = 0;
    unsigned char* db = gmb_base64_decode(s.data ? s.data : "", &raw_len);
    unsigned char* cd = db ? gmb_decompress_by_name(algo, db, raw_len, &out_len) : NULL;
    if (db) free(db);
    if (!cd) return gmb_string_alloc("");
    /* length-aware: il buffer decompresso NON è NUL-terminato (xz/realloc non lo garantiscono). */
    gmb_string_t r = gmb_string_from_bytes((const char*)cd, (gmb_int_t)out_len);
    free(cd);
    return r;
}

/* Wrapper PUBBLICO (per il grep: decompressione trasparente per nome algoritmo). */
unsigned char* gmb_decompress_named(const char* algo, const unsigned char* d, size_t n, size_t* out_len) {
    return gmb_decompress_by_name(algo, d, n, out_len);
}

/* ── Variante char* (ABI stabile per P/Invoke C#): testo/base64 in e out ────────────────
 * Compress: testo → base64 (malloc). Decompress: base64 → testo (malloc). Liberare con
 * gmb_compress_free. */
char* gmb_compress_b64(const char* algo, const char* text) {
    size_t out_len = 0;
    const char* t = text ? text : "";
    unsigned char* cb = gmb_compress_by_name(algo, (const unsigned char*)t, strlen(t), &out_len);
    if (!cb) return NULL;
    char* b64 = gmb_base64_encode(cb, out_len);
    free(cb);
    return b64;
}
char* gmb_decompress_b64(const char* algo, const char* b64) {
    size_t raw_len = 0, out_len = 0;
    unsigned char* db = gmb_base64_decode(b64 ? b64 : "", &raw_len);
    unsigned char* cd = db ? gmb_decompress_by_name(algo, db, raw_len, &out_len) : NULL;
    if (db) free(db);
    if (!cd) return NULL;
    char* out = (char*)malloc(out_len + 1);
    if (out) { memcpy(out, cd, out_len); out[out_len] = 0; }
    free(cd);
    return out;
}
void gmb_compress_free(char* p) { free(p); }

/* ---- ZIP store-format (archivio senza compressione) ---- */
static uint32_t gmb_crc32_table[256] = {0};
static uint32_t gmb_crc32_once(uint32_t crc, const uint8_t* data, size_t len)
{
    if (gmb_crc32_table[1] == 0)
        for (int i = 0; i < 256; i++)
        {
            uint32_t cr = (uint32_t)i;
            for (int j = 0; j < 8; j++) cr = (cr & 1) ? (cr >> 1) ^ 0xEDB88320u : cr >> 1;
            gmb_crc32_table[i] = cr;
        }
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) crc = gmb_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static int gmb_zip_write_u16(FILE* f, uint16_t v) { uint8_t b[2] = { (uint8_t)(v & 0xFF), (uint8_t)(v >> 8) }; return fwrite(b, 1, 2, f) == 2 ? 0 : -1; }
static int gmb_zip_write_u32(FILE* f, uint32_t v) { uint8_t b[4] = { (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF), (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 24) & 0xFF) }; return fwrite(b, 1, 4, f) == 4 ? 0 : -1; }
static int gmb_zip_read_u16(FILE* f, uint16_t* v) { uint8_t b[2]; if (fread(b, 1, 2, f) != 2) return -1; *v = (uint16_t)(b[0] | (b[1] << 8)); return 0; }
static int gmb_zip_read_u32(FILE* f, uint32_t* v) { uint8_t b[4]; if (fread(b, 1, 4, f) != 4) return -1; *v = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24)); return 0; }

int gmb_zip_files(const char* const* files, int count, const char* dest)
{
    /* Delegato a gmb_zip_create: era una seconda implementazione, per di piu' malformata
     * (central directory intercalata tra le entry e offset EOCD sempre 0 -> archivi che
     * nessun lettore accetta). Una sola implementazione (AGENTS 0bis). */
    return gmb_zip_create((char*)dest, (char* const*)files, count);
}

/* Ritorno 0 = OK, -1 = errore (CONVENZIONE UNICA della famiglia gmb_zip_*: prima
 * ritornava 1 = successo e 0 = fallimento, convenzione opposta a gmb_zip_create/
 * gmb_zip_files/gmb_zip_extract: ogni chiamante doveva ricordare quale delle due valesse,
 * ed e' gia' costato un risultato falsato (COMPRESSION.UNZIP su C99Native)). */
/* Nome di entry ZIP SICURO (zip-slip): niente path assoluto, niente componenti "..",
 * separatori normalizzati a '/'. Ritorna 1 e scrive in `out`, oppure 0 = entry da scartare. */
int gmb_zip_entry_safe(const char* name, char* out, size_t cap)
{
    if (!name || !*name || !out || cap < 2) return 0;
    if (name[0] == '/' || name[0] == '\\') return 0;   /* assoluto */
    size_t w = 0;
    const char* p = name;
    while (*p) {
        const char* s = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t len = (size_t)(p - s);
        if (*p) p++;
        if (len == 0) continue;                          /* "//" o "/" finale: collassa */
        if (len == 1 && s[0] == '.') continue;           /* "./" */
        if (len == 2 && s[0] == '.' && s[1] == '.') return 0;  /* traversal */
        if (w + (w ? 1 : 0) + len + 1 > cap) return 0;
        if (w) out[w++] = '/';
        memcpy(out + w, s, len);
        w += len;
    }
    if (w == 0) return 0;
    out[w] = 0;
    return 1;
}

int gmb_unzip_files(const char* zip, const char* outdir)
{
    FILE* f = fopen(zip, "rb");
    if (!f) return -1;
    int ok = 0;
    uint32_t sig;
    while (gmb_zip_read_u32(f, &sig) == 0 && sig == 0x04034b50u)
    {
        uint16_t ver, flag, method, t1, t2;
        uint32_t crc, csize, usize;
        uint16_t nlen, elen;
        gmb_zip_read_u16(f, &ver); gmb_zip_read_u16(f, &flag);
        gmb_zip_read_u16(f, &method); gmb_zip_read_u16(f, &t1); gmb_zip_read_u16(f, &t2);
        gmb_zip_read_u32(f, &crc); gmb_zip_read_u32(f, &csize); gmb_zip_read_u32(f, &usize);
        gmb_zip_read_u16(f, &nlen); gmb_zip_read_u16(f, &elen);
        char name[1024]; if (nlen >= sizeof(name)) { fseek(f, (long)(csize + elen), SEEK_CUR); continue; }
        if (fread(name, 1, nlen, f) != nlen) { break; }
        name[nlen] = 0;
        fseek(f, (long)elen, SEEK_CUR);
        /* ZIP-SLIP: il nome dell'entry arriva dall'archivio e NON è fidato. Un pacchetto
         * confezionato male (o ostile) con `../../una/cosa` scriverebbe FUORI dalla cartella
         * di destinazione. Si scarta l'intera entry (né `..`, né path assoluti). */
        {
            char safe[1024];
            if (!gmb_zip_entry_safe(name, safe, sizeof(safe))) {
                fseek(f, (long)csize, SEEK_CUR);
                continue;
            }
            snprintf(name, sizeof(name), "%s", safe);
        }
        if (method == 0 && csize == usize && name[0] != 0)
        {
            char path[2048];
            snprintf(path, sizeof(path), "%s/%s", outdir ? outdir : ".", name);
            char* slash = strrchr(path, '/');
            if (slash) { *slash = 0; gmb_file_mkdir(gmb_string_alloc(path)); *slash = '/'; }
            FILE* out = fopen(path, "wb");
            if (out)
            {
                uint8_t buf[8192]; size_t rem = csize;
                while (rem > 0) { size_t chunk = rem > sizeof(buf) ? sizeof(buf) : rem; size_t r = fread(buf, 1, chunk, f); if (r == 0) break; fwrite(buf, 1, r, out); rem -= r; }
                fclose(out);
            }
            else fseek(f, (long)csize, SEEK_CUR);
            ok = 1;
        }
        else if (method == 8 && name[0] != 0)
        {
            /* DEFLATE (zlib raw, windowBits -15): leggi csize byte, inflate, scrivi usize byte.
             * I .gmbpkg di `gmb pack` sono STORED; qui si accettano anche zip DEFLATE esterni. */
            unsigned char* cbuf = (unsigned char*)malloc(csize ? csize : 1);
            if (cbuf && (csize == 0 || fread(cbuf, 1, csize, f) == csize))
            {
                unsigned char* ubuf = (unsigned char*)malloc(usize ? usize : 1);
                if (ubuf)
                {
                    z_stream zs; memset(&zs, 0, sizeof(zs));
                    zs.next_in = cbuf; zs.avail_in = csize;
                    zs.next_out = ubuf; zs.avail_out = usize;
                    if (inflateInit2(&zs, -15) == Z_OK)
                    {
                        int rc = inflate(&zs, Z_FINISH);
                        inflateEnd(&zs);
                        if ((rc == Z_STREAM_END || rc == Z_OK) && zs.total_out == (uLong)usize)
                        {
                            char path[2048];
                            snprintf(path, sizeof(path), "%s/%s", outdir ? outdir : ".", name);
                            char* slash = strrchr(path, '/');
                            if (slash) { *slash = 0; gmb_file_mkdir(gmb_string_alloc(path)); *slash = '/'; }
                            FILE* out = fopen(path, "wb");
                            if (out) { if (usize) fwrite(ubuf, 1, usize, out); fclose(out); ok = 1; }
                        }
                    }
                    free(ubuf);
                }
            }
            free(cbuf);
        }
        else fseek(f, (long)csize, SEEK_CUR);
    }
    fclose(f);
    return ok ? 0 : -1;   /* nessuna entry estratta = errore esplicito, non successo vuoto */
}

/* COMPRESSION.ZIP con elenco ';'-separato (parità PCode: stringa sorgente splittata su ';'). */
int gmb_zip_files_semicolon(const char* paths, const char* dest)
{
    if (!paths || !dest) return 0;
    const char* const* out = 0;
    int n = 0;
    char* copy = strdup(paths);
    if (!copy) return 0;
    char** parts = (char**)calloc(strlen(copy) + 1, sizeof(char*));
    if (!parts) { free(copy); return 0; }
    char* save = NULL;
    for (char* tok = strtok_r(copy, ";", &save); tok; tok = strtok_r(NULL, ";", &save))
    {
        char* end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
        *end = '\0';
        while (*tok == ' ' || *tok == '\t') tok++;
        if (*tok) parts[n++] = tok;
    }
    if (n > 0) out = (const char* const*)parts;
    int rc = gmb_zip_files(out, n, dest);
    free(parts);
    free(copy);
    return rc;
}
