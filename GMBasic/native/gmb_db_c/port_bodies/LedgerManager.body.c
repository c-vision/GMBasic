#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#ifndef _WIN32
#include <strings.h>
#endif

/* ponytail: fixed index slots (C# ConcurrentDictionary grows). Ceiling 4096 txs; upgrade: realloc. */
#define GMB_LEDGER_MAX_TX 4096
#define GMB_LEDGER_TX_TEXT 8192
#define GMB_LEDGER_MAX_POSTINGS 32
#define GMB_LEDGER_MAX_CACHE 64 /* ponytail: GetTransaction cache; upgrade: grow like ConcurrentDictionary */
#define GMB_LEDGER_MAX_COA 256
#define GMB_LEDGER_TYPE 64
#define GMB_LEDGER_DESC 128
#define GMB_LEDGER_MAX_FILTERED 4096 /* ponytail: GetFilteredEntries cap; C# List grows */
#define GMB_LEDGER_CSV_FIELD 256
#define GMB_LEDGER_MAX_CSV_COLS 32

typedef struct {
    int64_t txId;
    int n;
    LedgerEntry e[GMB_LEDGER_MAX_POSTINGS];
} LedgerTxCache;

struct LedgerContext {
    char* JournalFilePath;
    char* IndexFilePath;
    char* BudgetFilePath;
    int IsDirty;
    int BudgetsDirty;
    int nIndex;
    int64_t indexIds[GMB_LEDGER_MAX_TX];
    LedgerIndexRecord indexRecs[GMB_LEDGER_MAX_TX];
    int nCache;
    LedgerTxCache cache[GMB_LEDGER_MAX_CACHE];
    int nCoa;
    char coaName[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    char coaType[GMB_LEDGER_MAX_COA][GMB_LEDGER_TYPE];
    char coaDesc[GMB_LEDGER_MAX_COA][GMB_LEDGER_DESC];
    int nBudget;
    char budgetAccount[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    char budgetPeriod[GMB_LEDGER_MAX_COA][16];
    double budgetAmount[GMB_LEDGER_MAX_COA];
    pthread_mutex_t Lock;
};

/* .NET Path.ChangeExtension: replace last '.' in the filename, else append. */
static char* ledger_change_ext(const char* path, const char* ext)
{
    const char* slash;
    const char* base;
    const char* dot;
    size_t stem, elen;
    char* out;
    if (!path || !ext) return NULL;
    slash = strrchr(path, '/');
    base = slash ? slash + 1 : path;
    dot = strrchr(base, '.');
    stem = dot ? (size_t)(dot - path) : strlen(path);
    elen = strlen(ext);
    out = (char*)malloc(stem + elen + 1);
    if (!out) return NULL;
    memcpy(out, path, stem);
    memcpy(out + stem, ext, elen + 1);
    return out;
}

static void ledger_write7(FILE* f, unsigned v)
{
    while (v >= 0x80) {
        fputc((int)((v & 0x7Fu) | 0x80u), f);
        v >>= 7;
    }
    fputc((int)v, f);
}

static void ledger_write_bw_string(FILE* f, const char* s)
{
    unsigned n = (unsigned)strlen(s);
    ledger_write7(f, n);
    fwrite(s, 1, n, f);
}

static void ledger_write_le32(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
    fwrite(b, 1, 4, f);
}

static void ledger_write_le64(FILE* f, int64_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        fputc((int)((v >> (8 * i)) & 0xFF), f);
}

static int ledger_read7(FILE* f, unsigned* v)
{
    unsigned n = 0, shift = 0;
    for (;;) {
        int c = fgetc(f);
        if (c < 0) return -1;
        n |= ((unsigned)c & 0x7Fu) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift > 28) return -1;
    }
    *v = n;
    return 0;
}

static int ledger_read_bw_string(FILE* f, char* buf, int cap)
{
    unsigned n;
    if (ledger_read7(f, &n) != 0) return -1;
    if ((int)n >= cap) return -1;
    if (n > 0 && fread(buf, 1, n, f) != n) return -1;
    buf[n] = 0;
    return 0;
}

static int ledger_read_le32(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}

static int ledger_read_le64(FILE* f, int64_t* v)
{
    uint8_t b[8];
    int i;
    uint64_t u = 0;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 7; i >= 0; i--)
        u = (u << 8) | b[i];
    *v = (int64_t)u;
    return 0;
}

static long ledger_file_len(FILE* f)
{
    long cur = ftell(f), end;
    if (cur < 0) return -1;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

/* Match decimal.ToString(InvariantCulture) for values whose scale fits double.
 * ponytail: no System.Decimal; ceiling ~15 digits / lost trailing scale (100.50m→100.5). */
static void ledger_fmt_num(double v, char* out, int cap)
{
    char tmp[64];
    int i;
    snprintf(tmp, sizeof(tmp), "%.15f", v);
    if (strchr(tmp, '.')) {
        i = (int)strlen(tmp) - 1;
        while (i > 0 && tmp[i] == '0') tmp[i--] = 0;
        if (i > 0 && tmp[i] == '.') tmp[i] = 0;
    }
    snprintf(out, (size_t)cap, "%s", tmp);
}

static int ledger_index_find(const LedgerContext* ctx, int64_t txId)
{
    int i;
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] == txId) return i;
    }
    return -1;
}

static int64_t ledger_index_max_id(const LedgerContext* ctx)
{
    int64_t m = 0;
    int i;
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexIds[i] > m) m = ctx->indexIds[i];
    }
    return m;
}

static int ledger_index_put(LedgerContext* ctx, int64_t txId, int64_t offset, int32_t date)
{
    int i = ledger_index_find(ctx, txId);
    if (i >= 0) {
        ctx->indexRecs[i].FileOffset = offset;
        ctx->indexRecs[i].Date = date;
        return GMB_DB_OK;
    }
    if (ctx->nIndex >= GMB_LEDGER_MAX_TX) return GMB_DB_ERR;
    ctx->indexIds[ctx->nIndex] = txId;
    ctx->indexRecs[ctx->nIndex].FileOffset = offset;
    ctx->indexRecs[ctx->nIndex].Date = date;
    ctx->nIndex++;
    return GMB_DB_OK;
}

static void ledger_index_clear(LedgerContext* ctx)
{
    if (ctx) ctx->nIndex = 0;
}

static void ledger_entry_clear_owned(LedgerEntry* e)
{
    if (!e) return;
    free((void*)e->Description);
    free((void*)e->Account);
    free((void*)e->Comment);
    free((void*)e->Currency);
    memset(e, 0, sizeof(*e));
}

static void ledger_cache_slot_free(LedgerTxCache* slot)
{
    int i;
    if (!slot) return;
    for (i = 0; i < slot->n; i++)
        ledger_entry_clear_owned(&slot->e[i]);
    slot->n = 0;
    slot->txId = 0;
}

static void ledger_cache_clear(LedgerContext* ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < ctx->nCache; i++)
        ledger_cache_slot_free(&ctx->cache[i]);
    ctx->nCache = 0;
}

static int ledger_cache_find(const LedgerContext* ctx, int64_t txId)
{
    int i;
    for (i = 0; i < ctx->nCache; i++) {
        if (ctx->cache[i].txId == txId) return i;
    }
    return -1;
}

static char* ledger_dup(const char* s)
{
    return s ? strdup(s) : NULL;
}

static int ledger_is_blank(const char* s)
{
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static void ledger_rtrim(char* s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = 0;
}

static int ledger_glob_ci(const char* s, const char* pat)
{
    if (!s || !pat) return 0;
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*s) {
                if (ledger_glob_ci(s, pat)) return 1;
                s++;
            }
            return 0;
        }
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*pat))
            return 0;
        s++;
        pat++;
    }
    return *s == 0;
}

