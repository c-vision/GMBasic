#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

/* .NET BinaryWriter (UTF-8): 7-bit length + bytes; ints little-endian. */
typedef struct {
    uint8_t* p;
    int n, cap;
} Bw;

static void bw_grow(Bw* b, int add)
{
    if (b->n + add <= b->cap) return;
    b->cap = b->cap ? b->cap * 2 : 256;
    while (b->n + add > b->cap) b->cap *= 2;
    b->p = (uint8_t*)realloc(b->p, (size_t)b->cap);
}

static void bw_u8(Bw* b, uint8_t v) { bw_grow(b, 1); b->p[b->n++] = v; }
static void bw_i32(Bw* b, int32_t v)
{
    bw_grow(b, 4);
    b->p[b->n++] = (uint8_t)v; b->p[b->n++] = (uint8_t)(v >> 8);
    b->p[b->n++] = (uint8_t)(v >> 16); b->p[b->n++] = (uint8_t)(v >> 24);
}
static void bw_i64(Bw* b, int64_t v)
{
    int i;
    bw_grow(b, 8);
    for (i = 0; i < 8; i++) b->p[b->n++] = (uint8_t)(v >> (8 * i));
}
static void bw_7bit(Bw* b, unsigned v)
{
    while (v >= 0x80) { bw_u8(b, (uint8_t)(v | 0x80)); v >>= 7; }
    bw_u8(b, (uint8_t)v);
}
static void bw_str(Bw* b, const char* s)
{
    size_t n = s ? strlen(s) : 0;
    bw_7bit(b, (unsigned)n);
    if (n) { bw_grow(b, (int)n); memcpy(b->p + b->n, s, n); b->n += (int)n; }
}
static void bw_bool(Bw* b, int v) { bw_u8(b, v ? 1 : 0); }

int SchemaManager_GetFieldSize(const char* typeCode, int length)
{
    if (!typeCode) return -1;
    if (strcmp(typeCode, "StringType") == 0) return length;
    if (strcmp(typeCode, "JsonType") == 0) return 8;
    if (strcmp(typeCode, "BitType") == 0) return 1;
    if (strcmp(typeCode, "ByteType") == 0) return 1;
    if (strcmp(typeCode, "SByteType") == 0) return 1;
    if (strcmp(typeCode, "IntegerType") == 0) return 2;
    if (strcmp(typeCode, "UIntegerType") == 0) return 2;
    if (strcmp(typeCode, "LongType") == 0) return 4;
    if (strcmp(typeCode, "ULongType") == 0) return 4;
    if (strcmp(typeCode, "QuadType") == 0) return 8;
    if (strcmp(typeCode, "UQuadType") == 0) return 8;
    if (strcmp(typeCode, "SingleType") == 0) return 4;
    if (strcmp(typeCode, "DoubleType") == 0) return 8;
    if (strcmp(typeCode, "CurrencyType") == 0) return 16;
    if (strcmp(typeCode, "DateType") == 0) return 8;
    if (strcmp(typeCode, "TimeType") == 0) return 8;
    if (strcmp(typeCode, "TimeSpanType") == 0) return 8;
    if (strcmp(typeCode, "DateTimeType") == 0) return 8;
    return -1;
}

int SchemaManager_CalculateRecordSize(const IsamSchema* schema)
{
    int i, n = RecordMetadata_Size;
    if (!schema) return -1;
    for (i = 0; i < schema->FieldCount; i++) {
        int sz = SchemaManager_GetFieldSize(schema->Fields[i].DataTypeCode, schema->Fields[i].Length);
        if (sz < 0) return -1;
        n += sz;
    }
    return n;
}

static void schema_write_index(Bw* b, const IsamIndexDefinition* d)
{
    bw_str(b, d->Name);
    bw_str(b, d->KeyExpression);
    bw_i32(b, d->KeyLength);
    bw_bool(b, d->IsUnique);
    bw_bool(b, d->IsUnsafe);
    bw_str(b, d->PartialPredicate);
}

int SchemaManager_SerializeIndexMap(const char** names, const int64_t* offsets, int count,
                                    uint8_t** out, int* outLen)
{
    Bw b = {0};
    int i;
    bw_i32(&b, count);
    for (i = 0; i < count; i++) {
        bw_str(&b, names[i]);
        bw_i64(&b, offsets[i]);
    }
    *out = b.p;
    *outLen = b.n;
    return GMB_DB_OK;
}

