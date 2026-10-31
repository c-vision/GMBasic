/* FILE: native/gmb_db_c/src/ISAM/IsamIntegrityChecker.c */
/* Port 1:1 di modules/Database/ISAM/IsamIntegrityChecker.cs */
/* Controlli di integrita' strutturale estesi oltre la validazione B+ base. */
#include "../../include/gmb_db_port.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

/* --- helper: lettura grezza di N byte all'offset assoluto (port di IsamFileHandle.ReadBytesAt) --- */
static int isam_read_at(IsamFileHandle* self, int64_t offset, void* out, int len)
{
    if (!self || !self->fs || offset < 0 || len < 0 || !out) return GMB_DB_ERR;
#ifdef _WIN32
    if (fseek(self->fs, (long)offset, SEEK_SET) != 0) return GMB_DB_ERR;
#else
    if (fseeko(self->fs, (off_t)offset, SEEK_SET) != 0) return GMB_DB_ERR;
#endif
    return fread(out, 1, (size_t)len, self->fs) == (size_t)len ? GMB_DB_OK : GMB_DB_ERR;
}

/* Port di IsamIntegrityChecker.ValidatePageFreeList: cammina la free list delle pagine. */
int IsamIntegrityChecker_ValidatePageFreeList(IsamFileHandle* self, char* err, int errCap)
{
    int64_t head;
    int64_t heapStart, heapEnd;
    int64_t current;
    int64_t next;
    int64_t visited[256];
    int nv = 0;
    int i;
    if (!self) { if (err && errCap > 0) snprintf(err, errCap, "Handle ISAM nullo."); return GMB_DB_ERR; }
    head = self->Header.PageFreeListHeadOffset;
    if (head == -1) return GMB_DB_OK;
    heapStart = self->Header.LobHeapStartOffset > 0
        ? self->Header.LobHeapStartOffset : Config_FileHeaderFieldsPrefixSize;
    heapEnd = self->Header.NextAvailablePageOffset;
    current = head;
    while (current != -1) {
        if (current < heapStart || current >= heapEnd) {
            if (err && errCap > 0)
                snprintf(err, errCap,
                    "Free list pagine: offset %lld fuori dall'area heap [%lld, %lld).",
                    (long long)current, (long long)heapStart, (long long)heapEnd);
            return GMB_DB_ERR;
        }
        for (i = 0; i < nv; i++) {
            if (visited[i] == current) {
                if (err && errCap > 0)
                    snprintf(err, errCap, "Free list pagine: ciclo rilevato all'offset %lld.",
                        (long long)current);
                return GMB_DB_ERR;
            }
        }
        if (nv < 256) visited[nv++] = current;
        if (isam_read_at(self, current, &next, 8) != GMB_DB_OK) {
            if (err && errCap > 0)
                snprintf(err, errCap, "Free list pagine: lettura fallita a offset %lld.",
                    (long long)current);
            return GMB_DB_ERR;
        }
        if (next != -1 && (next <= 0 || next % IsamPageChecksum_PageSize != 0)) {
            if (err && errCap > 0)
                snprintf(err, errCap,
                    "Free list pagine: puntatore successivo non valido (%lld) a offset %lld.",
                    (long long)next, (long long)current);
            return GMB_DB_ERR;
        }
        current = next;
    }
    return GMB_DB_OK;
}

/* Port di IsamIntegrityChecker.ValidateJsonLobPointers (privata nel C#): valida i
 * puntatori LOB JSON di un record. */