static int ledger_parse_ymd(const char* s, int32_t* out)
{
    int y, m, d;
    if (!s || !out) return 0;
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (y < 1 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    *out = y * 10000 + m * 100 + d;
    return 1;
}

static int ledger_date_match(const LedgerQueryFilter* f, int32_t d)
{
    if (!f || !f->hasDate) return 1;
    if (strcmp(f->dateOp, ">") == 0) return d > f->dateVal;
    if (strcmp(f->dateOp, ">=") == 0) return d >= f->dateVal;
    if (strcmp(f->dateOp, "<") == 0) return d < f->dateVal;
    if (strcmp(f->dateOp, "<=") == 0) return d <= f->dateVal;
    if (strcmp(f->dateOp, "=") == 0) return d == f->dateVal;
    return 1;
}

typedef struct {
    int64_t id;
    int32_t date;
} LedgerQueryRec;

static int ledger_query_rec_cmp(const void* a, const void* b)
{
    const LedgerQueryRec* x = (const LedgerQueryRec*)a;
    const LedgerQueryRec* y = (const LedgerQueryRec*)b;
    if (x->date != y->date) return x->date < y->date ? -1 : 1;
    if (x->id != y->id) return x->id < y->id ? -1 : 1;
    return 0;
}

static int ledger_name_cmp(const void* a, const void* b)
{
    return strcmp((const char*)a, (const char*)b);
}

static int ledger_coa_find(const LedgerContext* ctx, const char* name)
{
    int i;
    if (!ctx || !name) return -1;
    for (i = 0; i < ctx->nCoa; i++) {
        if (gmb_strcasecmp(ctx->coaName[i], name) == 0) return i;
    }
    return -1;
}

static int ledger_is_under_root(const char* account, const char* root)
{
    size_t n;
    if (!account || !root) return 0;
    if (gmb_strcasecmp(account, root) == 0) return 1;
    n = strlen(root);
    return gmb_strncasecmp(account, root, n) == 0 && account[n] == ':';
}

static void ledger_copy_trim(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    while (src && *src && isspace((unsigned char)*src)) src++;
    snprintf(dst, cap, "%s", src ? src : "");
    ledger_rtrim(dst);
}

static int ledger_budget_find(const LedgerContext* ctx, const char* account, const char* period)
{
    char a[GMB_LEDGER_NAME], p[16], key[GMB_LEDGER_NAME + 20], have[GMB_LEDGER_NAME + 20];
    int i;
    if (!ctx) return -1;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    snprintf(key, sizeof(key), "%s|%s", a, p);
    for (i = 0; i < ctx->nBudget; i++) {
        snprintf(have, sizeof(have), "%s|%s", ctx->budgetAccount[i], ctx->budgetPeriod[i]);
        if (gmb_strcasecmp(have, key) == 0) return i;
    }
    return -1;
}

static int ledger_try_parse_budget_period(const char* period, int* year, int* month)
{
    int y = 0, m = 0;
    if (!period || !year || !month) return 0;
    if (sscanf(period, "%d-%d", &y, &m) != 2) return 0;
    if (m < 1 || m > 12) return 0;
    *year = y;
    *month = m;
    return 1;
}

static int ledger_days_in_month(int y, int m)
{
    static const int d[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 0;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m];
}

typedef struct {
    char period[16];
    char account[GMB_LEDGER_NAME];
    double amount;
} LedgerBudgetRow;

static int ledger_budget_row_cmp(const void* a, const void* b)
{
    const LedgerBudgetRow* x = (const LedgerBudgetRow*)a;
    const LedgerBudgetRow* y = (const LedgerBudgetRow*)b;
    int c = strcmp(x->period, y->period);
    if (c) return c;
    return strcmp(x->account, y->account);
}

LedgerContext* LedgerContext_New(const char* journalPath)
{
    LedgerContext* ctx;
    if (!journalPath) return NULL;
    ctx = (LedgerContext*)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    pthread_mutex_init(&ctx->Lock, NULL);
    ctx->JournalFilePath = strdup(journalPath);
    ctx->IndexFilePath = ledger_change_ext(journalPath, ".idx");
    ctx->BudgetFilePath = ledger_change_ext(journalPath, ".budget");
    if (!ctx->JournalFilePath || !ctx->IndexFilePath || !ctx->BudgetFilePath) {
        LedgerContext_Dispose(ctx);
        return NULL;
    }
    return ctx;
}

void LedgerContext_Dispose(LedgerContext* self)
{
    int i;
    if (!self) return;
    for (i = 0; i < self->nCache; i++)
        ledger_cache_slot_free(&self->cache[i]);
    pthread_mutex_destroy(&self->Lock);
    free(self->JournalFilePath);
    free(self->IndexFilePath);
    free(self->BudgetFilePath);
    free(self);
}

const char* LedgerContext_JournalFilePath(const LedgerContext* self)
{
    return self ? self->JournalFilePath : NULL;
}

const char* LedgerContext_IndexFilePath(const LedgerContext* self)
{
    return self ? self->IndexFilePath : NULL;
}

const char* LedgerContext_BudgetFilePath(const LedgerContext* self)
{
    return self ? self->BudgetFilePath : NULL;
}

int LedgerContext_IsDirty(const LedgerContext* self)
{
    return self ? self->IsDirty : 0;
}

int LedgerManager_IndexCount(const LedgerContext* ctx)
{
    return ctx ? ctx->nIndex : 0;
}

int LedgerManager_TryGetIndex(const LedgerContext* ctx, int64_t txId, LedgerIndexRecord* out)
{
    int i;
    if (!ctx || !out) return 0;
    i = ledger_index_find(ctx, txId);
    if (i < 0) return 0;
    *out = ctx->indexRecs[i];
    return 1;
}

int LedgerManager_SaveIndexToFile(const char* indexPath, const LedgerContext* ctx)
{
    char* tmpPath;
    FILE* f;
    int i, n;
    size_t plen;
    if (!indexPath || !ctx) return GMB_DB_ARG;
    plen = strlen(indexPath);
    tmpPath = (char*)malloc(plen + 5);
    if (!tmpPath) return GMB_DB_ERR;
    memcpy(tmpPath, indexPath, plen);
    memcpy(tmpPath + plen, ".tmp", 5);
    f = fopen(tmpPath, "wb");
    if (!f) {
        free(tmpPath);
        return GMB_DB_ERR;
    }
    ledger_write_bw_string(f, "LDGIDX");
    ledger_write_le32(f, 1);
    ledger_write_le32(f, ctx->nIndex);
    for (i = 0; i < ctx->nIndex; i++) {
        ledger_write_le64(f, ctx->indexIds[i]);
        ledger_write_le64(f, ctx->indexRecs[i].FileOffset);
        ledger_write_le32(f, ctx->indexRecs[i].Date);
    }
    n = ferror(f) ? -1 : 0;
    fclose(f);
    if (n != 0) {
        unlink(tmpPath);
        free(tmpPath);
        return GMB_DB_ERR;
    }
    if (rename(tmpPath, indexPath) != 0) {
        unlink(tmpPath);
        free(tmpPath);
        return GMB_DB_ERR;
    }
    free(tmpPath);
    return GMB_DB_OK;
}

int LedgerManager_ReadIndexFromFile(const char* indexPath, LedgerContext* ctx)
{
    FILE* f;
    long len;
    char magic[16];
    int32_t version, count, date, i;
    int64_t txId, offset;
    if (!indexPath || !ctx) return GMB_DB_ARG;
    f = fopen(indexPath, "rb");
    if (!f) return GMB_DB_ERR;
    len = ledger_file_len(f);
    if (len < 14) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_bw_string(f, magic, (int)sizeof(magic)) != 0 || strcmp(magic, "LDGIDX") != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_le32(f, &version) != 0 || version != 1) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (ledger_read_le32(f, &count) != 0 || count < 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    for (i = 0; i < count; i++) {
        if (ledger_read_le64(f, &txId) != 0 || ledger_read_le64(f, &offset) != 0 || ledger_read_le32(f, &date) != 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        if (ledger_index_put(ctx, txId, offset, date) != GMB_DB_OK) {
            fclose(f);
            return GMB_DB_ERR;
        }
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_IsTransactionBalanced(LedgerEntry* entries, int n)
{
    int i, implicitCount = 0, implicitIdx = -1;
    double sum = 0.0;
    if (n == 0) return 1;
    if (!entries) return 0;
    for (i = 0; i < n; i++) {
        double rate = entries[i].ExchangeRate == 0.0 ? 1.0 : entries[i].ExchangeRate;
        if (!entries[i].AmountIsExplicit) {
            implicitCount++;
            implicitIdx = i;
        }
        sum += entries[i].Amount * rate;
    }
    if (implicitCount > 1) return 0;
    if (implicitCount != 1) return fabs(sum) < 0.0001;
    entries[implicitIdx].Amount = -sum;
    entries[implicitIdx].ExchangeRate = 1.0;
    return 1;
}

int LedgerManager_FormatAmountForJournal(double amount, const char* currency, double exchangeRate,
                                         char* out, int cap)
{
    char amt[64], rate[64];
    if (!out || cap < 8) return GMB_DB_ARG;
    ledger_fmt_num(amount, amt, (int)sizeof(amt));
    if (!currency || currency[0] == 0) {
        snprintf(out, (size_t)cap, "%s", amt);
        return GMB_DB_OK;
    }
    if (exchangeRate == 1.0) {
        snprintf(out, (size_t)cap, "%s %s", amt, currency);
        return GMB_DB_OK;
    }
    ledger_fmt_num(exchangeRate, rate, (int)sizeof(rate));
    snprintf(out, (size_t)cap, "%s %s @ %s", amt, currency, rate);
    return GMB_DB_OK;
}

/* Encoding.UTF8 StreamWriter constructed at Position 0: first Flush writes BOM at the
 * current stream position (after the optional last-byte seek). */
static int ledger_append_journal_text(const char* path, const char* text, long* outOffset)
{
    FILE* f;
    int extraNl;
    long last, newOffset;
    size_t tlen;
    static const unsigned char kUtf8Bom[3] = { 0xEF, 0xBB, 0xBF };
    if (!path || !text || !outOffset) return GMB_DB_ARG;
    tlen = strlen(text);
    f = fopen(path, "r+b");
    if (!f) f = fopen(path, "w+b");
    if (!f) return GMB_DB_ERR;
    extraNl = 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    newOffset = ftell(f);
    if (newOffset > 0) {
        if (fseek(f, -1, SEEK_END) != 0) {
            fclose(f);
            return GMB_DB_ERR;
        }
        last = fgetc(f);
        extraNl = (last != '\n');
        newOffset = ftell(f);
    }
    if (fwrite(kUtf8Bom, 1, 3, f) != 3) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (extraNl && fputc('\n', f) == EOF) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fwrite(text, 1, tlen, f) != tlen || fputc('\n', f) == EOF || fflush(f) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    *outOffset = newOffset;
    return GMB_DB_OK;
}

static int ledger_readline(FILE* f, char* buf, int cap)
{
    int n = 0, c;
    if (!f || cap < 2) return -1;
    c = fgetc(f);
    if (c == EOF) return -1;
    while (c != EOF && c != '\n') {
        if (c != '\r' && n < cap - 1)
            buf[n++] = (char)c;
        c = fgetc(f);
    }
    buf[n] = 0;
    return n;
}

static char* ledger_trim_inplace(char* s)
{
    char* e;
    unsigned char* u;
    if (!s) return s;
    u = (unsigned char*)s;
    /* .NET string.Trim() also strips U+FEFF (UTF-8 BOM decoded as a char). */
    while (*u && (isspace(*u) || (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF))) {
        if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) u += 3;
        else u++;
    }
    s = (char*)u;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static int ledger_parse_date(const char* s, int* y, int* m, int* d)
{
    int Y = 0, M = 0, D = 0;
    if (!s || sscanf(s, "%d-%d-%d", &Y, &M, &D) != 3)
        return 0;
    if (Y < 1 || M < 1 || M > 12 || D < 1 || D > 31)
        return 0;
    *y = Y;
    *m = M;
    *d = D;
    return 1;
}

static int ledger_split_ws(char* line, char** parts, int max)
{
    int n = 0;
    char* p = line;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        parts[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    return n;
}

static void ledger_join_parts(char** parts, int start, int n, char* out, int cap)
{
    int i, pos = 0;
    out[0] = 0;
    for (i = start; i < n; i++) {
        int used = snprintf(out + pos, (size_t)(cap - pos), "%s%s", i == start ? "" : " ", parts[i]);
        if (used < 0 || used >= cap - pos) break;
        pos += used;
    }
}

/* C# MyAmountRegex = \s{2,}(.*) */
static int ledger_split_amount(const char* line, char* account, int acap, char* amount, int ncap)
{
    const char* p = line;
    int alen;
    account[0] = 0;
    amount[0] = 0;
    while (*p) {
        if (isspace((unsigned char)*p) && p[1] && isspace((unsigned char)p[1])) {
            alen = (int)(p - line);
            while (alen > 0 && isspace((unsigned char)line[alen - 1])) alen--;
            if (alen >= acap) alen = acap - 1;
            memcpy(account, line, (size_t)alen);
            account[alen] = 0;
            while (*p && isspace((unsigned char)*p)) p++;
            snprintf(amount, (size_t)ncap, "%s", p);
            return 1;
        }
        p++;
    }
    snprintf(account, (size_t)acap, "%s", line);
    return 0;
}

/* C#: LedgerManager.Extended.ParseAmountWithCurrency — sequential, no Regex. */
static void ledger_parse_amount_with_currency(const char* amountStr, double* amount, char* currency, int ccap,
                                              double* rate)
{
    char tmp[128];
    const char* s;
    char* end;
    double a;
    int i, j, sign;
    currency[0] = 0;
    *amount = 0;
    *rate = 1.0;
    if (!amountStr) return;
    s = amountStr;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return;

        a = strtod(s, &end);
    if (end != s) {
        const char* p = end;
        while (*p && isspace((unsigned char)*p)) p++;
        if (isalpha((unsigned char)p[0]) && isalpha((unsigned char)p[1]) && isalpha((unsigned char)p[2])
            && (p[3] == 0 || isspace((unsigned char)p[3]))) {
            if (ccap > 3) {
                currency[0] = p[0];
                currency[1] = p[1];
                currency[2] = p[2];
                currency[3] = 0;
            }
            p += 3;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '@') {
                p++;
                while (*p && isspace((unsigned char)*p)) p++;
                *rate = strtod(p, &end);
                if (end == p) *rate = 1.0;
            }
            *amount = a;
            return;
        }
    }

    /* fallback: strip commas and leading $ */
    j = 0;
    sign = 1;
    for (i = 0; amountStr[i] && j < (int)sizeof(tmp) - 1; i++) {
        unsigned char c = (unsigned char)amountStr[i];
        if (c == ',') continue;
        if (j == 0 && (c == '+' || c == '-')) {
            if (c == '-') sign = -1;
            continue;
        }
        if (j == 0 && (c == '$' || c == ' ')) continue;
        tmp[j++] = (char)c;
    }
    tmp[j] = 0;
    *amount = sign * strtod(tmp, &end);
}

static int ledger_skip_bom(FILE* f)
{
    long pos = ftell(f);
    unsigned char b[3];
    if (pos < 0) return 0;
    if (fread(b, 1, 3, f) != 3) {
        fseek(f, pos, SEEK_SET);
        return 0;
    }
    if (b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) return 1;
    fseek(f, pos, SEEK_SET);
    return 0;
}

static int ledger_parse_transaction_block(FILE* f, int64_t txId, LedgerEntry* out, int cap, int* nOut)
{
    char raw[1024], headerCopy[1024];
    char* header;
    char* parts[32];
    int nparts, y = 0, m = 0, d = 0, i, n = 0, peek;
    char status = ' ';
    char description[512];
    if (!f || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    if (ledger_readline(f, raw, (int)sizeof(raw)) < 0) return GMB_DB_ERR;
    header = ledger_trim_inplace(raw);
    if (!header[0]) return GMB_DB_ERR;
    snprintf(headerCopy, sizeof(headerCopy), "%s", header);
    nparts = ledger_split_ws(headerCopy, parts, 32);
    if (nparts < 1 || !ledger_parse_date(parts[0], &y, &m, &d)) return GMB_DB_ERR;
    i = 1;
    if (nparts > 1 && parts[1][0] && parts[1][1] == 0 && (parts[1][0] == '*' || parts[1][0] == '!')) {
        status = parts[1][0];
        i = 2;
    }
    ledger_join_parts(parts, i, nparts, description, (int)sizeof(description));

    for (;;) {
        char line[1024], work[1024], *trimmed, account[256], amountStr[256], comment[256];
        char cur[8];
        double amt = 0, rate = 1.0;
        int explicitAmt, ci;
        peek = fgetc(f);
        if (peek == EOF) break;
        ungetc(peek, f);
        if (peek != ' ' && peek != '\t') break;
        if (ledger_readline(f, line, (int)sizeof(line)) < 0) break;
        snprintf(work, sizeof(work), "%s", line);
        trimmed = ledger_trim_inplace(work);
        if (!trimmed[0]) continue;
        comment[0] = 0;
        ci = 0;
        while (trimmed[ci] && trimmed[ci] != ';') ci++;
        if (trimmed[ci] == ';') {
            snprintf(comment, sizeof(comment), "%s", trimmed + ci + 1);
            ledger_trim_inplace(comment);
            trimmed[ci] = 0;
            ledger_trim_inplace(trimmed);
        }
        if (ledger_split_amount(trimmed, account, (int)sizeof(account), amountStr, (int)sizeof(amountStr))
            && amountStr[0]) {
            ledger_parse_amount_with_currency(amountStr, &amt, cur, (int)sizeof(cur), &rate);
            explicitAmt = 1;
        } else {
            snprintf(account, sizeof(account), "%s", trimmed);
            cur[0] = 0;
            explicitAmt = 0;
            amt = 0;
            rate = 1.0;
        }
        if (n >= cap) return GMB_DB_ARG;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].TransactionId = txId;
        out[n].DateY = y;
        out[n].DateM = m;
        out[n].DateD = d;
        out[n].Status = status;
        out[n].Description = ledger_dup(description);
        out[n].Account = ledger_dup(account);
        out[n].Amount = amt;
        out[n].Comment = comment[0] ? ledger_dup(comment) : NULL;
        out[n].AmountIsExplicit = explicitAmt;
        out[n].Currency = cur[0] ? ledger_dup(cur) : NULL;
        out[n].ExchangeRate = rate;
        n++;
    }
    if (!LedgerManager_IsTransactionBalanced(out, n)) {
        for (i = 0; i < n; i++) ledger_entry_clear_owned(&out[i]);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n; i++) out[i].TransactionId = txId;
    *nOut = n;
    return GMB_DB_OK;
}

int64_t LedgerManager_AddTransaction(LedgerContext* ctx, int year, int month, int day,
                                     const char* description, LedgerEntry* entries, int n)
{
    char text[GMB_LEDGER_TX_TEXT];
    char amt[128];
    int pos, i;
    long newOffset;
    int64_t txId;
    int32_t dateAsInt;

    if (!ctx || !description || !entries || n < 1) return GMB_DB_ARG;
    if (!LedgerManager_IsTransactionBalanced(entries, n)) return GMB_DB_ERR;

    pos = snprintf(text, sizeof(text), "%04d-%02d-%02d %s", year, month, day, description);
    if (pos < 0 || pos >= (int)sizeof(text)) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        int used;
        if (!entries[i].Account) return GMB_DB_ARG;
        if (LedgerManager_FormatAmountForJournal(entries[i].Amount, entries[i].Currency,
                                                 entries[i].ExchangeRate == 0.0 ? 1.0 : entries[i].ExchangeRate,
                                                 amt, (int)sizeof(amt)) != GMB_DB_OK)
            return GMB_DB_ERR;
        used = snprintf(text + pos, sizeof(text) - (size_t)pos, "\n    %s      %s", entries[i].Account, amt);
        if (used < 0 || used >= (int)sizeof(text) - pos) return GMB_DB_ERR;
        pos += used;
    }

    pthread_mutex_lock(&ctx->Lock);
    if (ledger_append_journal_text(ctx->JournalFilePath, text, &newOffset) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    txId = ledger_index_max_id(ctx) + 1;
    dateAsInt = year * 10000 + month * 100 + day;
    if (ledger_index_put(ctx, txId, newOffset, dateAsInt) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ctx->IsDirty = 1;
    pthread_mutex_unlock(&ctx->Lock);
    return txId;
}

int LedgerManager_GetTransaction(LedgerContext* ctx, int64_t txId, LedgerEntry* out, int cap, int* nOut)
{
    int slot;
    LedgerIndexRecord rec;
    FILE* f;
    LedgerTxCache tmp;
    if (!ctx || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    slot = ledger_cache_find(ctx, txId);
    if (slot >= 0) {
        if (ctx->cache[slot].n > cap) return GMB_DB_ARG;
        memcpy(out, ctx->cache[slot].e, (size_t)ctx->cache[slot].n * sizeof(LedgerEntry));
        *nOut = ctx->cache[slot].n;
        return GMB_DB_OK;
    }
    if (!LedgerManager_TryGetIndex(ctx, txId, &rec) || rec.FileOffset == -1)
        return GMB_DB_ERR;
    f = fopen(ctx->JournalFilePath, "rb");
    if (!f) return GMB_DB_ERR;
    if (fseek(f, (long)rec.FileOffset, SEEK_SET) != 0) {
        fclose(f);
        return GMB_DB_ERR;
    }
    ledger_skip_bom(f);
    memset(&tmp, 0, sizeof(tmp));
    if (ledger_parse_transaction_block(f, txId, tmp.e, GMB_LEDGER_MAX_POSTINGS, &tmp.n) != GMB_DB_OK) {
        fclose(f);
        ledger_cache_slot_free(&tmp);
        return GMB_DB_ERR;
    }
    fclose(f);
    tmp.txId = txId;
    if (ctx->nCache < GMB_LEDGER_MAX_CACHE) {
        ctx->cache[ctx->nCache] = tmp;
        slot = ctx->nCache++;
    } else {
        /* ponytail: cache full — evict slot 0 */
        ledger_cache_slot_free(&ctx->cache[0]);
        ctx->cache[0] = tmp;
        slot = 0;
    }
    if (ctx->cache[slot].n > cap) return GMB_DB_ARG;
    memcpy(out, ctx->cache[slot].e, (size_t)ctx->cache[slot].n * sizeof(LedgerEntry));
    *nOut = ctx->cache[slot].n;
    return GMB_DB_OK;
}

int LedgerManager_DeleteTransaction(LedgerContext* ctx, int64_t txId)
{
    LedgerEntry got[GMB_LEDGER_MAX_POSTINGS];
    char text[GMB_LEDGER_TX_TEXT];
    char amt[128];
    int n = 0, i, pos;
    long newOffset;
    double rate;
    if (!ctx) return 0;
    pthread_mutex_lock(&ctx->Lock);
    if (LedgerManager_GetTransaction(ctx, txId, got, GMB_LEDGER_MAX_POSTINGS, &n) != GMB_DB_OK || n < 1) {
        pthread_mutex_unlock(&ctx->Lock);
        return 0;
    }
    pos = snprintf(text, sizeof(text), "%04d-%02d-%02d * %s",
                   got[0].DateY, got[0].DateM, got[0].DateD,
                   got[0].Description ? got[0].Description : "");
    if (pos < 0 || pos >= (int)sizeof(text)) {
        pthread_mutex_unlock(&ctx->Lock);
        return 0;
    }
    for (i = 0; i < n; i++) {
        int used;
        rate = got[i].ExchangeRate == 0.0 ? 1.0 : got[i].ExchangeRate;
        if (LedgerManager_FormatAmountForJournal(-got[i].Amount, got[i].Currency, rate, amt, (int)sizeof(amt))
            != GMB_DB_OK) {
            pthread_mutex_unlock(&ctx->Lock);
            return 0;
        }
        used = snprintf(text + pos, sizeof(text) - (size_t)pos, "\n    %s      %s", got[i].Account, amt);
        if (used < 0 || used >= (int)sizeof(text) - pos) {
            pthread_mutex_unlock(&ctx->Lock);
            return 0;
        }
        pos += used;
    }
    if (ledger_append_journal_text(ctx->JournalFilePath, text, &newOffset) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return 0;
    }
    (void)newOffset;
    ctx->IsDirty = 1;
    pthread_mutex_unlock(&ctx->Lock);
    return 1;
}

static int ledger_line_has_date(const char* line)
{
    char tmp[1024];
    char* t;
    char* sp;
    int y = 0, m = 0, d = 0;
    snprintf(tmp, sizeof(tmp), "%s", line ? line : "");
    t = ledger_trim_inplace(tmp);
    if (!t[0] || !isdigit((unsigned char)t[0])) return 0;
    sp = t;
    while (*sp && *sp != ' ') sp++;
    if (*sp) *sp = 0;
    return ledger_parse_date(t, &y, &m, &d);
}

/* C#: ReadTransactionBlockAsText — unread next header via Seek; we use ftell/fseek
 * instead of UTF8.GetByteCount+NewLine (StreamReader buffer desync). */
static int ledger_read_block_as_text(FILE* f, char* out, int cap)
{
    char line[2048];
    int n, pos = 0;
    long unread;
    if (!f || !out || cap < 2) return -1;
    out[0] = 0;
    if (ledger_readline(f, line, (int)sizeof(line)) < 0) return 0;
    n = snprintf(out, (size_t)cap, "%s\n", line);
    if (n < 0 || n >= cap) return -1;
    pos = n;
    for (;;) {
        char tmp[2048];
        char* t;
        unread = ftell(f);
        if (unread < 0) return -1;
        if (ledger_readline(f, line, (int)sizeof(line)) < 0) break;
        snprintf(tmp, sizeof(tmp), "%s", line);
        t = ledger_trim_inplace(tmp);
        if (!t[0]) break;
        if (isdigit((unsigned char)t[0]) && ledger_line_has_date(t)) {
            fseek(f, unread, SEEK_SET);
            break;
        }
        n = snprintf(out + pos, (size_t)cap - (size_t)pos, "%s\n", line);
        if (n < 0 || n >= cap - pos) return -1;
        pos += n;
    }
    {
        char* s = ledger_trim_inplace(out);
        if (s != out) memmove(out, s, strlen(s) + 1);
    }
    return 1;
}

int LedgerManager_RebuildIndexFromFile(const char* journalPath, LedgerContext* ctx)
{
    FILE* f;
    char line[2048];
    long currentOffset = 0;
    int64_t txId = 0;
    if (!journalPath || !ctx) return GMB_DB_ARG;
    ledger_index_clear(ctx);
    f = fopen(journalPath, "rb");
    if (!f) return GMB_DB_OK;
    ledger_skip_bom(f);
    while (ledger_readline(f, line, (int)sizeof(line)) >= 0) {
        char tmp[2048];
        char* t;
        int y = 0, m = 0, d = 0;
        snprintf(tmp, sizeof(tmp), "%s", line);
        t = ledger_trim_inplace(tmp);
        /* C# checks line[0] on the raw StreamReader string; after a leading file BOM
         * that is already consumed. Mid-file AddTransaction BOMs decode as U+FEFF —
         * we skip BOM before the digit check so every header is found. */
        if (t[0] && strchr(";#%*", t[0]) == NULL && isdigit((unsigned char)t[0])
            && ledger_parse_date(t, &y, &m, &d)) {
            if (ledger_index_put(ctx, ++txId, currentOffset, y * 10000 + m * 100 + d) != GMB_DB_OK) {
                fclose(f);
                return GMB_DB_ERR;
            }
        }
        currentOffset = ftell(f);
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_PackJournal(LedgerContext* ctx)
{
    char packed[PATH_MAX];
    char block[GMB_LEDGER_TX_TEXT];
    FILE *in, *out;
    long offset;
    int64_t nextId;
    static const unsigned char kUtf8Bom[3] = { 0xEF, 0xBB, 0xBF };
    if (!ctx) return GMB_DB_ARG;
    pthread_mutex_lock(&ctx->Lock);
    snprintf(packed, sizeof(packed), "%s.packed", ctx->JournalFilePath);
    in = fopen(ctx->JournalFilePath, "rb");
    if (!in) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    out = fopen(packed, "wb");
    if (!out) {
        fclose(in);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    if (fwrite(kUtf8Bom, 1, 3, out) != 3) {
        fclose(in);
        fclose(out);
        unlink(packed);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ledger_skip_bom(in);
    ledger_index_clear(ctx);
    ledger_cache_clear(ctx);
    offset = 0;
    nextId = 1;
    for (;;) {
        int rc = ledger_read_block_as_text(in, block, (int)sizeof(block));
        size_t blen;
        if (rc < 0) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        if (rc == 0) break;
        {
            char* t = block;
            while (*t && isspace((unsigned char)*t)) t++;
            if (!*t) continue;
        }
        /* ponytail: Flush per blocco così FileOffset è reale. C# legge BaseStream.Position
         * senza Flush (buffer StreamWriter → offset spesso 0). */
        if (ledger_index_put(ctx, nextId, offset, 0) != GMB_DB_OK) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        blen = strlen(block);
        if (fwrite(block, 1, blen, out) != blen) {
            fclose(in);
            fclose(out);
            unlink(packed);
            pthread_mutex_unlock(&ctx->Lock);
            return GMB_DB_ERR;
        }
        if (blen == 0 || block[blen - 1] != '\n') {
            if (fputc('\n', out) == EOF) {
                fclose(in);
                fclose(out);
                unlink(packed);
                pthread_mutex_unlock(&ctx->Lock);
                return GMB_DB_ERR;
            }
        }
        fflush(out);
        offset = ftell(out);
        nextId++;
    }
    fclose(in);
    fclose(out);
    if (rename(packed, ctx->JournalFilePath) != 0) {
        unlink(packed);
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    if (ctx->IndexFilePath) unlink(ctx->IndexFilePath);
    if (LedgerManager_SaveIndexToFile(ctx->IndexFilePath, ctx) != GMB_DB_OK) {
        pthread_mutex_unlock(&ctx->Lock);
        return GMB_DB_ERR;
    }
    ctx->IsDirty = 0;
    pthread_mutex_unlock(&ctx->Lock);
    return GMB_DB_OK;
}

int LedgerManager_ParseLedgerQuery(const char* query, LedgerQueryFilter* out)
{
    const char *accAt, *dateAt, *p, *end;
    size_t n;
    if (!out) return GMB_DB_ARG;
    memset(out, 0, sizeof(*out));
    if (ledger_is_blank(query)) return GMB_DB_OK;

    accAt = strstr(query, "account:");
    if (accAt) {
        p = accAt + 8;
        if (*p == '"') {
            p++;
            end = strchr(p, '"');
            n = end ? (size_t)(end - p) : strlen(p);
        } else {
            end = p;
            while (*end && !isspace((unsigned char)*end)) end++;
            n = (size_t)(end - p);
        }
        if (n >= GMB_LEDGER_NAME) n = GMB_LEDGER_NAME - 1;
        if (n > 0) {
            memcpy(out->accountPat, p, n);
            out->accountPat[n] = 0;
            out->hasAccount = 1;
        }
    }

    dateAt = strstr(query, "date:");
    if (dateAt) {
        p = dateAt + 5;
        if (p[0] == '>' && p[1] == '=') {
            memcpy(out->dateOp, ">=", 3);
            p += 2;
        } else if (p[0] == '<' && p[1] == '=') {
            memcpy(out->dateOp, "<=", 3);
            p += 2;
        } else if (p[0] == '>' || p[0] == '<' || p[0] == '=') {
            out->dateOp[0] = p[0];
            out->dateOp[1] = 0;
            p++;
        }
        if (out->dateOp[0] && ledger_parse_ymd(p, &out->dateVal))
            out->hasDate = 1;
        else
            out->dateOp[0] = 0;
    }
    return GMB_DB_OK;
}

int LedgerManager_LoadChartOfAccounts(const char* coaPath, LedgerContext* ctx)
{
    FILE* f;
    char line[512];
    char *sep, *tab, *account, *desc, *colon;
    int slot;
    if (!coaPath || !ctx) return GMB_DB_ARG;
    f = fopen(coaPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (strchr(";#%*", line[0])) continue;
        sep = strstr(line, "  ");
        tab = strchr(line, '\t');
        if (tab && (!sep || tab < sep)) sep = tab;
        desc = "";
        if (sep) {
            if (*sep == '\t') {
                *sep = 0;
                desc = sep + 1;
            } else {
                *sep = 0;
                desc = sep + 2;
            }
            while (*desc == ' ' || *desc == '\t') desc++;
            ledger_rtrim(desc);
        }
        account = line;
        while (isspace((unsigned char)*account)) account++;
        ledger_rtrim(account);
        if (!*account) continue;
        colon = strchr(account, ':');
        slot = ledger_coa_find(ctx, account);
        if (slot < 0) {
            if (ctx->nCoa >= GMB_LEDGER_MAX_COA) {
                fclose(f);
                return GMB_DB_ERR;
            }
            slot = ctx->nCoa++;
        }
        snprintf(ctx->coaName[slot], GMB_LEDGER_NAME, "%s", account);
        snprintf(ctx->coaType[slot], GMB_LEDGER_TYPE, "%s",
                 colon ? "" : "Senza categoria");
        if (colon) {
            size_t tlen = (size_t)(colon - account);
            if (tlen >= GMB_LEDGER_TYPE) tlen = GMB_LEDGER_TYPE - 1;
            memcpy(ctx->coaType[slot], account, tlen);
            ctx->coaType[slot][tlen] = 0;
        }
        snprintf(ctx->coaDesc[slot], GMB_LEDGER_DESC, "%s", desc);
    }
    fclose(f);
    return GMB_DB_OK;
}

int LedgerManager_GetFilteredEntries(LedgerContext* ctx, const char* query, LedgerEntry* out, int cap, int* nOut)
{
    LedgerQueryFilter filters;
    LedgerQueryRec recs[GMB_LEDGER_MAX_TX];
    LedgerEntry tx[GMB_LEDGER_MAX_POSTINGS];
    int nRec = 0, i, t, tn, n = 0;
    if (!ctx || !out || !nOut || cap < 1) return GMB_DB_ARG;
    *nOut = 0;
    if (LedgerManager_ParseLedgerQuery(query, &filters) != GMB_DB_OK) return GMB_DB_ARG;
    for (i = 0; i < ctx->nIndex; i++) {
        if (ctx->indexRecs[i].FileOffset == -1) continue;
        if (!ledger_date_match(&filters, ctx->indexRecs[i].Date)) continue;
        recs[nRec].id = ctx->indexIds[i];
        recs[nRec].date = ctx->indexRecs[i].Date;
        nRec++;
    }
    qsort(recs, (size_t)nRec, sizeof(recs[0]), ledger_query_rec_cmp);
    for (i = 0; i < nRec; i++) {
        tn = 0;
        if (LedgerManager_GetTransaction(ctx, recs[i].id, tx, GMB_LEDGER_MAX_POSTINGS, &tn) != GMB_DB_OK)
            return GMB_DB_ERR;
        for (t = 0; t < tn; t++) {
            if (filters.hasAccount && !ledger_glob_ci(tx[t].Account ? tx[t].Account : "", filters.accountPat))
                continue;
            if (n >= cap) return GMB_DB_ARG;
            out[n++] = tx[t];
        }
    }
    *nOut = n;
    return GMB_DB_OK;
}

int LedgerManager_GetAccounts(LedgerContext* ctx, const char* rootAccount,
                              char names[][GMB_LEDGER_NAME], int maxNames, int* nOut)
{
    char tmp[GMB_LEDGER_MAX_COA][GMB_LEDGER_NAME];
    int nTmp = 0, i, j, n = 0;
    int hasRoot;
    if (!ctx || !names || !nOut || maxNames < 1) return GMB_DB_ARG;
    *nOut = 0;
    hasRoot = !ledger_is_blank(rootAccount);
    if (ctx->nCoa > 0) {
        for (i = 0; i < ctx->nCoa; i++)
            snprintf(tmp[nTmp++], GMB_LEDGER_NAME, "%s", ctx->coaName[i]);
    } else {
        LedgerEntry* entries;
        int ne = 0;
        entries = (LedgerEntry*)malloc(sizeof(LedgerEntry) * GMB_LEDGER_MAX_FILTERED);
        if (!entries) return GMB_DB_ERR;
        if (LedgerManager_GetFilteredEntries(ctx, "", entries, GMB_LEDGER_MAX_FILTERED, &ne) != GMB_DB_OK) {
            free(entries);
            return GMB_DB_ERR;
        }
        for (i = 0; i < ne && nTmp < GMB_LEDGER_MAX_COA; i++) {
            const char* acc = entries[i].Account ? entries[i].Account : "";
            int seen = 0;
            for (j = 0; j < nTmp; j++) {
                if (gmb_strcasecmp(tmp[j], acc) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen)
                snprintf(tmp[nTmp++], GMB_LEDGER_NAME, "%s", acc);
        }
        free(entries);
    }
    qsort(tmp, (size_t)nTmp, sizeof(tmp[0]), ledger_name_cmp);
    for (i = 0; i < nTmp; i++) {
        char* rootTrim;
        char rootBuf[GMB_LEDGER_NAME];
        if (hasRoot) {
            snprintf(rootBuf, sizeof(rootBuf), "%s", rootAccount);
            rootTrim = rootBuf;
            while (isspace((unsigned char)*rootTrim)) rootTrim++;
            ledger_rtrim(rootTrim);
            if (!ledger_is_under_root(tmp[i], rootTrim)) continue;
        }
        if (n >= maxNames) return GMB_DB_ARG;
        snprintf(names[n++], GMB_LEDGER_NAME, "%s", tmp[i]);
    }
    *nOut = n;
    return GMB_DB_OK;
}

int LedgerManager_SetBudget(LedgerContext* ctx, const char* account, double amount, const char* period)
{
    char a[GMB_LEDGER_NAME], p[16];
    int slot;
    if (!ctx) return GMB_DB_ARG;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    if (!a[0] || !p[0]) return GMB_DB_ARG;
    slot = ledger_budget_find(ctx, a, p);
    if (slot < 0) {
        if (ctx->nBudget >= GMB_LEDGER_MAX_COA) return GMB_DB_ERR;
        slot = ctx->nBudget++;
    }
    snprintf(ctx->budgetAccount[slot], GMB_LEDGER_NAME, "%s", a);
    snprintf(ctx->budgetPeriod[slot], sizeof(ctx->budgetPeriod[slot]), "%s", p);
    ctx->budgetAmount[slot] = amount;
    ctx->BudgetsDirty = 1;
    return GMB_DB_OK;
}

int LedgerManager_LoadBudgets(const char* budgetPath, LedgerContext* ctx)
{
    FILE* f;
    char line[512];
    char *acc, *amtStr, *per, *rest;
    if (!budgetPath || !ctx) return GMB_DB_ARG;
    ctx->nBudget = 0;
    ctx->BudgetsDirty = 0;
    f = fopen(budgetPath, "r");
    if (!f) return GMB_DB_OK;
    while (fgets(line, (int)sizeof(line), f)) {
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (strchr(";#%*", line[0])) continue;
        acc = line;
        while (*acc && isspace((unsigned char)*acc)) acc++;
        amtStr = acc;
        while (*amtStr && !isspace((unsigned char)*amtStr)) amtStr++;
        if (!*amtStr) continue;
        *amtStr++ = 0;
        while (*amtStr && isspace((unsigned char)*amtStr)) amtStr++;
        per = amtStr;
        while (*per && !isspace((unsigned char)*per)) per++;
        if (!*per) continue;
        *per++ = 0;
        while (*per && isspace((unsigned char)*per)) per++;
        rest = per;
        while (*rest && !isspace((unsigned char)*rest)) rest++;
        *rest = 0;
        if (!*acc || !*amtStr || !*per) continue;
        {
            char* end = NULL;
            double amt = strtod(amtStr, &end);
            if (end == amtStr) continue;
            if (LedgerManager_SetBudget(ctx, acc, amt, per) != GMB_DB_OK) {
                fclose(f);
                return GMB_DB_ERR;
            }
        }
    }
    fclose(f);
    ctx->BudgetsDirty = 0;
    return GMB_DB_OK;
}

int LedgerManager_SaveBudgets(const char* budgetPath, LedgerContext* ctx)
{
    char tmpPath[PATH_MAX];
    FILE* f;
    LedgerBudgetRow rows[GMB_LEDGER_MAX_COA];
    int i, n;
    size_t plen;
    if (!budgetPath || !ctx) return GMB_DB_ARG;
    plen = strlen(budgetPath);
    if (plen + 5 >= sizeof(tmpPath)) return GMB_DB_ARG;
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", budgetPath);
    n = ctx->nBudget;
    for (i = 0; i < n; i++) {
        snprintf(rows[i].account, GMB_LEDGER_NAME, "%s", ctx->budgetAccount[i]);
        snprintf(rows[i].period, sizeof(rows[i].period), "%s", ctx->budgetPeriod[i]);
        rows[i].amount = ctx->budgetAmount[i];
    }
    qsort(rows, (size_t)n, sizeof(rows[0]), ledger_budget_row_cmp);
    f = fopen(tmpPath, "w");
    if (!f) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        if (fprintf(f, "%s  %.15g  %s\n", rows[i].account, rows[i].amount, rows[i].period) < 0) {
            fclose(f);
            unlink(tmpPath);
            return GMB_DB_ERR;
        }
    }
    if (fclose(f) != 0) {
        unlink(tmpPath);
        return GMB_DB_ERR;
    }
    unlink(budgetPath);
    if (rename(tmpPath, budgetPath) != 0) {
        unlink(tmpPath);
        return GMB_DB_ERR;
    }
    ctx->BudgetsDirty = 0;
    return GMB_DB_OK;
}

int LedgerManager_GetVariance(LedgerContext* ctx, const char* account, const char* period,
                              LedgerBudgetVariance* out)
{
    char a[GMB_LEDGER_NAME], p[16], query[GMB_LEDGER_NAME + 16];
    LedgerEntry* got;
    int year, month, startInt, endInt, n = 0, i, slot, rc;
    double actual = 0.0, budget = 0.0;
    if (!ctx || !out) return GMB_DB_ARG;
    ledger_copy_trim(a, sizeof(a), account);
    ledger_copy_trim(p, sizeof(p), period);
    if (!a[0] || !p[0]) return GMB_DB_ARG;
    if (!ledger_try_parse_budget_period(p, &year, &month)) return GMB_DB_ARG;
    slot = ledger_budget_find(ctx, a, p);
    if (slot >= 0) budget = ctx->budgetAmount[slot];
    startInt = year * 10000 + month * 100 + 1;
    endInt = year * 10000 + month * 100 + ledger_days_in_month(year, month);
    snprintf(query, sizeof(query), "account:%s", a);
    got = (LedgerEntry*)malloc(sizeof(LedgerEntry) * GMB_LEDGER_MAX_FILTERED);
    if (!got) return GMB_DB_ERR;
    rc = LedgerManager_GetFilteredEntries(ctx, query, got, GMB_LEDGER_MAX_FILTERED, &n);
    if (rc != GMB_DB_OK) {
        free(got);
        return rc;
    }
    for (i = 0; i < n; i++) {
        int d = got[i].DateY * 10000 + got[i].DateM * 100 + got[i].DateD;
        if (d < startInt || d > endInt) continue;
        actual += got[i].Amount * got[i].ExchangeRate;
    }
    free(got);
    memset(out, 0, sizeof(*out));
    snprintf(out->Account, GMB_LEDGER_NAME, "%s", a);
    snprintf(out->Period, sizeof(out->Period), "%s", p);
    out->BudgetAmount = budget;
    out->ActualAmount = actual;
    out->Variance = budget - actual;
    return GMB_DB_OK;
}

static int ledger_contains_ci(const char* s, const char* needle)
{
    size_t n;
    if (!s || !needle || !*needle) return 0;
    n = strlen(needle);
    for (; *s; s++) {
        if (gmb_strncasecmp(s, needle, n) == 0) return 1;
    }
    return 0;
}

static int ledger_split_csv_line(const char* line, char fields[][GMB_LEDGER_CSV_FIELD], int maxFields)
{
    int n = 0, inQuotes = 0, cl = 0;
    char cur[GMB_LEDGER_CSV_FIELD];
    const char* p;
    if (!line || !fields || maxFields < 1) return 0;
    cur[0] = 0;
    for (p = line; *p && *p != '\n' && *p != '\r'; p++) {
        if (*p == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (*p == ',' && !inQuotes) {
            if (n >= maxFields) return n;
            cur[cl] = 0;
            snprintf(fields[n++], GMB_LEDGER_CSV_FIELD, "%s", cur);
            cl = 0;
            cur[0] = 0;
            continue;
        }
        if (cl < GMB_LEDGER_CSV_FIELD - 1) cur[cl++] = (char)*p;
    }
    if (n < maxFields) {
        cur[cl] = 0;
        snprintf(fields[n++], GMB_LEDGER_CSV_FIELD, "%s", cur);
    }
    return n;
}

static int ledger_parse_import_mapping(const char* rules, int* dateCol, int* accountCol, int* amountCol,
                                       int* descCol, char* contra, int contraCap)
{
    char buf[512];
    char* tok;
    char* save = NULL;
    if (!rules || !dateCol || !accountCol || !amountCol || !descCol || !contra || contraCap < 2)
        return GMB_DB_ARG;
    *dateCol = *accountCol = *amountCol = -1;
    *descCol = 0; /* C#: TryGetValue miss → default 0 */
    snprintf(contra, (size_t)contraCap, "Equity:Import");
    snprintf(buf, sizeof(buf), "%s", rules);
    for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char key[64], val[GMB_LEDGER_NAME];
        char* eq;
        char* end = NULL;
        long col;
        while (*tok && isspace((unsigned char)*tok)) tok++;
        ledger_rtrim(tok);
        eq = strchr(tok, '=');
        if (!eq || eq == tok) continue;
        *eq = 0;
        ledger_copy_trim(key, sizeof(key), tok);
        ledger_copy_trim(val, sizeof(val), eq + 1);
        if (!key[0]) continue;
        if (gmb_strcasecmp(key, "contra") == 0) {
            snprintf(contra, (size_t)contraCap, "%s", val);
            continue;
        }
        col = strtol(val, &end, 10);
        if (end == val) continue;
        if (gmb_strcasecmp(key, "date") == 0) *dateCol = (int)col;
        else if (gmb_strcasecmp(key, "account") == 0) *accountCol = (int)col;
        else if (gmb_strcasecmp(key, "amount") == 0) *amountCol = (int)col;
        else if (gmb_strcasecmp(key, "desc") == 0) *descCol = (int)col;
    }
    if (*dateCol < 0 || *accountCol < 0 || *amountCol < 0) return GMB_DB_ARG;
    return GMB_DB_OK;
}

typedef struct {
    char name[GMB_LEDGER_NAME];
    double amount;
} LedgerPlotRow;

static int ledger_plot_row_cmp(const void* a, const void* b)
{
    return strcmp(((const LedgerPlotRow*)a)->name, ((const LedgerPlotRow*)b)->name);
}

int LedgerManager_ImportCsv(LedgerContext* ctx, const char* csvPath, const char* mappingRules, int* importedOut)
{
    FILE* f;
    char line[4096];
    char fields[GMB_LEDGER_MAX_CSV_COLS][GMB_LEDGER_CSV_FIELD];
    char contra[GMB_LEDGER_NAME];
    int dateCol, accountCol, amountCol, descCol, imported = 0, first = 1, nCols, maxCol, y, m, d;
    if (importedOut) *importedOut = 0;
    if (!ctx || ledger_is_blank(csvPath) || ledger_is_blank(mappingRules)) return GMB_DB_ARG;
    if (ledger_parse_import_mapping(mappingRules, &dateCol, &accountCol, &amountCol, &descCol, contra,
                                    (int)sizeof(contra)) != GMB_DB_OK)
        return GMB_DB_ARG;
    maxCol = dateCol;
    if (accountCol > maxCol) maxCol = accountCol;
    if (amountCol > maxCol) maxCol = amountCol;
    f = fopen(csvPath, "r");
    if (!f) return GMB_DB_ERR;
    while (fgets(line, (int)sizeof(line), f)) {
        LedgerEntry entries[2];
        char *end = NULL, desc[GMB_LEDGER_NAME], acc[GMB_LEDGER_NAME];
        double amt;
        int64_t id;
        ledger_rtrim(line);
        if (ledger_is_blank(line)) continue;
        if (first && ledger_contains_ci(line, "date")) {
            first = 0;
            continue;
        }
        first = 0;
        nCols = ledger_split_csv_line(line, fields, GMB_LEDGER_MAX_CSV_COLS);
        if (nCols <= maxCol) continue;
        ledger_copy_trim(acc, sizeof(acc), fields[dateCol]);
        if (sscanf(acc, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) continue;
        ledger_copy_trim(acc, sizeof(acc), fields[amountCol]);
        amt = strtod(acc, &end);
        if (end == acc) continue;
        ledger_copy_trim(acc, sizeof(acc), fields[accountCol]);
        if (!acc[0]) continue;
        if (descCol >= 0 && descCol < nCols)
            ledger_copy_trim(desc, sizeof(desc), fields[descCol]);
        else
            snprintf(desc, sizeof(desc), "CSV import");
        memset(entries, 0, sizeof(entries));
        entries[0].Account = acc;
        entries[0].Amount = amt;
        entries[0].AmountIsExplicit = 1;
        entries[0].ExchangeRate = 1.0;
        entries[1].Account = contra;
        entries[1].Amount = -amt;
        entries[1].AmountIsExplicit = 1;
        entries[1].ExchangeRate = 1.0;
        /* ponytail: C# holds Lock for the whole import; AddTransaction already locks per row */
        id = LedgerManager_AddTransaction(ctx, y, m, d, desc, entries, 2);
        if (id < 1) {
            fclose(f);
            return GMB_DB_ERR;
        }
        imported++;
    }
    fclose(f);
    if (importedOut) *importedOut = imported;
    return GMB_DB_OK;
}

int LedgerManager_GetBalancePlotData(LedgerContext* ctx, const char* query,
                                     char labels[][GMB_LEDGER_NAME], double* amounts, int max, int* nOut)
{
    LedgerEntry* got;
    LedgerPlotRow rows[GMB_LEDGER_MAX_COA];
    int n = 0, nRows = 0, i, j;
    if (!ctx || !labels || !amounts || !nOut || max < 1) return GMB_DB_ARG;
    *nOut = 0;
    got = (LedgerEntry*)malloc(sizeof(LedgerEntry) * GMB_LEDGER_MAX_FILTERED);
    if (!got) return GMB_DB_ERR;
    if (LedgerManager_GetFilteredEntries(ctx, query, got, GMB_LEDGER_MAX_FILTERED, &n) != GMB_DB_OK) {
        free(got);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n && nRows < GMB_LEDGER_MAX_COA; i++) {
        const char* acc = got[i].Account ? got[i].Account : "";
        double base = got[i].Amount * got[i].ExchangeRate;
        int found = 0;
        for (j = 0; j < nRows; j++) {
            if (gmb_strcasecmp(rows[j].name, acc) == 0) {
                rows[j].amount += base;
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(rows[nRows].name, GMB_LEDGER_NAME, "%s", acc);
            rows[nRows].amount = base;
            nRows++;
        }
    }
    free(got);
    qsort(rows, (size_t)nRows, sizeof(rows[0]), ledger_plot_row_cmp);
    if (nRows > max) return GMB_DB_ARG;
    for (i = 0; i < nRows; i++) {
        snprintf(labels[i], GMB_LEDGER_NAME, "%s", rows[i].name);
        amounts[i] = rows[i].amount;
    }
    *nOut = nRows;
    return GMB_DB_OK;
}

int LedgerManager_AppendAuditLogEntry(LedgerContext* ctx, const char* description)
{
    time_t now;
    struct tm utc;
    LedgerEntry entries[2];
    int64_t id;
    if (!ctx || ledger_is_blank(description)) return GMB_DB_ARG;
    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &utc)) return GMB_DB_ERR;
    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Audit:MCP";
    entries[0].Amount = 0.0;
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Audit:MCP:Clearing";
    entries[1].Amount = 0.0;
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                                      description, entries, 2);
    return id < 1 ? GMB_DB_ERR : GMB_DB_OK;
}

static void ledger_selfcheck_rm(const char* path)
{
    if (path) unlink(path);
}

int LedgerManager_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgersc_XXXXXX";
    char journal[PATH_MAX];
    char buf[128];
    LedgerContext *ctx, *reopen;
    LedgerEntry entries[2];
    LedgerEntry got[4];
    LedgerIndexRecord rec;
    FILE* f;
    unsigned char head[32];
    size_t nread;
    int64_t id;
    char* text;
    long jlen;
    int gn, bomN, bi;

    if (!mkdtemp(dir)) return -2;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);

    ctx = LedgerContext_New(journal);
    if (!ctx) return -3;
    if (!ctx->IndexFilePath || !strstr(ctx->IndexFilePath, ".idx")) {
        LedgerContext_Dispose(ctx);
        return -4;
    }
    if (!ctx->BudgetFilePath || !strstr(ctx->BudgetFilePath, ".budget")) {
        LedgerContext_Dispose(ctx);
        return -5;
    }

    if (LedgerManager_FormatAmountForJournal(100.0, NULL, 1.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100") != 0) {
        LedgerContext_Dispose(ctx);
        return -6;
    }
    if (LedgerManager_FormatAmountForJournal(100.0, "USD", 1.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100 USD") != 0) {
        LedgerContext_Dispose(ctx);
        return -7;
    }
    if (LedgerManager_FormatAmountForJournal(100.0, "USD", 2.0, buf, (int)sizeof(buf)) != GMB_DB_OK
        || strcmp(buf, "100 USD @ 2") != 0) {
        LedgerContext_Dispose(ctx);
        return -8;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Cash";
    entries[0].Amount = 100.0;
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Equity";
    entries[1].Amount = -99.0;
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    if (LedgerManager_IsTransactionBalanced(entries, 2)) {
        LedgerContext_Dispose(ctx);
        return -9;
    }
    entries[1].Amount = -100.0;
    if (!LedgerManager_IsTransactionBalanced(entries, 2)) {
        LedgerContext_Dispose(ctx);
        return -10;
    }
    entries[1].Amount = 0.0;
    entries[1].AmountIsExplicit = 0;
    if (!LedgerManager_IsTransactionBalanced(entries, 2) || entries[1].Amount != -100.0) {
        LedgerContext_Dispose(ctx);
        return -11;
    }

    entries[0].Account = "Cash";
    entries[0].Amount = 100.0;
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[0].Currency = NULL;
    entries[1].Account = "Equity";
    entries[1].Amount = -100.0;
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    entries[1].Currency = NULL;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 20, "seed", entries, 2);
    if (id != 1) {
        LedgerContext_Dispose(ctx);
        return -12;
    }
    if (!LedgerContext_IsDirty(ctx)) {
        LedgerContext_Dispose(ctx);
        return -13;
    }
    if (!LedgerManager_TryGetIndex(ctx, 1, &rec) || rec.FileOffset != 0 || rec.Date != 20260820) {
        LedgerContext_Dispose(ctx);
        return -14;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2) {
        LedgerContext_Dispose(ctx);
        return -30;
    }
    if (!got[0].Account || strcmp(got[0].Account, "Cash") != 0 || fabs(got[0].Amount - 100.0) >= 0.0001
        || !got[1].Account || strcmp(got[1].Account, "Equity") != 0 || fabs(got[1].Amount + 100.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -31;
    }
    if (got[0].DateY != 2026 || got[0].DateM != 8 || got[0].DateD != 20
        || !got[0].Description || strcmp(got[0].Description, "seed") != 0
        || got[0].TransactionId != 1) {
        LedgerContext_Dispose(ctx);
        return -32;
    }
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2) {
        LedgerContext_Dispose(ctx);
        return -33;
    }
    if (LedgerManager_GetTransaction(ctx, 99, got, 4, &gn) != GMB_DB_ERR) {
        LedgerContext_Dispose(ctx);
        return -34;
    }

    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -15;
    }
    nread = fread(head, 1, sizeof(head), f);
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -16;
    }
    jlen = ftell(f);
    fclose(f);
    if (nread < 18 || head[0] != 0xEF || head[1] != 0xBB || head[2] != 0xBF) {
        LedgerContext_Dispose(ctx);
        return -17;
    }
    if (memcmp(head + 3, "2026-08-20 seed", 15) != 0) {
        LedgerContext_Dispose(ctx);
        return -18;
    }

    text = (char*)malloc((size_t)jlen + 1);
    if (!text) {
        LedgerContext_Dispose(ctx);
        return -19;
    }
    f = fopen(journal, "rb");
    if (!f || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        if (f) fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -20;
    }
    fclose(f);
    text[jlen] = 0;
    if (!strstr(text, "    Cash      100") || !strstr(text, "    Equity      -100")) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -21;
    }
    free(text);

    if (LedgerManager_SaveIndexToFile(ctx->IndexFilePath, ctx) != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        return -22;
    }
    f = fopen(ctx->IndexFilePath, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -23;
    }
    nread = fread(head, 1, 11, f);
    fclose(f);
    if (nread != 11 || head[0] != 0x06 || memcmp(head + 1, "LDGIDX", 6) != 0
        || head[7] != 1 || head[8] != 0 || head[9] != 0 || head[10] != 0) {
        LedgerContext_Dispose(ctx);
        return -24;
    }

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        return -25;
    }
    if (LedgerManager_ReadIndexFromFile(ctx->IndexFilePath, reopen) != GMB_DB_OK) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -26;
    }
    if (LedgerManager_IndexCount(reopen) != 1
        || !LedgerManager_TryGetIndex(reopen, 1, &rec)
        || rec.FileOffset != 0 || rec.Date != 20260820) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -27;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(reopen, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Account || strcmp(got[0].Account, "Cash") != 0) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -35;
    }
    LedgerContext_Dispose(reopen);

    entries[0].Amount = 50.0;
    entries[1].Amount = -50.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "follow", entries, 2);
    if (id != 2) {
        LedgerContext_Dispose(ctx);
        return -28;
    }
    if (!LedgerManager_TryGetIndex(ctx, 2, &rec) || rec.Date != 20260821 || rec.FileOffset <= 0) {
        LedgerContext_Dispose(ctx);
        return -29;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 2, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "follow") != 0
        || fabs(got[0].Amount - 50.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -36;
    }
    if (!LedgerManager_DeleteTransaction(ctx, 1)) {
        LedgerContext_Dispose(ctx);
        return -37;
    }
    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -38;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -39;
    }
    jlen = ftell(f);
    rewind(f);
    text = (char*)malloc((size_t)jlen + 1);
    if (!text || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -40;
    }
    fclose(f);
    text[jlen] = 0;
    if (!strstr(text, " * seed") || !strstr(text, "    Cash      -100")) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -41;
    }
    free(text);

    if (LedgerManager_PackJournal(ctx) != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        return -42;
    }
    if (LedgerContext_IsDirty(ctx) || LedgerManager_IndexCount(ctx) != 3) {
        LedgerContext_Dispose(ctx);
        return -43;
    }
    if (!LedgerManager_TryGetIndex(ctx, 1, &rec) || rec.Date != 0
        || !LedgerManager_TryGetIndex(ctx, 3, &rec) || rec.Date != 0) {
        LedgerContext_Dispose(ctx);
        return -44;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "seed") != 0
        || fabs(got[0].Amount - 100.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -45;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 2, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "follow") != 0) {
        LedgerContext_Dispose(ctx);
        return -46;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 3, got, 4, &gn) != GMB_DB_OK || gn != 2
        || got[0].Status != '*' || fabs(got[0].Amount + 100.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        return -47;
    }
    f = fopen(journal, "rb");
    if (!f) {
        LedgerContext_Dispose(ctx);
        return -48;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        LedgerContext_Dispose(ctx);
        return -49;
    }
    jlen = ftell(f);
    rewind(f);
    text = (char*)malloc((size_t)jlen + 1);
    if (!text || (long)fread(text, 1, (size_t)jlen, f) != jlen) {
        fclose(f);
        free(text);
        LedgerContext_Dispose(ctx);
        return -50;
    }
    fclose(f);
    bomN = 0;
    for (bi = 0; bi < (int)jlen - 2; bi++) {
        if ((unsigned char)text[bi] == 0xEF && (unsigned char)text[bi + 1] == 0xBB
            && (unsigned char)text[bi + 2] == 0xBF)
            bomN++;
    }
    if (bomN != 1 || text[0] != (char)0xEF) {
        free(text);
        LedgerContext_Dispose(ctx);
        return -51;
    }
    free(text);

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        return -52;
    }
    if (LedgerManager_RebuildIndexFromFile(journal, reopen) != GMB_DB_OK
        || LedgerManager_IndexCount(reopen) != 3
        || !LedgerManager_TryGetIndex(reopen, 1, &rec) || rec.Date != 20260820
        || !LedgerManager_TryGetIndex(reopen, 2, &rec) || rec.Date != 20260821
        || !LedgerManager_TryGetIndex(reopen, 3, &rec) || rec.Date != 20260820) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        return -53;
    }
    LedgerContext_Dispose(reopen);

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Query_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerq_XXXXXX";
    char journal[PATH_MAX];
    char coaPath[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry entries[2];
    LedgerEntry got[16];
    LedgerQueryFilter q;
    char names[8][GMB_LEDGER_NAME];
    FILE* f;
    int n, nn;
    int64_t id;

    if (!mkdtemp(dir)) return -60;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    snprintf(coaPath, sizeof(coaPath), "%s/chart.coa", dir);

    ctx = LedgerContext_New(journal);
    if (!ctx) return -61;

    if (LedgerManager_ParseLedgerQuery("  ", &q) != GMB_DB_OK || q.hasDate || q.hasAccount) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -62;
    }
    if (LedgerManager_ParseLedgerQuery("account:\"Expenses:Office\" date:>=2026-08-21", &q) != GMB_DB_OK
        || !q.hasAccount || strcmp(q.accountPat, "Expenses:Office") != 0
        || !q.hasDate || strcmp(q.dateOp, ">=") != 0 || q.dateVal != 20260821) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -63;
    }
    if (LedgerManager_ParseLedgerQuery("account:Expenses:*", &q) != GMB_DB_OK
        || !q.hasAccount || strcmp(q.accountPat, "Expenses:*") != 0 || q.hasDate) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -64;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Assets:Cash";
    entries[0].Amount = 100.0;
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Equity";
    entries[1].Amount = -100.0;
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 20, "seed", entries, 2);
    if (id < 1) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -65;
    }

    entries[0].Account = "Expenses:Office";
    entries[0].Amount = 40.0;
    entries[1].Account = "Assets:Cash";
    entries[1].Amount = -40.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "office", entries, 2);
    if (id < 1) {
        ledger_selfcheck_rm(ctx->IndexFilePath);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -66;
    }

    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "", got, 16, &n) != GMB_DB_OK || n != 4) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -67;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "account:Assets:Cash", got, 16, &n) != GMB_DB_OK || n != 2
        || !got[0].Account || strcmp(got[0].Account, "Assets:Cash") != 0
        || !got[1].Account || strcmp(got[1].Account, "Assets:Cash") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -68;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "date:>=2026-08-21", got, 16, &n) != GMB_DB_OK || n != 2
        || !got[0].Description || strcmp(got[0].Description, "office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -69;
    }
    n = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "account:Expenses:*", got, 16, &n) != GMB_DB_OK || n != 1
        || !got[0].Account || strcmp(got[0].Account, "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -70;
    }

    nn = 0;
    if (LedgerManager_GetAccounts(ctx, NULL, names, 8, &nn) != GMB_DB_OK || nn != 3
        || strcmp(names[0], "Assets:Cash") != 0
        || strcmp(names[1], "Equity") != 0
        || strcmp(names[2], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -71;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, "assets", names, 8, &nn) != GMB_DB_OK || nn != 1
        || strcmp(names[0], "Assets:Cash") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -72;
    }

    f = fopen(coaPath, "w");
    if (!f) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -73;
    }
    fputs("; comment\n", f);
    fputs("Assets:Cash  Liquid\n", f);
    fputs("Expenses:Office\tOffice costs\n", f);
    fclose(f);
    if (LedgerManager_LoadChartOfAccounts(coaPath, ctx) != GMB_DB_OK || ctx->nCoa != 2) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -74;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, NULL, names, 8, &nn) != GMB_DB_OK || nn != 2
        || strcmp(names[0], "Assets:Cash") != 0
        || strcmp(names[1], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -75;
    }
    nn = 0;
    if (LedgerManager_GetAccounts(ctx, "Expenses", names, 8, &nn) != GMB_DB_OK || nn != 1
        || strcmp(names[0], "Expenses:Office") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(coaPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -76;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    ledger_selfcheck_rm(ctx->BudgetFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    ledger_selfcheck_rm(coaPath);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Budget_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerb_XXXXXX";
    char journal[PATH_MAX];
    LedgerContext *ctx, *reopen;
    LedgerEntry entries[2];
    LedgerBudgetVariance v;
    FILE* f;
    char line[128];
    int64_t id;

    if (!mkdtemp(dir)) return -80;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -81;

    if (LedgerManager_LoadBudgets(ctx->BudgetFilePath, ctx) != GMB_DB_OK || ctx->nBudget != 0) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -82;
    }
    if (LedgerManager_SetBudget(ctx, "  ", 100.0, "2026-08") != GMB_DB_ARG
        || LedgerManager_SetBudget(ctx, "Expenses:Office", 100.0, "  ") != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -83;
    }
    if (LedgerManager_SetBudget(ctx, "Expenses:Office", 100.0, "2026-08") != GMB_DB_OK
        || !ctx->BudgetsDirty || ctx->nBudget != 1) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -84;
    }
    if (LedgerManager_GetVariance(ctx, "Expenses:Office", "2026-13", &v) != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -85;
    }

    memset(entries, 0, sizeof(entries));
    entries[0].Account = "Expenses:Office";
    entries[0].Amount = 40.0;
    entries[0].AmountIsExplicit = 1;
    entries[0].ExchangeRate = 1.0;
    entries[1].Account = "Assets:Cash";
    entries[1].Amount = -40.0;
    entries[1].AmountIsExplicit = 1;
    entries[1].ExchangeRate = 1.0;
    id = LedgerManager_AddTransaction(ctx, 2026, 8, 21, "office", entries, 2);
    if (id < 1) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -86;
    }
    memset(&v, 0, sizeof(v));
    if (LedgerManager_GetVariance(ctx, "Expenses:Office", "2026-08", &v) != GMB_DB_OK
        || fabs(v.BudgetAmount - 100.0) >= 0.0001
        || fabs(v.ActualAmount - 40.0) >= 0.0001
        || fabs(v.Variance - 60.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -87;
    }
    if (LedgerManager_SaveBudgets(ctx->BudgetFilePath, ctx) != GMB_DB_OK || ctx->BudgetsDirty) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -88;
    }
    f = fopen(ctx->BudgetFilePath, "r");
    if (!f || !fgets(line, (int)sizeof(line), f)) {
        if (f) fclose(f);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -89;
    }
    fclose(f);
    if (!strstr(line, "Expenses:Office") || !strstr(line, "2026-08")) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -90;
    }

    reopen = LedgerContext_New(journal);
    if (!reopen) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -91;
    }
    if (LedgerManager_LoadBudgets(ctx->BudgetFilePath, reopen) != GMB_DB_OK
        || reopen->nBudget != 1 || reopen->BudgetsDirty) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -92;
    }
    if (LedgerManager_SetBudget(reopen, "expenses:office", 100.0, "2026-08") != GMB_DB_OK
        || reopen->nBudget != 1) {
        LedgerContext_Dispose(reopen);
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -93;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    ledger_selfcheck_rm(ctx->BudgetFilePath);
    LedgerContext_Dispose(reopen);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Csv_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgerc_XXXXXX";
    char journal[PATH_MAX];
    char csvPath[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry got[8];
    char labels[8][GMB_LEDGER_NAME];
    double amounts[8];
    FILE* f;
    int imported = 0, n = 0, gn = 0;

    if (!mkdtemp(dir)) return -100;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    snprintf(csvPath, sizeof(csvPath), "%s/bank.csv", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -101;

    if (LedgerManager_ImportCsv(ctx, "", "date=0,account=2,amount=3", &imported) != GMB_DB_ARG
        || LedgerManager_ImportCsv(ctx, csvPath, "account=2,amount=3", &imported) != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -102;
    }

    f = fopen(csvPath, "w");
    if (!f) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -103;
    }
    fputs("Date,Desc,Account,Amount\n", f);
    fputs("2026-08-21,Coffee,Expenses:Office,12.5\n", f);
    fputs("2026-08-21,Rent,Expenses:Rent,800\n", f);
    fputs("2026-08-22,\"Lunch, team\",Expenses:Food,20\n", f);
    fputs("\n", f);
    fputs("not-a-date,Skip,Expenses:X,10\n", f);
    fputs("2026-08-23,Bad,Expenses:X,notnum\n", f);
    fclose(f);

    imported = -1;
    if (LedgerManager_ImportCsv(ctx, csvPath,
                                "date=0,desc=1,account=2,amount=3,contra=Assets:Bank",
                                &imported) != GMB_DB_OK || imported != 3) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -104;
    }
    gn = 0;
    if (LedgerManager_GetFilteredEntries(ctx, "", got, 8, &gn) != GMB_DB_OK || gn != 6) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -105;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 3, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "Lunch, team") != 0
        || !got[0].Account || strcmp(got[0].Account, "Expenses:Food") != 0
        || fabs(got[0].Amount - 20.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -106;
    }

    n = 0;
    if (LedgerManager_GetBalancePlotData(ctx, "", labels, amounts, 8, &n) != GMB_DB_OK || n != 4
        || strcmp(labels[0], "Assets:Bank") != 0 || fabs(amounts[0] + 832.5) >= 0.0001
        || strcmp(labels[1], "Expenses:Food") != 0 || fabs(amounts[1] - 20.0) >= 0.0001
        || strcmp(labels[2], "Expenses:Office") != 0 || fabs(amounts[2] - 12.5) >= 0.0001
        || strcmp(labels[3], "Expenses:Rent") != 0 || fabs(amounts[3] - 800.0) >= 0.0001) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -107;
    }
    n = 0;
    if (LedgerManager_GetBalancePlotData(ctx, "account:Expenses:*", labels, amounts, 8, &n) != GMB_DB_OK
        || n != 3
        || strcmp(labels[0], "Expenses:Food") != 0
        || strcmp(labels[1], "Expenses:Office") != 0
        || strcmp(labels[2], "Expenses:Rent") != 0) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(csvPath);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -108;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    ledger_selfcheck_rm(csvPath);
    rmdir(dir);
    return GMB_DB_OK;
}

