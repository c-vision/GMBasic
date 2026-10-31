// FILE: Basic.ISAM.IsamQuery

using Basic.Databases;
using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Wrapper fluente minimo su <see cref="Engine"/> per scan ordinato, filtro e aggregazioni streaming.
/// </summary>
public sealed class IsamQuery
{
    private readonly Engine _engine;
    private readonly KeyExpressionEvaluatorCallback _keyEvaluator;
    private Func<UdtInstance, bool>? _where;
    private string? _whereField;
    private object? _whereValue;
    private string? _orderByIndex;
    private int? _take;

    /// <summary>Record esaminati dall'ultima operazione (1 = seek pushdown).</summary>
    internal int LastRecordsExamined { get; private set; }

    /// <summary>True se l'ultima operazione ha usato seek pushdown sull'indice attivo.</summary>
    internal bool LastUsedSeekPushdown { get; private set; }

    private IsamQuery(Engine engine, KeyExpressionEvaluatorCallback keyEvaluator)
    {
        _engine = engine;
        _keyEvaluator = keyEvaluator;
    }

    public static IsamQuery From(Engine engine, KeyExpressionEvaluatorCallback? keyEvaluator = null) =>
        new(engine, IsamExpressionBridge.ResolveKeyEvaluator(engine, keyEvaluator));

    public IsamQuery Where(Func<UdtInstance, bool> predicate)
    {
        ArgumentNullException.ThrowIfNull(predicate);
        _where = predicate;
        _whereField = null;
        _whereValue = null;
        return this;
    }

    /// <summary>Filtro stringa MVP: <c>Field == value</c> abilita seek pushdown su chiave semplice.</summary>
    public IsamQuery Where(string predicate)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(predicate);
        if (!IsamQueryPredicate.TryParseEquality(predicate, out var fieldName, out var value))
        {
            throw new ArgumentException(
                $"Predicato IsamQuery non supportato: '{predicate}'. MVP: Field == value.",
                nameof(predicate));
        }

