/* FILE: native/gmb_db_c/src/rdd/IsamDriver.c */
/* Port 1:1 di modules/Database/Rdd/IsamDriver.cs (navigazione + Open su Engine). */
#include "../../include/gmb_db_port.h"

#include <unistd.h>

void IsamDriver_SyncCursorState(IsamDriver* self)
{
    if (!self || !self->engine) return;
    self->eof = Engine_IsEof(self->engine);
    self->bof = Engine_IsBof(self->engine);
    self->found = Engine_WasFound(self->engine);
    self->recNo = Engine_GetCurrentRecordNumber(self->engine);
    self->recCount = Engine_GetTotalRecordCount(self->engine);
}

IsamDriver* IsamDriver_New(void)
{
    return (IsamDriver*)calloc(1, sizeof(IsamDriver));
}

IsamDriver* IsamDriver_FromEngine(Engine* engine, const char* alias, int ownsEngine)
{
    IsamDriver* self = IsamDriver_New();
    if (!self) return NULL;
    self->engine = engine;
    self->ownsEngine = ownsEngine ? 1 : 0;
    self->isOpen = engine != NULL;
    if (alias) snprintf(self->alias, sizeof(self->alias), "%s", alias);
    IsamDriver_SyncCursorState(self);
    return self;
}

void IsamDriver_Close(IsamDriver* self)
{
    if (!self) return;
    if (self->ownsEngine && self->engine)
        Engine_Dispose(self->engine);
    self->engine = NULL;
    self->ownsEngine = 0;
    self->isOpen = 0;
}

void IsamDriver_Dispose(IsamDriver* self)
{
    if (!self) return;
    IsamDriver_Close(self);
    free(self);
}

void IsamDriver_Open(IsamDriver* self, const char* filePath, const char* alias)
{
    if (!self || !filePath) return;
    IsamDriver_Close(self);
    self->engine = Engine_Open(filePath, NULL);
    self->ownsEngine = 1;
    self->isOpen = self->engine != NULL;
    snprintf(self->fileName, sizeof(self->fileName), "%s", filePath);
    if (alias) snprintf(self->alias, sizeof(self->alias), "%s", alias);
    IsamDriver_SyncCursorState(self);
}

void IsamDriver_GoTop(IsamDriver* self)
{
    if (!self || !self->engine) return;
    Engine_Go(self->engine, CursorMoveOperation_Top);
    IsamDriver_SyncCursorState(self);
}

void IsamDriver_GoBottom(IsamDriver* self)
{
    if (!self || !self->engine) return;
    Engine_Go(self->engine, CursorMoveOperation_Bottom);
    IsamDriver_SyncCursorState(self);
}

void IsamDriver_Skip(IsamDriver* self, int n)
{
    if (!self || !self->engine) return;
    Engine_Skip(self->engine, n);
    IsamDriver_SyncCursorState(self);
}

int IsamDriver_SeekText(IsamDriver* self, const char* keyText)
{
    int found;
    if (!self || !self->engine || !keyText) return 0;
    found = Engine_SeekText(self->engine, keyText);
    IsamDriver_SyncCursorState(self);
    self->found = found;
    return found;
}

int IsamDriver_get_Eof(const IsamDriver* self)
{
    return self && self->eof;
}

int IsamDriver_get_Found(const IsamDriver* self)
{
    return self && self->found;
}

int IsamDriver_Append(IsamDriver* self, const IsamFieldValue* fields, int fieldCount)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = Engine_Append(self->engine, fields, fieldCount);
    IsamDriver_SyncCursorState(self);
    return rc;
}

int IsamDriver_GetCurrent(IsamDriver* self, IsamFieldValue* fields, int fieldCount, RecordMetadata* meta)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_GetCurrent(self->engine, fields, fieldCount, meta);
}

int IsamDriver_PutCurrent(IsamDriver* self, const IsamFieldValue* fields, int fieldCount)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = Engine_Put(self->engine, fields, fieldCount);
    IsamDriver_SyncCursorState(self);
    return rc;
}

