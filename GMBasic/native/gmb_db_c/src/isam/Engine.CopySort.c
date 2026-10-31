/* FILE: native/gmb_db_c/src/isam/Engine.CopySort.c */
/* Port 1:1 di modules/Database/ISAM/Engine.cs — CopyTo/SortTo/CSV/AppendMany. */
#include "../../include/gmb_db_port.h"

#include <unistd.h>

#define COPY_TEXT_CAP 256
#define COPY_MAX_ROWS 100000

typedef struct {
    const IsamSchema* schema;
    const IsamFieldValue* fields;
} CopyFilterCtx;

typedef struct {
    const IsamSchema* schema;
    int fieldIndex;
    int descending;
} SortCmpCtx;

static SortCmpCtx g_sort_cmp;

static void copy_fields_text_from_out(IsamFieldValue* fields, int fieldCount)
{
    int i;
    for (i = 0; i < fieldCount; i++) {
        if (fields[i].textOut && fields[i].textOut[0] && !fields[i].text)
            fields[i].text = fields[i].textOut;
    }
}

static int copy_filter_get(void* ctx, const char* fieldName, char* out, int cap)
{
    CopyFilterCtx* c = (CopyFilterCtx*)ctx;
    int i;
    if (!c || !c->schema || !c->fields || !fieldName || !out || cap <= 0) return GMB_DB_ERR;
    out[0] = 0;
    for (i = 0; i < c->schema->FieldCount; i++) {
        if (gmb_strcasecmp(c->schema->Fields[i].Name, fieldName) != 0) continue;
        if (strcmp(c->schema->Fields[i].DataTypeCode, "StringType") == 0) {
            const char* t = c->fields[i].text ? c->fields[i].text
                : (c->fields[i].textOut ? c->fields[i].textOut : "");
            snprintf(out, (size_t)cap, "%s", t);
        } else if (IsamField_IsFloat(&c->schema->Fields[i])) {
            IsamField_FormatFloat(&c->schema->Fields[i], c->fields[i].i64, out, cap);
        } else {
            snprintf(out, (size_t)cap, "%lld", (long long)c->fields[i].i64);
        }
        return GMB_DB_OK;
    }
    return GMB_DB_ERR;
}

static int copy_row_matches(const IsamSchema* schema, const IsamFieldValue* fields, const char* forCondition)
{
    CopyFilterCtx ctx;
    if (!forCondition || !forCondition[0]) return 1;
    ctx.schema = schema;
    ctx.fields = fields;
    return RuntimeExpressionEvaluator_Eval(forCondition, copy_filter_get, &ctx);
}

static IsamFieldValue* copy_alloc_fields(const IsamSchema* s)
{
    IsamFieldValue* fields;
    int i;
    if (!s || s->FieldCount <= 0) return NULL;
    fields = (IsamFieldValue*)calloc((size_t)s->FieldCount, sizeof(*fields));
    if (!fields) return NULL;
    for (i = 0; i < s->FieldCount; i++) {
        int cap;
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") != 0) continue;
        cap = s->Fields[i].Length + 1;
        if (cap < COPY_TEXT_CAP) cap = COPY_TEXT_CAP;
        fields[i].textOut = (char*)malloc((size_t)cap);
        if (!fields[i].textOut) {
            int k;
            for (k = 0; k < i; k++) free(fields[k].textOut);
            free(fields);
            return NULL;
        }
        fields[i].textOutCap = cap;
    }
    return fields;
}

static void copy_free_fields(IsamFieldValue* fields, int fieldCount)
{
    int i;
    if (!fields) return;
    for (i = 0; i < fieldCount; i++) free(fields[i].textOut);
    free(fields);
}

static void copy_fail_dest(Engine* dest, const char* destPath)
{
    if (dest) Engine_Dispose(dest);
    if (destPath) unlink(destPath);
}

static int copy_field_index(const IsamSchema* schema, const char* fieldName)
{
    int i;
    if (!schema) return -1;
    if (!fieldName || !fieldName[0]) return 0;
    for (i = 0; i < schema->FieldCount; i++) {
        if (gmb_strcasecmp(schema->Fields[i].Name, fieldName) == 0) return i;
    }
    return -1;
}

