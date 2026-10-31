struct Engine {
    IsamFileHandle* handle;
    BPlusTreeManager* primary;
    BPlusTreeManager* active;
    BPlusTreeManager* secondary[15];
    int secondaryCount;
    int64_t currentOffset;
    int64_t lastReadVersion;
    int eof;
    int bof;
    char filterExpr[512];
    uint8_t* pushdownKey;
    int pushdownKeyLen;
    int pushdownMatchLength;
    int lastFilterUsedSeekPushdown;
    int lastJoinMode;
    IsamReplicationLog* repl;
};

static int engine_step(Engine* self, int direction);
static int engine_evaluate_filter(Engine* self);
static int engine_record_valid_for_filter(Engine* self);
static void engine_skip_filter_internal(Engine* self, int direction);
static void engine_clear_filter_pushdown(Engine* self);
static void engine_try_setup_filter_pushdown(Engine* self);
static int engine_go_raw(Engine* self, CursorMoveOperation operation);
static void engine_go_internal(Engine* self, CursorMoveOperation operation);
static void engine_fields_text_from_out(IsamFieldValue* fields, int fieldCount);
static const IsamIndexDefinition* engine_active_def(const Engine* self);

static int64_t engine_utc_ticks(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    /* ponytail: Unix epoch * 10^7, non DateTime epoch .NET; serve solo Version univoca. */
    return (int64_t)ts.tv_sec * 10000000LL + (int64_t)ts.tv_nsec / 100;
}

static int engine_index_key(const Engine* self, const IsamFieldValue* fields,
                            const IsamIndexDefinition* def, uint8_t* key)
{
    const IsamSchema* s = self->handle->Schema;
    int i, keyLen = def->KeyLength;
    const char* expr = def->KeyExpression[0] ? def->KeyExpression : def->Name;
    char names[IsamFilterPushdown_MaxEq][IsamFilterPushdown_NameCap];
    int nNames = 0;

    if (IsamFilterPushdown_TryGetKeyFieldNames(expr, names, IsamFilterPushdown_MaxEq, &nNames) && nNames >= 2) {
        char buf[512];
        int pos = 0, p;
        buf[0] = 0;
        for (p = 0; p < nNames; p++) {
            int fi = -1, j;
            char piece[64];
            if (p) buf[pos++] = '\x1F';
            for (j = 0; j < s->FieldCount; j++) {
                if (gmb_strcasecmp(s->Fields[j].Name, names[p]) == 0) {
                    fi = j;
                    break;
                }
            }
            if (fi < 0) piece[0] = 0;
            else if (strcmp(s->Fields[fi].DataTypeCode, "StringType") == 0)
                snprintf(piece, sizeof(piece), "%s",
                         fields[fi].text ? fields[fi].text : (fields[fi].textOut ? fields[fi].textOut : ""));
            else
                snprintf(piece, sizeof(piece), "%lld", (long long)fields[fi].i64);
            memcpy(buf + pos, piece, strlen(piece));
            pos += (int)strlen(piece);
            buf[pos] = 0;
            if (pos >= (int)sizeof(buf) - 1) break;
        }
        return SchemaManager_SerializeKeyValueText(buf, keyLen, key);
    }

    for (i = 0; i < s->FieldCount; i++) {
        if (gmb_strcasecmp(s->Fields[i].Name, expr) != 0
            && gmb_strcasecmp(s->Fields[i].Name, def->Name) != 0) continue;
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0) {
            const char* t = fields[i].text ? fields[i].text : fields[i].textOut;
            return SchemaManager_SerializeKeyValueText(t, keyLen, key);
        }
        return SchemaManager_SerializeKeyValueI64(fields[i].i64, keyLen, key);
    }
    return GMB_DB_ERR;
}

static int engine_pk_key(const Engine* self, const IsamFieldValue* fields, uint8_t* key)
{
    return engine_index_key(self, fields, &self->handle->Schema->PrimaryKey, key);
}

static int engine_secondary_mutate(Engine* self, const IsamFieldValue* fields, int64_t offset, int del)
{
    const IsamSchema* s;
    int i;
    if (!self || !fields) return GMB_DB_ERR;
    s = self->handle->Schema;
    for (i = 0; i < self->secondaryCount; i++) {
        uint8_t* key;
        int keyLen, rc;
        if (!self->secondary[i] || !s->SecondaryKeys) continue;
        keyLen = s->SecondaryKeys[i].KeyLength;
        key = (uint8_t*)malloc((size_t)keyLen);
        if (!key) return GMB_DB_ERR;
        if (engine_index_key(self, fields, &s->SecondaryKeys[i], key) != GMB_DB_OK) {
            free(key);
            return GMB_DB_ERR;
        }
        rc = del
            ? BPlusTreeManager_Delete(self->secondary[i], key, keyLen, offset)
            : BPlusTreeManager_Insert(self->secondary[i], key, keyLen, offset);
        free(key);
        if (rc != GMB_DB_OK) return rc;
    }
    return GMB_DB_OK;
}

static void engine_dispose_secondaries(Engine* self)
{
    int i;
    for (i = 0; i < self->secondaryCount; i++) {
        BPlusTreeManager_Dispose(self->secondary[i]);
        self->secondary[i] = NULL;
    }
    self->secondaryCount = 0;
}

static int engine_attach_secondaries(Engine* self)
{
    const IsamSchema* s = self->handle->Schema;
    int i, n;
    if (!s || !s->SecondaryKeys || s->SecondaryKeyCount <= 0) return GMB_DB_OK;
    n = s->SecondaryKeyCount;
    if (n > 15) n = 15;
    for (i = 0; i < n; i++) {
        self->secondary[i] = BPlusTreeManager_NewOnHandleAt(
            self->handle, s->SecondaryKeys[i].KeyLength, s->SecondaryKeys[i].IsUnique, i);
        if (!self->secondary[i]) {
            self->secondaryCount = i;
            engine_dispose_secondaries(self);
            return GMB_DB_ERR;
        }
    }
    self->secondaryCount = n;
    return GMB_DB_OK;
}

static void engine_on_heap_relocated(void* ctx, int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    Engine* self = (Engine*)ctx;
    int i;
    if (!self) return;
    if (self->currentOffset >= heapStart && self->currentOffset < heapEnd)
        self->currentOffset += delta;
    if (self->primary) {
        BPlusTreeManager_ReloadRootFromHeader(self->primary);
        BPlusTreeManager_RepairPointersAfterHeapMove(self->primary, heapStart, heapEnd, delta);
    }
    for (i = 0; i < self->secondaryCount; i++) {
        if (!self->secondary[i]) continue;
        BPlusTreeManager_ReloadRootFromHeader(self->secondary[i]);
        BPlusTreeManager_RepairPointersAfterHeapMove(self->secondary[i], heapStart, heapEnd, delta);
    }
}

Engine* Engine_Open(const char* filePath, const IsamSchema* schema)
{
    Engine* self;
    int keyLen;
    if (!filePath || !schema) return NULL;
    self = (Engine*)calloc(1, sizeof(*self));
    self->handle = IsamFileHandle_Open(filePath, schema);
    if (!self->handle) { free(self); return NULL; }
    keyLen = schema->PrimaryKey.KeyLength;
    self->primary = BPlusTreeManager_NewOnHandle(self->handle, keyLen, schema->PrimaryKey.IsUnique);
    if (!self->primary) {
        IsamFileHandle_Dispose(self->handle);
        free(self);
        return NULL;
    }
    if (engine_attach_secondaries(self) != GMB_DB_OK) {
        BPlusTreeManager_Dispose(self->primary);
        IsamFileHandle_Dispose(self->handle);
        free(self);
        return NULL;
    }
    self->active = self->primary;
    self->currentOffset = -1;
    self->lastReadVersion = -1;
    self->handle->AfterHeapRelocated = engine_on_heap_relocated;
    self->handle->afterHeapCtx = self;
    return self;
}

void Engine_Dispose(Engine* self)
{
    if (!self) return;
    if (self->repl) IsamReplicationLog_Dispose(self->repl);
    engine_dispose_secondaries(self);
    BPlusTreeManager_Dispose(self->primary);
    IsamFileHandle_Dispose(self->handle);
    free(self->pushdownKey);
    free(self);
}

static void engine_set_cursor(Engine* self, int64_t offset)
{
    RecordMetadata meta;
    self->currentOffset = offset;
    self->eof = 0;
    self->bof = 0;
    if (offset < 0) {
        self->lastReadVersion = -1;
        return;
    }
    if (IsamFileHandle_ReadRecordMetadata(self->handle, offset, &meta) == GMB_DB_OK)
        self->lastReadVersion = meta.Version;
    else
        self->lastReadVersion = -1;
}

static void engine_json_escape(char* dst, int cap, const char* s)
{
    int n = 0;
    if (!dst || cap <= 0) return;
    if (!s) s = "";
    while (*s && n < cap - 1) {
        if (*s == '"' || *s == '\\') {
            if (n + 2 >= cap) break;
            dst[n++] = '\\';
            dst[n++] = *s++;
        } else {
            dst[n++] = *s++;
        }
    }
    dst[n] = 0;
}

static void engine_repl_json(Engine* self, const IsamFieldValue* fields, int fieldCount, char* out, int cap)
{
    const IsamSchema* s;
    int i, n;
    if (!self || !self->handle || !fields || !out || cap <= 2) return;
    s = self->handle->Schema;
    n = snprintf(out, (size_t)cap, "{");
    for (i = 0; i < fieldCount && s && i < s->FieldCount && n < cap - 2; i++) {
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0) {
            char esc[512];
            const char* t = fields[i].text ? fields[i].text : (fields[i].textOut ? fields[i].textOut : "");
            engine_json_escape(esc, (int)sizeof(esc), t);
            n += snprintf(out + n, (size_t)(cap - n), "%s\"%s\":\"%s\"", i ? "," : "", s->Fields[i].Name, esc);
        } else {
            n += snprintf(out + n, (size_t)(cap - n), "%s\"%s\":%lld", i ? "," : "", s->Fields[i].Name, (long long)fields[i].i64);
        }
    }
    if (n < cap - 1) snprintf(out + n, (size_t)(cap - n), "}");
}

static void engine_repl_emit(Engine* self, ReplicationOp op, const IsamFieldValue* fields, int fieldCount)
{
    char json[1024], line[1400];
    int64_t recNo;
    if (!self || !self->repl) return;
    recNo = Engine_GetCurrentRecordNumber(self);
    if (recNo <= 0) return;
    json[0] = 0;
    if (fields) engine_repl_json(self, fields, fieldCount, json, (int)sizeof(json));
    ReplicationLogLine_Format(op, recNo, fields ? json : NULL, line, (int)sizeof(line));
    IsamReplicationLog_WriteLine(self->repl, line);
}

int Engine_Append(Engine* self, const IsamFieldValue* fields, int fieldCount)
{
    int64_t newOffset;
    RecordMetadata meta;
    uint8_t* key;
    int keyLen;
    if (!self || !fields) return GMB_DB_ERR;
    newOffset = IsamFileHandle_AllocateNewRecordOffsetCore(self->handle);
    if (newOffset < 0) return GMB_DB_ERR;
    memset(&meta, 0, sizeof(meta));
    meta.Status = Config_RecordStatusActive;
    meta.Version = engine_utc_ticks();
    meta.TransactionId = 0;
    meta.PreviousVersionOffset = -1;
    if (RecordMarshaller_WriteRecordWithMetadata(self->handle, newOffset, fields, fieldCount, &meta) != GMB_DB_OK)
        return GMB_DB_ERR;
    keyLen = self->handle->Schema->PrimaryKey.KeyLength;
    key = (uint8_t*)malloc((size_t)keyLen);
    if (!key) return GMB_DB_ERR;
    if (engine_pk_key(self, fields, key) != GMB_DB_OK) { free(key); return GMB_DB_ERR; }
    if (BPlusTreeManager_Insert(self->primary, key, keyLen, newOffset) != GMB_DB_OK) {
        free(key);
        return GMB_DB_ERR;
    }
    free(key);
    if (engine_secondary_mutate(self, fields, newOffset, 0) != GMB_DB_OK)
        return GMB_DB_ERR;
    engine_set_cursor(self, newOffset);
    /* auto-commit IntraProcess: WAL skip; flush header+record+B+ */
    if (IsamFileHandle_Flush(self->handle) != GMB_DB_OK) return GMB_DB_ERR;
    engine_repl_emit(self, ReplicationOp_Append, fields, fieldCount);
    return GMB_DB_OK;
}

