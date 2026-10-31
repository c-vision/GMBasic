#include <stdio.h>

/* C#: FieldParser — raw field bytes → testo (ABI/REPLACE roundtrip). */

static int fp_is_null(const uint8_t* data, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (data[i] != 0 && data[i] != 32) return 0;
    }
    return 1;
}

static int fp_trim_end(const uint8_t* data, int len)
{
    while (len > 0 && (data[len - 1] == 0 || data[len - 1] == 32)) len--;
    return len;
}

static int32_t fp_i32_le(const uint8_t* p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int64_t fp_i64_le(const uint8_t* p)
{
    int i;
    uint64_t u = 0;
    for (i = 0; i < 8; i++) u |= (uint64_t)p[i] << (8 * i);
    return (int64_t)u;
}

static int fp_copy_out(char* out, int cap, const char* s)
{
    if (!out || cap <= 0) return GMB_DB_ARG;
    snprintf(out, (size_t)cap, "%s", s ? s : "");
    return GMB_DB_OK;
}

static int fp_parse_character(const uint8_t* data, int len, char* out, int cap)
{
    int n = fp_trim_end(data, len);
    if (!out || cap <= 0) return GMB_DB_ARG;
    if (n >= cap) n = cap - 1;
    memcpy(out, data, (size_t)n);
    out[n] = 0;
    return GMB_DB_OK;
}

static int fp_parse_numeric(const uint8_t* data, int len, char* out, int cap)
{
    int n = fp_trim_end(data, len);
    const uint8_t* p = data;
    int i;
    while (n > 0 && (*p == ' ' || *p == '\t')) {
        p++;
        n--;
    }
    if (!out || cap <= 0) return GMB_DB_ARG;
    if (n <= 0) {
        out[0] = 0;
        return GMB_DB_OK;
    }
    if (n >= cap) n = cap - 1;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    for (i = 0; out[i]; i++) {
        if (out[i] == ',') out[i] = '.';
    }
    return GMB_DB_OK;
}

static int fp_parse_date(const uint8_t* data, int len, char* out, int cap)
{
    if (fp_is_null(data, len)) return fp_copy_out(out, cap, "");
    return fp_parse_character(data, len < 8 ? len : 8, out, cap);
}

static int fp_parse_logical(const uint8_t* data, int len, char* out, int cap)
{
    if (len <= 0) return fp_copy_out(out, cap, "");
    switch (data[0]) {
    case 'T':
    case 't':
    case 'Y':
    case 'y':
        return fp_copy_out(out, cap, "T");
    case 'F':
    case 'f':
    case 'N':
    case 'n':
        return fp_copy_out(out, cap, "F");
    default:
        return fp_copy_out(out, cap, "");
    }
}

static int fp_parse_integer(const uint8_t* data, int len, char* out, int cap)
{
    char tmp[32];
    if (len != 4) return fp_copy_out(out, cap, "");
    snprintf(tmp, sizeof(tmp), "%d", (int)fp_i32_le(data));
    return fp_copy_out(out, cap, tmp);
}

static int fp_parse_double(const uint8_t* data, int len, char* out, int cap)
{
    double v;
    char tmp[64];
    if (len != 8) return fp_copy_out(out, cap, "");
    memcpy(&v, data, 8);
    snprintf(tmp, sizeof(tmp), "%.15g", v);
    return fp_copy_out(out, cap, tmp);
}

static int fp_parse_currency(const uint8_t* data, int len, char* out, int cap)
{
    double v;
    char tmp[64];
    if (len != 8) return fp_copy_out(out, cap, "");
    v = (double)fp_i64_le(data) / 10000.0;
    snprintf(tmp, sizeof(tmp), "%.4f", v);
    return fp_copy_out(out, cap, tmp);
}

static int32_t fp_civil_days(int y, int m, int d)
{
    y -= m <= 2;
    {
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
        unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097 + (int)doe - 719468;
    }
}

static void fp_civil_from_days(int z, int* y, int* m, int* d)
{
    z += 719468;
    {
        int era = (z >= 0 ? z : z - 146096) / 146097;
        unsigned doe = (unsigned)(z - era * 146097);
        unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
        int yy = (int)yoe + era * 400;
        unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
        unsigned mp = (5u * doy + 2u) / 153u;
        unsigned dd = doy - (153u * mp + 2u) / 5u + 1u;
        unsigned mm = mp + (mp < 10 ? 3u : -9u);
        yy += (mm <= 2);
        *y = yy;
        *m = (int)mm;
        *d = (int)dd;
    }
}

static int fp_parse_timestamp(const uint8_t* data, int len, char* out, int cap)
{
    uint32_t julian, milli;
    int oa, y, mo, d, hh, mi, ss;
    char tmp[32];
    if (len != 8 || fp_is_null(data, len)) return fp_copy_out(out, cap, "");
    julian = (uint32_t)fp_i32_le(data);
    milli = (uint32_t)fp_i32_le(data + 4);
    if (julian == 0) return fp_copy_out(out, cap, "");
    oa = (int)julian - 1721425;
    fp_civil_from_days(oa + fp_civil_days(1899, 12, 30), &y, &mo, &d);
    hh = (int)(milli / 3600000u);
    milli %= 3600000u;
    mi = (int)(milli / 60000u);
    milli %= 60000u;
    ss = (int)(milli / 1000u);
    snprintf(tmp, sizeof(tmp), "%04d%02d%02d%02d%02d%02d", y, mo, d, hh, mi, ss);
    return fp_copy_out(out, cap, tmp);
}

int DbfFieldParser_ParseMemoIndex(const uint8_t* data, int len)
{
    char tmp[32];
    int n, i;
    if (!data || len <= 0) return 0;
    if (len == 4) return (int)fp_i32_le(data);
    n = fp_trim_end(data, len);
    if (n <= 0 || n >= (int)sizeof(tmp)) return 0;
    memcpy(tmp, data, (size_t)n);
    tmp[n] = 0;
    for (i = 0; tmp[i] == ' '; i++) { }
    return (int)strtol(tmp + i, NULL, 10);
}

static int fp_parse_memo(const uint8_t* data, int len, MemoFile* memo, char* out, int cap)
{
    int idx;
    MemoData md;
    int n, rc;
    if (!out || cap <= 0) return GMB_DB_ARG;
    out[0] = 0;
    idx = DbfFieldParser_ParseMemoIndex(data, len);
    if (idx == 0 || !memo) return GMB_DB_OK;
    memset(&md, 0, sizeof(md));
    if (MemoFile_GetMemo(memo, idx, &md) != GMB_DB_OK) return GMB_DB_OK;
    n = md.Length;
    while (n > 0 && md.Data && md.Data[n - 1] == 0) n--;
    if (n >= cap) n = cap - 1;
    if (n > 0 && md.Data) memcpy(out, md.Data, (size_t)n);
    out[n] = 0;
    MemoData_Dispose(&md);
    rc = GMB_DB_OK;
    return rc;
}

int DbfFieldParser_Parse(const DbfField* field, const uint8_t* data, int len, MemoFile* memo, char* out, int cap)
{
    uint8_t t;
    if (!field || !data || !out || cap <= 0) return GMB_DB_ARG;
    t = field->Type;
    switch (t) {
    case DbfFieldType_Character:
    case DbfFieldType_Varchar:
        return fp_parse_character(data, len, out, cap);
    case DbfFieldType_Date:
        return fp_parse_date(data, len, out, cap);
    case DbfFieldType_Float:
    case DbfFieldType_Numeric:
        return fp_parse_numeric(data, len, out, cap);
    case DbfFieldType_Integer:
    case DbfFieldType_AutoIncrement:
        return fp_parse_integer(data, len, out, cap);
    case DbfFieldType_Logical:
        return fp_parse_logical(data, len, out, cap);
    case DbfFieldType_Memo:
    case DbfFieldType_General:
    case DbfFieldType_Varbinary:
    case DbfFieldType_Blob:
        return fp_parse_memo(data, len, memo, out, cap);
    case DbfFieldType_Double:
        return fp_parse_double(data, len, out, cap);
    case DbfFieldType_DateTime:
        return fp_parse_timestamp(data, len, out, cap);
    case DbfFieldType_Currency:
        return fp_parse_currency(data, len, out, cap);
    default:
        return fp_parse_character(data, len, out, cap);
    }
}

int DbfFieldParser_SelfCheck(void)
{
    uint8_t buf[16];
    char out[32];
    DbfField f;
    memset(&f, 0, sizeof(f));
    f.Type = (uint8_t)DbfFieldType_Character;
    f.Length = 10;
    memcpy(buf, "HI        ", 10);
    if (DbfFieldParser_Parse(&f, buf, 10, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "HI") != 0)
        return -1;
    f.Type = (uint8_t)DbfFieldType_Numeric;
    memcpy(buf, " 12.50", 6);
    if (DbfFieldParser_Parse(&f, buf, 6, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "12.50") != 0)
        return -2;
    f.Type = (uint8_t)DbfFieldType_Logical;
    buf[0] = 'Y';
    if (DbfFieldParser_Parse(&f, buf, 1, NULL, out, sizeof(out)) != GMB_DB_OK || strcmp(out, "T") != 0)
        return -3;
    return GMB_DB_OK;
}
