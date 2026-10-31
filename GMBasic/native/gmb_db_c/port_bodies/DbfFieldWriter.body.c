#include <stdio.h>
#include <time.h>

/* C#: DbfFieldWriter — valore come stringa (REPLACE/APPEND). Byte-for-byte vs C# per C/N/D/L/F/I/T/Y/B/M. */

static void fw_fill_space(uint8_t* dest, int destLen)
{
    if (dest && destLen > 0) memset(dest, ' ', (size_t)destLen);
}

static void fw_fill_zero(uint8_t* dest, int destLen)
{
    if (dest && destLen > 0) memset(dest, 0, (size_t)destLen);
}

static void fw_put_i32_le(uint8_t* dest, int32_t v)
{
    dest[0] = (uint8_t)v;
    dest[1] = (uint8_t)((uint32_t)v >> 8);
    dest[2] = (uint8_t)((uint32_t)v >> 16);
    dest[3] = (uint8_t)((uint32_t)v >> 24);
}

static void fw_put_i64_le(uint8_t* dest, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++) dest[i] = (uint8_t)((uint64_t)v >> (8 * i));
}

static void fw_put_f64_le(uint8_t* dest, double v)
{
    memcpy(dest, &v, 8);
}

static void fw_right_ascii(uint8_t* dest, int destLen, const char* text)
{
    int n;
    fw_fill_space(dest, destLen);
    if (!text) return;
    n = (int)strlen(text);
    if (n > destLen) return; /* C#: se non entra, lascia spazi */
    memcpy(dest + (destLen - n), text, (size_t)n);
}

void DbfFieldWriter_WriteCharacter(uint8_t* dest, int destLen, const char* value)
{
    int n;
    fw_fill_space(dest, destLen);
    if (!value) value = "";
    n = (int)strlen(value);
    if (n > destLen) n = destLen;
    if (n > 0) memcpy(dest, value, (size_t)n);
}

void DbfFieldWriter_WriteNumeric(uint8_t* dest, int destLen, const char* value, int decimals)
{
    char num[64];
    double v;
    char* end = NULL;
    fw_fill_space(dest, destLen);
    if (!value || !*value) return;
    v = strtod(value, &end);
    if (end == value) return;
    snprintf(num, sizeof(num), "%.*f", decimals < 0 ? 0 : decimals, v);
    fw_right_ascii(dest, destLen, num);
}

void DbfFieldWriter_WriteDate(uint8_t* dest, int destLen, const char* value)
{
    char ymd[9];
    int n, i, digits = 0;
    if (destLen < 8) {
        fw_fill_space(dest, destLen);
        return;
    }
    memset(ymd, ' ', 8);
    ymd[8] = 0;
    if (value) {
        n = (int)strlen(value);
        for (i = 0; i < n && digits < 8; i++) {
            if (value[i] >= '0' && value[i] <= '9') ymd[digits++] = value[i];
        }
        if (digits != 8) memset(ymd, ' ', 8);
    }
    memcpy(dest, ymd, 8);
    if (destLen > 8) memset(dest + 8, ' ', (size_t)(destLen - 8));
}

void DbfFieldWriter_WriteLogical(uint8_t* dest, int destLen, const char* value)
{
    char c;
    if (destLen <= 0) return;
    if (!value || !*value) {
        dest[0] = '?';
    } else {
        c = value[0];
        if (c == 'T' || c == 't' || c == 'Y' || c == 'y' || c == '1') dest[0] = 'T';
        else if (c == 'F' || c == 'f' || c == 'N' || c == 'n' || c == '0') dest[0] = 'F';
        else dest[0] = '?';
    }
    if (destLen > 1) memset(dest + 1, ' ', (size_t)(destLen - 1));
}

void DbfFieldWriter_WriteFloat(uint8_t* dest, int destLen, const char* value, int decimals)
{
    DbfFieldWriter_WriteNumeric(dest, destLen, value, decimals);
}

