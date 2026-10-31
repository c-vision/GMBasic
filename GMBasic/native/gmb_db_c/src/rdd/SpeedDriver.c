/* FILE: native/gmb_db_c/src/rdd/SpeedDriver.c */
/* Port 1:1 di modules/Database/Rdd/SpeedDriver.cs */
#include "../../include/gmb_db_port.h"

#include <string.h>
#include <unistd.h>

/* --- C# originale: SpeedDriver avvolge Database. Navigazione = cursore SPEED.
 * Open senza USE TABLE: prima tabella del manifest, altrimenti CreateTable("KV"). --- */

static void spd_free_current(SpeedDriver* self)
{
    if (!self) return;
    free(self->currentKey);
    free(self->currentValue);
    self->currentKey = NULL;
    self->currentValue = NULL;
    self->currentKeyLen = 0;
    self->currentValueLen = 0;
}

static void spd_fetch_current(SpeedDriver* self)
{
    uint8_t* v = NULL;
    int vl = 0;
    if (!self || !self->db || self->tableId <= 0) return;
    spd_free_current(self);
    if (Database_GetCurrent(self->db, self->tableId, &v, &vl)) {
        self->currentValue = v;
        self->currentValueLen = vl;
    } else {
        free(v);
    }
}

static void spd_sync(SpeedDriver* self)
{
    if (!self || !self->db || self->tableId <= 0) return;
    self->eof = Database_Eof(self->db, self->tableId);
    self->bof = Database_Bof(self->db, self->tableId);
    self->recNo = self->eof ? 0 : 1;
    self->recCount = Manifest_GetRowCount(Database_GetManifest(self->db), self->tableId);
}

SpeedDriver* SpeedDriver_New(void)
{
    return (SpeedDriver*)calloc(1, sizeof(SpeedDriver));
}

SpeedDriver* SpeedDriver_FromDatabase(Database* db, const char* alias, int ownsDb)
{
    SpeedDriver* self = SpeedDriver_New();
    if (!self) return NULL;
    self->db = db;
    self->ownsDb = ownsDb;
    self->isOpen = db ? 1 : 0;
    if (alias) snprintf(self->alias, sizeof(self->alias), "%s", alias);
    spd_sync(self);
    return self;
}

void SpeedDriver_Dispose(SpeedDriver* self)
{
    if (!self) return;
    SpeedDriver_Close(self);
    free(self);
}

void SpeedDriver_Open(SpeedDriver* self, const char* dir, const char* alias)
{
    int firstId = 0;
    Manifest* man;
    if (!self || !dir) return;
    SpeedDriver_Close(self);
    self->db = Database_Open(dir);
    self->ownsDb = 1;
    self->isOpen = self->db != NULL;
    snprintf(self->fileName, sizeof(self->fileName), "%s", dir);
    if (alias) snprintf(self->alias, sizeof(self->alias), "%s", alias);
    if (!self->db) return;
    man = Database_GetManifest(self->db);
    if (self->tableId <= 0) {
        if (Manifest_TableCount(man) > 0 && Manifest_GetTableIdAt(man, 0, &firstId) && firstId > 0)
            self->tableId = firstId;
        else
            self->tableId = Database_CreateTable(self->db, "KV");
    }
    spd_sync(self);
}

void SpeedDriver_Close(SpeedDriver* self)
{
    if (!self) return;
    spd_free_current(self);
    if (self->ownsDb && self->db)
        Database_Dispose(self->db);
    self->db = NULL;
    self->ownsDb = 0;
    self->isOpen = 0;
}

void SpeedDriver_UseTable(SpeedDriver* self, int tableId)
{
    if (!self) return;
    self->tableId = tableId;
    spd_free_current(self);
    spd_sync(self);
}

void SpeedDriver_GoTop(SpeedDriver* self)
{
    if (!self || !self->db) return;
    Database_GoTop(self->db, self->tableId);
    spd_fetch_current(self);
    spd_sync(self);
}

