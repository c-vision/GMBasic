#include <sys/stat.h>
#include <unistd.h>

#define MANIFEST_MAX_TABLES 32
#define MANIFEST_MAX_SST 64
#define MANIFEST_MAX_SEC 8
#define MANIFEST_MAX_FIELDS 32
#define MANIFEST_MAX_GC 64

typedef struct {
    int level;
    char name[256];
} ManifestSstRef;

typedef struct {
    char name[64];
    char typeName[32];
    int length;
} ManifestField;

typedef struct {
    char name[64];
    char keyExpression[64];
    char typeName[32];
    int keyLength;
    int isUnique;
} ManifestSecIndex;

typedef struct {
    int id;
    char name[128];
    int64_t rowCount;
    int rowCountTrusted;
    ManifestSstRef sst[MANIFEST_MAX_SST];
    int sstN;
    ManifestSecIndex sec[MANIFEST_MAX_SEC];
    int secN;
    ManifestField fields[MANIFEST_MAX_FIELDS];
    int fieldN;
} ManifestTable;

struct Manifest {
    char databasePath[1024];
    char manifestPath[1024];
    uint64_t nextFileNumber;
    int nextTableId;
    ManifestTable tables[MANIFEST_MAX_TABLES];
    int tableN;
    char gcFiles[MANIFEST_MAX_GC][256];
    int gcN;
};

static const char* json_skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    return s;
}

static int json_parse_string(const char** ps, char* out, int cap)
{
    const char* s = json_skip_ws(*ps);
    int n = 0;
    if (*s != '"') return -1;
    s++;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1]) s++;
        if (n + 1 < cap) out[n++] = *s;
        s++;
    }
    if (*s != '"') return -1;
    if (out && cap) out[n] = 0;
    *ps = s + 1;
    return n;
}

static int json_expect(const char** ps, char c)
{
    const char* s = json_skip_ws(*ps);
    if (*s != c) return -1;
    *ps = s + 1;
    return 0;
}

static int64_t json_parse_int(const char** ps)
{
    const char* s = json_skip_ws(*ps);
    char* end = NULL;
    int64_t v = strtoll(s, &end, 10);
    *ps = end;
    return v;
}

static int json_parse_bool(const char** ps)
{
    const char* s = json_skip_ws(*ps);
    if (strncmp(s, "true", 4) == 0) { *ps = s + 4; return 1; }
    if (strncmp(s, "false", 5) == 0) { *ps = s + 5; return 0; }
    return 0;
}

static void json_skip_value(const char** ps);

static void json_skip_object_or_array(const char** ps, char open, char close)
{
    int depth = 1;
    const char* s = json_skip_ws(*ps);
    if (*s != open) return;
    s++;
    while (*s && depth) {
        if (*s == '"') {
            s++;
            while (*s && *s != '"') {
                if (*s == '\\' && s[1]) s++;
                s++;
            }
            if (*s == '"') s++;
            continue;
        }
        if (*s == open) depth++;
        else if (*s == close) depth--;
        s++;
    }
    *ps = s;
}

static void json_skip_value(const char** ps)
{
    const char* s = json_skip_ws(*ps);
    if (*s == '{') { *ps = s; json_skip_object_or_array(ps, '{', '}'); return; }
    if (*s == '[') { *ps = s; json_skip_object_or_array(ps, '[', ']'); return; }
    if (*s == '"') { char tmp[8]; json_parse_string(ps, tmp, (int)sizeof(tmp)); return; }
    if (strncmp(s, "true", 4) == 0) { *ps = s + 4; return; }
    if (strncmp(s, "false", 5) == 0) { *ps = s + 5; return; }
    if (strncmp(s, "null", 4) == 0) { *ps = s + 4; return; }
    json_parse_int(ps);
}