int SchemaManager_SerializeSchema(const IsamSchema* schema, uint8_t** out, int* outLen)
{
    Bw b = {0};
    int i;
    if (!schema) return GMB_DB_ERR;
    bw_grow(&b, 4); memcpy(b.p, SchemaManager_SchemaMagic, 4); b.n = 4;
    bw_i32(&b, SchemaManager_SchemaFormatVersion);
    bw_str(&b, schema->TypeName);
    schema_write_index(&b, &schema->PrimaryKey);
    bw_i32(&b, schema->SecondaryKeyCount);
    for (i = 0; i < schema->SecondaryKeyCount; i++)
        schema_write_index(&b, &schema->SecondaryKeys[i]);
    bw_i32(&b, schema->FieldCount);
    for (i = 0; i < schema->FieldCount; i++) {
        bw_str(&b, schema->Fields[i].Name);
        bw_str(&b, schema->Fields[i].DataTypeCode);
        bw_i32(&b, schema->Fields[i].Length);
    }
    *out = b.p;
    *outLen = b.n;
    return GMB_DB_OK;
}

/* C#: method SchemaManager.DeserializeIndexMap */
int SchemaManager_SerializeKeyValueI64(int64_t value, int keyLength, uint8_t* out)
{
    char num[32];
    int n, copy;
    if (!out || keyLength <= 0) return GMB_DB_ERR;
    snprintf(num, sizeof(num), "%lld", (long long)value);
    n = (int)strlen(num);
    memset(out, '0', (size_t)keyLength);
    if (n >= keyLength) {
        memcpy(out, num, (size_t)keyLength);
        return GMB_DB_OK;
    }
    copy = n;
    memcpy(out + (keyLength - copy), num, (size_t)copy);
    return GMB_DB_OK;
}

int SchemaManager_SerializeKeyValueText(const char* text, int keyLength, uint8_t* out)
{
    int i, n;
    if (!out || keyLength <= 0) return GMB_DB_ERR;
    memset(out, ' ', (size_t)keyLength);
    if (!text) return GMB_DB_OK;
    n = (int)strlen(text);
    if (n > keyLength) n = keyLength;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        out[i] = (uint8_t)toupper(c);
    }
    return GMB_DB_OK;
}

int SchemaManager_DeserializeIndexMap(void) { return GMB_DB_ERR; }
int SchemaManager_DeserializeSchema(void) { return GMB_DB_ERR; }
int SchemaManager_WriteIndexDefinition(void) { return GMB_DB_ERR; }
int SchemaManager_ReadIndexDefinition(void) { return GMB_DB_ERR; }
int SchemaManager_WriteFieldSchema(void) { return GMB_DB_ERR; }
int SchemaManager_ReadFieldSchema(void) { return GMB_DB_ERR; }
int SchemaManager_GetDataTypeCode(void) { return GMB_DB_ERR; }
int SchemaManager_CreateDataType(void) { return GMB_DB_ERR; }
int SchemaManager_AreSchemasCompatible(void) { return GMB_DB_ERR; }
int SchemaManager_DeserializeIndexMapFromStream(void) { return GMB_DB_ERR; }
int SchemaManager_GetFieldStorageSize(void) { return GMB_DB_ERR; }