int Engine_Go(Engine* self, CursorMoveOperation operation)
{
    int rc = engine_go_raw(self, operation);
    if (rc != GMB_DB_OK) return rc;
    if (self->filterExpr[0] && !self->eof)
        engine_skip_filter_internal(self, operation == CursorMoveOperation_Top ? 1 : -1);
    return GMB_DB_OK;
}

static int engine_go_raw(Engine* self, CursorMoveOperation operation)
{
    int64_t off;
    if (!self) return GMB_DB_ERR;
    self->lastReadVersion = -1;
    self->eof = 0;
    self->bof = 0;
    off = operation == CursorMoveOperation_Top
        ? BPlusTreeManager_GetFirstRecordOffset(self->active)
        : BPlusTreeManager_GetLastRecordOffset(self->active);
    if (off < 0) {
        self->eof = 1;
        self->bof = 1;
        self->currentOffset = -1;
        return GMB_DB_OK;
    }
    engine_set_cursor(self, off);
    return GMB_DB_OK;
}

int Engine_Skip(Engine* self, int direction)
{
    int rc;
    if (!self || direction == 0) return GMB_DB_OK;
    rc = engine_step(self, direction);
    if (rc != GMB_DB_OK) return rc;
    if (self->filterExpr[0] && !self->eof && !self->bof)
        engine_skip_filter_internal(self, direction);
    return GMB_DB_OK;
}

static int engine_step(Engine* self, int direction)
{
    int64_t newOff;
    int dir;
    if (!self || direction == 0) return GMB_DB_OK;
    if ((self->eof && direction > 0) || (self->bof && direction < 0)) return GMB_DB_OK;
    if (self->eof && direction < 0) self->eof = 0;
    if (self->bof && direction > 0) self->bof = 0;
    dir = direction > 0 ? 1 : -1;
    if (self->currentOffset < 0) return engine_go_raw(self, dir > 0 ? CursorMoveOperation_Top : CursorMoveOperation_Bottom);
    newOff = BPlusTreeManager_FindNextRecordOffset(self->active, self->currentOffset, dir);
    if (newOff < 0) {
        if (dir > 0) { self->eof = 1; self->currentOffset = -1; }
        else { self->bof = 1; self->currentOffset = -1; }
        self->lastReadVersion = -1;
        return GMB_DB_OK;
    }
    engine_set_cursor(self, newOff);
    return GMB_DB_OK;
}

int Engine_Seek(Engine* self, const uint8_t* key, int keyLen)
{
    int64_t off = -1;
    int n = 0, need, isText = 1, i;
    const IsamIndexDefinition* def;
    const IsamSchema* s;
    const uint8_t* seekKey = key;
    uint8_t* padded = NULL;
    int rc;

    if (!self || !key || keyLen < 0) return GMB_DB_ERR;
    self->handle->FoundFlag = 0;
    self->lastReadVersion = -1;
    def = engine_active_def(self);
    need = def ? def->KeyLength : keyLen;
    s = self->handle->Schema;
    if (def && s) {
        const char* expr = def->KeyExpression[0] ? def->KeyExpression : def->Name;
        for (i = 0; i < s->FieldCount; i++) {
            if (gmb_strcasecmp(s->Fields[i].Name, expr) != 0) continue;
            isText = strcmp(s->Fields[i].DataTypeCode, "StringType") == 0;
            break;
        }
    }
    /* "T2" (2 byte) ≠ "T2        " (KeyLength) → NOT FOUND. Il motore completa
     * gli spazi (testo, UPPER) o gli zeri (numerico) come SerializeKeyValue. */
    if (need > 0 && keyLen != need) {
        char tmp[256];
        int copy = keyLen < (int)sizeof(tmp) - 1 ? keyLen : (int)sizeof(tmp) - 1;
        padded = (uint8_t*)malloc((size_t)need);
        if (!padded) return GMB_DB_ERR;
        memcpy(tmp, key, (size_t)copy);
        tmp[copy] = 0;
        while (copy > 0 && isspace((unsigned char)tmp[copy - 1])) tmp[--copy] = 0;
        rc = isText
            ? SchemaManager_SerializeKeyValueText(tmp, need, padded)
            : SchemaManager_SerializeKeyValueI64(atoll(tmp), need, padded);
        if (rc != GMB_DB_OK) { free(padded); return rc; }
        seekKey = padded;
        keyLen = need;
    }
    rc = BPlusTreeManager_Search(self->active, seekKey, keyLen, &off, 1, &n);
    free(padded);
    if (rc != GMB_DB_OK) return GMB_DB_ERR;
    if (n > 0 && off >= 0) {
        self->handle->FoundFlag = 1;
        engine_set_cursor(self, off);
        return GMB_DB_OK;
    }
    self->currentOffset = -1;
    self->eof = 1;
    self->bof = 0;
    return GMB_DB_OK;
}

int Engine_SeekI64(Engine* self, int64_t keyValue)
{
    uint8_t* key;
    int keyLen, rc;
    if (!self) return GMB_DB_ERR;
    keyLen = self->handle->Schema->PrimaryKey.KeyLength;
    key = (uint8_t*)malloc((size_t)keyLen);
    if (!key) return GMB_DB_ERR;
    if (SchemaManager_SerializeKeyValueI64(keyValue, keyLen, key) != GMB_DB_OK) { free(key); return GMB_DB_ERR; }
    rc = Engine_Seek(self, key, keyLen);
    free(key);
    return rc;
}

int Engine_SeekText(Engine* self, const char* text)
{
    const IsamIndexDefinition* def;
    uint8_t* key;
    int keyLen, rc;
    if (!self || !text) return GMB_DB_ERR;
    def = engine_active_def(self);
    if (!def) return GMB_DB_ERR;
    keyLen = def->KeyLength;
    key = (uint8_t*)malloc((size_t)keyLen);
    if (!key) return GMB_DB_ERR;
    if (SchemaManager_SerializeKeyValueText(text, keyLen, key) != GMB_DB_OK) {
        free(key);
        return GMB_DB_ERR;
    }
    rc = Engine_Seek(self, key, keyLen);
    free(key);
    return rc;
}

int Engine_GetCurrent(Engine* self, IsamFieldValue* fields, int fieldCount, RecordMetadata* meta)
{
    int recSize, rc;
    uint8_t* buf;
    RecordMetadata local;
    if (!self || self->currentOffset < 0) return GMB_DB_ERR;
    recSize = self->handle->Header.RecordSize;
    buf = (uint8_t*)malloc((size_t)recSize);
    if (!buf) return GMB_DB_ERR;
    if (IsamFileHandle_ReadRecordBytes(self->handle, self->currentOffset, buf, recSize) != GMB_DB_OK) {
        free(buf);
        return GMB_DB_ERR;
    }
    rc = RecordMarshaller_DeserializeFromBuffer(self->handle->Schema, buf, recSize,
                                                meta ? meta : &local, fields, fieldCount);
    if (rc == GMB_DB_OK)
        self->lastReadVersion = (meta ? meta : &local)->Version;
    free(buf);
    return rc;
}

int Engine_Put(Engine* self, const IsamFieldValue* fields, int fieldCount)
{
    RecordMetadata current;
    RecordMetadata newMeta;
    IsamFieldValue* oldFields = NULL;
    uint8_t *oldKey = NULL, *newKey = NULL;
    int64_t undoOff;
    int keyLen, i, rc = GMB_DB_ERR;
    const IsamSchema* s;

    if (!self || !fields || self->currentOffset < 0 || self->lastReadVersion == -1)
        return GMB_DB_ERR;

    if (IsamFileHandle_ReadRecordMetadata(self->handle, self->currentOffset, &current) != GMB_DB_OK)
        goto done;
    if (current.Status == Config_RecordStatusDeleted)
        goto done;
    if (current.Version != self->lastReadVersion)
        goto done;

    s = self->handle->Schema;
    if (!s || fieldCount != s->FieldCount)
        goto done;

    oldFields = (IsamFieldValue*)calloc((size_t)fieldCount, sizeof(*oldFields));
    if (!oldFields) goto done;
    for (i = 0; i < fieldCount; i++) {
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0) {
            int cap = s->Fields[i].Length + 1;
            if (cap < 2) cap = 2;
            oldFields[i].textOut = (char*)malloc((size_t)cap);
            if (!oldFields[i].textOut) goto done;
            oldFields[i].textOutCap = cap;
        }
    }
    if (Engine_GetCurrent(self, oldFields, fieldCount, &current) != GMB_DB_OK)
        goto done;

    keyLen = s->PrimaryKey.KeyLength;
    oldKey = (uint8_t*)malloc((size_t)keyLen);
    newKey = (uint8_t*)malloc((size_t)keyLen);
    if (!oldKey || !newKey) goto done;
    if (engine_pk_key(self, oldFields, oldKey) != GMB_DB_OK) goto done;
    if (engine_pk_key(self, fields, newKey) != GMB_DB_OK) goto done;
    /* ponytail: B+ Delete 1:1 — PUT con PK diversa toglie la vecchia chiave e inserisce la nuova. */
    if (memcmp(oldKey, newKey, (size_t)keyLen) != 0) {
        if (BPlusTreeManager_Delete(self->primary, oldKey, keyLen, self->currentOffset) != GMB_DB_OK)
            goto done;
        if (BPlusTreeManager_Insert(self->primary, newKey, keyLen, self->currentOffset) != GMB_DB_OK)
            goto done;
    }
    if (engine_secondary_mutate(self, oldFields, self->currentOffset, 1) != GMB_DB_OK) goto done;
    if (engine_secondary_mutate(self, fields, self->currentOffset, 0) != GMB_DB_OK) goto done;

    undoOff = IsamFileHandle_WriteUndoVersion(self->handle, self->currentOffset);
    if (undoOff < 0) goto done;

    memset(&newMeta, 0, sizeof(newMeta));
    newMeta.Status = Config_RecordStatusActive;
    newMeta.Version = engine_utc_ticks();
    newMeta.TransactionId = 0;
    newMeta.PreviousVersionOffset = undoOff;
    if (RecordMarshaller_WriteRecordWithMetadata(self->handle, self->currentOffset,
            fields, fieldCount, &newMeta) != GMB_DB_OK)
        goto done;

    MvccUndoGc_PruneAfterGridWrite(self->handle, self->currentOffset);
    if (IsamFileHandle_Flush(self->handle) != GMB_DB_OK) goto done;
    rc = GMB_DB_OK;
    engine_repl_emit(self, ReplicationOp_Put, fields, fieldCount);

done:
    free(oldKey);
    free(newKey);
    if (oldFields) {
        for (i = 0; i < fieldCount; i++)
            free(oldFields[i].textOut);
        free(oldFields);
    }
    self->lastReadVersion = -1;
    return rc;
}

static IsamFieldValue* engine_alloc_fields(const IsamSchema* s)
{
    IsamFieldValue* fields;
    int i;
    if (!s || s->FieldCount <= 0) return NULL;
    fields = (IsamFieldValue*)calloc((size_t)s->FieldCount, sizeof(*fields));
    if (!fields) return NULL;
    for (i = 0; i < s->FieldCount; i++) {
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") != 0) continue;
        {
            int cap = s->Fields[i].Length + 1;
            if (cap < 2) cap = 2;
            fields[i].textOut = (char*)malloc((size_t)cap);
            if (!fields[i].textOut) {
                int k;
                for (k = 0; k < i; k++) free(fields[k].textOut);
                free(fields);
                return NULL;
            }
            fields[i].textOutCap = cap;
        }
    }
    return fields;
}