void IsamDriver_Zap(IsamDriver* self)
{
    if (!self || !self->engine) return;
    Engine_Zap(self->engine);
    IsamDriver_SyncCursorState(self);
}

int IsamDriver_Pack(IsamDriver* self)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = Engine_Pack(self->engine);
    IsamDriver_SyncCursorState(self);
    return rc;
}

int IsamDriver_CreateIndex(IsamDriver* self, const char* indexName, const char* keyExpression, int keyLength)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_CreateIndex(self->engine, indexName, keyExpression, keyLength);
}

int IsamDriver_SetFilter(IsamDriver* self, const char* expression)
{
    int rc;
    if (!self || !self->engine) return GMB_DB_ARG;
    rc = Engine_SetFilter(self->engine, expression);
    IsamDriver_SyncCursorState(self);
    return rc;
}

int IsamDriver_CopyTo(IsamDriver* self, const char* destinationFilePath, const char* forCondition)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_CopyTo(self->engine, destinationFilePath, forCondition);
}

int IsamDriver_SortTo(IsamDriver* self, const char* destinationFilePath, const char* fieldName, int descending,
                      const char* forCondition)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_SortTo(self->engine, destinationFilePath, fieldName, descending, forCondition);
}

int IsamDriver_LockCurrentRecord(IsamDriver* self)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_LockCurrentRecord(self->engine);
}

void IsamDriver_UnlockCurrentRecord(IsamDriver* self)
{
    if (!self || !self->engine) return;
    Engine_UnlockCurrentRecord(self->engine);
}

int IsamDriver_LockFile(IsamDriver* self)
{
    if (!self || !self->engine) return GMB_DB_ARG;
    return Engine_LockFile(self->engine);
}

void IsamDriver_UnlockFile(IsamDriver* self)
{
    if (!self || !self->engine) return;
    Engine_UnlockFile(self->engine);
}

int IsamDriver_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isamdrv_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine* e;
    IsamDriver* drv;
    IsamFieldValue vals[2];
    int i;

    unlink(path);
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -400;
    e = Engine_Open(path, &schema);
    if (!e) return -401;
    for (i = 1; i <= 2; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -402; }
    }
    drv = IsamDriver_FromEngine(e, "P", 0);
    if (!drv) { Engine_Dispose(e); return -403; }
    IsamDriver_GoTop(drv);
    if (IsamDriver_get_Eof(drv)) { IsamDriver_Dispose(drv); Engine_Dispose(e); return -404; }
    IsamDriver_Skip(drv, 1);
    if (IsamDriver_get_Eof(drv)) { IsamDriver_Dispose(drv); Engine_Dispose(e); return -405; }
    IsamDriver_Skip(drv, 1);
    if (!IsamDriver_get_Eof(drv)) { IsamDriver_Dispose(drv); Engine_Dispose(e); return -406; }
    IsamDriver_GoTop(drv);
    if (IsamDriver_LockCurrentRecord(drv) != GMB_DB_OK) {
        IsamDriver_Dispose(drv); Engine_Dispose(e); return -407;
    }
    IsamDriver_UnlockCurrentRecord(drv);
    {
        char dst[] = "native/gmb_db_c/build/isamdrv_copy.isdb";
        unlink(dst);
        if (IsamDriver_CopyTo(drv, dst, NULL) != GMB_DB_OK) {
            IsamDriver_Dispose(drv); Engine_Dispose(e); return -408;
        }
        unlink(dst);
    }
    IsamDriver_Zap(drv);
    if (drv->recCount != 0) { IsamDriver_Dispose(drv); Engine_Dispose(e); return -409; }
    {
        IsamDriver* opened = IsamDriver_New();
        if (!opened) { IsamDriver_Dispose(drv); Engine_Dispose(e); return -410; }
        IsamDriver_Dispose(drv);
        Engine_Dispose(e);
        IsamDriver_Open(opened, path, "P");
        if (!opened->isOpen) { IsamDriver_Dispose(opened); return -411; }
        IsamDriver_Dispose(opened);
        unlink(path);
        return GMB_DB_OK;
    }
}