static int manifest_parse_datatype(const char** ps, char* typeOut, int cap)
{
    const char* s = json_skip_ws(*ps);
    if (typeOut && cap) snprintf(typeOut, (size_t)cap, "StringType");
    if (*s != '{') {
        json_skip_value(ps);
        return 0;
    }
    if (json_expect(&s, '{') != 0) return -1;
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "$type") == 0) {
            if (json_parse_string(&s, typeOut, cap) < 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    if (json_expect(&s, '}') != 0) return -1;
    *ps = s;
    return 0;
}

static int manifest_parse_field(const char** ps, ManifestField* field)
{
    const char* s = json_skip_ws(*ps);
    if (json_expect(&s, '{') != 0) return -1;
    memset(field, 0, sizeof(*field));
    snprintf(field->typeName, sizeof(field->typeName), "StringType");
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "Name") == 0) {
            if (json_parse_string(&s, field->name, (int)sizeof(field->name)) < 0) return -1;
        } else if (strcmp(key, "Length") == 0) {
            field->length = (int)json_parse_int(&s);
        } else if (strcmp(key, "DataType") == 0) {
            if (manifest_parse_datatype(&s, field->typeName, (int)sizeof(field->typeName)) != 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    if (json_expect(&s, '}') != 0) return -1;
    *ps = s;
    return 0;
}

static int manifest_parse_index_def(const char** ps, ManifestSecIndex* ix)
{
    const char* s = json_skip_ws(*ps);
    int seenUnique = 0;
    if (json_expect(&s, '{') != 0) return -1;
    memset(ix, 0, sizeof(*ix));
    snprintf(ix->typeName, sizeof(ix->typeName), "StringType");
    ix->isUnique = 1;
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "Name") == 0) {
            if (json_parse_string(&s, ix->name, (int)sizeof(ix->name)) < 0) return -1;
        } else if (strcmp(key, "KeyExpression") == 0) {
            if (json_parse_string(&s, ix->keyExpression, (int)sizeof(ix->keyExpression)) < 0) return -1;
        } else if (strcmp(key, "KeyLength") == 0) {
            ix->keyLength = (int)json_parse_int(&s);
        } else if (strcmp(key, "IsUnique") == 0) {
            ix->isUnique = json_parse_bool(&s);
            seenUnique = 1;
        } else if (strcmp(key, "DataType") == 0) {
            if (manifest_parse_datatype(&s, ix->typeName, (int)sizeof(ix->typeName)) != 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    if (!seenUnique) ix->isUnique = 1;
    if (json_expect(&s, '}') != 0) return -1;
    *ps = s;
    return 0;
}

static int manifest_parse_schema(const char** ps, ManifestTable* t)
{
    const char* s = json_skip_ws(*ps);
    if (json_expect(&s, '{') != 0) return -1;
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "SecondaryKeys") == 0) {
            if (json_expect(&s, '[') != 0) return -1;
            while (*json_skip_ws(s) && *json_skip_ws(s) != ']') {
                ManifestSecIndex ix;
                s = json_skip_ws(s);
                if (*s == ']') break;
                if (manifest_parse_index_def(&s, &ix) != 0) return -1;
                if (t->secN < MANIFEST_MAX_SEC && ix.name[0])
                    t->sec[t->secN++] = ix;
                s = json_skip_ws(s);
                if (*s == ',') s++;
            }
            if (json_expect(&s, ']') != 0) return -1;
        } else if (strcmp(key, "Fields") == 0) {
            if (json_expect(&s, '[') != 0) return -1;
            while (*json_skip_ws(s) && *json_skip_ws(s) != ']') {
                ManifestField field;
                s = json_skip_ws(s);
                if (*s == ']') break;
                if (manifest_parse_field(&s, &field) != 0) return -1;
                if (t->fieldN < MANIFEST_MAX_FIELDS && field.name[0])
                    t->fields[t->fieldN++] = field;
                s = json_skip_ws(s);
                if (*s == ',') s++;
            }
            if (json_expect(&s, ']') != 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    if (json_expect(&s, '}') != 0) return -1;
    *ps = s;
    return 0;
}

static ManifestTable* manifest_table_by_id(Manifest* self, int tableId)
{
    int i;
    for (i = 0; i < self->tableN; i++)
        if (self->tables[i].id == tableId) return &self->tables[i];
    return NULL;
}

static ManifestTable* manifest_table_by_name(Manifest* self, const char* name)
{
    int i;
    for (i = 0; i < self->tableN; i++)
        if (gmb_strcasecmp(self->tables[i].name, name) == 0) return &self->tables[i];
    return NULL;
}

static int manifest_parse_sst_level(const char** ps, ManifestTable* t, int level)
{
    const char* s = json_skip_ws(*ps);
    if (*s != '[') return -1;
    s++;
    s = json_skip_ws(s);
    while (*s && *s != ']') {
        char fname[256];
        if (json_parse_string(&s, fname, (int)sizeof(fname)) < 0) return -1;
        if (t->sstN < MANIFEST_MAX_SST) {
            t->sst[t->sstN].level = level;
            snprintf(t->sst[t->sstN].name, sizeof(t->sst[t->sstN].name), "%s", fname);
            t->sstN++;
        }
        s = json_skip_ws(s);
        if (*s == ',') { s++; s = json_skip_ws(s); }
    }
    if (*s != ']') return -1;
    *ps = s + 1;
    return 0;
}

static int manifest_parse_table_object(const char** ps, ManifestTable* t)
{
    const char* s = json_skip_ws(*ps);
    if (json_expect(&s, '{') != 0) return -1;
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "Id") == 0) t->id = (int)json_parse_int(&s);
        else if (strcmp(key, "Name") == 0) {
            if (json_parse_string(&s, t->name, (int)sizeof(t->name)) < 0) return -1;
        } else if (strcmp(key, "RowCount") == 0) t->rowCount = json_parse_int(&s);
        else if (strcmp(key, "RowCountTrusted") == 0) t->rowCountTrusted = json_parse_bool(&s);
        else if (strcmp(key, "SstFilesByLevel") == 0) {
            if (json_expect(&s, '{') != 0) return -1;
            while (*json_skip_ws(s) && *json_skip_ws(s) != '}') {
                char lvl[16];
                s = json_skip_ws(s);
                if (*s == '}') break;
                if (json_parse_string(&s, lvl, (int)sizeof(lvl)) < 0) return -1;
                if (json_expect(&s, ':') != 0) return -1;
                if (manifest_parse_sst_level(&s, t, atoi(lvl)) != 0) return -1;
                s = json_skip_ws(s);
                if (*s == ',') s++;
            }
            if (json_expect(&s, '}') != 0) return -1;
        } else if (strcmp(key, "Schema") == 0) {
            if (manifest_parse_schema(&s, t) != 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    if (json_expect(&s, '}') != 0) return -1;
    *ps = s;
    return 0;
}

static int manifest_parse(Manifest* self, const char* json)
{
    const char* s = json_skip_ws(json);
    if (json_expect(&s, '{') != 0) return -1;
    self->nextFileNumber = 1;
    self->nextTableId = 1;
    self->tableN = 0;
    self->gcN = 0;
    while (*s && *s != '}') {
        char key[64];
        s = json_skip_ws(s);
        if (*s == '}') break;
        if (json_parse_string(&s, key, (int)sizeof(key)) < 0) return -1;
        if (json_expect(&s, ':') != 0) return -1;
        if (strcmp(key, "NextFileNumber") == 0) self->nextFileNumber = (uint64_t)json_parse_int(&s);
        else if (strcmp(key, "NextTableId") == 0) self->nextTableId = (int)json_parse_int(&s);
        else if (strcmp(key, "Tables") == 0) {
            if (json_expect(&s, '{') != 0) return -1;
            while (*json_skip_ws(s) && *json_skip_ws(s) != '}') {
                ManifestTable* t;
                char tname[128];
                s = json_skip_ws(s);
                if (*s == '}') break;
                if (json_parse_string(&s, tname, (int)sizeof(tname)) < 0) return -1;
                if (json_expect(&s, ':') != 0) return -1;
                if (self->tableN >= MANIFEST_MAX_TABLES) return -1;
                t = &self->tables[self->tableN];
                memset(t, 0, sizeof(*t));
                snprintf(t->name, sizeof(t->name), "%s", tname);
                if (manifest_parse_table_object(&s, t) != 0) return -1;
                self->tableN++;
                s = json_skip_ws(s);
                if (*s == ',') s++;
            }
            if (json_expect(&s, '}') != 0) return -1;
        } else if (strcmp(key, "FilesToDelete") == 0) {
            if (json_expect(&s, '[') != 0) return -1;
            while (*json_skip_ws(s) && *json_skip_ws(s) != ']') {
                char fname[256];
                s = json_skip_ws(s);
                if (*s == ']') break;
                if (json_parse_string(&s, fname, (int)sizeof(fname)) < 0) return -1;
                if (self->gcN < MANIFEST_MAX_GC && fname[0]) {
                    snprintf(self->gcFiles[self->gcN], sizeof(self->gcFiles[0]), "%s", fname);
                    self->gcN++;
                }
                s = json_skip_ws(s);
                if (*s == ',') s++;
            }
            if (json_expect(&s, ']') != 0) return -1;
        } else {
            json_skip_value(&s);
        }
        s = json_skip_ws(s);
        if (*s == ',') s++;
        s = json_skip_ws(s);
    }
    return 0;
}

Manifest* Manifest_Load(const char* databasePath)
{
    Manifest* self;
    FILE* f;
    long len;
    char* buf;
    if (!databasePath) return NULL;
    self = (Manifest*)calloc(1, sizeof(*self));
    snprintf(self->databasePath, sizeof(self->databasePath), "%s", databasePath);
    snprintf(self->manifestPath, sizeof(self->manifestPath), "%s/%s", databasePath, Config_ManifestFileName);
    self->nextFileNumber = 1;
    self->nextTableId = 1;
    f = fopen(self->manifestPath, "rb");
    if (!f) return self;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); Manifest_Dispose(self); return NULL; }
    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); Manifest_Dispose(self); return NULL;
    }
    buf[len] = 0;
    fclose(f);
    if (manifest_parse(self, buf) != 0) { free(buf); Manifest_Dispose(self); return NULL; }
    free(buf);
    return self;
}

