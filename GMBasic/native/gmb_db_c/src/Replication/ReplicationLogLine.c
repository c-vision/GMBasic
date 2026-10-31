/* FILE: native/gmb_db_c/src/Replication/ReplicationLogLine.c */
/* Port 1:1 di modules/Database/Replication/ReplicationLogLine.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationLogLine
//
//using System.Text.Json;
//
//namespace Basic.Databases.Replication;
//
//internal enum ReplicationOp : byte
//{
//    Append = 1,
//    Put = 2,
//    Delete = 3,
//    Recall = 4,
//    TxnBegin = 10,
//    TxnCommit = 11,
//    Pack = 20,
//    Schema = 21,
//    Reindex = 22,
//}
//
//internal static class ReplicationLogLine
//{
//    public static string FormatTxnMarker(ReplicationOp op) =>
//        op switch
//        {
//            ReplicationOp.TxnBegin => "TXN_BEGIN\t0",
//            ReplicationOp.TxnCommit => "TXN_COMMIT\t0",
//            _ => throw new ArgumentOutOfRangeException(nameof(op), op, "Expected TXN_BEGIN or TXN_COMMIT."),
//        };
//
//    public static string FormatPack(long generation)
//    {
//        if (generation <= 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(generation), generation, "PACK generation must be positive.");
//        }
//
//        return $"{ReplicationOp.Pack.ToString().ToUpperInvariant()}\t{generation}";
//    }
//
//    /// <summary>
//    /// SCHEMA payload carries a generation and artifact bytes (ISCH or .gms).
//    /// ponytail: ISAM apply writes ISCH block then REINDEX; no in-place field migration.
//    /// </summary>
//    public static string FormatSchema(IReadOnlyDictionary<string, object?> metadata)
//    {
//        ArgumentNullException.ThrowIfNull(metadata);
//        return $"{ReplicationOp.Schema.ToString().ToUpperInvariant()}\t0\t{JsonSerializer.Serialize(metadata)}";
//    }
//
//    public static string FormatReindex(long generation)
//    {
//        if (generation <= 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(generation), generation, "REINDEX generation must be positive.");
//        }
//
//        return $"{ReplicationOp.Reindex.ToString().ToUpperInvariant()}\t{generation}";
//    }
//
//    public static string Format(ReplicationOp op, long recNo, IReadOnlyDictionary<string, object?>? values = null)
//    {
//        if (op is ReplicationOp.TxnBegin or ReplicationOp.TxnCommit)
//        {
//            return FormatTxnMarker(op);
//        }
//
//        if (recNo <= 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(recNo), recNo, "Record number must be positive.");
//        }
//
//        var opName = op.ToString().ToUpperInvariant();
//        if (values is null or { Count: 0 })
//        {
//            return $"{opName}\t{recNo}";
//        }
//
//        return $"{opName}\t{recNo}\t{JsonSerializer.Serialize(values)}";
//    }
//
//    public static bool TryParse(
//        string line,
//        out ReplicationOp op,
//        out long recNo,
//        out Dictionary<string, object?>? values)
//    {
//        op = default;
//        recNo = 0;
//        values = null;
//
//        if (string.IsNullOrWhiteSpace(line))
//        {
//            return false;
//        }
//
//        var parts = line.Split('\t');
//        if (parts.Length is < 2 or > 3)
//        {
//            return false;
//        }
//
//        if (!TryParseOp(parts[0], out op))
//        {
//            return false;
//        }
//
//        if (!long.TryParse(parts[1], out recNo))
//        {
//            return false;
//        }
//
//        if (recNo <= 0 && op is not ReplicationOp.TxnBegin and not ReplicationOp.TxnCommit and not ReplicationOp.Schema)
//        {
//            return false;
//        }
//
//        if (parts.Length == 3)
//        {
//            values = JsonSerializer.Deserialize<Dictionary<string, object?>>(parts[2]);
//            if (values is null)
//            {
//                return false;
//            }
//        }
//
//        return true;
//    }
//
//    private static bool TryParseOp(string name, out ReplicationOp op) =>
//        name.ToUpperInvariant() switch
//        {
//            "TXN_BEGIN" => Assign(ReplicationOp.TxnBegin, out op),
//            "TXN_COMMIT" => Assign(ReplicationOp.TxnCommit, out op),
//            _ => Enum.TryParse(name, ignoreCase: true, out op),
//        };
//
//    private static bool Assign(ReplicationOp value, out ReplicationOp op)
//    {
//        op = value;
//        return true;
//    }
//
//    public static Dictionary<string, object?> NormalizeValues(IReadOnlyDictionary<string, object?> source)
//    {
//        var result = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//        foreach (var (key, value) in source)
//        {
//            result[key] = NormalizeJsonValue(value);
//        }
//
//        return result;
//    }
//
//    private static object? NormalizeJsonValue(object? value) =>
//        value switch
//        {
//            JsonElement element => element.ValueKind switch
//            {
//                JsonValueKind.String => element.GetString(),
//                JsonValueKind.Number when element.TryGetInt64(out var i) => i,
//                JsonValueKind.Number => element.GetDouble(),
//                JsonValueKind.True => true,
//                JsonValueKind.False => false,
//                JsonValueKind.Null => null,
//                _ => element.GetRawText(),
//            },
//            _ => value,
//        };
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <stdio.h>
#include <ctype.h>
#include <strings.h>

/* C#: ReplicationLogLine — "OP\\trecNo[\\tjson]". JSON valori tutti stringa. */

