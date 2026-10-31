#include <time.h>
#include <limits.h>

int DbfVersionExtensions_SupportsMemoFields(DbfVersion version)
{
    /* ponytail: VFP 0x30 tables use .fpt sidecars like 0xF5 — both need memo I/O */
    return version == DbfVersion_DBase3PlusWithMemo || version == DbfVersion_DBase4WithMemo
        || version == DbfVersion_VisualFoxPro || version == DbfVersion_VisualFoxProWithMemo;
}

int DbfHeader_Read(FILE* reader, DbfHeader* out)
{
    uint8_t headerBytes[DbfHeader_Size];
    if (!reader || !out) return GMB_DB_ARG;
    if (fread(headerBytes, 1, DbfHeader_Size, reader) != DbfHeader_Size) return GMB_DB_ERR;
    memcpy(out, headerBytes, DbfHeader_Size);
    return GMB_DB_OK;
}

void DbfHeader_ToBytes(const DbfHeader* header, uint8_t out[DbfHeader_Size])
{
    memcpy(out, header, DbfHeader_Size);
}

DbfField DbfField_Create(const char* name, uint8_t type, uint8_t length, uint8_t decimalCount)
{
    DbfField f;
    memset(&f, 0, sizeof(f));
    if (name) snprintf(f.Name, sizeof(f.Name), "%s", name);
    f.Type = type;
    f.Length = length;
    f.DecimalCount = decimalCount;
    return f;
}

int DbfField_ReadAll(FILE* reader, DbfField** outFields, int* outCount)
{
    DbfField* fields = NULL;
    int n = 0, cap = 0;
    int c;
    if (!reader || !outFields || !outCount) return GMB_DB_ARG;
    for (;;) {
        uint8_t buffer[DbfField_StructSize];
        DbfField f;
        int nameLen;
        long remaining;
        long cur, end;
        c = fgetc(reader);
        if (c == EOF) break;
        ungetc(c, reader);
        if (c == 0x0D) break;
        cur = ftell(reader);
        fseek(reader, 0, SEEK_END);
        end = ftell(reader);
        fseek(reader, cur, SEEK_SET);
        remaining = end - cur;
        if (remaining < DbfField_StructSize) break;
        if (fread(buffer, 1, DbfField_StructSize, reader) != DbfField_StructSize) break;
        if (buffer[0] == 0x0D) break;
        memset(&f, 0, sizeof(f));
        nameLen = 11;
        while (nameLen > 0 && buffer[nameLen - 1] == 0) nameLen--;
        memcpy(f.Name, buffer, (size_t)nameLen);
        f.Type = buffer[11];
        f.Address = (uint32_t)buffer[12] | ((uint32_t)buffer[13] << 8) | ((uint32_t)buffer[14] << 16) | ((uint32_t)buffer[15] << 24);
        f.Length = buffer[16];
        f.DecimalCount = buffer[17];
        if (n >= cap) {
            cap = cap ? cap * 2 : 8;
            fields = (DbfField*)realloc(fields, (size_t)cap * sizeof(DbfField));
        }
        fields[n++] = f;
    }
    c = fgetc(reader);
    if (c != EOF && c != 0x0D) ungetc(c, reader);
    *outFields = fields;
    *outCount = n;
    return GMB_DB_OK;
}

static const struct { uint8_t b; const char* n; } DbfCodePage_Mappings[] = {
    { 0x01, "ibm437" }, { 0x02, "ibm850" }, { 0x03, "windows-1252" }, { 0x04, "macintosh" },
    { 0x08, "ibm865" }, { 0x09, "ibm437" }, { 0x0A, "ibm850" }, { 0x0B, "ibm437" },
    { 0x0D, "ibm437" }, { 0x0E, "ibm850" }, { 0x0F, "ibm437" }, { 0x10, "ibm850" },
    { 0x11, "ibm437" }, { 0x12, "ibm850" }, { 0x13, "sjis" }, { 0x14, "ibm850" },
    { 0x15, "ibm437" }, { 0x16, "ibm850" }, { 0x17, "ibm865" }, { 0x18, "ibm437" },
    { 0x19, "ibm437" }, { 0x1A, "ibm850" }, { 0x1B, "ibm437" }, { 0x1C, "ibm863" },
    { 0x1D, "ibm850" }, { 0x1F, "ibm852" }, { 0x22, "ibm852" }, { 0x23, "ibm852" },
    { 0x24, "ibm860" }, { 0x25, "ibm850" }, { 0x26, "ibm866" }, { 0x37, "ibm850" },
    { 0x40, "ibm852" }, { 0x4D, "gb2312" }, { 0x4E, "euc-kr" }, { 0x4F, "big5" },
    { 0x50, "windows-874" }, { 0x57, "windows-1252" }, { 0x58, "windows-1252" },
    { 0x59, "windows-1252" }, { 0x64, "ibm852" }, { 0x65, "ibm866" }, { 0x66, "ibm865" },
    { 0x67, "ibm861" }, { 0x6A, "ibm737" }, { 0x6B, "ibm857" }, { 0x78, "big5" },
    { 0x79, "euc-kr" }, { 0x7A, "gb2312" }, { 0x7B, "sjis" }, { 0x7C, "windows-874" },
    { 0x7D, "windows-1255" }, { 0x7E, "windows-1256" }, { 0x96, "x-mac-cyrillic" },
    { 0x97, "x-mac-ce" }, { 0x98, "x-mac-greek" }, { 0xC8, "windows-1250" },
    { 0xC9, "windows-1251" }, { 0xCA, "windows-1254" }, { 0xCB, "windows-1253" },
    { 0xF0, "utf-8" },
};

const char* DbfCodePage_GetEncodingName(uint8_t languageDriver)
{
    size_t i;
    for (i = 0; i < sizeof(DbfCodePage_Mappings) / sizeof(DbfCodePage_Mappings[0]); i++)
        if (DbfCodePage_Mappings[i].b == languageDriver) return DbfCodePage_Mappings[i].n;
    return "us-ascii";
}

int DbfCodePage_GetLanguageDriver(const char* encodingWebName)
{
    size_t i;
    if (!encodingWebName) return -1;
    for (i = 0; i < sizeof(DbfCodePage_Mappings) / sizeof(DbfCodePage_Mappings[0]); i++)
        if (gmb_strcasecmp(DbfCodePage_Mappings[i].n, encodingWebName) == 0)
            return DbfCodePage_Mappings[i].b;
    return -1;
}