static int copy_sort_cmp(const void* a, const void* b)
{
    const IsamFieldValue* ra = *(const IsamFieldValue* const*)a;
    const IsamFieldValue* rb = *(const IsamFieldValue* const*)b;
    int fi = g_sort_cmp.fieldIndex;
    int c = 0;
    const char* ta;
    const char* tb;
    if (fi < 0) return 0;
    if (strcmp(g_sort_cmp.schema->Fields[fi].DataTypeCode, "IntegerType") == 0
        || strcmp(g_sort_cmp.schema->Fields[fi].DataTypeCode, "LongType") == 0) {
        if (ra[fi].i64 < rb[fi].i64) c = -1;
        else if (ra[fi].i64 > rb[fi].i64) c = 1;
    } else {
        ta = ra[fi].text ? ra[fi].text : (ra[fi].textOut ? ra[fi].textOut : "");
        tb = rb[fi].text ? rb[fi].text : (rb[fi].textOut ? rb[fi].textOut : "");
        c = strcmp(ta, tb);
    }
    return g_sort_cmp.descending ? -c : c;
}

static int copy_clone_row(const IsamSchema* s, const IsamFieldValue* src, IsamFieldValue** outRow)
{
    IsamFieldValue* row;
    int i, n;
    n = s->FieldCount;
    row = (IsamFieldValue*)calloc((size_t)n, sizeof(*row));
    if (!row) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        row[i].i64 = src[i].i64;
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0) {
            const char* t = src[i].text ? src[i].text : (src[i].textOut ? src[i].textOut : "");
            char* dup = (char*)malloc(strlen(t) + 1);
            if (!dup) {
                int k;
                for (k = 0; k < i; k++) free((void*)row[k].text);
                free(row);
                return GMB_DB_ERR;
            }
            memcpy(dup, t, strlen(t) + 1);
            row[i].text = dup;
        }
    }
    *outRow = row;
    return GMB_DB_OK;
}

static void copy_free_row(IsamFieldValue* row, const IsamSchema* s)
{
    int i;
    if (!row) return;
    for (i = 0; i < s->FieldCount; i++) {
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0)
            free((void*)row[i].text);
    }
    free(row);
}

static int copy_csv_escape(FILE* f, const char* s)
{
    int need = 0;
    const char* p;
    if (!s) s = "";
    for (p = s; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            need = 1;
            break;
        }
    }
    if (!need) {
        fputs(s, f);
        return GMB_DB_OK;
    }
    fputc('"', f);
    for (p = s; *p; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
    return GMB_DB_OK;
}

static int copy_split_csv(char* line, char** cols, int cap)
{
    int n = 0;
    char* p = line;
    while (*p && n < cap) {
        if (*p == '"') {
            char* dst;
            p++;
            cols[n++] = p;
            dst = p;
            while (*p) {
                if (*p == '"' && p[1] == '"') {
                    *dst++ = '"';
                    p += 2;
                    continue;
                }
                if (*p == '"') {
                    p++;
                    break;
                }
                *dst++ = *p++;
            }
            *dst = 0;
            if (*p == ',') p++;
        } else {
            cols[n++] = p;
            while (*p && *p != ',') p++;
            if (*p == ',') {
                *p = 0;
                p++;
            }
        }
    }
    return n;
}

int Engine_CopyTo(Engine* self, const char* destinationFilePath, const char* forCondition)
{
    const IsamSchema* schema;
    Engine* dest = NULL;
    IsamFieldValue* fields;
    int n, rc = GMB_DB_ERR;
    int64_t recno;
    if (!self || !destinationFilePath || !destinationFilePath[0]) return GMB_DB_ARG;
    schema = Engine_GetSchema(self);
    if (!schema) return GMB_DB_ERR;
    n = schema->FieldCount;
    fields = copy_alloc_fields(schema);
    if (!fields) return GMB_DB_ERR;
    recno = Engine_GetCurrentRecordNumber(self);
    unlink(destinationFilePath);
    if (IsamFileHandle_CreateNew(destinationFilePath, schema) != GMB_DB_OK) goto done;
    dest = Engine_Open(destinationFilePath, schema);
    if (!dest) {
        unlink(destinationFilePath);
        goto done;
    }
    Engine_BeginTransaction(dest);
    if (Engine_Go(self, CursorMoveOperation_Top) != GMB_DB_OK && !Engine_IsEof(self)) {
        Engine_Rollback(dest);
        copy_fail_dest(dest, destinationFilePath);
        dest = NULL;
        goto done;
    }
    while (!Engine_IsEof(self)) {
        if (Engine_GetCurrent(self, fields, n, NULL) != GMB_DB_OK) {
            Engine_Rollback(dest);
            copy_fail_dest(dest, destinationFilePath);
            dest = NULL;
            goto done;
        }
        copy_fields_text_from_out(fields, n);
        if (copy_row_matches(schema, fields, forCondition)) {
            if (Engine_Append(dest, fields, n) != GMB_DB_OK) {
                Engine_Rollback(dest);
                copy_fail_dest(dest, destinationFilePath);
                dest = NULL;
                goto done;
            }
        }
        if (Engine_Skip(self, 1) != GMB_DB_OK && !Engine_IsEof(self)) {
            Engine_Rollback(dest);
            copy_fail_dest(dest, destinationFilePath);
            dest = NULL;
            goto done;
        }
    }
    if (Engine_Commit(dest) != GMB_DB_OK) {
        copy_fail_dest(dest, destinationFilePath);
        dest = NULL;
        goto done;
    }
    Engine_Dispose(dest);
    dest = NULL;
    if (recno > 0) Engine_PositionAtRecord(self, recno);
    else Engine_Go(self, CursorMoveOperation_Top);
    rc = GMB_DB_OK;
done:
    copy_free_fields(fields, n);
    if (dest) Engine_Dispose(dest);
    return rc;
}