static void engine_free_fields(IsamFieldValue* fields, int fieldCount)
{
    int i;
    if (!fields) return;
    for (i = 0; i < fieldCount; i++) free(fields[i].textOut);
    free(fields);
}

static const IsamIndexDefinition* engine_active_def(const Engine* self)
{
    int i;
    if (!self || !self->handle || !self->handle->Schema) return NULL;
    if (self->active == self->primary) return &self->handle->Schema->PrimaryKey;
    for (i = 0; i < self->secondaryCount; i++) {
        if (self->secondary[i] == self->active && self->handle->Schema->SecondaryKeys)
            return &self->handle->Schema->SecondaryKeys[i];
    }
    return &self->handle->Schema->PrimaryKey;
}

static int engine_filter_get(void* ctx, const char* name, char* out, int cap)
{
    Engine* self = (Engine*)ctx;
    IsamFieldValue* fields;
    const IsamSchema* s;
    int i, n, len;
    if (!self || !name || !out || cap <= 0) return GMB_DB_ERR;
    out[0] = 0;
    s = self->handle->Schema;
    n = s->FieldCount;
    fields = engine_alloc_fields(s);
    if (!fields) return GMB_DB_ERR;
    if (Engine_GetCurrent(self, fields, n, NULL) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    engine_fields_text_from_out(fields, n);
    for (i = 0; i < n; i++) {
        if (gmb_strcasecmp(s->Fields[i].Name, name) != 0) continue;
        if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0) {
            const char* t = fields[i].text ? fields[i].text : "";
            snprintf(out, (size_t)cap, "%s", t);
            len = (int)strlen(out);
            while (len > 0 && isspace((unsigned char)out[len - 1])) out[--len] = 0;
        } else {
            snprintf(out, (size_t)cap, "%lld", (long long)fields[i].i64);
        }
        engine_free_fields(fields, n);
        return GMB_DB_OK;
    }
    engine_free_fields(fields, n);
    return GMB_DB_ERR;
}

static int engine_evaluate_filter(Engine* self)
{
    if (!self || !self->filterExpr[0] || self->bof || self->eof) return 1;
    return RuntimeExpressionEvaluator_Eval(self->filterExpr, engine_filter_get, self);
}

static int engine_record_valid_for_filter(Engine* self)
{
    RecordMetadata meta;
    if (!self || self->bof || self->eof || self->currentOffset < 0) return 0;
    if (IsamFileHandle_ReadRecordMetadata(self->handle, self->currentOffset, &meta) != GMB_DB_OK)
        return 0;
    return meta.Status != Config_RecordStatusDeleted && engine_evaluate_filter(self);
}

static void engine_skip_filter_internal(Engine* self, int direction)
{
    int maxIterations = 100000;
    int dir = direction > 0 ? 1 : -1;
    while (!self->eof && !self->bof && maxIterations-- > 0) {
        if (engine_evaluate_filter(self)) break;
        engine_step(self, dir);
    }
}

static void engine_clear_filter_pushdown(Engine* self)
{
    free(self->pushdownKey);
    self->pushdownKey = NULL;
    self->pushdownKeyLen = 0;
    self->pushdownMatchLength = -1;
}