void Manifest_Dispose(Manifest* self)
{
    free(self);
}

int Manifest_Save(Manifest* self)
{
    char tmp[1040];
    FILE* f;
    int i, j;
    if (!self) return GMB_DB_ERR;
    snprintf(tmp, sizeof(tmp), "%s.tmp", self->manifestPath);
    f = fopen(tmp, "wb");
    if (!f) return GMB_DB_ERR;
    fprintf(f, "{\n  \"NextFileNumber\": %llu,\n  \"NextTableId\": %d,\n  \"Tables\": {\n",
            (unsigned long long)self->nextFileNumber, self->nextTableId);
    for (i = 0; i < self->tableN; i++) {
        ManifestTable* t = &self->tables[i];
        int maxLevel = -1, level, firstLevel = 1;
        fprintf(f, "    \"%s\": {\n      \"Id\": %d,\n      \"Name\": \"%s\",\n", t->name, t->id, t->name);
        fprintf(f, "      \"Schema\": {\n        \"TypeName\": \"%s\",\n        \"Fields\": [", t->name);
        for (j = 0; j < t->fieldN; j++) {
            const char* tn = t->fields[j].typeName[0] ? t->fields[j].typeName : "StringType";
            fprintf(f, "%s\n          {\n            \"Name\": \"%s\",\n            \"DataType\": {\n              \"$type\": \"%s\"\n            },\n            \"Length\": %d\n          }",
                    j ? "," : "", t->fields[j].name, tn, t->fields[j].length);
        }
        fprintf(f, "%s],\n", t->fieldN ? "\n        " : "");
        fprintf(f, "        \"PrimaryKey\": {\n          \"Name\": \"Id\",\n          \"KeyExpression\": \"Id\",\n");
        fprintf(f, "          \"DataType\": {\n            \"$type\": \"StringType\"\n          },\n          \"KeyLength\": 0,\n          \"IsUnique\": true,\n          \"IsUnsafe\": false\n        },\n");
        fprintf(f, "        \"SecondaryKeys\": [");
        for (j = 0; j < t->secN; j++) {
            const char* tn = t->sec[j].typeName[0] ? t->sec[j].typeName : "StringType";
            fprintf(f, "%s\n          {\n            \"Name\": \"%s\",\n            \"KeyExpression\": \"%s\",\n",
                    j ? "," : "", t->sec[j].name, t->sec[j].keyExpression);
            fprintf(f, "            \"DataType\": {\n              \"$type\": \"%s\"\n            },\n            \"KeyLength\": %d,\n            \"IsUnique\": %s,\n            \"IsUnsafe\": false\n          }",
                    tn, t->sec[j].keyLength, t->sec[j].isUnique ? "true" : "false");
        }
        fprintf(f, "%s],\n        \"ViewDefinitionQuery\": null\n      },\n", t->secN ? "\n        " : "");
        fprintf(f, "      \"SstFilesByLevel\": {");
        for (j = 0; j < t->sstN; j++)
            if (t->sst[j].level > maxLevel) maxLevel = t->sst[j].level;
        for (level = 0; level <= maxLevel; level++) {
            int nlev = 0, k, first = 1;
            for (k = 0; k < t->sstN; k++) if (t->sst[k].level == level) nlev++;
            if (nlev == 0) continue;
            fprintf(f, "%s\n        \"%d\": [", firstLevel ? "" : ",", level);
            firstLevel = 0;
            for (k = 0; k < t->sstN; k++) {
                if (t->sst[k].level != level) continue;
                fprintf(f, "%s\n          \"%s\"", first ? "" : ",", t->sst[k].name);
                first = 0;
            }
            fprintf(f, "\n        ]");
        }
        fprintf(f, "\n      },\n      \"RowCount\": %lld,\n      \"RowCountTrusted\": %s\n    }%s\n",
                (long long)t->rowCount, t->rowCountTrusted ? "true" : "false",
                i + 1 < self->tableN ? "," : "");
    }
    fprintf(f, "  },\n  \"FilesToDelete\": [");
    for (j = 0; j < self->gcN; j++) {
        fprintf(f, "%s\n    \"%s\"", j ? "," : "", self->gcFiles[j]);
    }
    fprintf(f, "%s]\n}\n", self->gcN ? "\n  " : "");
    fclose(f);
    if (rename(tmp, self->manifestPath) != 0) { unlink(tmp); return GMB_DB_ERR; }
    return GMB_DB_OK;
}

