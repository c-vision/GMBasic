/* FILE: native/gmb_db_c/src/rdd/DbfJoin.c */
/* Port 1:1 di DbfDriver.NestedLoopJoin (modules/Database/Rdd/DbfDriver.cs). */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
// src.GoTop; while !Eof { sec.GoTop; while !Eof { eval; Append merged }; Skip }

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <limits.h>

/* C#: DbfDriver.NestedLoopJoin — nested loop su due driver, Append sul dest. */

typedef struct JoinEvalCtx {
    DbfDriver* src;
    DbfDriver* sec;
} JoinEvalCtx;

static int join_get_field(void* ctx, const char* fieldName, char* out, int cap)
{
    JoinEvalCtx* j = (JoinEvalCtx*)ctx;
    int idx;
    if (!j || !fieldName || !out) return GMB_DB_ARG;
    idx = DbfDriver_GetFieldIndex(j->src, fieldName);
    if (idx >= 0) return DbfDriver_GetFieldText(j->src, idx, out, cap);
    idx = DbfDriver_GetFieldIndex(j->sec, fieldName);
    if (idx >= 0) return DbfDriver_GetFieldText(j->sec, idx, out, cap);
    out[0] = 0;
    return GMB_DB_ERR;
}

static void join_uniq_name(char* name, int cap, const DbfField* already, int nAlready)
{
    int i, clash, n;
    char base[12];
    snprintf(base, sizeof(base), "%s", name);
    for (;;) {
        clash = 0;
        for (i = 0; i < nAlready; i++) {
            if (gmb_strcasecmp(already[i].Name, name) == 0) {
                clash = 1;
                break;
            }
        }
        if (!clash) return;
        n = (int)strlen(base);
        if (n > 9) n = 9;
        snprintf(name, (size_t)cap, "%.*s2", n, base);
        snprintf(base, sizeof(base), "%s", name);
    }
}

