/* FILE: native/gmb_db_c/src/rdd/DbfDriver.c */
/* Port 1:1 di modules/Database/Rdd/DbfDriver.cs (+ BaseDriver SetRelation/GetFieldIndex) */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
// DbfDriver avvolge DbfEngine. SetRelation salva parent+key. Seek → DbfEngine.Seek.
// GetFieldIndex su schema engine (C# lascia _fields vuoto — qui popolato così RELATION funziona).

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <strings.h>
#include <ctype.h>

/* C#: modules/Database/Rdd/DbfDriver.cs + BaseDriver SetRelation/GetFieldIndex. */

static void drv_load_fields(DbfDriver* self)
{
    int n = 0, i;
    const DbfField* f;
    self->fieldCount = 0;
    if (!self->engine) return;
    f = DbfEngine_GetFields(self->engine, &n);
    if (!f || n <= 0) return;
    if (n > GMB_RDD_MAX_FIELDS) n = GMB_RDD_MAX_FIELDS;
    for (i = 0; i < n; i++) self->fields[i] = f[i];
    self->fieldCount = n;
}

void DbfDriver_SyncCursorState(DbfDriver* self)
{
    if (!self || !self->engine) return;
    self->eof = DbfEngine_get_Eof(self->engine);
    self->bof = 0;
    self->found = DbfEngine_get_Found(self->engine);
    self->recNo = DbfEngine_GetCurrentRecordNumber(self->engine);
    self->recCount = DbfEngine_GetTotalRecordCount(self->engine);
    drv_load_fields(self);
}

DbfDriver* DbfDriver_New(void)
{
    DbfDriver* self = (DbfDriver*)calloc(1, sizeof(DbfDriver));
    return self;
}

DbfDriver* DbfDriver_FromEngine(DbfEngine* engine, const char* alias, int ownsEngine)
{
    DbfDriver* self = DbfDriver_New();
    if (!self) return NULL;
    self->engine = engine;
    self->ownsEngine = ownsEngine ? 1 : 0;
    self->isOpen = engine != NULL;
    snprintf(self->alias, sizeof(self->alias), "%s", alias ? alias : "");
    if (engine && DbfFile_get_FilePath(DbfEngine_get_DbfFile(engine)))
        snprintf(self->fileName, sizeof(self->fileName), "%s", DbfFile_get_FilePath(DbfEngine_get_DbfFile(engine)));
    DbfDriver_SyncCursorState(self);
    return self;
}

void DbfDriver_Open(DbfDriver* self, const char* filePath, const char* alias)
{
    if (!self || !filePath) return;
    DbfDriver_Close(self);
    self->engine = DbfEngine_New(filePath, NULL, 0, 1, LockingStrategy_IntraProcess);
    self->ownsEngine = 1;
    self->isOpen = self->engine != NULL;
    snprintf(self->alias, sizeof(self->alias), "%s", alias ? alias : "");
    snprintf(self->fileName, sizeof(self->fileName), "%s", filePath);
    DbfDriver_SyncCursorState(self);
}

void DbfDriver_Close(DbfDriver* self)
{
    if (!self) return;
    if (self->ownsEngine && self->engine) DbfEngine_Dispose(self->engine);
    self->engine = NULL;
    self->ownsEngine = 0;
    self->isOpen = 0;
    self->parentRelation = NULL;
    self->relationKey[0] = 0;
    self->fieldCount = 0;
}

void DbfDriver_Dispose(DbfDriver* self)
{
    if (!self) return;
    DbfDriver_Close(self);
    free(self);
}

void DbfDriver_GoTop(DbfDriver* self)
{
    if (!self || !self->engine) return;
    DbfEngine_GoTop(self->engine);
    DbfDriver_SyncCursorState(self);
}

void DbfDriver_GoBottom(DbfDriver* self)
{
    if (!self || !self->engine) return;
    DbfEngine_GoBottom(self->engine);
    DbfDriver_SyncCursorState(self);
}

void DbfDriver_Skip(DbfDriver* self, int n)
{
    if (!self || !self->engine) return;
    DbfEngine_Skip(self->engine, n);
    DbfDriver_SyncCursorState(self);
}

int DbfDriver_Seek(DbfDriver* self, const char* keyText)
{
    uint8_t key[256];
    int cap, n;
    if (!self || !self->engine || !keyText) return 0;
    cap = DbfEngine_GetActiveKeyLength(self->engine);
    if (cap <= 0 || cap > (int)sizeof(key)) {
        DbfEngine_GoBottom(self->engine);
        DbfEngine_Skip(self->engine, 1);
        DbfDriver_SyncCursorState(self);
        return 0;
    }
    memset(key, ' ', (size_t)cap);
    n = (int)strlen(keyText);
    if (n > cap) n = cap;
    memcpy(key, keyText, (size_t)n);
    if (DbfEngine_Seek(self->engine, key, cap) != GMB_DB_OK) {
        DbfDriver_SyncCursorState(self);
        return 0;
    }
    DbfDriver_SyncCursorState(self);
    return self->found;
}

int DbfDriver_GetFieldIndex(const DbfDriver* self, const char* name)
{
    int i;
    if (!self || !name) return -1;
    for (i = 0; i < self->fieldCount; i++) {
        if (gmb_strcasecmp(self->fields[i].Name, name) == 0) return i;
    }
    return -1;
}

int DbfDriver_GetFieldText(DbfDriver* self, int index, char* out, int cap)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return DbfEngine_GetFieldText(self->engine, index, out, cap);
}

int DbfDriver_get_Eof(const DbfDriver* self)
{
    return self ? self->eof : 1;
}

int DbfDriver_get_Found(const DbfDriver* self)
{
    return self ? self->found : 0;
}

void DbfDriver_SetRelation(DbfDriver* self, DbfDriver* parent, const char* keyExpression)
{
    if (!self) return;
    self->parentRelation = parent;
    snprintf(self->relationKey, sizeof(self->relationKey), "%s", keyExpression ? keyExpression : "");
}

int DbfDriver_SyncRelation(DbfDriver* self)
{
    int idx;
    char key[256];
    int n;
    if (!self || !self->parentRelation || !self->relationKey[0] || !self->engine)
        return self ? self->found : 0;
    idx = DbfDriver_GetFieldIndex(self->parentRelation, self->relationKey);
    if (idx < 0) {
        DbfDriver_SyncCursorState(self);
        return 0;
    }
    if (DbfDriver_GetFieldText(self->parentRelation, idx, key, (int)sizeof(key)) != GMB_DB_OK) {
        DbfDriver_SyncCursorState(self);
        return 0;
    }
    n = (int)strlen(key);
    while (n > 0 && isspace((unsigned char)key[n - 1])) key[--n] = 0;
    if (!key[0]) {
        DbfDriver_SyncCursorState(self);
        return 0;
    }
    return DbfDriver_Seek(self, key);
}

int DbfDriver_CopyTo(DbfDriver* self, const char* destinationFilePath)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return DbfEngine_CopyTo(self->engine, destinationFilePath);
}

int DbfDriver_SortTo(DbfDriver* self, const char* destinationFilePath, const char* fieldName, int descending)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return DbfEngine_SortTo(self->engine, destinationFilePath, fieldName, descending);
}

int DbfDriver_Zap(DbfDriver* self)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = DbfEngine_Zap(self->engine);
    DbfDriver_SyncCursorState(self);
    return rc;
}

int DbfDriver_Pack(DbfDriver* self)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = DbfEngine_Pack(self->engine);
    DbfDriver_SyncCursorState(self);
    return rc;
}