int Engine_SortTo(Engine* self, const char* destinationFilePath, const char* fieldName, int descending,
                  const char* forCondition)
{
    const IsamSchema* schema;
    Engine* dest = NULL;
    IsamFieldValue* fields;
    IsamFieldValue** rows = NULL;
    int n, i, count = 0, fi, rc = GMB_DB_ERR;
    int64_t recno;
    if (!self || !destinationFilePath || !destinationFilePath[0]) return GMB_DB_ARG;
    schema = Engine_GetSchema(self);
    if (!schema) return GMB_DB_ERR;
    fi = copy_field_index(schema, fieldName);
    if (fi < 0) return GMB_DB_ARG;
    n = schema->FieldCount;
    fields = copy_alloc_fields(schema);
    if (!fields) return GMB_DB_ERR;
    recno = Engine_GetCurrentRecordNumber(self);
    if (Engine_Go(self, CursorMoveOperation_Top) != GMB_DB_OK && !Engine_IsEof(self)) goto done;
    while (!Engine_IsEof(self) && count < COPY_MAX_ROWS) {
        IsamFieldValue* cloned = NULL;
        if (Engine_GetCurrent(self, fields, n, NULL) != GMB_DB_OK) goto done;
        copy_fields_text_from_out(fields, n);
        if (copy_row_matches(schema, fields, forCondition)) {
            if (copy_clone_row(schema, fields, &cloned) != GMB_DB_OK) goto done;
            rows = (IsamFieldValue**)realloc(rows, (size_t)(count + 1) * sizeof(*rows));
            if (!rows) {
                copy_free_row(cloned, schema);
                goto done;
            }
            rows[count++] = cloned;
        }
        if (Engine_Skip(self, 1) != GMB_DB_OK && !Engine_IsEof(self)) goto done;
    }
    g_sort_cmp.schema = schema;
    g_sort_cmp.fieldIndex = fi;
    g_sort_cmp.descending = descending ? 1 : 0;
    if (count > 1) qsort(rows, (size_t)count, sizeof(*rows), copy_sort_cmp);
    unlink(destinationFilePath);
    if (IsamFileHandle_CreateNew(destinationFilePath, schema) != GMB_DB_OK) goto done;
    dest = Engine_Open(destinationFilePath, schema);
    if (!dest) {
        unlink(destinationFilePath);
        goto done;
    }
    Engine_BeginTransaction(dest);
    for (i = 0; i < count; i++) {
        if (Engine_Append(dest, rows[i], n) != GMB_DB_OK) {
            Engine_Rollback(dest);
            copy_fail_dest(dest, destinationFilePath);
            dest = NULL;
            goto done;
        }
    }
    if (Engine_Commit(dest) != GMB_DB_OK) {
        copy_fail_dest(dest, destinationFilePath);
        dest = NULL;
        goto done;
    }
    Engine_Dispose(dest);
    dest = NULL;
    if (recno > 0) Engine_PositionAtRecord(self, recno);
    else Engine_Go(self, CursorMoveOperation_Top);
    rc = GMB_DB_OK;
done:
    for (i = 0; i < count; i++) copy_free_row(rows[i], schema);
    free(rows);
    copy_free_fields(fields, n);
    if (dest) Engine_Dispose(dest);
    return rc;
}