void SpeedDriver_GoBottom(SpeedDriver* self)
{
    if (!self || !self->db) return;
    Database_GoBottom(self->db, self->tableId);
    spd_fetch_current(self);
    spd_sync(self);
}

void SpeedDriver_Skip(SpeedDriver* self, int n)
{
    if (!self || !self->db) return;
    Database_Skip(self->db, self->tableId, n);
    spd_fetch_current(self);
    spd_sync(self);
}

int SpeedDriver_Seek(SpeedDriver* self, const uint8_t* key, int keyLen)
{
    uint8_t* copy = NULL;
    if (!self || !self->db || !key) return 0;
    spd_free_current(self);
    self->found = Database_Seek(self->db, self->tableId, key, keyLen, NULL);
    if (keyLen > 0) {
        copy = (uint8_t*)malloc((size_t)keyLen);
        if (copy) memcpy(copy, key, (size_t)keyLen);
    }
    self->currentKey = copy;
    self->currentKeyLen = copy ? keyLen : 0;
    if (self->found)
        Database_Get(self->db, self->tableId, key, keyLen, &self->currentValue, &self->currentValueLen);
    self->recNo = 1;
    spd_sync(self);
    return self->found;
}

int SpeedDriver_GetCurrent(SpeedDriver* self, uint8_t** value, int* valueLen)
{
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !self->currentValue || self->currentValueLen <= 0) return 0;
    if (value) {
        *value = (uint8_t*)malloc((size_t)self->currentValueLen);
        if (*value) memcpy(*value, self->currentValue, (size_t)self->currentValueLen);
    }
    if (valueLen) *valueLen = self->currentValueLen;
    return 1;
}

int SpeedDriver_get_Eof(const SpeedDriver* self)
{
    return self && self->eof;
}

int SpeedDriver_get_Found(const SpeedDriver* self)
{
    return self && self->found;
}

int SpeedDriver_CreateIndex(SpeedDriver* self, const char* name, const char* keyExpression, int keyLength)
{
    if (!self || !self->db || !name || !name[0]) return GMB_DB_ARG;
    if (Database_CreateSecondaryIndex(self->db, self->tableId, name, keyExpression, keyLength) <= 0)
        return GMB_DB_ERR;
    return GMB_DB_OK;
}

int SpeedDriver_DropIndex(SpeedDriver* self, const char* indexName)
{
    if (!self || !self->db || !indexName) return GMB_DB_ARG;
    return Database_DropIndex(self->db, self->tableId, indexName);
}

int SpeedDriver_SetOrder(SpeedDriver* self, const char* indexName)
{
    if (!self || !self->db) return 0;
    return Database_SetOrder(self->db, self->tableId, indexName ? indexName : "");
}

void SpeedDriver_Zap(SpeedDriver* self)
{
    DatabaseScan* scan;
    uint8_t *k = NULL, *v = NULL;
    int kl = 0, vl = 0;
    uint8_t** keys = NULL;
    int* lens = NULL;
    int n = 0, i;
    if (!self || !self->db) return;
    scan = Database_Scan(self->db, self->tableId);
    if (scan) {
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            keys = (uint8_t**)realloc(keys, (size_t)(n + 1) * sizeof(uint8_t*));
            lens = (int*)realloc(lens, (size_t)(n + 1) * sizeof(int));
            keys[n] = k;
            lens[n] = kl;
            n++;
            free(v);
        }
        Database_ScanDispose(scan);
    }
    for (i = 0; i < n; i++) {
        Database_Delete(self->db, self->tableId, keys[i], lens[i]);
        free(keys[i]);
    }
    free(keys);
    free(lens);
    spd_free_current(self);
    spd_sync(self);
}

int SpeedDriver_Pack(SpeedDriver* self)
{
    char name[128];
    if (!self || !self->db) return GMB_DB_ARG;
    if (!Manifest_TryGetTable(Database_GetManifest(self->db), self->tableId, name, (int)sizeof(name)))
        return GMB_DB_ERR;
    return Database_ForceCompactTable(self->db, name) >= 0 ? GMB_DB_OK : GMB_DB_ERR;
}