static int IsamIntegrityChecker_ValidateJsonLobPointers(IsamFileHandle* self, int64_t recordOffset,
                                                        char* err, int errCap)
{
    int64_t jsonOffsets[64];
    int nf;
    int64_t recordSize;
    uint8_t* buf;
    int64_t heapStart, heapEnd;
    int i;
    nf = HeapRelocation_GetJsonFieldOffsets(self ? self->Schema : NULL, jsonOffsets, 64);
    if (nf == 0) return GMB_DB_OK;
    recordSize = self->Header.RecordSize;
    buf = (uint8_t*)malloc(recordSize > 0 ? (size_t)recordSize : 1);
    if (!buf) return GMB_DB_OK;
    if (IsamFileHandle_ReadRecordBytes(self, recordOffset, buf, (int)recordSize) != GMB_DB_OK) {
        free(buf);
        return GMB_DB_OK;
    }
    heapStart = self->Header.LobHeapStartOffset > 0
        ? self->Header.LobHeapStartOffset : self->Header.NextAvailablePageOffset;
    heapEnd = self->Header.NextAvailablePageOffset;
    for (i = 0; i < nf; i++) {
        int64_t fieldOffset = jsonOffsets[i];
        int64_t lobOffset;
        char tmp[16];
        if (fieldOffset + (int64_t)sizeof(long) > recordSize) {
            if (err && errCap > 0)
                snprintf(err, errCap,
                    "Record %lld: offset campo JSON %lld fuori dal record.",
                    (long long)recordOffset, (long long)fieldOffset);
            free(buf);
            return GMB_DB_ERR;
        }
        memcpy(&lobOffset, buf + fieldOffset, 8);
        if (lobOffset == 0) continue;
        if (lobOffset < heapStart || lobOffset >= heapEnd) {
            if (err && errCap > 0)
                snprintf(err, errCap,
                    "Record %lld: puntatore LOB JSON %lld fuori heap [%lld, %lld).",
                    (long long)recordOffset, (long long)lobOffset,
                    (long long)heapStart, (long long)heapEnd);
            free(buf);
            return GMB_DB_ERR;
        }
        /* ReadLob ritorna la lunghezza del payload (>0) a successo; <0 = errore. */
        if (IsamFileHandle_ReadLob(self, lobOffset, tmp, (int)sizeof(tmp)) < 0) {
            if (err && errCap > 0)
                snprintf(err, errCap, "Record %lld: LOB JSON a %lld illeggibile.",
                    (long long)recordOffset, (long long)lobOffset);
            free(buf);
            return GMB_DB_ERR;
        }
    }
    free(buf);
    return GMB_DB_OK;
}

/* Port di IsamIntegrityChecker.ValidateActiveRecordPointers: per ogni offset referenziato
 * dall'indice primario, verifica che sia dentro l'area dati, con stato record valido e
 * puntatori LOB JSON integri. */
typedef struct {
    IsamFileHandle* handle;
    int64_t dataStart;
    char* err;
    int errCap;
    int fail;
    int64_t recordCount;
} IsamRecCtx;

static int isam_validate_record(void* ctx, int64_t offset)
{
    IsamRecCtx* c = (IsamRecCtx*)ctx;
    RecordMetadata meta;
    c->recordCount++;
    if (offset < c->dataStart) {
        if (c->err && c->errCap > 0)
            snprintf(c->err, c->errCap,
                "Offset record %lld precede l'area dati (%lld).",
                (long long)offset, (long long)c->dataStart);
        c->fail = 1;
        return 1;
    }
    if (IsamFileHandle_ReadRecordMetadata(c->handle, offset, &meta) != GMB_DB_OK) {
        if (c->err && c->errCap > 0)
            snprintf(c->err, c->errCap, "Offset %lld: metadata illeggibili.",
                (long long)offset);
        c->fail = 1;
        return 1;
    }
    if (meta.Status != Config_RecordStatusActive && meta.Status != Config_RecordStatusDeleted) {
        if (c->err && c->errCap > 0)
            snprintf(c->err, c->errCap, "Offset %lld: stato record non valido 0x%02X.",
                (long long)offset, meta.Status);
        c->fail = 1;
        return 1;
    }
    if (IsamIntegrityChecker_ValidateJsonLobPointers(c->handle, offset, c->err, c->errCap) != GMB_DB_OK) {
        c->fail = 1;
        return 1;
    }
    return 0;
}

int IsamIntegrityChecker_ValidateActiveRecordPointers(IsamFileHandle* self, BPlusTreeManager* primary,
                                                      char* err, int errCap)
{
    IsamRecCtx c;
    if (!self || !primary) {
        if (err && errCap > 0) snprintf(err, errCap, "Handle/indice ISAM nullo.");
        return GMB_DB_ERR;
    }
    if (BPlusTreeManager_GetRootOffset(primary) == -1) return GMB_DB_OK;
    memset(&c, 0, sizeof(c));
    c.handle = self;
    c.dataStart = IsamFileHandle_DataAreaStartOffset(self);
    c.err = err;
    c.errCap = errCap;
    if (BPlusTreeManager_VisitRecordOffsets(primary, isam_validate_record, &c) != GMB_DB_OK)
        return GMB_DB_ERR;
    return c.fail ? GMB_DB_ERR : GMB_DB_OK;
}


/* Conteggio record nell'indice primario (port del conteggio di Engine.ValidateIntegrityCore). */
typedef struct { int64_t count; } IsamCountCtx;
static int isam_count_cb(void* ctx, int64_t offset)
{
    (void)offset;
    ((IsamCountCtx*)ctx)->count++;
    return 0;
}
int IsamIntegrityChecker_CountRecords(BPlusTreeManager* primary, int64_t* out)
{
    IsamCountCtx c;
    memset(&c, 0, sizeof(c));
    if (!primary || !out) return GMB_DB_ERR;
    if (BPlusTreeManager_VisitRecordOffsets(primary, isam_count_cb, &c) != GMB_DB_OK) return GMB_DB_ERR;
    *out = c.count;
    return GMB_DB_OK;
}

