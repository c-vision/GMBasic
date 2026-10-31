/* FILE: native/gmb_db_c/src/ISAM/IsamQuery.c */
/* Port 1:1 di modules/Database/ISAM/IsamQuery.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamQuery
//
//using Basic.Databases;
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Wrapper fluente minimo su <see cref="Engine"/> per scan ordinato, filtro e aggregazioni streaming.
///// </summary>
//public sealed class IsamQuery
//{
//    private readonly Engine _engine;
//    private readonly KeyExpressionEvaluatorCallback _keyEvaluator;
//    private Func<UdtInstance, bool>? _where;
//    private string? _whereField;
//    private object? _whereValue;
//    private string? _orderByIndex;
//    private int? _take;
//
//    /// <summary>Record esaminati dall'ultima operazione (1 = seek pushdown).</summary>
//    internal int LastRecordsExamined { get; private set; }
//
//    /// <summary>True se l'ultima operazione ha usato seek pushdown sull'indice attivo.</summary>
//    internal bool LastUsedSeekPushdown { get; private set; }
//
//    private IsamQuery(Engine engine, KeyExpressionEvaluatorCallback keyEvaluator)
//    {
//        _engine = engine;
//        _keyEvaluator = keyEvaluator;
//    }
//
//    public static IsamQuery From(Engine engine, KeyExpressionEvaluatorCallback? keyEvaluator = null) =>
//        new(engine, IsamExpressionBridge.ResolveKeyEvaluator(engine, keyEvaluator));
//
//    public IsamQuery Where(Func<UdtInstance, bool> predicate)
//    {
//        ArgumentNullException.ThrowIfNull(predicate);
//        _where = predicate;
//        _whereField = null;
//        _whereValue = null;
//        return this;
//    }
//
//    /// <summary>Filtro stringa MVP: <c>Field == value</c> abilita seek pushdown su chiave semplice.</summary>
//    public IsamQuery Where(string predicate)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(predicate);
//        if (!IsamQueryPredicate.TryParseEquality(predicate, out var fieldName, out var value))
//        {
//            throw new ArgumentException(
//                $"Predicato IsamQuery non supportato: '{predicate}'. MVP: Field == value.",
//                nameof(predicate));
//        }
//
//        _whereField = fieldName;
//        _whereValue = value;
//        _where = record => FieldEquals(record, fieldName, value);
//        return this;
//    }
//
//    public IsamQuery OrderBy(string indexName)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(indexName);
//        _orderByIndex = indexName;
//        return this;
//    }
//
//    public IsamQuery Take(int count)
//    {
//        if (count <= 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(count));
//        }
//
//        _take = count;
//        return this;
//    }
//
//    /// <summary>Esegue la query e materializza i record (rispetta Take).</summary>
//    public List<UdtInstance> Execute() => ToList();
//
//    public List<UdtInstance> ToList()
//    {
//        ApplyOrder();
//        if (TryExecuteSeekPushdown(out var seekResults))
//        {
//            return seekResults;
//        }
//
//        var results = new List<UdtInstance>();
//        _engine.Go(Config.CursorMoveOperation.Top);
//        while (!_engine.IsEof())
//        {
//            var record = _engine.GetCurrentRecord();
//            if (record != null && MatchesWhere(record))
//            {
//                results.Add(record);
//                if (_take is { } limit && results.Count >= limit)
//                {
//                    break;
//                }
//            }
//
//            _engine.Skip(1);
//            LastRecordsExamined++;
//        }
//
//        return results;
//    }
//
//    /// <summary>Conteggio streaming senza materializzare tutti i record.</summary>
//    public int Count()
//    {
//        ApplyOrder();
//        if (TryExecuteSeekPushdown(out var seekResults))
//        {
//            return seekResults.Count;
//        }
//
//        var count = 0;
//        _engine.Go(Config.CursorMoveOperation.Top);
//        while (!_engine.IsEof())
//        {
//            var record = _engine.GetCurrentRecord();
//            if (record != null && MatchesWhere(record))
//            {
//                count++;
//            }
//
//            _engine.Skip(1);
//            LastRecordsExamined++;
//        }
//
//        return count;
//    }
//
//    /// <summary>Somma numerica di un campo intero/long, scan streaming.</summary>
//    public long Sum(string fieldName)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(fieldName);
//        ApplyOrder();
//        if (TryExecuteSeekPushdown(out var seekResults))
//        {
//            long total = 0;
//            foreach (var record in seekResults)
//            {
//                total += ReadNumericField(record, fieldName);
//            }
//
//            return total;
//        }
//
//        long sum = 0;
//        _engine.Go(Config.CursorMoveOperation.Top);
//        while (!_engine.IsEof())
//        {
//            var record = _engine.GetCurrentRecord();
//            if (record != null && MatchesWhere(record))
//            {
//                sum += ReadNumericField(record, fieldName);
//            }
//
//            _engine.Skip(1);
//            LastRecordsExamined++;
//        }
//
//        return sum;
//    }
//
//    private void ApplyOrder()
//    {
//        LastRecordsExamined = 0;
//        LastUsedSeekPushdown = false;
//        if (_orderByIndex != null)
//        {
//            _engine.SetActiveOrder(_orderByIndex);
//        }
//    }
//
//    private bool MatchesWhere(UdtInstance record) => _where?.Invoke(record) ?? true;
//
//    private bool TryExecuteSeekPushdown(out List<UdtInstance> results)
//    {
//        results = [];
//        if (_whereField == null)
//        {
//            return false;
//        }
//
//        var indexDef = ResolveQueryIndexDefinition();
//        if (!IsamQueryPredicate.IsSimpleFieldKeyExpression(indexDef.KeyExpression, _whereField))
//        {
//            return false;
//        }
//
//        LastUsedSeekPushdown = true;
//        var seekKey = BuildSeekKey(indexDef);
//        _engine.Seek(seekKey);
//        LastRecordsExamined = 1;
//
//        if (!_engine.WasFound())
//        {
//            return true;
//        }
//
//        if (indexDef.IsUnique)
//        {
//            var record = _engine.GetCurrentRecord();
//            if (record != null && MatchesWhere(record))
//            {
//                results.Add(record);
//            }
//
//            return true;
//        }
//
//        // ponytail: indice non-unico — seek + scan in avanti finché la chiave coincide
//        var targetKey = seekKey.AsSpan();
//        while (!_engine.IsEof())
//        {
//            var record = _engine.GetCurrentRecord();
//            if (record == null)
//            {
//                break;
//            }
//
//            if (!RecordKeyMatches(indexDef, record, targetKey))
//            {
//                break;
//            }
//
//            if (MatchesWhere(record))
//            {
//                results.Add(record);
//                if (_take is { } limit && results.Count >= limit)
//                {
//                    break;
//                }
//            }
//
//            _engine.Skip(1);
//            LastRecordsExamined++;
//        }
//
//        return true;
//    }
//
//    private Config.IndexDefinition ResolveQueryIndexDefinition()
//    {
//        if (!string.IsNullOrEmpty(_orderByIndex))
//        {
//            return _engine.ResolveIndexDefinition(_orderByIndex);
//        }
//
//        return _engine.Schema.PrimaryKey;
//    }
//
//    private byte[] BuildSeekKey(Config.IndexDefinition indexDef)
//    {
//        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
//        {
//            [_whereField!] = _whereValue,
//        };
//        var targetType = _engine.GetFieldDataType(_whereField!)
//                             ?? new StringType(indexDef.KeyLength);
//        var keyValue = _keyEvaluator(indexDef.KeyExpression, context, targetType);
//        return SchemaManager.SerializeKeyValue(keyValue, indexDef);
//    }
//
//    private bool RecordKeyMatches(Config.IndexDefinition indexDef, UdtInstance record, ReadOnlySpan<byte> targetKey)
//    {
//        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        foreach (var field in record.Definition.Fields)
//        {
//            record.Fields.TryGetValue(field.SymbolId, out var value);
//            context[field.Name.Lexeme] = value;
//        }
//
//        var targetType = _engine.GetFieldDataType(_whereField!)
//                             ?? new StringType(indexDef.KeyLength);
//        var keyValue = _keyEvaluator(indexDef.KeyExpression, context, targetType);
//        var keyBytes = SchemaManager.SerializeKeyValue(keyValue, indexDef);
//        return keyBytes.AsSpan().SequenceEqual(targetKey);
//    }
//
//    private static bool FieldEquals(UdtInstance record, string fieldName, object? expected)
//    {
//        var field = record.Definition.Fields.FirstOrDefault(f =>
//            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
//        if (field is null || !record.Fields.TryGetValue(field.SymbolId, out var raw))
//        {
//            return false;
//        }
//
//        if (expected is long expectedLong && TryToInt64(raw, out var actualLong))
//        {
//            return actualLong == expectedLong;
//        }
//
//        return string.Equals(raw?.ToString(), expected?.ToString(), StringComparison.OrdinalIgnoreCase);
//    }
//
//    private static bool TryToInt64(object? value, out long result)
//    {
//        result = 0;
//        if (value is null)
//        {
//            return false;
//        }
//
//        switch (value)
//        {
//            case long l:
//                result = l;
//                return true;
//            case int i:
//                result = i;
//                return true;
//            case short s:
//                result = s;
//                return true;
//            case byte b:
//                result = b;
//                return true;
//            default:
//                return long.TryParse(value.ToString(), out result);
//        }
//    }
//
//    private static long ReadNumericField(UdtInstance record, string fieldName)
//    {
//        var field = record.Definition.Fields.FirstOrDefault(f =>
//            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
//        if (field is null || !record.Fields.TryGetValue(field.SymbolId, out var raw) || raw is null)
//        {
//            return 0;
//        }
//
//        return raw switch
//        {
//            long l => l,
//            int i => i,
//            short s => s,
//            byte b => b,
//            _ => long.TryParse(raw.ToString(), out var parsed) ? parsed : 0,
//        };
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

struct IsamQuery {
    Engine* engine;
    char whereField[64];
    char whereValue[256];
    int whereIsNum;
    int hasWhere;
    char orderBy[64];
    int take;
    int lastRecordsExamined;
    int lastUsedSeekPushdown;
};

static IsamFieldValue* query_alloc_fields(const IsamSchema* s)
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

static void query_free_fields(IsamFieldValue* fields, int n)
{
    int i;
    if (!fields) return;
    for (i = 0; i < n; i++) free(fields[i].textOut);
    free(fields);
}

static int query_read_current(Engine* e, IsamFieldValue** outFields, int* outN)
{
    const IsamSchema* s = Engine_GetSchema(e);
    IsamFieldValue* fields;
    int n, i;
    if (!s) return GMB_DB_ERR;
    n = s->FieldCount;
    fields = query_alloc_fields(s);
    if (!fields) return GMB_DB_ERR;
    if (Engine_GetCurrent(e, fields, n, NULL) != GMB_DB_OK) {
        query_free_fields(fields, n);
        return GMB_DB_ERR;
    }
    for (i = 0; i < n; i++) {
        if (fields[i].textOut && !fields[i].text)
            fields[i].text = fields[i].textOut;
    }
    *outFields = fields;
    *outN = n;
    return GMB_DB_OK;
}

static int query_field_index(const IsamSchema* s, const char* name)
{
    int i;
    for (i = 0; i < s->FieldCount; i++) {
        if (gmb_strcasecmp(s->Fields[i].Name, name) == 0) return i;
    }
    return -1;
}

static int query_field_equals(const IsamSchema* s, const IsamFieldValue* fields, const IsamQuery* q)
{
    int i, len;
    char buf[256];
    if (!q->hasWhere) return 1;
    i = query_field_index(s, q->whereField);
    if (i < 0) return 0;
    if (q->whereIsNum || strcmp(s->Fields[i].DataTypeCode, "StringType") != 0)
        return fields[i].i64 == atoll(q->whereValue);
    snprintf(buf, sizeof(buf), "%s", fields[i].text ? fields[i].text : "");
    len = (int)strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len - 1])) buf[--len] = 0;
    return gmb_strcasecmp(buf, q->whereValue) == 0;
}

static int64_t query_read_numeric(const IsamSchema* s, const IsamFieldValue* fields, const char* fieldName)
{
    int i = query_field_index(s, fieldName);
    if (i < 0) return 0;
    if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0)
        return fields[i].text ? atoll(fields[i].text) : 0;
    return fields[i].i64;
}

static const IsamIndexDefinition* query_resolve_index(const IsamQuery* q)
{
    const IsamSchema* s = Engine_GetSchema(q->engine);
    int i;
    if (!s) return NULL;
    if (!q->orderBy[0]) return &s->PrimaryKey;
    if (gmb_strcasecmp(q->orderBy, s->PrimaryKey.Name) == 0) return &s->PrimaryKey;
    for (i = 0; i < s->SecondaryKeyCount; i++) {
        if (gmb_strcasecmp(s->SecondaryKeys[i].Name, q->orderBy) == 0)
            return &s->SecondaryKeys[i];
    }
    return &s->PrimaryKey;
}

static void query_apply_order(IsamQuery* q)
{
    q->lastRecordsExamined = 0;
    q->lastUsedSeekPushdown = 0;
    if (q->orderBy[0])
        Engine_SetOrder(q->engine, q->orderBy);
}

static int query_build_seek_key(IsamQuery* q, const IsamIndexDefinition* def, uint8_t* key)
{
    const IsamSchema* s = Engine_GetSchema(q->engine);
    int i;
    if (!s || !def) return GMB_DB_ERR;
    i = query_field_index(s, q->whereField);
    if (i < 0) return GMB_DB_ERR;
    if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0)
        return SchemaManager_SerializeKeyValueText(q->whereValue, def->KeyLength, key);
    return SchemaManager_SerializeKeyValueI64(atoll(q->whereValue), def->KeyLength, key);
}

static int query_record_key_matches(IsamQuery* q, const IsamIndexDefinition* def, const IsamFieldValue* fields,
                                    const uint8_t* target, int keyLen)
{
    uint8_t* cur;
    int rc;
    const IsamSchema* s = Engine_GetSchema(q->engine);
    int i;
    if (!s) return 0;
    cur = (uint8_t*)malloc((size_t)keyLen);
    if (!cur) return 0;
    i = query_field_index(s, q->whereField);
    if (i < 0) { free(cur); return 0; }
    if (strcmp(s->Fields[i].DataTypeCode, "StringType") == 0)
        rc = SchemaManager_SerializeKeyValueText(fields[i].text, def->KeyLength, cur);
    else
        rc = SchemaManager_SerializeKeyValueI64(fields[i].i64, def->KeyLength, cur);
    rc = rc == GMB_DB_OK && memcmp(cur, target, (size_t)keyLen) == 0;
    free(cur);
    return rc;
}

typedef struct {
    int64_t* ids;
    int cap;
    int n;
    int64_t sum;
    const char* sumField;
    int count;
} QueryAcc;

static int query_accept(IsamQuery* q, QueryAcc* acc, const IsamSchema* s, const IsamFieldValue* fields)
{
    if (!query_field_equals(s, fields, q)) return 0;
    if (acc->ids && acc->n < acc->cap) acc->ids[acc->n] = fields[0].i64;
    acc->n++;
    acc->count++;
    if (acc->sumField) acc->sum += query_read_numeric(s, fields, acc->sumField);
    if (q->take > 0 && acc->n >= q->take) return 2;
    return 1;
}

static int query_try_seek_pushdown(IsamQuery* q, QueryAcc* acc)
{
    const IsamIndexDefinition* def;
    uint8_t* key;
    int keyLen, stop = 0;
    const IsamSchema* s;
    if (!q->hasWhere) return 0;
    def = query_resolve_index(q);
    if (!def || !IsamQueryPredicate_IsSimpleFieldKeyExpression(def->KeyExpression, q->whereField))
        return 0;
    q->lastUsedSeekPushdown = 1;
    keyLen = def->KeyLength;
    key = (uint8_t*)malloc((size_t)keyLen);
    if (!key) return 1;
    if (query_build_seek_key(q, def, key) != GMB_DB_OK) { free(key); return 1; }
    Engine_Seek(q->engine, key, keyLen);
    q->lastRecordsExamined = 1;
    if (!Engine_WasFound(q->engine)) { free(key); return 1; }
    s = Engine_GetSchema(q->engine);
    if (def->IsUnique) {
        IsamFieldValue* fields;
        int n;
        if (query_read_current(q->engine, &fields, &n) == GMB_DB_OK) {
            query_accept(q, acc, s, fields);
            query_free_fields(fields, n);
        }
        free(key);
        return 1;
    }
    while (!Engine_IsEof(q->engine) && !stop) {
        IsamFieldValue* fields;
        int n;
        if (query_read_current(q->engine, &fields, &n) != GMB_DB_OK) break;
        if (!query_record_key_matches(q, def, fields, key, keyLen)) {
            query_free_fields(fields, n);
            break;
        }
        stop = query_accept(q, acc, s, fields) == 2;
        query_free_fields(fields, n);
        if (stop) break;
        Engine_Skip(q->engine, 1);
        q->lastRecordsExamined++;
    }
    free(key);
    return 1;
}

static void query_scan(IsamQuery* q, QueryAcc* acc)
{
    const IsamSchema* s = Engine_GetSchema(q->engine);
    Engine_Go(q->engine, CursorMoveOperation_Top);
    while (!Engine_IsEof(q->engine)) {
        IsamFieldValue* fields;
        int n, r;
        if (query_read_current(q->engine, &fields, &n) == GMB_DB_OK) {
            r = query_accept(q, acc, s, fields);
            query_free_fields(fields, n);
            if (r == 2) return;
        }
        Engine_Skip(q->engine, 1);
        q->lastRecordsExamined++;
    }
}

static void query_run(IsamQuery* q, QueryAcc* acc)
{
    query_apply_order(q);
    if (!query_try_seek_pushdown(q, acc))
        query_scan(q, acc);
}

IsamQuery* IsamQuery_From(Engine* engine)
{
    IsamQuery* q;
    if (!engine) return NULL;
    q = (IsamQuery*)calloc(1, sizeof(*q));
    if (!q) return NULL;
    q->engine = engine;
    return q;
}

void IsamQuery_Dispose(IsamQuery* self)
{
    free(self);
}

int IsamQuery_Where(IsamQuery* self, const char* predicate)
{
    if (!self || !predicate || !*predicate) return GMB_DB_ARG;
    self->hasWhere = 0;
    self->whereField[0] = 0;
    self->whereValue[0] = 0;
    if (!IsamQueryPredicate_TryParseEquality(predicate, self->whereField, (int)sizeof(self->whereField),
                                             self->whereValue, (int)sizeof(self->whereValue),
                                             &self->whereIsNum))
        return GMB_DB_ERR;
    self->hasWhere = 1;
    return GMB_DB_OK;
}

int IsamQuery_OrderBy(IsamQuery* self, const char* indexName)
{
    if (!self || !indexName || !*indexName) return GMB_DB_ARG;
    snprintf(self->orderBy, sizeof(self->orderBy), "%s", indexName);
    return GMB_DB_OK;
}

int IsamQuery_Take(IsamQuery* self, int count)
{
    if (!self || count <= 0) return GMB_DB_ARG;
    self->take = count;
    return GMB_DB_OK;
}

int IsamQuery_Count(IsamQuery* self)
{
    QueryAcc acc;
    int savedTake;
    if (!self) return 0;
    memset(&acc, 0, sizeof(acc));
    query_apply_order(self);
    if (query_try_seek_pushdown(self, &acc))
        return acc.count;
    savedTake = self->take;
    self->take = 0;
    query_scan(self, &acc);
    self->take = savedTake;
    return acc.count;
}

int64_t IsamQuery_Sum(IsamQuery* self, const char* fieldName)
{
    QueryAcc acc;
    if (!self || !fieldName || !*fieldName) return 0;
    memset(&acc, 0, sizeof(acc));
    acc.sumField = fieldName;
    query_run(self, &acc);
    return acc.sum;
}

int IsamQuery_ToList(IsamQuery* self, int64_t* ids, int cap)
{
    QueryAcc acc;
    if (!self) return 0;
    memset(&acc, 0, sizeof(acc));
    acc.ids = ids;
    acc.cap = cap;
    query_run(self, &acc);
    return acc.n;
}

int IsamQuery_Execute(IsamQuery* self, int64_t* ids, int cap)
{
    return IsamQuery_ToList(self, ids, cap);
}

int IsamQuery_LastUsedSeekPushdown(const IsamQuery* self)
{
    return self ? self->lastUsedSeekPushdown : 0;
}

int IsamQuery_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_query_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamIndexDefinition sec;
    IsamSchema schema;
    Engine* e;
    IsamQuery* q;
    IsamFieldValue vals[2];
    int64_t ids[8];
    int i, n;

    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    memset(&sec, 0, sizeof(sec));
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
    snprintf(sec.Name, sizeof(sec.Name), "%s", "Name");
    snprintf(sec.KeyExpression, sizeof(sec.KeyExpression), "%s", "Name");
    sec.KeyLength = 20;
    schema.SecondaryKeys = &sec;
    schema.SecondaryKeyCount = 1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -1;
    e = Engine_Open(path, &schema);
    if (!e) return -2;
    for (i = 1; i <= 5; i++) {
        char nbuf[8];
        snprintf(nbuf, sizeof(nbuf), "N%d", i);
        memset(vals, 0, sizeof(vals));
        vals[0].i64 = i;
        vals[1].text = nbuf;
        if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -3; }
    }
    q = IsamQuery_From(e);
    if (!q) { Engine_Dispose(e); return -4; }
    if (IsamQuery_Count(q) != 5) { IsamQuery_Dispose(q); Engine_Dispose(e); return -5; }
    if (IsamQuery_Sum(q, "Id") != 15) { IsamQuery_Dispose(q); Engine_Dispose(e); return -6; }
    if (IsamQuery_Where(q, "Id=3") != GMB_DB_OK) { IsamQuery_Dispose(q); Engine_Dispose(e); return -7; }
    if (IsamQuery_Count(q) != 1 || !IsamQuery_LastUsedSeekPushdown(q)) {
        IsamQuery_Dispose(q); Engine_Dispose(e); return -8;
    }
    n = IsamQuery_ToList(q, ids, 8);
    if (n != 1 || ids[0] != 3) { IsamQuery_Dispose(q); Engine_Dispose(e); return -9; }
    IsamQuery_Dispose(q);
    q = IsamQuery_From(e);
    if (IsamQuery_Where(q, "Name='N3'") != GMB_DB_OK) { IsamQuery_Dispose(q); Engine_Dispose(e); return -10; }
    if (IsamQuery_Count(q) != 1) { IsamQuery_Dispose(q); Engine_Dispose(e); return -11; }
    if (IsamQuery_LastUsedSeekPushdown(q)) { IsamQuery_Dispose(q); Engine_Dispose(e); return -12; }
    if (IsamQuery_OrderBy(q, "Name") != GMB_DB_OK) { IsamQuery_Dispose(q); Engine_Dispose(e); return -13; }
    if (IsamQuery_Count(q) != 1 || !IsamQuery_LastUsedSeekPushdown(q)) {
        IsamQuery_Dispose(q); Engine_Dispose(e); return -14;
    }
    IsamQuery_Dispose(q);
    q = IsamQuery_From(e);
    if (IsamQuery_Take(q, 2) != GMB_DB_OK) { IsamQuery_Dispose(q); Engine_Dispose(e); return -15; }
    n = IsamQuery_ToList(q, ids, 8);
    if (n != 2 || ids[0] != 1 || ids[1] != 2) { IsamQuery_Dispose(q); Engine_Dispose(e); return -16; }
    IsamQuery_Dispose(q);
    Engine_Dispose(e);
    return GMB_DB_OK;
}