void DbfFieldWriter_WriteInteger(uint8_t* dest, int destLen, const char* value)
{
    int32_t v;
    fw_fill_zero(dest, destLen);
    if (destLen != 4 || !value || !*value) return;
    v = (int32_t)strtol(value, NULL, 10);
    fw_put_i32_le(dest, v);
}

static int32_t fw_civil_days(int y, int m, int d)
{
    /* Howard Hinnant days_from_civil — OADate è giorni da 1899-12-30. */
    y -= m <= 2;
    {
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
        unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097 + (int)doe - 719468;
    }
}

static uint32_t fw_oa_julian(int y, int m, int d)
{
    int oa = fw_civil_days(y, m, d) - fw_civil_days(1899, 12, 30);
    return (uint32_t)(oa + 1721425);
}

void DbfFieldWriter_WriteDateTime(uint8_t* dest, int destLen, const char* value)
{
    int y = 0, mo = 0, d = 0, hh = 0, mi = 0, ss = 0, ms = 0;
    uint32_t julian, milli;
    fw_fill_zero(dest, destLen);
    if (destLen < 8 || !value || !*value) return;
    if (sscanf(value, "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &hh, &mi, &ss) < 3
        && sscanf(value, "%4d%2d%2d%2d%2d%2d", &y, &mo, &d, &hh, &mi, &ss) < 3
        && sscanf(value, "%4d%2d%2d", &y, &mo, &d) < 3)
        return;
    julian = fw_oa_julian(y, mo, d);
    milli = (uint32_t)(((hh * 60 + mi) * 60 + ss) * 1000 + ms);
    fw_put_i32_le(dest, (int32_t)julian);
    fw_put_i32_le(dest + 4, (int32_t)milli);
}

void DbfFieldWriter_WriteCurrency(uint8_t* dest, int destLen, const char* value)
{
    double v;
    int64_t scaled;
    fw_fill_zero(dest, destLen);
    if (destLen != 8 || !value || !*value) return;
    v = strtod(value, NULL);
    scaled = (int64_t)(v * 10000.0);
    fw_put_i64_le(dest, scaled);
}

void DbfFieldWriter_WriteDouble(uint8_t* dest, int destLen, const char* value)
{
    double v;
    fw_fill_zero(dest, destLen);
    if (destLen != 8 || !value || !*value) return;
    v = strtod(value, NULL);
    fw_put_f64_le(dest, v);
}

void DbfFieldWriter_WriteMemoIndex(uint8_t* dest, int destLen, int blockIndex)
{
    char text[32];
    if (destLen == 4) {
        fw_put_i32_le(dest, blockIndex);
        return;
    }
    snprintf(text, sizeof(text), "%d", blockIndex);
    fw_fill_space(dest, destLen);
    fw_right_ascii(dest, destLen, text);
}

void DbfFieldWriter_WriteMemo(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile)
{
    int block;
    fw_fill_space(dest, destLen);
    if (!value || !*value || !memoFile || MemoFile_IsNull(memoFile)) return;
    block = MemoFile_WriteMemo(memoFile, (const uint8_t*)value, (int)strlen(value),
                               field && field->Type != (uint8_t)DbfFieldType_General);
    if (block <= 0) return;
    DbfFieldWriter_WriteMemoIndex(dest, destLen, block);
}

void DbfFieldWriter_Write(uint8_t* dest, int destLen, const DbfField* field, const char* value, MemoFile* memoFile)
{
    uint8_t t;
    if (!dest || destLen <= 0 || !field) return;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
        DbfFieldWriter_WriteCharacter(dest, destLen, value);
        break;
    case DbfFieldType_Numeric:
        DbfFieldWriter_WriteNumeric(dest, destLen, value, field->DecimalCount);
        break;
    case DbfFieldType_Date:
        DbfFieldWriter_WriteDate(dest, destLen, value);
        break;
    case DbfFieldType_Logical:
        DbfFieldWriter_WriteLogical(dest, destLen, value);
        break;
    case DbfFieldType_Float:
        DbfFieldWriter_WriteFloat(dest, destLen, value, field->DecimalCount);
        break;
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        DbfFieldWriter_WriteInteger(dest, destLen, value);
        break;
    case DbfFieldType_DateTime:
        DbfFieldWriter_WriteDateTime(dest, destLen, value);
        break;
    case DbfFieldType_Currency:
        DbfFieldWriter_WriteCurrency(dest, destLen, value);
        break;
    case DbfFieldType_Double:
        DbfFieldWriter_WriteDouble(dest, destLen, value);
        break;
    case DbfFieldType_Memo:
    case DbfFieldType_General:
    case DbfFieldType_Varbinary:
    case DbfFieldType_Blob:
        DbfFieldWriter_WriteMemo(dest, destLen, field, value, memoFile);
        break;
    default:
        fw_fill_zero(dest, destLen);
        break;
    }
}