uint64_t Manifest_GetNextFileNumber(Manifest* self)
{
    return self->nextFileNumber++;
}

int Manifest_CreateTable(Manifest* self, const char* typeName)
{
    ManifestTable* t;
    if (!self || !typeName || !typeName[0]) return -1;
    t = manifest_table_by_name(self, typeName);
    if (t) return t->id;
    if (self->tableN >= MANIFEST_MAX_TABLES) return -1;
    t = &self->tables[self->tableN++];
    memset(t, 0, sizeof(*t));
    t->id = self->nextTableId++;
    snprintf(t->name, sizeof(t->name), "%s", typeName);
    t->rowCountTrusted = 1;
    if (Manifest_Save(self) != GMB_DB_OK) return -1;
    return t->id;
}

int Manifest_TryGetTable(const Manifest* self, int tableId, char* nameOut, int nameCap)
{
    int i;
    if (!self) return 0;
    for (i = 0; i < self->tableN; i++) {
        if (self->tables[i].id != tableId) continue;
        if (nameOut && nameCap) snprintf(nameOut, (size_t)nameCap, "%s", self->tables[i].name);
        return 1;
    }
    return 0;
}

int Manifest_AddSstFile(Manifest* self, int tableId, const char* sstFileName)
{
    ManifestTable* t;
    const char* base;
    if (!self || !sstFileName) return GMB_DB_ERR;
    t = manifest_table_by_id(self, tableId);
    if (!t) return GMB_DB_OK; /* C#: if (table == null) return; */
    if (t->sstN >= MANIFEST_MAX_SST) return GMB_DB_ERR;
    base = strrchr(sstFileName, '/');
    base = base ? base + 1 : sstFileName;
    t->sst[t->sstN].level = 0;
    snprintf(t->sst[t->sstN].name, sizeof(t->sst[t->sstN].name), "%s", base);
    t->sstN++;
    return GMB_DB_OK;
}