int Engine_ExportToCsv(Engine* self, const char* filePath)
{
    const IsamSchema* schema;
    FILE* f;
    IsamFieldValue* fields;
    int n, i;
    int64_t recno;
    if (!self || !filePath) return GMB_DB_ARG;
    schema = Engine_GetSchema(self);
    if (!schema) return GMB_DB_ERR;
    n = schema->FieldCount;
    fields = copy_alloc_fields(schema);
    if (!fields) return GMB_DB_ERR;
    f = fopen(filePath, "w");
    if (!f) {
        copy_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    recno = Engine_GetCurrentRecordNumber(self);
    for (i = 0; i < n; i++) {
        if (i) fputc(',', f);
        copy_csv_escape(f, schema->Fields[i].Name);
    }
    fputc('\n', f);
    if (Engine_Go(self, CursorMoveOperation_Top) != GMB_DB_OK && !Engine_IsEof(self)) {
        fclose(f);
        copy_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    while (!Engine_IsEof(self)) {
        if (Engine_GetCurrent(self, fields, n, NULL) != GMB_DB_OK) {
            fclose(f);
            copy_free_fields(fields, n);
            return GMB_DB_ERR;
        }
        copy_fields_text_from_out(fields, n);
        for (i = 0; i < n; i++) {
            char num[32];
            if (i) fputc(',', f);
            if (strcmp(schema->Fields[i].DataTypeCode, "StringType") == 0) {
                copy_csv_escape(f, fields[i].text ? fields[i].text : "");
            } else {
                if (IsamField_IsFloat(&schema->Fields[i]))
                    IsamField_FormatFloat(&schema->Fields[i], fields[i].i64, num, (int)sizeof(num));
                else
                    snprintf(num, sizeof(num), "%lld", (long long)fields[i].i64);
                fputs(num, f);
            }
        }
        fputc('\n', f);
        if (Engine_Skip(self, 1) != GMB_DB_OK && !Engine_IsEof(self)) {
            fclose(f);
            copy_free_fields(fields, n);
            return GMB_DB_ERR;
        }
    }
    fclose(f);
    if (recno > 0) Engine_PositionAtRecord(self, recno);
    copy_free_fields(fields, n);
    return GMB_DB_OK;
}

int Engine_ImportFromCsv(Engine* self, const char* filePath)
{
    const IsamSchema* schema;
    FILE* f;
    char line[4096];
    char* cols[64];
    int map[64];
    int n, i, headerN, first = 1;
    if (!self || !filePath) return GMB_DB_ARG;
    schema = Engine_GetSchema(self);
    if (!schema) return GMB_DB_ERR;
    n = schema->FieldCount;
    f = fopen(filePath, "r");
    if (!f) return GMB_DB_ERR;
    Engine_BeginTransaction(self);
    while (fgets(line, (int)sizeof(line), f)) {
        size_t len = strlen(line);
        IsamFieldValue vals[64];
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        if (!line[0]) continue;
        headerN = copy_split_csv(line, cols, 64);
        if (first) {
            first = 0;
            if (headerN < 1) {
                Engine_Rollback(self);
                fclose(f);
                return GMB_DB_ERR;
            }
            for (i = 0; i < n; i++) map[i] = -1;
            for (i = 0; i < headerN && i < 64; i++) {
                int fi = copy_field_index(schema, cols[i]);
                if (fi >= 0) map[fi] = i;
            }
            continue;
        }
        memset(vals, 0, sizeof(vals));
        for (i = 0; i < n; i++) {
            const char* cell = (map[i] >= 0 && map[i] < headerN) ? cols[map[i]] : "";
            if (strcmp(schema->Fields[i].DataTypeCode, "StringType") == 0) {
                vals[i].text = cell;
            } else {
                if (IsamField_IsFloat(&schema->Fields[i]))
                    vals[i].i64 = IsamField_DoubleToBits(&schema->Fields[i], cell && cell[0] ? atof(cell) : 0.0);
                else
                    vals[i].i64 = cell && cell[0] ? (int64_t)strtoll(cell, NULL, 10) : 0;
            }
        }
        if (Engine_Append(self, vals, n) != GMB_DB_OK) {
            Engine_Rollback(self);
            fclose(f);
            return GMB_DB_ERR;
        }
    }
    fclose(f);
    return Engine_Commit(self);
}

/* Engine_AppendMany e' definito in Engine.c (usa gli helper static: allocazione batch). */

static void copysort_person_schema(IsamFieldSchema fields[2], IsamSchema* schema)
{
    memset(schema, 0, sizeof(*schema));
    memset(fields, 0, sizeof(IsamFieldSchema) * 2);
    snprintf(schema->TypeName, sizeof(schema->TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema->Fields = fields;
    schema->FieldCount = 2;
    snprintf(schema->PrimaryKey.Name, sizeof(schema->PrimaryKey.Name), "%s", "Id");
    snprintf(schema->PrimaryKey.KeyExpression, sizeof(schema->PrimaryKey.KeyExpression), "%s", "Id");
    schema->PrimaryKey.KeyLength = 10;
    schema->PrimaryKey.IsUnique = 1;
}

int Engine_CopySort_SelfCheck(void)
{
    char src[] = "native/gmb_db_c/build/isam_copysort_src.isdb";
    char dst[] = "native/gmb_db_c/build/isam_copysort_dst.isdb";
    char sorted[] = "native/gmb_db_c/build/isam_copysort_ord.isdb";
    char csv[] = "native/gmb_db_c/build/isam_copysort.csv";
    char imp[] = "native/gmb_db_c/build/isam_copysort_imp.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine *e, *d, *opened;
    IsamFieldValue vals[4];
    int i;
    copysort_person_schema(fields, &schema);
    unlink(src);
    unlink(dst);
    unlink(sorted);
    unlink(csv);
    unlink(imp);
    if (IsamFileHandle_CreateNew(src, &schema) != GMB_DB_OK) return -430;
    e = Engine_Open(src, &schema);
    if (!e) return -431;
    for (i = 1; i <= 3; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", 4 - i);
        memset(vals, 0, sizeof(vals[0]) * 2);
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -432; }
    }
    Engine_BeginTransaction(e);
    memset(vals, 0, sizeof(vals[0]) * 2);
    vals[0].i64 = 4;
    vals[1].text = "N0";
    if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -433; }
    if (Engine_Commit(e) != GMB_DB_OK) { Engine_Dispose(e); return -434; }
    if (Engine_GetTotalRecordCount(e) != 4) { Engine_Dispose(e); return -435; }
    if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(e); return -436; }
    if (Engine_LockCurrentRecord(e) != GMB_DB_OK) { Engine_Dispose(e); return -437; }
    Engine_UnlockCurrentRecord(e);
    if (Engine_LockFile(e) != GMB_DB_OK) { Engine_Dispose(e); return -438; }
    Engine_UnlockFile(e);
    if (Engine_CopyTo(e, dst, "Id=2") != GMB_DB_OK) { Engine_Dispose(e); return -439; }
    d = Engine_Open(dst, NULL);
    if (!d) { Engine_Dispose(e); return -440; }
    if (Engine_GetTotalRecordCount(d) != 1) { Engine_Dispose(d); Engine_Dispose(e); return -441; }
    Engine_Dispose(d);
    if (Engine_SortTo(e, sorted, "Name", 0, NULL) != GMB_DB_OK) { Engine_Dispose(e); return -442; }
    d = Engine_Open(sorted, NULL);
    if (!d) { Engine_Dispose(e); return -443; }
    if (Engine_GetTotalRecordCount(d) != 4) { Engine_Dispose(d); Engine_Dispose(e); return -444; }
    Engine_Dispose(d);
    if (Engine_ExportToCsv(e, csv) != GMB_DB_OK) { Engine_Dispose(e); return -446; }
    if (IsamFileHandle_CreateNew(imp, &schema) != GMB_DB_OK) { Engine_Dispose(e); return -447; }
    d = Engine_Open(imp, &schema);
    if (!d) { Engine_Dispose(e); return -448; }
    if (Engine_ImportFromCsv(d, csv) != GMB_DB_OK) { Engine_Dispose(d); Engine_Dispose(e); return -449; }
    if (Engine_GetTotalRecordCount(d) != 4) { Engine_Dispose(d); Engine_Dispose(e); return -450; }
    Engine_Dispose(d);
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 10;
    vals[1].text = "A";
    vals[2].i64 = 11;
    vals[3].text = "B";
    if (Engine_AppendMany(e, vals, 2, 2) != GMB_DB_OK) { Engine_Dispose(e); return -451; }
    if (Engine_GetTotalRecordCount(e) != 6) { Engine_Dispose(e); return -452; }
    Engine_Dispose(e);
    opened = Engine_Open(src, NULL);
    if (!opened) return -453;
    if (Engine_GetTotalRecordCount(opened) != 6) { Engine_Dispose(opened); return -454; }
    Engine_Zap(opened);
    if (Engine_GetTotalRecordCount(opened) != 0) { Engine_Dispose(opened); return -455; }
    Engine_Dispose(opened);
    unlink(src);
    unlink(dst);
    unlink(sorted);
    unlink(csv);
    unlink(imp);
    return GMB_DB_OK;
}