void DbfFieldWriter_WriteBlank(uint8_t* dest, int destLen, const DbfField* field, MemoFile* memoFile)
{
    uint8_t t;
    if (!dest || destLen <= 0 || !field) return;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
    case DbfFieldType_Memo:
        DbfFieldWriter_WriteCharacter(dest, destLen, "");
        break;
    case DbfFieldType_Numeric:
        DbfFieldWriter_WriteNumeric(dest, destLen, "0", field->DecimalCount);
        break;
    case DbfFieldType_Float:
        DbfFieldWriter_WriteFloat(dest, destLen, "0", field->DecimalCount);
        break;
    case DbfFieldType_Double:
        DbfFieldWriter_WriteDouble(dest, destLen, "0");
        break;
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        DbfFieldWriter_WriteInteger(dest, destLen, "0");
        break;
    case DbfFieldType_Currency:
        DbfFieldWriter_WriteCurrency(dest, destLen, "0");
        break;
    case DbfFieldType_Logical:
        DbfFieldWriter_WriteLogical(dest, destLen, "F");
        break;
    case DbfFieldType_Date:
        DbfFieldWriter_WriteDate(dest, destLen, "00010101");
        break;
    case DbfFieldType_DateTime:
        DbfFieldWriter_WriteDateTime(dest, destLen, "00010101");
        break;
    default:
        DbfFieldWriter_Write(dest, destLen, field, NULL, memoFile);
        break;
    }
}

int DbfFieldWriter_SelfCheck(void)
{
    uint8_t buf[16];
    DbfField f;
    memset(&f, 0, sizeof(f));
    f.Type = (uint8_t)DbfFieldType_Character;
    f.Length = 10;
    DbfFieldWriter_Write(buf, 10, &f, "HI", NULL);
    if (memcmp(buf, "HI        ", 10) != 0) return -1;
    f.Type = (uint8_t)DbfFieldType_Numeric;
    f.Length = 6;
    f.DecimalCount = 2;
    DbfFieldWriter_Write(buf, 6, &f, "12.5", NULL);
    if (memcmp(buf, " 12.50", 6) != 0) return -2;
    f.Type = (uint8_t)DbfFieldType_Date;
    f.Length = 8;
    DbfFieldWriter_Write(buf, 8, &f, "2024-01-15", NULL);
    if (memcmp(buf, "20240115", 8) != 0) return -3;
    f.Type = (uint8_t)DbfFieldType_Logical;
    f.Length = 1;
    DbfFieldWriter_Write(buf, 1, &f, "Y", NULL);
    if (buf[0] != 'T') return -4;
    f.Type = (uint8_t)DbfFieldType_Integer;
    f.Length = 4;
    DbfFieldWriter_Write(buf, 4, &f, "42", NULL);
    if (buf[0] != 42 || buf[1] != 0 || buf[2] != 0 || buf[3] != 0) return -5;
    return GMB_DB_OK;
}