int Manifest_CollectSstNewestFirst(const Manifest* self, int tableId, char names[][256], int max)
{
    const ManifestTable* t = NULL;
    int i, level, maxLevel = -1, n = 0;
    if (!self || max <= 0) return 0;
    for (i = 0; i < self->tableN; i++)
        if (self->tables[i].id == tableId) { t = &self->tables[i]; break; }
    if (!t) return 0;
    for (i = 0; i < t->sstN; i++)
        if (t->sst[i].level > maxLevel) maxLevel = t->sst[i].level;
    for (level = 0; level <= maxLevel; level++) {
        int idx[MANIFEST_MAX_SST], c = 0, k;
        for (i = 0; i < t->sstN; i++)
            if (t->sst[i].level == level) idx[c++] = i;
        for (k = c - 1; k >= 0 && n < max; k--)
            snprintf(names[n++], 256, "%s", t->sst[idx[k]].name);
    }
    return n;
}

int Manifest_CollectSstOldestFirst(const Manifest* self, int tableId, char names[][256], int max)
{
    const ManifestTable* t = NULL;
    int i, n = 0;
    if (!self || max <= 0) return 0;
    for (i = 0; i < self->tableN; i++)
        if (self->tables[i].id == tableId) { t = &self->tables[i]; break; }
    if (!t) return 0;
    for (i = 0; i < t->sstN && n < max; i++)
        snprintf(names[n++], 256, "%s", t->sst[i].name);
    return n;
}

int Manifest_TryGetTableByName(const Manifest* self, const char* name, int* idOut)
{
    const ManifestTable* t;
    if (idOut) *idOut = 0;
    if (!self || !name) return 0;
    t = manifest_table_by_name((Manifest*)self, name);
    if (!t) return 0;
    if (idOut) *idOut = t->id;
    return 1;
}