int IsamFileHandle_BuildNewFileBytes(const IsamSchema* schema, uint8_t** out, int* outLen)
{
    uint8_t *schemaBuf = NULL, *mapBuf = NULL, *file;
    int schemaLen = 0, mapLen = 0, recordSize, fileLen, i;
    IsamFileHeader h;
    int64_t heapStart;
    if (!schema) return GMB_DB_ERR;
    recordSize = SchemaManager_CalculateRecordSize(schema);
    if (recordSize < 0) return GMB_DB_ERR;
    if (SchemaManager_SerializeSchema(schema, &schemaBuf, &schemaLen) != GMB_DB_OK) return GMB_DB_ERR;
    if (SchemaManager_SerializeIndexMap(NULL, NULL, 0, &mapBuf, &mapLen) != GMB_DB_OK) {
        free(schemaBuf); return GMB_DB_ERR;
    }
    /* secondary names → map: C# writes each SecondaryKeys name with offset -1 */
    if (schema->SecondaryKeyCount > 0) {
        const char** names = (const char**)malloc((size_t)schema->SecondaryKeyCount * sizeof(char*));
        int64_t* offs = (int64_t*)malloc((size_t)schema->SecondaryKeyCount * sizeof(int64_t));
        free(mapBuf);
        for (i = 0; i < schema->SecondaryKeyCount; i++) {
            names[i] = schema->SecondaryKeys[i].Name;
            offs[i] = -1;
        }
        if (SchemaManager_SerializeIndexMap(names, offs, schema->SecondaryKeyCount, &mapBuf, &mapLen) != GMB_DB_OK) {
            free(names); free(offs); free(schemaBuf); return GMB_DB_ERR;
        }
        free(names); free(offs);
    }
    heapStart = (int64_t)Config_HeaderSize + schemaLen + mapLen;
    memset(&h, 0, sizeof(h));
    memcpy(h.Magic, Config_MagicBytes, 8);
    h.Version = 1;
    h.RecordSize = recordSize;
    h.SchemaOffset = Config_HeaderSize;
    h.SchemaLength = schemaLen;
    h.SecondaryIndexMapOffset = (int64_t)Config_HeaderSize + schemaLen;
    h.PrimaryKeyRootOffset = -1;
    h.FreeListHeadOffset = -1;
    h.NextAvailablePageOffset = heapStart;
    h.LobHeapStartOffset = heapStart;
    h.PageFreeListHeadOffset = -1;
    for (i = 0; i < 15; i++) h.SecondaryIndexRootOffsets[i] = -1;
    fileLen = (int)heapStart;
    if (fileLen < Config_HeaderSize) fileLen = Config_HeaderSize;
    file = (uint8_t*)calloc(1, (size_t)fileLen);
    memcpy(file, &h, sizeof(h));
    memcpy(file + Config_HeaderSize, schemaBuf, (size_t)schemaLen);
    memcpy(file + Config_HeaderSize + schemaLen, mapBuf, (size_t)mapLen);
    free(schemaBuf); free(mapBuf);
    *out = file;
    *outLen = fileLen;
    return GMB_DB_OK;
}

static int isam_mkdir_parent(const char* filePath)
{
    char buf[PATH_MAX], *slash;
    snprintf(buf, sizeof(buf), "%s", filePath);
    slash = strrchr(buf, '/');
    if (!slash || slash == buf) return 0;
    *slash = 0;
    if (mkdir(buf, 0755) == 0 || errno == EEXIST) return 0;
    return -1;
}

int IsamFileHandle_CreateNew(const char* filePath, const IsamSchema* schema)
{
    uint8_t* bytes = NULL;
    int len = 0;
    FILE* f;
    if (!filePath || !schema) return GMB_DB_ERR;
    isam_mkdir_parent(filePath);
    if (IsamFileHandle_BuildNewFileBytes(schema, &bytes, &len) != GMB_DB_OK) return GMB_DB_ERR;
    f = fopen(filePath, "wb");
    if (!f) { free(bytes); return GMB_DB_ERR; }
    if ((int)fwrite(bytes, 1, (size_t)len, f) != len) { fclose(f); free(bytes); return GMB_DB_ERR; }
    fflush(f); fclose(f); free(bytes);
    return GMB_DB_OK;
}

int IsamFileHandle_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_create_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    FILE* f;
    IsamFileHeader h;
    uint8_t magic[4];
    int recSize;
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
    recSize = SchemaManager_CalculateRecordSize(&schema);
    if (recSize != RecordMetadata_Size + 2 + 20) return -2;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -3;
    f = fopen(path, "rb");
    if (!f) return -4;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) { fclose(f); return -5; }
    if (h.RecordSize != recSize) { fclose(f); return -6; }
    fseek(f, (long)h.SchemaOffset, SEEK_SET);
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return -7; }
    fclose(f);
    if (memcmp(magic, SchemaManager_SchemaMagic, 4) != 0) return -8;
    return GMB_DB_OK;
}

/* leftover inventory stubs that would collide were replaced above */