static int rll_parse_op(const char* name, ReplicationOp* op)
{
    if (!name || !op) return 0;
    if (gmb_strcasecmp(name, "TXN_BEGIN") == 0) { *op = ReplicationOp_TxnBegin; return 1; }
    if (gmb_strcasecmp(name, "TXN_COMMIT") == 0) { *op = ReplicationOp_TxnCommit; return 1; }
    if (gmb_strcasecmp(name, "APPEND") == 0) { *op = ReplicationOp_Append; return 1; }
    if (gmb_strcasecmp(name, "PUT") == 0) { *op = ReplicationOp_Put; return 1; }
    if (gmb_strcasecmp(name, "DELETE") == 0) { *op = ReplicationOp_Delete; return 1; }
    if (gmb_strcasecmp(name, "RECALL") == 0) { *op = ReplicationOp_Recall; return 1; }
    if (gmb_strcasecmp(name, "PACK") == 0) { *op = ReplicationOp_Pack; return 1; }
    if (gmb_strcasecmp(name, "SCHEMA") == 0) { *op = ReplicationOp_Schema; return 1; }
    if (gmb_strcasecmp(name, "REINDEX") == 0) { *op = ReplicationOp_Reindex; return 1; }
    return 0;
}

static const char* rll_op_name(ReplicationOp op)
{
    switch (op) {
    case ReplicationOp_Append: return "APPEND";
    case ReplicationOp_Put: return "PUT";
    case ReplicationOp_Delete: return "DELETE";
    case ReplicationOp_Recall: return "RECALL";
    case ReplicationOp_TxnBegin: return "TXN_BEGIN";
    case ReplicationOp_TxnCommit: return "TXN_COMMIT";
    case ReplicationOp_Pack: return "PACK";
    case ReplicationOp_Schema: return "SCHEMA";
    case ReplicationOp_Reindex: return "REINDEX";
    default: return "APPEND";
    }
}

void ReplicationLogLine_FormatTxnMarker(ReplicationOp op, char* out, int cap)
{
    if (op == ReplicationOp_TxnBegin) snprintf(out, (size_t)cap, "TXN_BEGIN\t0");
    else snprintf(out, (size_t)cap, "TXN_COMMIT\t0");
}

void ReplicationLogLine_FormatPack(int64_t generation, char* out, int cap)
{
    snprintf(out, (size_t)cap, "PACK\t%lld", (long long)generation);
}

void ReplicationLogLine_FormatReindex(int64_t generation, char* out, int cap)
{
    snprintf(out, (size_t)cap, "REINDEX\t%lld", (long long)generation);
}

void ReplicationLogLine_Format(ReplicationOp op, int64_t recNo, const char* jsonValues, char* out, int cap)
{
    if (op == ReplicationOp_TxnBegin || op == ReplicationOp_TxnCommit) {
        ReplicationLogLine_FormatTxnMarker(op, out, cap);
        return;
    }
    if (!jsonValues || !jsonValues[0])
        snprintf(out, (size_t)cap, "%s\t%lld", rll_op_name(op), (long long)recNo);
    else
        snprintf(out, (size_t)cap, "%s\t%lld\t%s", rll_op_name(op), (long long)recNo, jsonValues);
}

int ReplicationLogLine_TryParse(const char* line, ReplicationOp* op, int64_t* recNo, char* jsonOut, int jsonCap)
{
    char name[32];
    const char* p;
    const char* tab2;
    long long n = 0;
    if (jsonOut && jsonCap > 0) jsonOut[0] = 0;
    if (!line || !op || !recNo) return 0;
    while (*line == ' ' || *line == '\t' || *line == '\n') line++;
    if (!*line) return 0;
    p = strchr(line, '\t');
    if (!p) return 0;
    {
        int nl = (int)(p - line);
        if (nl <= 0 || nl >= (int)sizeof(name)) return 0;
        memcpy(name, line, (size_t)nl);
        name[nl] = 0;
    }
    if (!rll_parse_op(name, op)) return 0;
    p++;
    tab2 = strchr(p, '\t');
    if (tab2) {
        char num[32];
        int nl = (int)(tab2 - p);
        if (nl <= 0 || nl >= (int)sizeof(num)) return 0;
        memcpy(num, p, (size_t)nl);
        num[nl] = 0;
        n = strtoll(num, NULL, 10);
        if (jsonOut && jsonCap > 0) snprintf(jsonOut, (size_t)jsonCap, "%s", tab2 + 1);
    } else {
        n = strtoll(p, NULL, 10);
    }
    *recNo = (int64_t)n;
    if (*recNo <= 0 && *op != ReplicationOp_TxnBegin && *op != ReplicationOp_TxnCommit && *op != ReplicationOp_Schema)
        return 0;
    return 1;
}