int Manifest_GetFilesForLevel(const Manifest* self, int tableId, int level, char names[][256], int max)
{
    const ManifestTable* t = NULL;
    int i, n = 0;
    if (!self || max <= 0) return 0;
    for (i = 0; i < self->tableN; i++)
        if (self->tables[i].id == tableId) { t = &self->tables[i]; break; }
    if (!t) return 0;
    for (i = 0; i < t->sstN && n < max; i++) {
        if (t->sst[i].level != level) continue;
        snprintf(names[n++], 256, "%s", t->sst[i].name);
    }
    return n;
}

static int manifest_name_in(const char names[][256], int n, const char* s)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(names[i], s) == 0) return 1;
    return 0;
}

static void manifest_remove_names(ManifestTable* t, int level, const char names[][256], int n)
{
    int i, w = 0;
    for (i = 0; i < t->sstN; i++) {
        if (t->sst[i].level == level && manifest_name_in(names, n, t->sst[i].name)) continue;
        t->sst[w++] = t->sst[i];
    }
    t->sstN = w;
}

static int manifest_add_at_level(ManifestTable* t, int level, const char* newFile)
{
    if (t->sstN >= MANIFEST_MAX_SST) return GMB_DB_ERR;
    t->sst[t->sstN].level = level;
    snprintf(t->sst[t->sstN].name, sizeof(t->sst[t->sstN].name), "%s", newFile);
    t->sstN++;
    return GMB_DB_OK;
}

int Manifest_UpdateAfterCompaction(Manifest* self, int tableId, int sourceLevel,
                                   char compacted[][256], int compactedN, int destLevel, const char* newFile)
{
    ManifestTable* t;
    if (!self || !newFile) return GMB_DB_ARG;
    t = manifest_table_by_id(self, tableId);
    if (!t) return GMB_DB_OK;
    manifest_remove_names(t, sourceLevel, compacted, compactedN);
    return manifest_add_at_level(t, destLevel, newFile);
}

int Manifest_ReplaceLevelsAfterCompaction(Manifest* self, int tableId,
                                          char l0[][256], int n0, char l1[][256], int n1,
                                          int destLevel, const char* newFile)
{
    ManifestTable* t;
    if (!self || !newFile) return GMB_DB_ARG;
    t = manifest_table_by_id(self, tableId);
    if (!t) return GMB_DB_OK;
    if (n0 > 0) manifest_remove_names(t, 0, l0, n0);
    if (n1 > 0) manifest_remove_names(t, 1, l1, n1);
    return manifest_add_at_level(t, destLevel, newFile);
}

int Manifest_ReplaceAllSstAfterMajorCompact(Manifest* self, int tableId, int destLevel,
                                            const char* newFile)
{
    ManifestTable* t;
    if (!self || !newFile) return GMB_DB_ARG;
    t = manifest_table_by_id(self, tableId);
    if (!t) return GMB_DB_OK;
    t->sstN = 0;
    return manifest_add_at_level(t, destLevel, newFile);
}

void Manifest_IndexTableName(const char* mainTableName, const char* indexName, char* out, int cap)
{
    if (!out || cap <= 0) return;
    snprintf(out, (size_t)cap, "_idx_%s_%s", mainTableName ? mainTableName : "", indexName ? indexName : "");
}

int Manifest_AddSecondaryIndex(Manifest* self, const char* mainTableName, const char* indexName,
                               const char* keyExpression, int keyLength)
{
    ManifestTable* t;
    int i;
    if (!self || !mainTableName || !indexName || !indexName[0]) return GMB_DB_ARG;
    t = manifest_table_by_name(self, mainTableName);
    if (!t) return GMB_DB_ERR;
    for (i = 0; i < t->secN; i++) {
        if (gmb_strcasecmp(t->sec[i].name, indexName) == 0) return GMB_DB_OK;
    }
    if (t->secN >= MANIFEST_MAX_SEC) return GMB_DB_ERR;
    snprintf(t->sec[t->secN].name, sizeof(t->sec[0].name), "%s", indexName);
    snprintf(t->sec[t->secN].keyExpression, sizeof(t->sec[0].keyExpression), "%s", keyExpression ? keyExpression : "");
    t->sec[t->secN].keyLength = keyLength;
    t->sec[t->secN].isUnique = 1;
    snprintf(t->sec[t->secN].typeName, sizeof(t->sec[0].typeName), "StringType");
    if (keyExpression && keyExpression[0]) {
        for (i = 0; i < t->fieldN; i++) {
            if (gmb_strcasecmp(t->fields[i].name, keyExpression) == 0 && t->fields[i].typeName[0]) {
                snprintf(t->sec[t->secN].typeName, sizeof(t->sec[0].typeName), "%s", t->fields[i].typeName);
                break;
            }
        }
    }
    t->secN++;
    return GMB_DB_OK;
}

