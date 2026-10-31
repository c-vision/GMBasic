// FILE: Basic.Databases.Replication.ReplicationLogLine

using System.Text.Json;

namespace Basic.Databases.Replication;

internal enum ReplicationOp : byte
{
    Append = 1,
    Put = 2,
    Delete = 3,
    Recall = 4,
    TxnBegin = 10,
    TxnCommit = 11,
    Pack = 20,
    Schema = 21,
    Reindex = 22,
}

internal static class ReplicationLogLine
{
    public static string FormatTxnMarker(ReplicationOp op) =>
        op switch
        {
            ReplicationOp.TxnBegin => "TXN_BEGIN\t0",
            ReplicationOp.TxnCommit => "TXN_COMMIT\t0",
            _ => throw new ArgumentOutOfRangeException(nameof(op), op, "Expected TXN_BEGIN or TXN_COMMIT."),
        };

    public static string FormatPack(long generation)
    {
        if (generation <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(generation), generation, "PACK generation must be positive.");
        }

        return $"{ReplicationOp.Pack.ToString().ToUpperInvariant()}\t{generation}";
    }

    /// <summary>
    /// SCHEMA payload carries a generation and artifact bytes (ISCH or .gms).
    /// ponytail: ISAM apply writes ISCH block then REINDEX; no in-place field migration.
    /// </summary>
    public static string FormatSchema(IReadOnlyDictionary<string, object?> metadata)
    {
        ArgumentNullException.ThrowIfNull(metadata);
        return $"{ReplicationOp.Schema.ToString().ToUpperInvariant()}\t0\t{JsonSerializer.Serialize(metadata)}";
    }

    public static string FormatReindex(long generation)
    {
        if (generation <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(generation), generation, "REINDEX generation must be positive.");
        }

        return $"{ReplicationOp.Reindex.ToString().ToUpperInvariant()}\t{generation}";
    }

    public static string Format(ReplicationOp op, long recNo, IReadOnlyDictionary<string, object?>? values = null)
    {
        if (op is ReplicationOp.TxnBegin or ReplicationOp.TxnCommit)
        {
            return FormatTxnMarker(op);
        }

        if (recNo <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(recNo), recNo, "Record number must be positive.");
        }

        var opName = op.ToString().ToUpperInvariant();
        if (values is null or { Count: 0 })
        {
            return $"{opName}\t{recNo}";
        }

        return $"{opName}\t{recNo}\t{JsonSerializer.Serialize(values)}";
    }

    public static bool TryParse(
        string line,
        out ReplicationOp op,
        out long recNo,
        out Dictionary<string, object?>? values)
    {
        op = default;
        recNo = 0;
        values = null;

        if (string.IsNullOrWhiteSpace(line))
        {
            return false;
        }

        var parts = line.Split('\t');
        if (parts.Length is < 2 or > 3)
        {
            return false;
        }

        if (!TryParseOp(parts[0], out op))
        {
            return false;
        }

        if (!long.TryParse(parts[1], out recNo))
        {
            return false;
        }

        if (recNo <= 0 && op is not ReplicationOp.TxnBegin and not ReplicationOp.TxnCommit and not ReplicationOp.Schema)
        {
            return false;
        }

        if (parts.Length == 3)
        {
            values = JsonSerializer.Deserialize<Dictionary<string, object?>>(parts[2]);
            if (values is null)
            {
                return false;
            }
        }

        return true;
    }

    private static bool TryParseOp(string name, out ReplicationOp op) =>
        name.ToUpperInvariant() switch
        {
            "TXN_BEGIN" => Assign(ReplicationOp.TxnBegin, out op),
            "TXN_COMMIT" => Assign(ReplicationOp.TxnCommit, out op),
            _ => Enum.TryParse(name, ignoreCase: true, out op),
        };

    private static bool Assign(ReplicationOp value, out ReplicationOp op)
    {
        op = value;
        return true;
    }

    public static Dictionary<string, object?> NormalizeValues(IReadOnlyDictionary<string, object?> source)
    {
        var result = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var (key, value) in source)
        {
            result[key] = NormalizeJsonValue(value);
        }

        return result;
    }

    private static object? NormalizeJsonValue(object? value) =>
        value switch
        {
            JsonElement element => element.ValueKind switch
            {
                JsonValueKind.String => element.GetString(),
                JsonValueKind.Number when element.TryGetInt64(out var i) => i,
                JsonValueKind.Number => element.GetDouble(),
                JsonValueKind.True => true,
                JsonValueKind.False => false,
                JsonValueKind.Null => null,
                _ => element.GetRawText(),
            },
            _ => value,
        };
}