int SpeedDriver_Append(SpeedDriver* self, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen)
{
    int rc;
    if (!self || !self->db || !key) return GMB_DB_ARG;
    rc = Database_Set(self->db, self->tableId, key, keyLen, value, valueLen);
    spd_sync(self);
    return rc;
}

int SpeedDriver_PutCurrent(SpeedDriver* self, const uint8_t* value, int valueLen)
{
    int rc;
    if (!self || !self->db || !self->currentKey || self->currentKeyLen <= 0) return GMB_DB_ERR;
    rc = Database_Set(self->db, self->tableId, self->currentKey, self->currentKeyLen, value, valueLen);
    spd_fetch_current(self);
    spd_sync(self);
    return rc;
}

int SpeedDriver_CreateTableWithFields(SpeedDriver* self, const char* typeName,
                                      const char* const* names, const char* const* types,
                                      const int* lengths, int fieldCount)
{
    int id;
    if (!self || !self->db) return -1;
    id = Database_CreateTableWithFields(self->db, typeName, names, types, lengths, fieldCount);
    if (id > 0) {
        self->tableId = id;
        spd_sync(self);
    }
    return id;
}

int SpeedDriver_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/speeddrv_XXXXXX";
    Database* db;
    SpeedDriver* drv;
    int tableId, vlen;
    uint8_t* val = NULL;
    if (!mkdtemp(dir)) return -90;
    db = Database_Open(dir);
    if (!db) return -91;
    tableId = Database_CreateTable(db, "KV");
    if (tableId <= 0) { Database_Dispose(db); return -92; }
    if (Database_Set(db, tableId, (const uint8_t*)"a001", 4, (const uint8_t*)"va01", 4) != GMB_DB_OK) {
        Database_Dispose(db); return -93;
    }
    if (Database_Set(db, tableId, (const uint8_t*)"b002", 4, (const uint8_t*)"vb02", 4) != GMB_DB_OK) {
        Database_Dispose(db); return -93;
    }
    drv = SpeedDriver_FromDatabase(db, "KV", 0);
    if (!drv) { Database_Dispose(db); return -94; }
    SpeedDriver_UseTable(drv, tableId);
    SpeedDriver_GoTop(drv);
    if (SpeedDriver_get_Eof(drv)) { SpeedDriver_Dispose(drv); Database_Dispose(db); return -95; }
    if (!SpeedDriver_GetCurrent(drv, &val, &vlen) || vlen != 4 || memcmp(val, "va01", 4) != 0) {
        free(val); SpeedDriver_Dispose(drv); Database_Dispose(db); return -96;
    }
    free(val);
    SpeedDriver_Skip(drv, 1);
    if (!SpeedDriver_GetCurrent(drv, &val, &vlen) || memcmp(val, "vb02", 4) != 0) {
        free(val); SpeedDriver_Dispose(drv); Database_Dispose(db); return -97;
    }
    free(val);
    if (SpeedDriver_CreateIndex(drv, "ByVal", "VALUE", 0) != GMB_DB_OK) {
        SpeedDriver_Dispose(drv); Database_Dispose(db); return -98;
    }
    if (SpeedDriver_DropIndex(drv, "ByVal") != GMB_DB_OK) {
        SpeedDriver_Dispose(drv); Database_Dispose(db); return -99;
    }
    SpeedDriver_Zap(drv);
    SpeedDriver_GoTop(drv);
    if (!SpeedDriver_get_Eof(drv)) {
        SpeedDriver_Dispose(drv); Database_Dispose(db); return -100;
    }
    if (SpeedDriver_Append(drv, (const uint8_t*)"z9", 2, (const uint8_t*)"vz", 2) != GMB_DB_OK) {
        SpeedDriver_Dispose(drv); Database_Dispose(db); return -101;
    }
    SpeedDriver_Dispose(drv);
    Database_Dispose(db);
    return GMB_DB_OK;
}