int Manifest_SecondaryKeyCount(const Manifest* self, int tableId)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    return t ? t->secN : 0;
}

int Manifest_GetSecondaryKey(const Manifest* self, int tableId, int i,
                             char* nameOut, int nameCap, char* exprOut, int exprCap, int* keyLength)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    if (!t || i < 0 || i >= t->secN) return 0;
    if (nameOut && nameCap) snprintf(nameOut, (size_t)nameCap, "%s", t->sec[i].name);
    if (exprOut && exprCap) snprintf(exprOut, (size_t)exprCap, "%s", t->sec[i].keyExpression);
    if (keyLength) *keyLength = t->sec[i].keyLength;
    return 1;
}

int Manifest_CountSstFiles(const Manifest* self, int tableId)
{
    char names[64][256];
    return Manifest_CollectSstOldestFirst(self, tableId, names, 64);
}

int64_t Manifest_GetRowCount(const Manifest* self, int tableId)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    return (t && t->rowCountTrusted) ? t->rowCount : 0;
}

int Manifest_IsRowCountTrusted(const Manifest* self, int tableId)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    return t ? t->rowCountTrusted : 0;
}

static void manifest_normalize_type(const char* typeName, char* out, int cap)
{
    SpeedDataTypeKind kind;
    const char* canon;
    if (!out || cap <= 0) return;
    kind = SpeedDataType_Parse(typeName);
    canon = SpeedDataType_Name(kind);
    snprintf(out, (size_t)cap, "%s", canon && canon[0] ? canon : "StringType");
}

int Manifest_AddField(Manifest* self, int tableId, const char* name, const char* typeName, int length)
{
    ManifestTable* t;
    int i;
    if (!self || !name || !name[0]) return GMB_DB_ARG;
    t = manifest_table_by_id(self, tableId);
    if (!t) return GMB_DB_ERR;
    for (i = 0; i < t->fieldN; i++) {
        if (gmb_strcasecmp(t->fields[i].name, name) == 0) {
            manifest_normalize_type(typeName, t->fields[i].typeName, (int)sizeof(t->fields[i].typeName));
            t->fields[i].length = length;
            return Manifest_Save(self);
        }
    }
    if (t->fieldN >= MANIFEST_MAX_FIELDS) return GMB_DB_ERR;
    snprintf(t->fields[t->fieldN].name, sizeof(t->fields[0].name), "%s", name);
    manifest_normalize_type(typeName, t->fields[t->fieldN].typeName, (int)sizeof(t->fields[0].typeName));
    t->fields[t->fieldN].length = length;
    t->fieldN++;
    return Manifest_Save(self);
}

int Manifest_FieldCount(const Manifest* self, int tableId)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    return t ? t->fieldN : 0;
}

int Manifest_GetField(const Manifest* self, int tableId, int i,
                      char* nameOut, int nameCap, char* typeOut, int typeCap, int* length)
{
    const ManifestTable* t;
    if (!self) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    if (!t || i < 0 || i >= t->fieldN) return 0;
    if (nameOut && nameCap) snprintf(nameOut, (size_t)nameCap, "%s", t->fields[i].name);
    if (typeOut && typeCap) snprintf(typeOut, (size_t)typeCap, "%s", t->fields[i].typeName);
    if (length) *length = t->fields[i].length;
    return 1;
}

int Manifest_FindField(const Manifest* self, int tableId, const char* name,
                       char* typeOut, int typeCap, int* length)
{
    const ManifestTable* t;
    int i;
    if (!self || !name) return 0;
    t = manifest_table_by_id((Manifest*)self, tableId);
    if (!t) return 0;
    for (i = 0; i < t->fieldN; i++) {
        if (gmb_strcasecmp(t->fields[i].name, name) != 0) continue;
        if (typeOut && typeCap) snprintf(typeOut, (size_t)typeCap, "%s", t->fields[i].typeName);
        if (length) *length = t->fields[i].length;
        return 1;
    }
    return 0;
}