        _whereField = fieldName;
        _whereValue = value;
        _where = record => FieldEquals(record, fieldName, value);
        return this;
    }

    public IsamQuery OrderBy(string indexName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(indexName);
        _orderByIndex = indexName;
        return this;
    }

    public IsamQuery Take(int count)
    {
        if (count <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(count));
        }

        _take = count;
        return this;
    }

    /// <summary>Esegue la query e materializza i record (rispetta Take).</summary>
    public List<UdtInstance> Execute() => ToList();

    public List<UdtInstance> ToList()
    {
        ApplyOrder();
        if (TryExecuteSeekPushdown(out var seekResults))
        {
            return seekResults;
        }

        var results = new List<UdtInstance>();
        _engine.Go(Config.CursorMoveOperation.Top);
        while (!_engine.IsEof())
        {
            var record = _engine.GetCurrentRecord();
            if (record != null && MatchesWhere(record))
            {
                results.Add(record);
                if (_take is { } limit && results.Count >= limit)
                {
                    break;
                }
            }

            _engine.Skip(1);
            LastRecordsExamined++;
        }

        return results;
    }

    /// <summary>Conteggio streaming senza materializzare tutti i record.</summary>
    public int Count()
    {
        ApplyOrder();
        if (TryExecuteSeekPushdown(out var seekResults))
        {
            return seekResults.Count;
        }

        var count = 0;
        _engine.Go(Config.CursorMoveOperation.Top);
        while (!_engine.IsEof())
        {
            var record = _engine.GetCurrentRecord();
            if (record != null && MatchesWhere(record))
            {
                count++;
            }

            _engine.Skip(1);
            LastRecordsExamined++;
        }

        return count;
    }

    /// <summary>Somma numerica di un campo intero/long, scan streaming.</summary>
    public long Sum(string fieldName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(fieldName);
        ApplyOrder();
        if (TryExecuteSeekPushdown(out var seekResults))
        {
            long total = 0;
            foreach (var record in seekResults)
            {
                total += ReadNumericField(record, fieldName);
            }

            return total;
        }

        long sum = 0;
        _engine.Go(Config.CursorMoveOperation.Top);
        while (!_engine.IsEof())
        {
            var record = _engine.GetCurrentRecord();
            if (record != null && MatchesWhere(record))
            {
                sum += ReadNumericField(record, fieldName);
            }

            _engine.Skip(1);
            LastRecordsExamined++;
        }

        return sum;
    }

    private void ApplyOrder()
    {
        LastRecordsExamined = 0;
        LastUsedSeekPushdown = false;
        if (_orderByIndex != null)
        {
            _engine.SetActiveOrder(_orderByIndex);
        }
    }

    private bool MatchesWhere(UdtInstance record) => _where?.Invoke(record) ?? true;

    private bool TryExecuteSeekPushdown(out List<UdtInstance> results)
    {
        results = [];
        if (_whereField == null)
        {
            return false;
        }

        var indexDef = ResolveQueryIndexDefinition();
        if (!IsamQueryPredicate.IsSimpleFieldKeyExpression(indexDef.KeyExpression, _whereField))
        {
            return false;
        }

        LastUsedSeekPushdown = true;
        var seekKey = BuildSeekKey(indexDef);
        _engine.Seek(seekKey);
        LastRecordsExamined = 1;

        if (!_engine.WasFound())
        {
            return true;
        }

        if (indexDef.IsUnique)
        {
            var record = _engine.GetCurrentRecord();
            if (record != null && MatchesWhere(record))
            {
                results.Add(record);
            }

            return true;
        }

        // ponytail: indice non-unico — seek + scan in avanti finché la chiave coincide
        var targetKey = seekKey.AsSpan();
        while (!_engine.IsEof())
        {
            var record = _engine.GetCurrentRecord();
            if (record == null)
            {
                break;
            }

            if (!RecordKeyMatches(indexDef, record, targetKey))
            {
                break;
            }

            if (MatchesWhere(record))
            {
                results.Add(record);
                if (_take is { } limit && results.Count >= limit)
                {
                    break;
                }
            }

            _engine.Skip(1);
            LastRecordsExamined++;
        }

        return true;
    }

    private Config.IndexDefinition ResolveQueryIndexDefinition()
    {
        if (!string.IsNullOrEmpty(_orderByIndex))
        {
            return _engine.ResolveIndexDefinition(_orderByIndex);
        }

        return _engine.Schema.PrimaryKey;
    }

    private byte[] BuildSeekKey(Config.IndexDefinition indexDef)
    {
        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
        {
            [_whereField!] = _whereValue,
        };
        var targetType = _engine.GetFieldDataType(_whereField!)
                             ?? new StringType(indexDef.KeyLength);
        var keyValue = _keyEvaluator(indexDef.KeyExpression, context, targetType);
        return SchemaManager.SerializeKeyValue(keyValue, indexDef);
    }

    private bool RecordKeyMatches(Config.IndexDefinition indexDef, UdtInstance record, ReadOnlySpan<byte> targetKey)
    {
        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in record.Definition.Fields)
        {
            record.Fields.TryGetValue(field.SymbolId, out var value);
            context[field.Name.Lexeme] = value;
        }

        var targetType = _engine.GetFieldDataType(_whereField!)
                             ?? new StringType(indexDef.KeyLength);
        var keyValue = _keyEvaluator(indexDef.KeyExpression, context, targetType);
        var keyBytes = SchemaManager.SerializeKeyValue(keyValue, indexDef);
        return keyBytes.AsSpan().SequenceEqual(targetKey);
    }

    private static bool FieldEquals(UdtInstance record, string fieldName, object? expected)
    {
        var field = record.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
        if (field is null || !record.Fields.TryGetValue(field.SymbolId, out var raw))
        {
            return false;
        }

        if (expected is long expectedLong && TryToInt64(raw, out var actualLong))
        {
            return actualLong == expectedLong;
        }

        return string.Equals(raw?.ToString(), expected?.ToString(), StringComparison.OrdinalIgnoreCase);
    }

    private static bool TryToInt64(object? value, out long result)
    {
        result = 0;
        if (value is null)
        {
            return false;
        }

        switch (value)
        {
            case long l:
                result = l;
                return true;
            case int i:
                result = i;
                return true;
            case short s:
                result = s;
                return true;
            case byte b:
                result = b;
                return true;
            default:
                return long.TryParse(value.ToString(), out result);
        }
    }

    private static long ReadNumericField(UdtInstance record, string fieldName)
    {
        var field = record.Definition.Fields.FirstOrDefault(f =>
            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
        if (field is null || !record.Fields.TryGetValue(field.SymbolId, out var raw) || raw is null)
        {
            return 0;
        }

        return raw switch
        {
            long l => l,
            int i => i,
            short s => s,
            byte b => b,
            _ => long.TryParse(raw.ToString(), out var parsed) ? parsed : 0,
        };
    }
}
