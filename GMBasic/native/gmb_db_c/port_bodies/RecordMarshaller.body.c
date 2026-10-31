static int rm_is_numeric(const char* typeCode)
{
    return typeCode && (
        strcmp(typeCode, "IntegerType") == 0 || strcmp(typeCode, "UIntegerType") == 0 ||
        strcmp(typeCode, "LongType") == 0 || strcmp(typeCode, "ULongType") == 0 ||
        strcmp(typeCode, "QuadType") == 0 || strcmp(typeCode, "UQuadType") == 0 ||
        strcmp(typeCode, "ByteType") == 0 || strcmp(typeCode, "SByteType") == 0 ||
        strcmp(typeCode, "BitType") == 0);
}

static void rm_wr_i16(uint8_t* p, int16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)((uint16_t)v >> 8);
}

static int16_t rm_rd_i16(const uint8_t* p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void rm_wr_i32(uint8_t* p, int32_t v)
{
    int i;
    for (i = 0; i < 4; i++) p[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static int32_t rm_rd_i32(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void rm_wr_i64(uint8_t* p, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++) p[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static int64_t rm_rd_i64(const uint8_t* p)
{
    int i;
    uint64_t u = 0;
    for (i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

static int rm_write_field(uint8_t* dst, int cap, const IsamFieldSchema* field, const IsamFieldValue* val)
{
    int sz = SchemaManager_GetFieldSize(field->DataTypeCode, field->Length);
    if (sz < 0 || sz > cap) return -1;
    memset(dst, 0, (size_t)sz);
    if (strcmp(field->DataTypeCode, "StringType") == 0) {
        memset(dst, ' ', (size_t)sz);
        if (val->text) {
            int n = (int)strlen(val->text);
            if (n > sz) n = sz;
            memcpy(dst, val->text, (size_t)n);
        }
        return sz;
    }
    if (strcmp(field->DataTypeCode, "BitType") == 0 || strcmp(field->DataTypeCode, "ByteType") == 0
        || strcmp(field->DataTypeCode, "SByteType") == 0) {
        dst[0] = (uint8_t)val->i64;
        return sz;
    }
    if (strcmp(field->DataTypeCode, "IntegerType") == 0 || strcmp(field->DataTypeCode, "UIntegerType") == 0) {
        rm_wr_i16(dst, (int16_t)val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "LongType") == 0 || strcmp(field->DataTypeCode, "ULongType") == 0) {
        rm_wr_i32(dst, (int32_t)val->i64);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "QuadType") == 0 || strcmp(field->DataTypeCode, "UQuadType") == 0
        || strcmp(field->DataTypeCode, "DateTimeType") == 0 || strcmp(field->DataTypeCode, "DateType") == 0
        || strcmp(field->DataTypeCode, "TimeType") == 0 || strcmp(field->DataTypeCode, "TimeSpanType") == 0
        || strcmp(field->DataTypeCode, "JsonType") == 0) {
        rm_wr_i64(dst, val->i64);
        return sz;
    }
    return sz;
}

static int rm_read_field(const uint8_t* src, const IsamFieldSchema* field, IsamFieldValue* val)
{
    int sz = SchemaManager_GetFieldSize(field->DataTypeCode, field->Length);
    int n;
    if (sz < 0) return -1;
    val->i64 = 0;
    if (strcmp(field->DataTypeCode, "StringType") == 0) {
        n = sz;
        while (n > 0 && (src[n - 1] == ' ' || src[n - 1] == '\0')) n--;
        if (val->textOut && val->textOutCap > 0) {
            if (n >= val->textOutCap) n = val->textOutCap - 1;
            memcpy(val->textOut, src, (size_t)n);
            val->textOut[n] = 0;
        }
        return sz;
    }
    if (strcmp(field->DataTypeCode, "BitType") == 0 || strcmp(field->DataTypeCode, "ByteType") == 0
        || strcmp(field->DataTypeCode, "SByteType") == 0) {
        val->i64 = src[0];
        return sz;
    }
    if (strcmp(field->DataTypeCode, "IntegerType") == 0 || strcmp(field->DataTypeCode, "UIntegerType") == 0) {
        val->i64 = rm_rd_i16(src);
        return sz;
    }
    if (strcmp(field->DataTypeCode, "LongType") == 0 || strcmp(field->DataTypeCode, "ULongType") == 0) {
        val->i64 = rm_rd_i32(src);
        return sz;
    }
    if (rm_is_numeric(field->DataTypeCode) || strcmp(field->DataTypeCode, "JsonType") == 0
        || strcmp(field->DataTypeCode, "DateTimeType") == 0 || strcmp(field->DataTypeCode, "DateType") == 0
        || strcmp(field->DataTypeCode, "TimeType") == 0 || strcmp(field->DataTypeCode, "TimeSpanType") == 0) {
        val->i64 = rm_rd_i64(src);
        return sz;
    }
    return sz;
}

int RecordMarshaller_SerializeToBuffer(const IsamSchema* schema, const RecordMetadata* meta,
                                       const IsamFieldValue* fields, int fieldCount,
                                       uint8_t* buffer, int bufferLen)
{
    int recSize, i, pos;
    if (!schema || !meta || !buffer) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(schema);
    if (recSize < 0 || bufferLen < recSize || fieldCount != schema->FieldCount) return GMB_DB_ERR;
    memset(buffer, 0, (size_t)recSize);
    memcpy(buffer, meta, RecordMetadata_Size);
    pos = RecordMetadata_Size;
    for (i = 0; i < schema->FieldCount; i++) {
        int n = rm_write_field(buffer + pos, recSize - pos, &schema->Fields[i], &fields[i]);
        if (n < 0) return GMB_DB_ERR;
        pos += n;
    }
    return pos == recSize ? GMB_DB_OK : GMB_DB_ERR;
}

int RecordMarshaller_DeserializeFromBuffer(const IsamSchema* schema, const uint8_t* buffer, int bufferLen,
                                           RecordMetadata* meta, IsamFieldValue* fields, int fieldCount)
{
    int recSize, i, pos;
    if (!schema || !buffer || !meta || !fields) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(schema);
    if (recSize < 0 || bufferLen < recSize || fieldCount != schema->FieldCount) return GMB_DB_ERR;
    memcpy(meta, buffer, RecordMetadata_Size);
    pos = RecordMetadata_Size;
    for (i = 0; i < schema->FieldCount; i++) {
        int n = rm_read_field(buffer + pos, &schema->Fields[i], &fields[i]);
        if (n < 0) return GMB_DB_ERR;
        pos += n;
    }
    return GMB_DB_OK;
}

int RecordMarshaller_WriteRecordWithMetadata(IsamFileHandle* fileHandle, int64_t offset,
                                             const IsamFieldValue* fields, int fieldCount,
                                             const RecordMetadata* meta)
{
    int recSize;
    uint8_t* buf;
    int rc;
    if (!fileHandle || !fileHandle->Schema || !meta) return GMB_DB_ERR;
    recSize = SchemaManager_CalculateRecordSize(fileHandle->Schema);
    if (recSize < 0) return GMB_DB_ERR;
    buf = (uint8_t*)malloc((size_t)recSize);
    if (!buf) return GMB_DB_ERR;
    rc = RecordMarshaller_SerializeToBuffer(fileHandle->Schema, meta, fields, fieldCount, buf, recSize);
    if (rc == GMB_DB_OK)
        rc = IsamFileHandle_WriteRecordFromBuffer(fileHandle, offset, buf, recSize);
    free(buf);
    return rc;
}