int Manifest_AddFilesToGarbageCollection(Manifest* self, const char* const* files, int n)
{
    int i;
    if (!self || !files || n <= 0) return GMB_DB_ARG;
    for (i = 0; i < n; i++) {
        int j, dup = 0;
        if (!files[i] || !files[i][0]) continue;
        for (j = 0; j < self->gcN; j++) {
            if (strcmp(self->gcFiles[j], files[i]) == 0) { dup = 1; break; }
        }
        if (dup) continue;
        if (self->gcN >= MANIFEST_MAX_GC) return GMB_DB_ERR;
        snprintf(self->gcFiles[self->gcN], sizeof(self->gcFiles[0]), "%s", files[i]);
        self->gcN++;
    }
    return GMB_DB_OK;
}

int Manifest_GetFilesForGarbageCollection(const Manifest* self, char names[][256], int max)
{
    int i, n;
    if (!self || !names || max <= 0) return 0;
    n = self->gcN < max ? self->gcN : max;
    for (i = 0; i < n; i++)
        snprintf(names[i], 256, "%s", self->gcFiles[i]);
    return n;
}

int Manifest_RemoveFileFromGarbageCollection(Manifest* self, const char* fileName)
{
    int i, w = 0;
    if (!self || !fileName) return GMB_DB_ARG;
    for (i = 0; i < self->gcN; i++) {
        if (strcmp(self->gcFiles[i], fileName) == 0) continue;
        if (w != i) memcpy(self->gcFiles[w], self->gcFiles[i], sizeof(self->gcFiles[0]));
        w++;
    }
    self->gcN = w;
    return GMB_DB_OK;
}

int Manifest_PrepareTableForDeletion(Manifest* self, const char* tableName,
                                     int* tableIdOut, char files[][256], int max)
{
    int i, n = 0;
    ManifestTable* t;
    if (tableIdOut) *tableIdOut = -1;
    if (!self || !tableName) return 0;
    t = manifest_table_by_name(self, tableName);
    if (!t) return 0;
    if (tableIdOut) *tableIdOut = t->id;
    if (files && max > 0) {
        for (i = 0; i < t->sstN && n < max; i++) {
            snprintf(files[n], 256, "%s", t->sst[i].name);
            n++;
        }
    } else {
        n = t->sstN;
    }
    i = (int)(t - self->tables);
    if (i + 1 < self->tableN)
        memmove(&self->tables[i], &self->tables[i + 1],
                (size_t)(self->tableN - i - 1) * sizeof(self->tables[0]));
    self->tableN--;
    return n;
}

int Manifest_RemoveSecondaryIndex(Manifest* self, const char* mainTableName, const char* indexName)
{
    ManifestTable* t;
    int i;
    if (!self || !mainTableName || !indexName) return GMB_DB_ARG;
    t = manifest_table_by_name(self, mainTableName);
    if (!t) return GMB_DB_ERR;
    for (i = 0; i < t->secN; i++) {
        if (gmb_strcasecmp(t->sec[i].name, indexName) != 0) continue;
        if (i + 1 < t->secN)
            memmove(&t->sec[i], &t->sec[i + 1], (size_t)(t->secN - i - 1) * sizeof(t->sec[0]));
        t->secN--;
        memset(&t->sec[t->secN], 0, sizeof(t->sec[0]));
        return GMB_DB_OK;
    }
    return GMB_DB_ERR;
}

void Manifest_SetRowCount(Manifest* self, int tableId, int64_t count)
{
    ManifestTable* t = self ? manifest_table_by_id(self, tableId) : NULL;
    if (!t) return;
    t->rowCount = count < 0 ? 0 : count;
    t->rowCountTrusted = 1;
}

void Manifest_AdjustRowCount(Manifest* self, int tableId, int64_t delta)
{
    ManifestTable* t = self ? manifest_table_by_id(self, tableId) : NULL;
    int64_t n;
    if (!t) return;
    n = t->rowCount + delta;
    t->rowCount = n < 0 ? 0 : n;
    t->rowCountTrusted = 1;
}

int Manifest_TableCount(const Manifest* self)
{
    return self ? self->tableN : 0;
}

int Manifest_GetTableIdAt(const Manifest* self, int index, int* idOut)
{
    if (idOut) *idOut = 0;
    if (!self || index < 0 || index >= self->tableN) return 0;
    if (idOut) *idOut = self->tables[index].id;
    return 1;
}