static int engine_current_active_key(Engine* self, uint8_t* key)
{
    IsamFieldValue* fields;
    const IsamIndexDefinition* def;
    int n, rc;
    def = engine_active_def(self);
    if (!def) return GMB_DB_ERR;
    n = self->handle->Schema->FieldCount;
    fields = engine_alloc_fields(self->handle->Schema);
    if (!fields) return GMB_DB_ERR;
    if (Engine_GetCurrent(self, fields, n, NULL) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    engine_fields_text_from_out(fields, n);
    rc = engine_index_key(self, fields, def, key);
    engine_free_fields(fields, n);
    return rc;
}

static int engine_record_key_matches_pushdown(Engine* self)
{
    uint8_t* current;
    int keyLen, n;
    const IsamIndexDefinition* def;
    if (!self->pushdownKey) return 0;
    def = engine_active_def(self);
    if (!def) return 0;
    keyLen = def->KeyLength;
    current = (uint8_t*)malloc((size_t)keyLen);
    if (!current) return 0;
    if (engine_current_active_key(self, current) != GMB_DB_OK) {
        free(current);
        return 0;
    }
    n = self->pushdownMatchLength;
    if (n < 0) n = self->pushdownKeyLen < keyLen ? self->pushdownKeyLen : keyLen;
    if (n > keyLen) n = keyLen;
    if (n > self->pushdownKeyLen) n = self->pushdownKeyLen;
    keyLen = memcmp(current, self->pushdownKey, (size_t)n) == 0;
    free(current);
    return keyLen;
}

static int engine_seek_lower_bound(Engine* self, const uint8_t* key, int keyLen)
{
    int64_t off = -1;
    int n = 0;
    if (!self || !key) return GMB_DB_ERR;
    self->handle->FoundFlag = 0;
    self->lastReadVersion = -1;
    if (BPlusTreeManager_SearchLowerBound(self->active, key, keyLen, &off, 1, &n) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (n > 0 && off >= 0) {
        self->handle->FoundFlag = 1;
        engine_set_cursor(self, off);
        return GMB_DB_OK;
    }
    self->currentOffset = -1;
    self->eof = 1;
    self->bof = 0;
    return GMB_DB_OK;
}

static void engine_position_filter_pushdown_top(Engine* self)
{
    self->lastFilterUsedSeekPushdown = 1;
    if (self->pushdownMatchLength > 0)
        engine_seek_lower_bound(self, self->pushdownKey, self->pushdownKeyLen);
    else
        Engine_Seek(self, self->pushdownKey, self->pushdownKeyLen);
    if (!Engine_WasFound(self)) {
        self->eof = 1;
        self->bof = 0;
        return;
    }
    while (!self->bof && !self->eof) {
        if (!engine_record_key_matches_pushdown(self)) {
            self->eof = 1;
            self->currentOffset = -1;
            self->lastReadVersion = -1;
            return;
        }
        if (engine_record_valid_for_filter(self)) return;
        engine_step(self, 1);
    }
}

static void engine_try_setup_filter_pushdown(Engine* self)
{
    IsamFilterEquality eqs[IsamFilterPushdown_MaxEq];
    char keyFields[IsamFilterPushdown_MaxEq][IsamFilterPushdown_NameCap];
    const IsamIndexDefinition* def;
    const IsamSchema* s;
    int nEq = 0, nFields = 0, matchCount, eqi, fi, keyLen;
    uint8_t* key;

    engine_clear_filter_pushdown(self);
    if (!self->filterExpr[0]) return;
    if (!IsamFilterPushdown_TryCollectEqualities(self->filterExpr, eqs, IsamFilterPushdown_MaxEq, &nEq))
        return;
    def = engine_active_def(self);
    if (!def) return;
    if (!IsamFilterPushdown_TryGetKeyFieldNames(def->KeyExpression, keyFields, IsamFilterPushdown_MaxEq, &nFields))
        return;
    matchCount = IsamFilterPushdown_CountMatchingPrefixFields(keyFields, nFields, eqs, nEq);
    if (matchCount == 0) return;
    eqi = IsamFilterPushdown_FindEquality(eqs, nEq, keyFields[0]);
    if (eqi < 0) return;
    s = self->handle->Schema;
    keyLen = def->KeyLength;
    key = (uint8_t*)malloc((size_t)keyLen);
    if (!key) return;
    fi = 0;
    for (; fi < s->FieldCount; fi++) {
        if (gmb_strcasecmp(s->Fields[fi].Name, keyFields[0]) == 0) break;
    }
    if (fi >= s->FieldCount) {
        free(key);
        return;
    }
    if (strcmp(s->Fields[fi].DataTypeCode, "StringType") == 0) {
        if (SchemaManager_SerializeKeyValueText(eqs[eqi].Value, keyLen, key) != GMB_DB_OK) {
            free(key);
            return;
        }
    } else {
        if (SchemaManager_SerializeKeyValueI64(atoll(eqs[eqi].Value), keyLen, key) != GMB_DB_OK) {
            free(key);
            return;
        }
    }
    self->pushdownKey = key;
    self->pushdownKeyLen = keyLen;
    self->pushdownMatchLength = IsamFilterPushdown_ComputeMatchLength(
        eqs[eqi].Value, matchCount == nFields);
}

static void engine_go_internal(Engine* self, CursorMoveOperation operation)
{
    self->lastFilterUsedSeekPushdown = 0;
    if (operation == CursorMoveOperation_Top && self->pushdownKey) {
        engine_position_filter_pushdown_top(self);
        return;
    }
    Engine_Go(self, operation);
    {
        int dir = operation == CursorMoveOperation_Top ? 1 : -1;
        while (!self->bof && !self->eof && !engine_record_valid_for_filter(self))
            engine_step(self, dir);
    }
}

int Engine_SetFilter(Engine* self, const char* expression)
{
    if (!self) return GMB_DB_ERR;
    if (!expression || !*expression) self->filterExpr[0] = 0;
    else snprintf(self->filterExpr, sizeof(self->filterExpr), "%s", expression);
    engine_clear_filter_pushdown(self);
    engine_try_setup_filter_pushdown(self);
    engine_go_internal(self, CursorMoveOperation_Top);
    return GMB_DB_OK;
}

int Engine_LastFilterUsedSeekPushdown(const Engine* self)
{
    return self ? self->lastFilterUsedSeekPushdown : 0;
}

static void engine_fields_text_from_out(IsamFieldValue* fields, int fieldCount)
{
    int i;
    for (i = 0; i < fieldCount; i++) {
        if (fields[i].textOut && !fields[i].text)
            fields[i].text = fields[i].textOut;
    }
}

int Engine_DeleteCurrent(Engine* self)
{
    RecordMetadata current, newMeta;
    IsamFieldValue* fields;
    int64_t undoOff;
    int n, rc = GMB_DB_ERR;

    if (!self || self->currentOffset < 0) return GMB_DB_OK;
    if (IsamFileHandle_ReadRecordMetadata(self->handle, self->currentOffset, &current) != GMB_DB_OK)
        goto done;
    if (current.Status == Config_RecordStatusDeleted)
        goto done;

    n = self->handle->Schema->FieldCount;
    fields = engine_alloc_fields(self->handle->Schema);
    if (!fields) goto done;
    if (Engine_GetCurrent(self, fields, n, &current) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        goto done;
    }
    engine_fields_text_from_out(fields, n);

    undoOff = IsamFileHandle_WriteUndoVersion(self->handle, self->currentOffset);
    if (undoOff < 0) { engine_free_fields(fields, n); goto done; }

    memset(&newMeta, 0, sizeof(newMeta));
    newMeta.Status = Config_RecordStatusDeleted;
    newMeta.Version = engine_utc_ticks();
    newMeta.TransactionId = 0;
    newMeta.PreviousVersionOffset = undoOff;
    if (RecordMarshaller_WriteRecordWithMetadata(self->handle, self->currentOffset, fields, n, &newMeta) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        goto done;
    }
    if (engine_secondary_mutate(self, fields, self->currentOffset, 1) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        goto done;
    }
    engine_free_fields(fields, n);

    self->handle->Header.ActiveRecordCount -= 1;
    if (IsamFileHandle_UpdateHeader(self->handle) != GMB_DB_OK) goto done;
    if (IsamFileHandle_Flush(self->handle) != GMB_DB_OK) goto done;
    rc = GMB_DB_OK;
    engine_repl_emit(self, ReplicationOp_Delete, NULL, 0);

done:
    self->currentOffset = -1;
    self->lastReadVersion = -1;
    return rc;
}

int Engine_RecallCurrent(Engine* self)
{
    RecordMetadata current, newMeta;
    IsamFieldValue* fields;
    int64_t undoOff;
    int n;

    if (!self || self->currentOffset < 0) return GMB_DB_OK;
    if (IsamFileHandle_ReadRecordMetadata(self->handle, self->currentOffset, &current) != GMB_DB_OK)
        return GMB_DB_ERR;
    if (current.Status == Config_RecordStatusActive) return GMB_DB_OK;

    n = self->handle->Schema->FieldCount;
    fields = engine_alloc_fields(self->handle->Schema);
    if (!fields) return GMB_DB_ERR;
    if (Engine_GetCurrent(self, fields, n, &current) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    engine_fields_text_from_out(fields, n);

    undoOff = IsamFileHandle_WriteUndoVersion(self->handle, self->currentOffset);
    if (undoOff < 0) { engine_free_fields(fields, n); return GMB_DB_ERR; }

    memset(&newMeta, 0, sizeof(newMeta));
    newMeta.Status = Config_RecordStatusActive;
    newMeta.Version = engine_utc_ticks();
    newMeta.TransactionId = 0;
    newMeta.PreviousVersionOffset = undoOff;
    if (RecordMarshaller_WriteRecordWithMetadata(self->handle, self->currentOffset, fields, n, &newMeta) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    if (engine_secondary_mutate(self, fields, self->currentOffset, 0) != GMB_DB_OK) {
        engine_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    engine_free_fields(fields, n);

    self->handle->Header.ActiveRecordCount += 1;
    if (IsamFileHandle_UpdateHeader(self->handle) != GMB_DB_OK) return GMB_DB_ERR;
    if (IsamFileHandle_Flush(self->handle) != GMB_DB_OK) return GMB_DB_ERR;
    engine_repl_emit(self, ReplicationOp_Recall, NULL, 0);
    return GMB_DB_OK;
}

int Engine_Pack(Engine* self)
{
    char orig[1024];
    char tmp[1040];
    const IsamSchema* schema;
    Engine* packed;
    IsamFieldValue* fields;
    RecordMetadata meta;
    int n, keyLen;

    if (!self || !self->handle || !self->handle->Schema) return GMB_DB_ERR;
    schema = self->handle->Schema;
    n = schema->FieldCount;
    keyLen = schema->PrimaryKey.KeyLength;
    snprintf(orig, sizeof(orig), "%s", self->handle->FilePath);
    snprintf(tmp, sizeof(tmp), "%s.pack", orig);

    if (IsamFileHandle_CreateNew(tmp, schema) != GMB_DB_OK) return GMB_DB_ERR;
    packed = Engine_Open(tmp, schema);
    if (!packed) { unlink(tmp); return GMB_DB_ERR; }

    fields = engine_alloc_fields(schema);
    if (!fields) { Engine_Dispose(packed); unlink(tmp); return GMB_DB_ERR; }

    if (Engine_Go(self, CursorMoveOperation_Top) != GMB_DB_OK) goto fail;
    while (!Engine_IsEof(self)) {
        if (Engine_GetCurrent(self, fields, n, &meta) != GMB_DB_OK) goto fail;
        if (meta.Status == Config_RecordStatusActive) {
            engine_fields_text_from_out(fields, n);
            if (Engine_Append(packed, fields, n) != GMB_DB_OK) goto fail;
        }
        if (Engine_Skip(self, 1) != GMB_DB_OK) goto fail;
    }
    engine_free_fields(fields, n);
    fields = NULL;
    Engine_Dispose(packed);
    packed = NULL;

    engine_dispose_secondaries(self);
    BPlusTreeManager_Dispose(self->primary);
    IsamFileHandle_Dispose(self->handle);
    self->primary = NULL;
    self->handle = NULL;
    self->active = NULL;
    if (rename(tmp, orig) != 0) { unlink(tmp); return GMB_DB_ERR; }

    self->handle = IsamFileHandle_Open(orig, schema);
    if (!self->handle) return GMB_DB_ERR;
    self->primary = BPlusTreeManager_NewOnHandle(self->handle, keyLen, schema->PrimaryKey.IsUnique);
    if (!self->primary) {
        IsamFileHandle_Dispose(self->handle);
        self->handle = NULL;
        return GMB_DB_ERR;
    }
    if (engine_attach_secondaries(self) != GMB_DB_OK) {
        BPlusTreeManager_Dispose(self->primary);
        IsamFileHandle_Dispose(self->handle);
        self->primary = NULL;
        self->handle = NULL;
        return GMB_DB_ERR;
    }
    self->active = self->primary;
    self->currentOffset = -1;
    self->lastReadVersion = -1;
    self->eof = 1;
    self->bof = 1;
    self->handle->FoundFlag = 0;
    return GMB_DB_OK;

fail:
    engine_free_fields(fields, n);
    if (packed) Engine_Dispose(packed);
    unlink(tmp);
    return GMB_DB_ERR;
}

int Engine_IsEof(const Engine* self) { return self ? self->eof : 1; }
int Engine_IsBof(const Engine* self) { return self ? self->bof : 1; }
int Engine_WasFound(const Engine* self) { return self && self->handle ? self->handle->FoundFlag : 0; }

int Engine_SetOrder(Engine* self, const char* indexName)
{
    const IsamSchema* s;
    int i;
    if (!self || !self->handle || !self->handle->Schema) return GMB_DB_ERR;
    s = self->handle->Schema;
    if (!indexName || indexName[0] == '\0'
        || gmb_strcasecmp(indexName, s->PrimaryKey.Name) == 0) {
        self->active = self->primary;
        return GMB_DB_OK;
    }
    for (i = 0; i < self->secondaryCount; i++) {
        if (gmb_strcasecmp(s->SecondaryKeys[i].Name, indexName) == 0) {
            self->active = self->secondary[i];
            return GMB_DB_OK;
        }
    }
    return GMB_DB_ERR;
}

int Engine_CreateIndex(Engine* self, const char* indexName, const char* keyExpression, int keyLength)
{
    IsamSchema* schema;
    IsamIndexDefinition* def;
    BPlusTreeManager* mgr = NULL;
    IsamFieldValue* fields = NULL;
    uint8_t *schemaBuf = NULL, *recBuf = NULL, *key = NULL;
    int schemaLen = 0, slot = -1, i, n = 0, recSize, rc = GMB_DB_ERR;
    int64_t lastRec, dataStart;

    if (!self || !indexName || !keyExpression || keyLength <= 0 || !self->handle)
        return GMB_DB_ERR;
    schema = (IsamSchema*)(void*)self->handle->Schema;
    if (!schema || !schema->SecondaryKeys) return GMB_DB_ERR;
    if (gmb_strcasecmp(indexName, schema->PrimaryKey.Name) == 0) return GMB_DB_ERR;
    for (i = 0; i < schema->SecondaryKeyCount; i++) {
        if (gmb_strcasecmp(schema->SecondaryKeys[i].Name, indexName) == 0)
            return GMB_DB_ERR;
    }
    for (i = 0; i < 15; i++) {
        if (self->handle->Header.SecondaryIndexRootOffsets[i] == -1) { slot = i; break; }
    }
    if (slot < 0 || schema->SecondaryKeyCount != slot || schema->SecondaryKeyCount >= 15)
        return GMB_DB_ERR;

    def = &schema->SecondaryKeys[schema->SecondaryKeyCount];
    memset(def, 0, sizeof(*def));
    snprintf(def->Name, sizeof(def->Name), "%s", indexName);
    snprintf(def->KeyExpression, sizeof(def->KeyExpression), "%s", keyExpression);
    def->KeyLength = keyLength;
    schema->SecondaryKeyCount++;

    if (SchemaManager_SerializeSchema(schema, &schemaBuf, &schemaLen) != GMB_DB_OK) goto fail;
    if (IsamFileHandle_EnsureSchemaSlot(self->handle, schemaLen) != GMB_DB_OK) goto fail;
    if (self->handle->Header.SchemaLength < schemaLen)
        self->handle->Header.SchemaLength = schemaLen;
    self->handle->Header.SecondaryIndexRootOffsets[slot] = -1;
    if (IsamFileHandle_UpdateHeader(self->handle) != GMB_DB_OK) goto fail;
    if (fseek(self->handle->fs, (long)self->handle->Header.SchemaOffset, SEEK_SET) != 0) goto fail;
    if ((int)fwrite(schemaBuf, 1, (size_t)schemaLen, self->handle->fs) != schemaLen) goto fail;
    fflush(self->handle->fs);
    free(schemaBuf);
    schemaBuf = NULL;

    mgr = BPlusTreeManager_NewOnHandleAt(self->handle, keyLength, 0, slot);
    if (!mgr) goto fail;
    self->secondary[slot] = mgr;
    self->secondaryCount = schema->SecondaryKeyCount;

    n = schema->FieldCount;
    recSize = self->handle->Header.RecordSize;
    lastRec = self->handle->Header.LastRecNumber;
    dataStart = self->handle->dataAreaStart;
    recBuf = (uint8_t*)malloc((size_t)recSize);
    key = (uint8_t*)malloc((size_t)keyLength);
    fields = engine_alloc_fields(schema);
    if (!recBuf || !key || !fields) goto fail;
    for (i = 0; i < lastRec; i++) {
        int64_t off = dataStart + (int64_t)i * recSize;
        RecordMetadata meta;
        if (IsamFileHandle_ReadRecordMetadata(self->handle, off, &meta) != GMB_DB_OK) goto fail;
        if (meta.Status != Config_RecordStatusActive) continue;
        if (IsamFileHandle_ReadRecordBytes(self->handle, off, recBuf, recSize) != GMB_DB_OK) goto fail;
        if (RecordMarshaller_DeserializeFromBuffer(schema, recBuf, recSize, &meta, fields, n) != GMB_DB_OK)
            goto fail;
        engine_fields_text_from_out(fields, n);
        if (engine_index_key(self, fields, def, key) != GMB_DB_OK) goto fail;
        if (BPlusTreeManager_Insert(mgr, key, keyLength, off) != GMB_DB_OK) goto fail;
    }
    rc = GMB_DB_OK;
    goto done;

fail:
    schema->SecondaryKeyCount--;
    if (mgr) {
        if (slot >= 0 && self->secondary[slot] == mgr) {
            self->secondary[slot] = NULL;
            self->secondaryCount = schema->SecondaryKeyCount;
        }
        BPlusTreeManager_Dispose(mgr);
    }
done:
    free(schemaBuf);
    free(recBuf);
    free(key);
    engine_free_fields(fields, n);
    return rc;
}

int Engine_DropIndex(Engine* self, const char* indexName)
{
    IsamSchema* schema;
    BPlusTreeManager* mgr;
    uint8_t* schemaBuf = NULL;
    int schemaLen = 0, pos = -1, i;

    if (!self || !indexName || !self->handle) return GMB_DB_ERR;
    schema = (IsamSchema*)(void*)self->handle->Schema;
    if (!schema) return GMB_DB_ERR;
    if (gmb_strcasecmp(indexName, schema->PrimaryKey.Name) == 0) return GMB_DB_ERR;
    for (i = 0; i < schema->SecondaryKeyCount; i++) {
        if (gmb_strcasecmp(schema->SecondaryKeys[i].Name, indexName) == 0) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return GMB_DB_ERR;

    mgr = (pos < self->secondaryCount) ? self->secondary[pos] : NULL;
    if (mgr) BPlusTreeManager_AddTreeToFreeList(mgr);
    if (self->active == mgr) self->active = self->primary;

    self->handle->Header.SecondaryIndexRootOffsets[pos] = -1;
    for (i = pos; i < schema->SecondaryKeyCount - 1; i++)
        schema->SecondaryKeys[i] = schema->SecondaryKeys[i + 1];
    schema->SecondaryKeyCount--;
    if (mgr) {
        BPlusTreeManager_Dispose(mgr);
        for (i = pos; i < self->secondaryCount - 1; i++)
            self->secondary[i] = self->secondary[i + 1];
        if (self->secondaryCount > 0) {
            self->secondary[self->secondaryCount - 1] = NULL;
            self->secondaryCount--;
        }
    }

    if (SchemaManager_SerializeSchema(schema, &schemaBuf, &schemaLen) != GMB_DB_OK)
        return GMB_DB_ERR;
    /* Non ridurre SchemaLength: l'area dati non torna indietro (1:1 C# DropIndexAsync). */
    if (self->handle->Header.SchemaLength < schemaLen)
        self->handle->Header.SchemaLength = schemaLen;
    if (IsamFileHandle_UpdateHeader(self->handle) != GMB_DB_OK) {
        free(schemaBuf);
        return GMB_DB_ERR;
    }
    if (fseek(self->handle->fs, (long)self->handle->Header.SchemaOffset, SEEK_SET) != 0) {
        free(schemaBuf);
        return GMB_DB_ERR;
    }
    if ((int)fwrite(schemaBuf, 1, (size_t)schemaLen, self->handle->fs) != schemaLen) {
        free(schemaBuf);
        return GMB_DB_ERR;
    }
    free(schemaBuf);
    return IsamFileHandle_Flush(self->handle);
}

int64_t Engine_GetCurrentRecordNumber(const Engine* self)
{
    int64_t start;
    if (!self || self->currentOffset < 0) return 0;
    start = self->handle->dataAreaStart;
    if (self->currentOffset < start) return 0;
    return (self->currentOffset - start) / self->handle->Header.RecordSize + 1;
}

int64_t Engine_GetTotalRecordCount(const Engine* self)
{
    return self ? self->handle->Header.ActiveRecordCount : 0;
}

const IsamSchema* Engine_GetSchema(const Engine* self)
{
    return self && self->handle ? self->handle->Schema : NULL;
}

int Engine_LastJoinExecutionMode(const Engine* self)
{
    return self ? self->lastJoinMode : IsamJoinMode_NestedLoop;
}

int Engine_CreateSnapshot(Engine* self, const char* backupFilePath)
{
    if (!self || !self->handle) return GMB_DB_ERR;
    return SnapshotManager_CreateSnapshot(self->handle, backupFilePath);
}

int Engine_PositionAtRecord(Engine* self, int64_t recNo)
{
    int64_t last, off;
    if (!self || !self->handle) return GMB_DB_ARG;
    last = self->handle->Header.LastRecNumber;
    if (recNo <= 0 || recNo > last) return GMB_DB_ARG;
    off = self->handle->dataAreaStart + (recNo - 1) * self->handle->Header.RecordSize;
    engine_set_cursor(self, off);
    return GMB_DB_OK;
}

int64_t Engine_GetLastPhysicalRecNumber(const Engine* self)
{
    return self && self->handle ? self->handle->Header.LastRecNumber : 0;
}

int Engine_SetReplication(Engine* self, const char* logPath)
{
    char path[1024];
    const char* base;
    if (!self || !self->handle) return GMB_DB_ARG;
    if (self->repl) {
        IsamReplicationLog_Dispose(self->repl);
        self->repl = NULL;
    }
    if (!logPath || !*logPath) return GMB_DB_OK;
    if (strcmp(logPath, ".") == 0) {
        base = strrchr(self->handle->FilePath, '/');
        if (!base) base = strrchr(self->handle->FilePath, '\\');
        snprintf(path, sizeof(path), "%s.repl", base ? base + 1 : self->handle->FilePath);
    } else if (gmb_strcasecmp(logPath, "DEFAULT") == 0) {
        snprintf(path, sizeof(path), "%s.repl", self->handle->FilePath);
    } else {
        snprintf(path, sizeof(path), "%s", logPath);
    }
    self->repl = IsamReplicationLog_New(path);
    return self->repl ? GMB_DB_OK : GMB_DB_ERR;
}

static int join_field_idx(const IsamSchema* s, const char* name)
{
    int i;
    if (!s || !name) return -1;
    for (i = 0; i < s->FieldCount; i++) {
        if (gmb_strcasecmp(s->Fields[i].Name, name) == 0) return i;
    }
    return -1;
}

static int join_is_text(const IsamSchema* s, int i)
{
    return i >= 0 && strcmp(s->Fields[i].DataTypeCode, "StringType") == 0;
}

static int join_try_index(Engine* e, const char* fieldName,
                          const IsamIndexDefinition** def, BPlusTreeManager** mgr)
{
    const IsamSchema* s;
    int i;
    if (!e || !fieldName || !def || !mgr) return 0;
    s = e->handle->Schema;
    if (IsamJoinStrategy_IndexCoversField(&s->PrimaryKey, fieldName)) {
        *def = &s->PrimaryKey;
        *mgr = e->primary;
        return 1;
    }
    for (i = 0; i < e->secondaryCount; i++) {
        if (!s->SecondaryKeys || !e->secondary[i]) continue;
        if (!IsamJoinStrategy_IndexCoversField(&s->SecondaryKeys[i], fieldName)) continue;
        *def = &s->SecondaryKeys[i];
        *mgr = e->secondary[i];
        return 1;
    }
    return 0;
}

static int join_estimate(const Engine* e)
{
    if (!e || !e->handle || e->handle->Header.RecordSize <= 0) return 0;
    return (int)e->handle->Header.LastRecNumber;
}

static void join_copy_val(IsamFieldValue* dst, const IsamFieldValue* src, int isText)
{
    memset(dst, 0, sizeof(*dst));
    if (!src) return;
    if (isText) {
        dst->text = src->text ? src->text : src->textOut;
        dst->textOut = src->textOut;
        dst->textOutCap = src->textOutCap;
    } else {
        dst->i64 = src->i64;
    }
}

static int join_append_merged(Engine* dest, const IsamSchema* destS,
                              const IsamSchema* ls, const IsamFieldValue* lv,
                              const IsamSchema* rs, const IsamFieldValue* rv)
{
    IsamFieldValue* merged;
    int i, rc, li, ri, isText;
    merged = (IsamFieldValue*)calloc((size_t)destS->FieldCount, sizeof(*merged));
    if (!merged) return GMB_DB_ERR;
    for (i = 0; i < destS->FieldCount; i++) {
        isText = join_is_text(destS, i);
        li = join_field_idx(ls, destS->Fields[i].Name);
        if (li >= 0) join_copy_val(&merged[i], &lv[li], isText);
        else {
            ri = join_field_idx(rs, destS->Fields[i].Name);
            if (ri >= 0) join_copy_val(&merged[i], &rv[ri], isText);
        }
    }
    rc = Engine_Append(dest, merged, destS->FieldCount);
    free(merged);
    return rc;
}

static int join_nested(Engine* left, Engine* right, Engine* dest,
                       const char* leftField, const char* rightField)
{
    const IsamSchema *ls = left->handle->Schema, *rs = right->handle->Schema, *ds = dest->handle->Schema;
    IsamFieldValue *lv, *rv;
    int li, ri, lt, rt, rc = GMB_DB_OK;
    li = join_field_idx(ls, leftField);
    ri = join_field_idx(rs, rightField);
    if (li < 0 || ri < 0) return GMB_DB_ERR;
    lt = join_is_text(ls, li);
    rt = join_is_text(rs, ri);
    lv = engine_alloc_fields(ls);
    rv = engine_alloc_fields(rs);
    if (!lv || !rv) { engine_free_fields(lv, ls->FieldCount); engine_free_fields(rv, rs->FieldCount); return GMB_DB_ERR; }
    if (Engine_Go(left, CursorMoveOperation_Top) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    while (!Engine_IsEof(left)) {
        if (Engine_GetCurrent(left, lv, ls->FieldCount, NULL) != GMB_DB_OK) { rc = GMB_DB_ERR; break; }
        engine_fields_text_from_out(lv, ls->FieldCount);
        if (Engine_Go(right, CursorMoveOperation_Top) != GMB_DB_OK) { rc = GMB_DB_ERR; break; }
        while (!Engine_IsEof(right)) {
            if (Engine_GetCurrent(right, rv, rs->FieldCount, NULL) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
            engine_fields_text_from_out(rv, rs->FieldCount);
            if (IsamJoinStrategy_FieldValuesEqual(&lv[li], lt, &rv[ri], rt)) {
                if (join_append_merged(dest, ds, ls, lv, rs, rv) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
            }
            if (Engine_Skip(right, 1) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
        }
        if (Engine_Skip(left, 1) != GMB_DB_OK) { rc = GMB_DB_ERR; break; }
    }
done:
    engine_free_fields(lv, ls->FieldCount);
    engine_free_fields(rv, rs->FieldCount);
    return rc;
}

static int join_index_nl(Engine* left, Engine* right, Engine* dest,
                         const char* probeField, const char* buildField,
                         const IsamIndexDefinition* buildDef, BPlusTreeManager* buildMgr,
                         int buildOnSecondary)
{
    Engine *probe = buildOnSecondary ? left : right;
    Engine *build = buildOnSecondary ? right : left;
    const IsamSchema *ps = probe->handle->Schema, *bs = build->handle->Schema;
    const IsamSchema *ls = left->handle->Schema, *rs = right->handle->Schema, *ds = dest->handle->Schema;
    IsamFieldValue *pv, *bv;
    int pi, bt, rc = GMB_DB_OK;
    uint8_t* key;
    int64_t offs[64];
    int n, k;
    pi = join_field_idx(ps, probeField);
    if (pi < 0 || !buildDef || !buildMgr) return GMB_DB_ERR;
    bt = join_is_text(bs, join_field_idx(bs, buildField));
    key = (uint8_t*)malloc((size_t)buildDef->KeyLength);
    pv = engine_alloc_fields(ps);
    bv = engine_alloc_fields(bs);
    if (!key || !pv || !bv) { rc = GMB_DB_ERR; goto done; }
    if (Engine_Go(probe, CursorMoveOperation_Top) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    while (!Engine_IsEof(probe)) {
        if (Engine_GetCurrent(probe, pv, ps->FieldCount, NULL) != GMB_DB_OK) { rc = GMB_DB_ERR; break; }
        engine_fields_text_from_out(pv, ps->FieldCount);
        if (IsamJoinStrategy_SerializeFieldKey(&pv[pi], bt,
                                               buildDef->KeyLength, key) != GMB_DB_OK) {
            rc = GMB_DB_ERR; break;
        }
        n = 0;
        if (BPlusTreeManager_Search(buildMgr, key, buildDef->KeyLength, offs, 64, &n) != GMB_DB_OK) {
            rc = GMB_DB_ERR; break;
        }
        for (k = 0; k < n; k++) {
            if (offs[k] < 0) continue;
            engine_set_cursor(build, offs[k]);
            if (Engine_GetCurrent(build, bv, bs->FieldCount, NULL) != GMB_DB_OK) continue;
            engine_fields_text_from_out(bv, bs->FieldCount);
            {
                const IsamFieldValue *lv = buildOnSecondary ? pv : bv;
                const IsamFieldValue *rv = buildOnSecondary ? bv : pv;
                if (join_append_merged(dest, ds, ls, lv, rs, rv) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
            }
        }
        if (Engine_Skip(probe, 1) != GMB_DB_OK) { rc = GMB_DB_ERR; break; }
    }
done:
    free(key);
    engine_free_fields(pv, ps->FieldCount);
    engine_free_fields(bv, bs->FieldCount);
    return rc;
}

typedef struct JoinHashNode {
    char key[64];
    IsamFieldValue* fields;
    int n;
    struct JoinHashNode* next;
} JoinHashNode;

static void join_hash_free(JoinHashNode* head, int fieldCount)
{
    while (head) {
        JoinHashNode* n = head->next;
        engine_free_fields(head->fields, fieldCount);
        free(head);
        head = n;
    }
}

static int join_snapshot(const IsamSchema* s, const IsamFieldValue* src, IsamFieldValue** out)
{
    IsamFieldValue* d;
    int i;
    d = engine_alloc_fields(s);
    if (!d) return GMB_DB_ERR;
    for (i = 0; i < s->FieldCount; i++) {
        if (join_is_text(s, i)) {
            const char* t = src[i].text ? src[i].text : (src[i].textOut ? src[i].textOut : "");
            snprintf(d[i].textOut, (size_t)d[i].textOutCap, "%s", t);
            d[i].text = d[i].textOut;
        } else {
            d[i].i64 = src[i].i64;
        }
    }
    *out = d;
    return GMB_DB_OK;
}

static int join_hash(Engine* left, Engine* right, Engine* dest,
                     const char* probeField, const char* buildField, int buildOnSecondary)
{
    Engine *build = buildOnSecondary ? right : left;
    Engine *probe = buildOnSecondary ? left : right;
    const IsamSchema *bs = build->handle->Schema, *ps = probe->handle->Schema;
    const IsamSchema *ls = left->handle->Schema, *rs = right->handle->Schema, *ds = dest->handle->Schema;
    IsamFieldValue *bv, *pv;
    JoinHashNode* head = NULL;
    int bi, pi, bt, pt, rc = GMB_DB_OK, built = 0;
    bi = join_field_idx(bs, buildField);
    pi = join_field_idx(ps, probeField);
    if (bi < 0 || pi < 0) return GMB_DB_ERR;
    bt = join_is_text(bs, bi);
    pt = join_is_text(ps, pi);
    bv = engine_alloc_fields(bs);
    pv = engine_alloc_fields(ps);
    if (!bv || !pv) { rc = GMB_DB_ERR; goto done; }
    if (Engine_Go(build, CursorMoveOperation_Top) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    while (!Engine_IsEof(build)) {
        JoinHashNode* node;
        if (built >= IsamJoinStrategy_HashJoinMaxBuildRecords) { rc = GMB_DB_ERR; goto done; }
        if (Engine_GetCurrent(build, bv, bs->FieldCount, NULL) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
        engine_fields_text_from_out(bv, bs->FieldCount);
        node = (JoinHashNode*)calloc(1, sizeof(*node));
        if (!node) { rc = GMB_DB_ERR; goto done; }
        IsamJoinStrategy_HashKey(&bv[bi], bt, node->key, (int)sizeof(node->key));
        node->n = bs->FieldCount;
        if (join_snapshot(bs, bv, &node->fields) != GMB_DB_OK) { free(node); rc = GMB_DB_ERR; goto done; }
        node->next = head;
        head = node;
        built++;
        if (Engine_Skip(build, 1) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    }
    if (Engine_Go(probe, CursorMoveOperation_Top) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    while (!Engine_IsEof(probe)) {
        char hk[64];
        JoinHashNode* n;
        if (Engine_GetCurrent(probe, pv, ps->FieldCount, NULL) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
        engine_fields_text_from_out(pv, ps->FieldCount);
        IsamJoinStrategy_HashKey(&pv[pi], pt, hk, (int)sizeof(hk));
        for (n = head; n; n = n->next) {
            if (gmb_strcasecmp(n->key, hk) != 0) continue;
            {
                const IsamFieldValue *lv = buildOnSecondary ? pv : n->fields;
                const IsamFieldValue *rv = buildOnSecondary ? n->fields : pv;
                if (join_append_merged(dest, ds, ls, lv, rs, rv) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
            }
        }
        if (Engine_Skip(probe, 1) != GMB_DB_OK) { rc = GMB_DB_ERR; goto done; }
    }
done:
    join_hash_free(head, bs->FieldCount);
    engine_free_fields(bv, bs->FieldCount);
    engine_free_fields(pv, ps->FieldCount);
    return rc;
}

int Engine_JoinWith(Engine* left, Engine* right, const char* destPath,
                    const char* leftField, const char* rightField)
{
    const IsamSchema *ls, *rs;
    IsamFieldSchema* destFields = NULL;
    IsamSchema destSchema;
    Engine* dest = NULL;
    int nL, nR, i, j, rc = GMB_DB_ERR;
    int64_t leftOff, rightOff;
    const IsamIndexDefinition* idxDef = NULL;
    BPlusTreeManager* idxMgr = NULL;
    int mode, buildOnSec, estL, estR;
    char probeField[IsamSchema_NameMax], buildField[IsamSchema_NameMax];

    if (!left || !right || !destPath || !leftField || !rightField) return GMB_DB_ERR;
    ls = left->handle->Schema;
    rs = right->handle->Schema;
    if (!ls || !rs) return GMB_DB_ERR;
    if (join_field_idx(ls, leftField) < 0 || join_field_idx(rs, rightField) < 0) return GMB_DB_ERR;
    if (gmb_strcasecmp(ls->PrimaryKey.Name, rs->PrimaryKey.Name) == 0) return GMB_DB_ERR;
    nL = ls->FieldCount;
    nR = rs->FieldCount;
    destFields = (IsamFieldSchema*)calloc((size_t)(nL + nR), sizeof(*destFields));
    if (!destFields) return GMB_DB_ERR;
    for (i = 0; i < nL; i++) destFields[i] = ls->Fields[i];
    for (j = 0; j < nR; j++) {
        for (i = 0; i < nL; i++) {
            if (gmb_strcasecmp(rs->Fields[j].Name, ls->Fields[i].Name) == 0) {
                free(destFields);
                return GMB_DB_ERR;
            }
        }
        destFields[nL + j] = rs->Fields[j];
    }
    memset(&destSchema, 0, sizeof(destSchema));
    snprintf(destSchema.TypeName, sizeof(destSchema.TypeName), "%s_%s", ls->TypeName, rs->TypeName);
    destSchema.Fields = destFields;
    destSchema.FieldCount = nL + nR;
    snprintf(destSchema.PrimaryKey.Name, sizeof(destSchema.PrimaryKey.Name), "%s", "JoinKey");
    snprintf(destSchema.PrimaryKey.KeyExpression, sizeof(destSchema.PrimaryKey.KeyExpression),
             "COMPOSITE(%s,%s)", ls->PrimaryKey.Name, rs->PrimaryKey.Name);
    destSchema.PrimaryKey.KeyLength = ls->PrimaryKey.KeyLength + rs->PrimaryKey.KeyLength;
    destSchema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(destPath, &destSchema) != GMB_DB_OK) goto done;
    dest = Engine_Open(destPath, &destSchema);
    if (!dest) goto done;

    leftOff = left->currentOffset;
    rightOff = right->currentOffset;
    probeField[0] = 0;
    buildField[0] = 0;
    mode = IsamJoinMode_NestedLoop;
    buildOnSec = 1;
    if (join_try_index(right, rightField, &idxDef, &idxMgr)) {
        mode = IsamJoinMode_IndexNestedLoop;
        buildOnSec = 1;
        snprintf(probeField, sizeof(probeField), "%s", leftField);
        snprintf(buildField, sizeof(buildField), "%s", rightField);
    } else if (join_try_index(left, leftField, &idxDef, &idxMgr)) {
        mode = IsamJoinMode_IndexNestedLoop;
        buildOnSec = 0;
        snprintf(probeField, sizeof(probeField), "%s", rightField);
        snprintf(buildField, sizeof(buildField), "%s", leftField);
    } else {
        estL = join_estimate(left);
        estR = join_estimate(right);
        buildOnSec = estR <= estL;
        {
            int buildN = buildOnSec ? estR : estL;
            if (buildN > 0 && buildN <= IsamJoinStrategy_HashJoinMaxBuildRecords) {
                mode = IsamJoinMode_HashJoin;
                if (buildOnSec) {
                    snprintf(probeField, sizeof(probeField), "%s", leftField);
                    snprintf(buildField, sizeof(buildField), "%s", rightField);
                } else {
                    snprintf(probeField, sizeof(probeField), "%s", rightField);
                    snprintf(buildField, sizeof(buildField), "%s", leftField);
                }
            }
        }
    }
    left->lastJoinMode = mode;
    dest->lastJoinMode = mode;
    if (mode == IsamJoinMode_IndexNestedLoop)
        rc = join_index_nl(left, right, dest, probeField, buildField, idxDef, idxMgr, buildOnSec);
    else if (mode == IsamJoinMode_HashJoin)
        rc = join_hash(left, right, dest, probeField, buildField, buildOnSec);
    else
        rc = join_nested(left, right, dest, leftField, rightField);

    if (leftOff >= 0) engine_set_cursor(left, leftOff);
    else Engine_Go(left, CursorMoveOperation_Top);
    if (rightOff >= 0) engine_set_cursor(right, rightOff);
    else Engine_Go(right, CursorMoveOperation_Top);
done:
    if (dest) Engine_Dispose(dest);
    free(destFields);
    return rc;
}


static void engine_selfcheck_schema(IsamFieldSchema fields[2], IsamIndexDefinition* sec, IsamSchema* schema)
{
    memset(schema, 0, sizeof(*schema));
    memset(fields, 0, sizeof(IsamFieldSchema) * 2);
    memset(sec, 0, sizeof(*sec));
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
    snprintf(sec->Name, sizeof(sec->Name), "%s", "Name");
    snprintf(sec->KeyExpression, sizeof(sec->KeyExpression), "%s", "Name");
    sec->KeyLength = 20;
    sec->IsUnique = 0;
    schema->SecondaryKeys = sec;
    schema->SecondaryKeyCount = 1;
}

int Engine_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_engine_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamIndexDefinition sec;
    IsamSchema schema;
    Engine* e;
    IsamFieldValue vals[2];
    RecordMetadata meta;
    char nameOut[32];
    int i, recSize;
    int64_t recno;
    engine_selfcheck_schema(fields, &sec, &schema);
    recSize = SchemaManager_CalculateRecordSize(&schema);
    if (recSize != RecordMetadata_Size + 2 + 20) return -1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -2;
    e = Engine_Open(path, &schema);
    if (!e) return -3;
    for (i = 1; i <= 5; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -10 - i; }
    }
    if (Engine_GetTotalRecordCount(e) != 5) { Engine_Dispose(e); return -4; }
    if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(e); return -5; }
    for (i = 1; i <= 5; i++) {
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK) {
            Engine_Dispose(e); return -20 - i;
        }
        if (vals[0].i64 != i || meta.Status != Config_RecordStatusActive) { Engine_Dispose(e); return -30 - i; }
        if (i < 5 && Engine_Skip(e, 1) != GMB_DB_OK) { Engine_Dispose(e); return -40 - i; }
    }
    recno = Engine_GetCurrentRecordNumber(e);
    if (recno != 5) { Engine_Dispose(e); return -6; }
    if (Engine_SeekI64(e, 3) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -7; }
    memset(vals, 0, sizeof(vals));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK || vals[0].i64 != 3) {
        Engine_Dispose(e); return -8;
    }
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 3;
    vals[1].text = "PUT3";
    if (Engine_Put(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -60; }
    vals[1].text = "PUT3x";
    if (Engine_Put(e, vals, 2) == GMB_DB_OK) { Engine_Dispose(e); return -61; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK || vals[0].i64 != 3) {
        Engine_Dispose(e); return -62;
    }
    if (strcmp(nameOut, "PUT3") != 0 || meta.PreviousVersionOffset < 0) {
        Engine_Dispose(e); return -63;
    }
    {
        uint8_t* ubuf = (uint8_t*)malloc((size_t)recSize);
        RecordMetadata umeta;
        IsamFieldValue uvals[2];
        char uname[32];
        if (!ubuf) { Engine_Dispose(e); return -64; }
        memset(uvals, 0, sizeof(uvals));
        memset(uname, 0, sizeof(uname));
        uvals[1].textOut = uname;
        uvals[1].textOutCap = (int)sizeof(uname);
        if (IsamFileHandle_ReadRecordBytes(e->handle, meta.PreviousVersionOffset, ubuf, recSize) != GMB_DB_OK
            || RecordMarshaller_DeserializeFromBuffer(&schema, ubuf, recSize, &umeta, uvals, 2) != GMB_DB_OK
            || strcmp(uname, "N3") != 0) {
            free(ubuf);
            Engine_Dispose(e);
            return -64;
        }
        free(ubuf);
    }
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 3;
    vals[1].text = "PUT3b";
    if (Engine_Put(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -65; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK || strcmp(nameOut, "PUT3b") != 0) {
        Engine_Dispose(e); return -66;
    }
    {
        RecordMetadata umeta;
        if (meta.PreviousVersionOffset < 0
            || IsamFileHandle_ReadRecordMetadata(e->handle, meta.PreviousVersionOffset, &umeta) != GMB_DB_OK
            || umeta.PreviousVersionOffset != -1) {
            Engine_Dispose(e); return -67;
        }
    }
    if (e->handle->Header.LastRecNumber != 5) { Engine_Dispose(e); return -68; }
    Engine_Dispose(e);
    e = Engine_Open(path, &schema);
    if (!e) return -9;
    if (e->handle->Header.LastRecNumber != 5) { Engine_Dispose(e); return -50; }
    if (Engine_SeekI64(e, 5) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -51; }
    memset(vals, 0, sizeof(vals));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 5) {
        Engine_Dispose(e); return -52;
    }
    if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(e); return -53; }
    memset(vals, 0, sizeof(vals));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1) {
        Engine_Dispose(e); return -54;
    }
    if (Engine_SeekI64(e, 3) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -69; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || strcmp(nameOut, "PUT3b") != 0) {
        Engine_Dispose(e); return -70;
    }
    if (Engine_DeleteCurrent(e) != GMB_DB_OK) { Engine_Dispose(e); return -80; }
    if (Engine_GetTotalRecordCount(e) != 4) { Engine_Dispose(e); return -81; }
    if (Engine_GetCurrent(e, vals, 2, NULL) == GMB_DB_OK) { Engine_Dispose(e); return -82; }
    if (Engine_SeekI64(e, 3) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -83; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK || meta.Status != Config_RecordStatusDeleted) {
        Engine_Dispose(e); return -84;
    }
    if (Engine_RecallCurrent(e) != GMB_DB_OK) { Engine_Dispose(e); return -85; }
    if (Engine_GetTotalRecordCount(e) != 5) { Engine_Dispose(e); return -86; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK
        || meta.Status != Config_RecordStatusActive
        || strcmp(nameOut, "PUT3b") != 0) {
        Engine_Dispose(e); return -87;
    }
    Engine_Dispose(e);
    e = Engine_Open(path, &schema);
    if (!e) return -88;
    if (Engine_SeekI64(e, 3) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -89; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, &meta) != GMB_DB_OK
        || meta.Status != Config_RecordStatusActive
        || strcmp(nameOut, "PUT3b") != 0) {
        Engine_Dispose(e); return -90;
    }
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 30;
    vals[1].text = "PUT3b";
    if (Engine_Put(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -91; }
    if (Engine_SeekI64(e, 3) != GMB_DB_OK || Engine_WasFound(e)) { Engine_Dispose(e); return -92; }
    if (Engine_SeekI64(e, 30) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -93; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 30) {
        Engine_Dispose(e); return -94;
    }
    Engine_Dispose(e);
    e = Engine_Open(path, &schema);
    if (!e) return -95;
    if (Engine_SeekI64(e, 30) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -96; }
    memset(vals, 0, sizeof(vals));
    memset(nameOut, 0, sizeof(nameOut));
    vals[1].textOut = nameOut;
    vals[1].textOutCap = (int)sizeof(nameOut);
    if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 30
        || strcmp(nameOut, "PUT3b") != 0) {
        Engine_Dispose(e); return -97;
    }
    if (Engine_DeleteCurrent(e) != GMB_DB_OK) { Engine_Dispose(e); return -98; }
    if (Engine_GetTotalRecordCount(e) != 4) { Engine_Dispose(e); return -99; }
    if (Engine_Pack(e) != GMB_DB_OK) { Engine_Dispose(e); return -100; }
    if (e->handle->Header.LastRecNumber != 4 || Engine_GetTotalRecordCount(e) != 4) {
        Engine_Dispose(e); return -101;
    }
    if (Engine_SeekI64(e, 30) != GMB_DB_OK || Engine_WasFound(e)) { Engine_Dispose(e); return -102; }
    if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(e); return -103; }
    {
        int64_t expect[4] = { 1, 2, 4, 5 };
        int i;
        for (i = 0; i < 4; i++) {
            memset(vals, 0, sizeof(vals));
            memset(nameOut, 0, sizeof(nameOut));
            vals[1].textOut = nameOut;
            vals[1].textOutCap = (int)sizeof(nameOut);
            if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != expect[i]) {
                Engine_Dispose(e); return -110 - i;
            }
            if (i < 3 && Engine_Skip(e, 1) != GMB_DB_OK) { Engine_Dispose(e); return -120 - i; }
        }
    }
    Engine_Dispose(e);
    e = Engine_Open(path, &schema);
    if (!e) return -104;
    if (e->handle->Header.LastRecNumber != 4) { Engine_Dispose(e); return -105; }
    if (Engine_SeekI64(e, 5) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -106; }
    if (Engine_SeekI64(e, 30) != GMB_DB_OK || Engine_WasFound(e)) { Engine_Dispose(e); return -107; }
    if (Engine_SetOrder(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -130; }
    {
        uint8_t nameKey[20];
        if (SchemaManager_SerializeKeyValueText("N5", 20, nameKey) != GMB_DB_OK) {
            Engine_Dispose(e); return -131;
        }
        if (Engine_Seek(e, nameKey, 20) != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -132;
        }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 5) {
            Engine_Dispose(e); return -133;
        }
        if (SchemaManager_SerializeKeyValueText("PUT3b", 20, nameKey) != GMB_DB_OK) {
            Engine_Dispose(e); return -134;
        }
        if (Engine_Seek(e, nameKey, 20) != GMB_DB_OK || Engine_WasFound(e)) {
            Engine_Dispose(e); return -135;
        }
    }
    if (Engine_SetOrder(e, "Id") != GMB_DB_OK) { Engine_Dispose(e); return -136; }
    if (Engine_SeekI64(e, 5) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -137; }
    Engine_Dispose(e);

    {
        char cpath[] = "native/gmb_db_c/build/isam_createindex_selfcheck.isdb";
        IsamFieldSchema cfields[2];
        IsamIndexDefinition secSlots[15];
        IsamSchema cschema;
        uint8_t nameKey[20];
        engine_selfcheck_schema(cfields, secSlots, &cschema);
        memset(secSlots, 0, sizeof(secSlots));
        cschema.SecondaryKeys = secSlots;
        cschema.SecondaryKeyCount = 0;
        if (IsamFileHandle_CreateNew(cpath, &cschema) != GMB_DB_OK) return -140;
        e = Engine_Open(cpath, &cschema);
        if (!e) return -141;
        for (i = 1; i <= 5; i++) {
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "N%d", i);
            memset(vals, 0, sizeof(vals));
            vals[0].i64 = i;
            vals[1].text = nbuf;
            if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -142; }
        }
        if (Engine_CreateIndex(e, "Name", "Name", 20) != GMB_DB_OK) { Engine_Dispose(e); return -143; }
        if (Engine_SeekI64(e, 1) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -144; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1) {
            Engine_Dispose(e); return -145;
        }
        if (Engine_SetOrder(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -146; }
        if (SchemaManager_SerializeKeyValueText("N3", 20, nameKey) != GMB_DB_OK) {
            Engine_Dispose(e); return -147;
        }
        if (Engine_Seek(e, nameKey, 20) != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -148;
        }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -149;
        }
        if (Engine_DropIndex(e, "Id") == GMB_DB_OK) { Engine_Dispose(e); return -150; }
        if (Engine_DropIndex(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -151; }
        if (Engine_SetOrder(e, "Name") == GMB_DB_OK) { Engine_Dispose(e); return -152; }
        if (Engine_SetOrder(e, "Id") != GMB_DB_OK) { Engine_Dispose(e); return -153; }
        if (Engine_SeekI64(e, 1) != GMB_DB_OK || !Engine_WasFound(e)) { Engine_Dispose(e); return -154; }
        if (Engine_CreateIndex(e, "Name", "Name", 20) != GMB_DB_OK) { Engine_Dispose(e); return -155; }
        if (Engine_SetOrder(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -156; }
        if (SchemaManager_SerializeKeyValueText("N3", 20, nameKey) != GMB_DB_OK) {
            Engine_Dispose(e); return -157;
        }
        if (Engine_Seek(e, nameKey, 20) != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -158;
        }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -159;
        }
        Engine_Dispose(e);
    }
    {
        char fpath[] = "native/gmb_db_c/build/isam_filter_selfcheck.isdb";
        IsamFieldSchema ffields[2];
        IsamIndexDefinition fsec;
        IsamSchema fschema;
        engine_selfcheck_schema(ffields, &fsec, &fschema);
        if (IsamFileHandle_CreateNew(fpath, &fschema) != GMB_DB_OK) return -160;
        e = Engine_Open(fpath, &fschema);
        if (!e) return -161;
        for (i = 1; i <= 5; i++) {
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "N%d", i);
            memset(vals, 0, sizeof(vals));
            vals[0].i64 = i;
            vals[1].text = nbuf;
            if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -162; }
        }
        if (Engine_SetFilter(e, "Name='N3'") != GMB_DB_OK) { Engine_Dispose(e); return -163; }
        if (Engine_LastFilterUsedSeekPushdown(e)) { Engine_Dispose(e); return -164; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -165;
        }
        if (Engine_Skip(e, 1) != GMB_DB_OK || !Engine_IsEof(e)) { Engine_Dispose(e); return -166; }
        if (Engine_SetFilter(e, NULL) != GMB_DB_OK) { Engine_Dispose(e); return -167; }
        if (Engine_Go(e, CursorMoveOperation_Top) != GMB_DB_OK) { Engine_Dispose(e); return -168; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1) {
            Engine_Dispose(e); return -169;
        }
        if (Engine_SetOrder(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -170; }
        if (Engine_SetFilter(e, "Name='N3'") != GMB_DB_OK) { Engine_Dispose(e); return -171; }
        if (!Engine_LastFilterUsedSeekPushdown(e)) { Engine_Dispose(e); return -172; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -173;
        }
        if (Engine_SetOrder(e, "Id") != GMB_DB_OK) { Engine_Dispose(e); return -174; }
        if (Engine_SetFilter(e, "Id=3") != GMB_DB_OK) { Engine_Dispose(e); return -175; }
        if (!Engine_LastFilterUsedSeekPushdown(e)) { Engine_Dispose(e); return -176; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -177;
        }
        if (Engine_SetFilter(e, "Name='ZZ'") != GMB_DB_OK) { Engine_Dispose(e); return -178; }
        if (!Engine_IsEof(e)) { Engine_Dispose(e); return -179; }
        if (Engine_SetFilter(e, "Name='N3' OR Id=1") != GMB_DB_OK) { Engine_Dispose(e); return -180; }
        if (Engine_LastFilterUsedSeekPushdown(e)) { Engine_Dispose(e); return -181; }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_IsEof(e) || Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 1) {
            Engine_Dispose(e); return -182;
        }
        Engine_Dispose(e);
    }
    {
        char spath[] = "native/gmb_db_c/build/isam_seekpad_selfcheck.isdb";
        IsamFieldSchema sfields[2];
        IsamIndexDefinition ssec;
        IsamSchema sschema;
        engine_selfcheck_schema(sfields, &ssec, &sschema);
        if (IsamFileHandle_CreateNew(spath, &sschema) != GMB_DB_OK) return -190;
        e = Engine_Open(spath, &sschema);
        if (!e) return -191;
        for (i = 1; i <= 5; i++) {
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "N%d", i);
            memset(vals, 0, sizeof(vals));
            vals[0].i64 = i;
            vals[1].text = nbuf;
            if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -192; }
        }
        if (Engine_SetOrder(e, "Name") != GMB_DB_OK) { Engine_Dispose(e); return -193; }
        if (Engine_Seek(e, (const uint8_t*)"N3", 2) != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -194;
        }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -195;
        }
        if (Engine_SeekText(e, "n3") != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -196;
        }
        if (Engine_SetOrder(e, "Id") != GMB_DB_OK) { Engine_Dispose(e); return -197; }
        if (Engine_Seek(e, (const uint8_t*)"3", 1) != GMB_DB_OK || !Engine_WasFound(e)) {
            Engine_Dispose(e); return -198;
        }
        memset(vals, 0, sizeof(vals));
        memset(nameOut, 0, sizeof(nameOut));
        vals[1].textOut = nameOut;
        vals[1].textOutCap = (int)sizeof(nameOut);
        if (Engine_GetCurrent(e, vals, 2, NULL) != GMB_DB_OK || vals[0].i64 != 3) {
            Engine_Dispose(e); return -199;
        }
        Engine_Dispose(e);
    }
    {
        char ppath[] = "native/gmb_db_c/build/isam_join_people.isdb";
        char opath[] = "native/gmb_db_c/build/isam_join_orders.isdb";
        char dpath[] = "native/gmb_db_c/build/isam_join_dest.isdb";
        char apath[] = "native/gmb_db_c/build/isam_join_alpha.isdb";
        char bpath[] = "native/gmb_db_c/build/isam_join_beta.isdb";
        char hpath[] = "native/gmb_db_c/build/isam_join_hash.isdb";
        IsamFieldSchema pfields[2], ofields[2], dfields[4], afields[2], bfields[2], hfields[4];
        IsamIndexDefinition psec;
        IsamSchema pschema, oschema, dschema, aschema, bschema, hschema;
        Engine *pe, *oe, *de, *ae, *be, *he;
        IsamFieldValue ov[2], dv[4], av[2], bv[2], hv[4];
        char tagOut[16], labOut[16];
        int njoin = 0;
        engine_selfcheck_schema(pfields, &psec, &pschema);
        memset(&oschema, 0, sizeof(oschema));
        memset(ofields, 0, sizeof(ofields));
        snprintf(oschema.TypeName, sizeof(oschema.TypeName), "%s", "Order");
        snprintf(ofields[0].Name, sizeof(ofields[0].Name), "%s", "OrdId");
        snprintf(ofields[0].DataTypeCode, sizeof(ofields[0].DataTypeCode), "%s", "IntegerType");
        snprintf(ofields[1].Name, sizeof(ofields[1].Name), "%s", "PersonId");
        snprintf(ofields[1].DataTypeCode, sizeof(ofields[1].DataTypeCode), "%s", "IntegerType");
        oschema.Fields = ofields;
        oschema.FieldCount = 2;
        snprintf(oschema.PrimaryKey.Name, sizeof(oschema.PrimaryKey.Name), "%s", "OrdId");
        snprintf(oschema.PrimaryKey.KeyExpression, sizeof(oschema.PrimaryKey.KeyExpression), "%s", "OrdId");
        oschema.PrimaryKey.KeyLength = 10;
        oschema.PrimaryKey.IsUnique = 1;
        if (IsamFileHandle_CreateNew(ppath, &pschema) != GMB_DB_OK) return -200;
        if (IsamFileHandle_CreateNew(opath, &oschema) != GMB_DB_OK) return -200;
        pe = Engine_Open(ppath, &pschema);
        oe = Engine_Open(opath, &oschema);
        if (!pe || !oe) { if (pe) Engine_Dispose(pe); if (oe) Engine_Dispose(oe); return -201; }
        for (i = 1; i <= 3; i++) {
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "N%d", i);
            memset(vals, 0, sizeof(vals));
            vals[0].i64 = i;
            vals[1].text = nbuf;
            if (Engine_Append(pe, vals, 2) != GMB_DB_OK) {
                Engine_Dispose(pe); Engine_Dispose(oe); return -202;
            }
        }
        {
            int64_t ords[3] = { 10, 11, 12 };
            int64_t pids[3] = { 1, 1, 2 };
            int k;
            for (k = 0; k < 3; k++) {
                memset(ov, 0, sizeof(ov));
                ov[0].i64 = ords[k];
                ov[1].i64 = pids[k];
                if (Engine_Append(oe, ov, 2) != GMB_DB_OK) {
                    Engine_Dispose(pe); Engine_Dispose(oe); return -203;
                }
            }
        }
        if (Engine_JoinWith(pe, pe, dpath, "Id", "Id") == GMB_DB_OK) {
            Engine_Dispose(pe); Engine_Dispose(oe); return -204;
        }
        if (Engine_JoinWith(pe, oe, dpath, "Id", "PersonId") != GMB_DB_OK) {
            Engine_Dispose(pe); Engine_Dispose(oe); return -205;
        }
        if (Engine_LastJoinExecutionMode(pe) != IsamJoinMode_IndexNestedLoop) {
            Engine_Dispose(pe); Engine_Dispose(oe); return -206;
        }
        memset(&dschema, 0, sizeof(dschema));
        dfields[0] = pfields[0];
        dfields[1] = pfields[1];
        dfields[2] = ofields[0];
        dfields[3] = ofields[1];
        snprintf(dschema.TypeName, sizeof(dschema.TypeName), "%s", "Person_Order");
        dschema.Fields = dfields;
        dschema.FieldCount = 4;
        snprintf(dschema.PrimaryKey.Name, sizeof(dschema.PrimaryKey.Name), "%s", "JoinKey");
        snprintf(dschema.PrimaryKey.KeyExpression, sizeof(dschema.PrimaryKey.KeyExpression),
                 "%s", "COMPOSITE(Id,OrdId)");
        dschema.PrimaryKey.KeyLength = 20;
        dschema.PrimaryKey.IsUnique = 1;
        de = Engine_Open(dpath, &dschema);
        if (!de) { Engine_Dispose(pe); Engine_Dispose(oe); return -207; }
        if (Engine_Go(de, CursorMoveOperation_Top) != GMB_DB_OK) {
            Engine_Dispose(de); Engine_Dispose(pe); Engine_Dispose(oe); return -208;
        }
        njoin = 0;
        while (!Engine_IsEof(de)) {
            memset(dv, 0, sizeof(dv));
            memset(nameOut, 0, sizeof(nameOut));
            dv[1].textOut = nameOut;
            dv[1].textOutCap = (int)sizeof(nameOut);
            if (Engine_GetCurrent(de, dv, 4, NULL) != GMB_DB_OK) {
                Engine_Dispose(de); Engine_Dispose(pe); Engine_Dispose(oe); return -209;
            }
            njoin++;
            if (Engine_Skip(de, 1) != GMB_DB_OK) {
                Engine_Dispose(de); Engine_Dispose(pe); Engine_Dispose(oe); return -210;
            }
        }
        if (njoin != 3) {
            Engine_Dispose(de); Engine_Dispose(pe); Engine_Dispose(oe); return -211;
        }
        Engine_Dispose(de);
        Engine_Dispose(pe);
        Engine_Dispose(oe);

        memset(&aschema, 0, sizeof(aschema));
        memset(afields, 0, sizeof(afields));
        snprintf(aschema.TypeName, sizeof(aschema.TypeName), "%s", "Alpha");
        snprintf(afields[0].Name, sizeof(afields[0].Name), "%s", "AId");
        snprintf(afields[0].DataTypeCode, sizeof(afields[0].DataTypeCode), "%s", "IntegerType");
        snprintf(afields[1].Name, sizeof(afields[1].Name), "%s", "Tag");
        snprintf(afields[1].DataTypeCode, sizeof(afields[1].DataTypeCode), "%s", "StringType");
        afields[1].Length = 8;
        aschema.Fields = afields;
        aschema.FieldCount = 2;
        snprintf(aschema.PrimaryKey.Name, sizeof(aschema.PrimaryKey.Name), "%s", "AId");
        snprintf(aschema.PrimaryKey.KeyExpression, sizeof(aschema.PrimaryKey.KeyExpression), "%s", "AId");
        aschema.PrimaryKey.KeyLength = 10;
        aschema.PrimaryKey.IsUnique = 1;
        memset(&bschema, 0, sizeof(bschema));
        memset(bfields, 0, sizeof(bfields));
        snprintf(bschema.TypeName, sizeof(bschema.TypeName), "%s", "Beta");
        snprintf(bfields[0].Name, sizeof(bfields[0].Name), "%s", "BId");
        snprintf(bfields[0].DataTypeCode, sizeof(bfields[0].DataTypeCode), "%s", "IntegerType");
        snprintf(bfields[1].Name, sizeof(bfields[1].Name), "%s", "Label");
        snprintf(bfields[1].DataTypeCode, sizeof(bfields[1].DataTypeCode), "%s", "StringType");
        bfields[1].Length = 8;
        bschema.Fields = bfields;
        bschema.FieldCount = 2;
        snprintf(bschema.PrimaryKey.Name, sizeof(bschema.PrimaryKey.Name), "%s", "BId");
        snprintf(bschema.PrimaryKey.KeyExpression, sizeof(bschema.PrimaryKey.KeyExpression), "%s", "BId");
        bschema.PrimaryKey.KeyLength = 10;
        bschema.PrimaryKey.IsUnique = 1;
        if (IsamFileHandle_CreateNew(apath, &aschema) != GMB_DB_OK) return -212;
        if (IsamFileHandle_CreateNew(bpath, &bschema) != GMB_DB_OK) return -212;
        ae = Engine_Open(apath, &aschema);
        be = Engine_Open(bpath, &bschema);
        if (!ae || !be) { if (ae) Engine_Dispose(ae); if (be) Engine_Dispose(be); return -213; }
        memset(av, 0, sizeof(av)); av[0].i64 = 1; av[1].text = "red";
        if (Engine_Append(ae, av, 2) != GMB_DB_OK) { Engine_Dispose(ae); Engine_Dispose(be); return -214; }
        memset(av, 0, sizeof(av)); av[0].i64 = 2; av[1].text = "blue";
        if (Engine_Append(ae, av, 2) != GMB_DB_OK) { Engine_Dispose(ae); Engine_Dispose(be); return -214; }
        memset(bv, 0, sizeof(bv)); bv[0].i64 = 10; bv[1].text = "red";
        if (Engine_Append(be, bv, 2) != GMB_DB_OK) { Engine_Dispose(ae); Engine_Dispose(be); return -215; }
        memset(bv, 0, sizeof(bv)); bv[0].i64 = 20; bv[1].text = "green";
        if (Engine_Append(be, bv, 2) != GMB_DB_OK) { Engine_Dispose(ae); Engine_Dispose(be); return -215; }
        if (Engine_JoinWith(ae, be, hpath, "Tag", "Label") != GMB_DB_OK) {
            Engine_Dispose(ae); Engine_Dispose(be); return -216;
        }
        if (Engine_LastJoinExecutionMode(ae) != IsamJoinMode_HashJoin) {
            Engine_Dispose(ae); Engine_Dispose(be); return -217;
        }
        memset(&hschema, 0, sizeof(hschema));
        hfields[0] = afields[0];
        hfields[1] = afields[1];
        hfields[2] = bfields[0];
        hfields[3] = bfields[1];
        snprintf(hschema.TypeName, sizeof(hschema.TypeName), "%s", "Alpha_Beta");
        hschema.Fields = hfields;
        hschema.FieldCount = 4;
        snprintf(hschema.PrimaryKey.Name, sizeof(hschema.PrimaryKey.Name), "%s", "JoinKey");
        snprintf(hschema.PrimaryKey.KeyExpression, sizeof(hschema.PrimaryKey.KeyExpression),
                 "%s", "COMPOSITE(AId,BId)");
        hschema.PrimaryKey.KeyLength = 20;
        hschema.PrimaryKey.IsUnique = 1;
        he = Engine_Open(hpath, &hschema);
        if (!he) { Engine_Dispose(ae); Engine_Dispose(be); return -218; }
        if (Engine_Go(he, CursorMoveOperation_Top) != GMB_DB_OK || Engine_IsEof(he)) {
            Engine_Dispose(he); Engine_Dispose(ae); Engine_Dispose(be); return -219;
        }
        memset(hv, 0, sizeof(hv));
        memset(tagOut, 0, sizeof(tagOut));
        memset(labOut, 0, sizeof(labOut));
        hv[1].textOut = tagOut; hv[1].textOutCap = (int)sizeof(tagOut);
        hv[3].textOut = labOut; hv[3].textOutCap = (int)sizeof(labOut);
        if (Engine_GetCurrent(he, hv, 4, NULL) != GMB_DB_OK || hv[0].i64 != 1 || hv[2].i64 != 10) {
            Engine_Dispose(he); Engine_Dispose(ae); Engine_Dispose(be); return -220;
        }
        if (Engine_Skip(he, 1) != GMB_DB_OK || !Engine_IsEof(he)) {
            Engine_Dispose(he); Engine_Dispose(ae); Engine_Dispose(be); return -221;
        }
        Engine_Dispose(he);
        Engine_Dispose(ae);
        Engine_Dispose(be);
    }
    return GMB_DB_OK;
}