static void join_rtrim(char* s)
{
    int n = s ? (int)strlen(s) : 0;
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static void join_trim_copy(char* dst, int cap, const char* s, int n)
{
    while (n > 0 && isspace((unsigned char)*s)) {
        s++;
        n--;
    }
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(dst, s, (size_t)n);
    dst[n] = 0;
}

static int join_eval(const char* condition, JoinEvalCtx* ctx)
{
    const char* eq;
    char left[64], right[64], lv[256], rv[256];
    int ln;
    if (!condition || !*condition) return 1;
    eq = strchr(condition, '=');
    if (eq && eq[1] != '=' && *(eq + 1)) {
        join_trim_copy(left, (int)sizeof(left), condition, (int)(eq - condition));
        join_trim_copy(right, (int)sizeof(right), eq + 1, (int)strlen(eq + 1));
        ln = (int)strlen(right);
        if (!(ln >= 2 && ((right[0] == '\'' && right[ln - 1] == '\'') || (right[0] == '"' && right[ln - 1] == '"')))
            && join_get_field(ctx, right, rv, (int)sizeof(rv)) == GMB_DB_OK) {
            if (join_get_field(ctx, left, lv, (int)sizeof(lv)) != GMB_DB_OK) return 0;
            join_rtrim(lv);
            join_rtrim(rv);
            return gmb_strcasecmp(lv, rv) == 0;
        }
    }
    return RuntimeExpressionEvaluator_Eval(condition, join_get_field, ctx);
}

int DbfDriver_NestedLoopJoin(DbfDriver* src, DbfDriver* sec, const char* destPath, const char* condition)
{
    DbfField merged[GMB_RDD_MAX_FIELDS];
    int nMerged = 0, i, payloadLen = 0, nOut = 0;
    DbfEngine* dest;
    JoinEvalCtx ctx;
    if (!src || !sec || !destPath) return GMB_DB_ARG;
    if (!src->engine || !sec->engine) return GMB_DB_ARG;
    DbfDriver_SyncCursorState(src);
    DbfDriver_SyncCursorState(sec);
    for (i = 0; i < src->fieldCount && nMerged < GMB_RDD_MAX_FIELDS; i++)
        merged[nMerged++] = src->fields[i];
    for (i = 0; i < sec->fieldCount && nMerged < GMB_RDD_MAX_FIELDS; i++) {
        merged[nMerged] = sec->fields[i];
        join_uniq_name(merged[nMerged].Name, (int)sizeof(merged[nMerged].Name), merged, nMerged);
        nMerged++;
    }
    unlink(destPath);
    if (DbfFile_CreateNew(destPath, "windows-1252", merged, nMerged) != GMB_DB_OK)
        return GMB_DB_ERR;
    dest = DbfEngine_New(destPath, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dest) return GMB_DB_ERR;
    payloadLen = (int)DbfEngine_GetHeader(dest)->RecordLength - 1;
    ctx.src = src;
    ctx.sec = sec;
    DbfDriver_GoTop(src);
    while (!DbfDriver_get_Eof(src)) {
        DbfDriver_GoTop(sec);
        while (!DbfDriver_get_Eof(sec)) {
            int keep = 1;
            if (condition && condition[0])
                keep = join_eval(condition, &ctx);
            if (keep) {
                uint8_t* payload = (uint8_t*)malloc((size_t)payloadLen);
                int off = 0, fi, srcN = src->fieldCount;
                MemoFile* memo = DbfEngine_get_MemoFile(dest);
                if (!payload) {
                    DbfEngine_Dispose(dest);
                    return GMB_DB_ERR;
                }
                memset(payload, ' ', (size_t)payloadLen);
                for (fi = 0; fi < nMerged; fi++) {
                    char val[512];
                    DbfDriver* from = fi < srcN ? src : sec;
                    int fromIdx = fi < srcN ? fi : fi - srcN;
                    if (DbfDriver_GetFieldText(from, fromIdx, val, (int)sizeof(val)) != GMB_DB_OK)
                        val[0] = 0;
                    DbfFieldWriter_Write(payload + off, merged[fi].Length, &merged[fi], val, memo);
                    off += merged[fi].Length;
                }
                if (DbfEngine_AppendRecord(dest, payload, payloadLen) != GMB_DB_OK) {
                    free(payload);
                    DbfEngine_Dispose(dest);
                    return GMB_DB_ERR;
                }
                free(payload);
                nOut++;
            }
            DbfDriver_Skip(sec, 1);
        }
        DbfDriver_Skip(src, 1);
    }
    DbfEngine_Dispose(dest);
    return nOut;
}

static int rdd_sc_append(DbfEngine* e, const char* a, const char* b)
{
    int nFields = 0, payload, off, rc;
    const DbfField* fields = DbfEngine_GetFields(e, &nFields);
    uint8_t* rec;
    MemoFile* memo;
    if (!fields || nFields < 2) return GMB_DB_ERR;
    payload = (int)DbfEngine_GetHeader(e)->RecordLength - 1;
    rec = (uint8_t*)malloc((size_t)payload);
    if (!rec) return GMB_DB_ERR;
    memset(rec, ' ', (size_t)payload);
    memo = DbfEngine_get_MemoFile(e);
    off = 0;
    DbfFieldWriter_Write(rec + off, fields[0].Length, &fields[0], a, memo);
    off += fields[0].Length;
    DbfFieldWriter_Write(rec + off, fields[1].Length, &fields[1], b, memo);
    rc = DbfEngine_AppendRecord(e, rec, payload);
    free(rec);
    return rc;
}

static void rdd_sc_drop(DbfDriver* d1, DbfDriver* d2)
{
    DbfDriver_Dispose(d1);
    DbfDriver_Dispose(d2);
}

int DbfDriver_RddSelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/rddsc_XXXXXX";
    char cust[PATH_MAX], ord[PATH_MAX], ntx[PATH_MAX], dest[PATH_MAX];
    DbfField cf[2], of[2];
    DbfEngine *ce, *oe;
    DbfDriver *d1, *d2;
    char text[32];
    int joined;
    if (!mkdtemp(dir)) return -1;
    snprintf(cust, sizeof(cust), "%s/cust.dbf", dir);
    snprintf(ord, sizeof(ord), "%s/ord.dbf", dir);
    snprintf(ntx, sizeof(ntx), "%s/ord.ntx", dir);
    snprintf(dest, sizeof(dest), "%s/join.dbf", dir);
    cf[0] = DbfField_Create("ID", (uint8_t)DbfFieldType_Character, 10, 0);
    cf[1] = DbfField_Create("NAME", (uint8_t)DbfFieldType_Character, 10, 0);
    of[0] = DbfField_Create("CID", (uint8_t)DbfFieldType_Character, 10, 0);
    of[1] = DbfField_Create("ITEM", (uint8_t)DbfFieldType_Character, 10, 0);
    if (DbfFile_CreateNew(cust, "windows-1252", cf, 2) != GMB_DB_OK) return -2;
    if (DbfFile_CreateNew(ord, "windows-1252", of, 2) != GMB_DB_OK) return -3;
    ce = DbfEngine_New(cust, NULL, 0, 1, LockingStrategy_IntraProcess);
    oe = DbfEngine_New(ord, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!ce || !oe) return -4;
    if (rdd_sc_append(ce, "A", "ALICE") != GMB_DB_OK || rdd_sc_append(ce, "B", "BOB") != GMB_DB_OK) {
        DbfEngine_Dispose(ce);
        DbfEngine_Dispose(oe);
        return -5;
    }
    if (rdd_sc_append(oe, "A", "X") != GMB_DB_OK || rdd_sc_append(oe, "A", "Y") != GMB_DB_OK
        || rdd_sc_append(oe, "B", "Z") != GMB_DB_OK) {
        DbfEngine_Dispose(ce);
        DbfEngine_Dispose(oe);
        return -6;
    }
    if (DbfEngine_IndexOn(oe, ntx, "CID", 10, 0, NULL, NULL, NULL) != GMB_DB_OK) {
        DbfEngine_Dispose(ce);
        DbfEngine_Dispose(oe);
        return -7;
    }
    d1 = DbfDriver_FromEngine(ce, "CUST", 1);
    d2 = DbfDriver_FromEngine(oe, "ORD", 1);
    if (!d1 || !d2) {
        if (d1) DbfDriver_Dispose(d1); else DbfEngine_Dispose(ce);
        if (d2) DbfDriver_Dispose(d2); else DbfEngine_Dispose(oe);
        return -8;
    }
    DbfDriver_SetRelation(d2, d1, "ID");
    DbfDriver_GoTop(d1);
    DbfDriver_SyncRelation(d2);
    if (DbfDriver_GetFieldText(d1, 1, text, (int)sizeof(text)) != GMB_DB_OK || strcmp(text, "ALICE") != 0) {
        rdd_sc_drop(d1, d2);
        return -9;
    }
    if (!DbfDriver_get_Found(d2) || DbfDriver_GetFieldText(d2, 1, text, (int)sizeof(text)) != GMB_DB_OK
        || strcmp(text, "X") != 0) {
        rdd_sc_drop(d1, d2);
        return -10;
    }
    DbfDriver_Skip(d1, 1);
    DbfDriver_SyncRelation(d2);
    if (DbfDriver_GetFieldText(d1, 1, text, (int)sizeof(text)) != GMB_DB_OK || strcmp(text, "BOB") != 0) {
        rdd_sc_drop(d1, d2);
        return -11;
    }
    if (!DbfDriver_get_Found(d2) || DbfDriver_GetFieldText(d2, 1, text, (int)sizeof(text)) != GMB_DB_OK
        || strcmp(text, "Z") != 0) {
        rdd_sc_drop(d1, d2);
        return -12;
    }
    joined = DbfDriver_NestedLoopJoin(d1, d2, dest, "ID=CID");
    rdd_sc_drop(d1, d2);
    if (joined != 3) return -13;
    unlink(cust);
    unlink(ord);
    unlink(ntx);
    unlink(dest);
    {
        char extra[PATH_MAX];
        snprintf(extra, sizeof(extra), "%s/cust.fpt", dir);
        unlink(extra);
        snprintf(extra, sizeof(extra), "%s/ord.fpt", dir);
        unlink(extra);
        snprintf(extra, sizeof(extra), "%s/join.fpt", dir);
        unlink(extra);
    }
    rmdir(dir);
    return GMB_DB_OK;
}