int LedgerManager_Audit_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ledgera_XXXXXX";
    char journal[PATH_MAX];
    LedgerContext* ctx;
    LedgerEntry got[4];
    int gn = 0;

    if (!mkdtemp(dir)) return -110;
    snprintf(journal, sizeof(journal), "%s/book.dat", dir);
    ctx = LedgerContext_New(journal);
    if (!ctx) return -111;

    if (LedgerManager_AppendAuditLogEntry(ctx, "  ") != GMB_DB_ARG
        || LedgerManager_AppendAuditLogEntry(NULL, "mcp ping") != GMB_DB_ARG) {
        LedgerContext_Dispose(ctx);
        rmdir(dir);
        return -112;
    }
    if (LedgerManager_AppendAuditLogEntry(ctx, "mcp ping") != GMB_DB_OK) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -113;
    }
    gn = 0;
    if (LedgerManager_GetTransaction(ctx, 1, got, 4, &gn) != GMB_DB_OK || gn != 2
        || !got[0].Description || strcmp(got[0].Description, "mcp ping") != 0
        || !got[0].Account || strcmp(got[0].Account, "Audit:MCP") != 0
        || !got[1].Account || strcmp(got[1].Account, "Audit:MCP:Clearing") != 0
        || fabs(got[0].Amount) >= 0.0001 || fabs(got[1].Amount) >= 0.0001
        || got[0].DateY < 2026) {
        LedgerContext_Dispose(ctx);
        ledger_selfcheck_rm(journal);
        rmdir(dir);
        return -114;
    }

    ledger_selfcheck_rm(ctx->IndexFilePath);
    LedgerContext_Dispose(ctx);
    ledger_selfcheck_rm(journal);
    rmdir(dir);
    return GMB_DB_OK;
}
