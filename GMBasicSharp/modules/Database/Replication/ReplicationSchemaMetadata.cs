// FILE: Basic.Databases.Replication.ReplicationSchemaMetadata

using System.Text.Json;

namespace Basic.Databases.Replication;

/// <summary>
/// JSON payload keys and helpers for SCHEMA / REINDEX replication lines.
/// </summary>
internal static class ReplicationSchemaMetadata
{
    public const string GenKey = "gen";
    public const string IschKey = "isch";
    public const string GmsKey = "gms";

    public static Dictionary<string, object?> ForIsam(long generation, byte[] ischBytes) =>
        new(StringComparer.Ordinal)
        {
            [GenKey] = generation,
            [IschKey] = Convert.ToBase64String(ischBytes),
        };

    public static Dictionary<string, object?> ForDbfGms(long generation, byte[] gmsBytes) =>
        new(StringComparer.Ordinal)
        {
            [GenKey] = generation,
            [GmsKey] = Convert.ToBase64String(gmsBytes),
        };

    public static long RequireGeneration(IReadOnlyDictionary<string, object?> values)
    {
        ArgumentNullException.ThrowIfNull(values);
        if (!values.TryGetValue(GenKey, out var raw))
        {
            throw new InvalidDataException("SCHEMA line missing 'gen'.");
        }

        return NormalizeJsonValue(raw) switch
        {
            long l => l,
            int i => i,
            JsonElement { ValueKind: JsonValueKind.Number } el when el.TryGetInt64(out var n) => n,
            _ => throw new InvalidDataException("SCHEMA line 'gen' is not a number."),
        };
    }

    public static byte[] RequireBase64(IReadOnlyDictionary<string, object?> values, string key)
    {
        ArgumentNullException.ThrowIfNull(values);
        if (!values.TryGetValue(key, out var raw))
        {
            throw new InvalidDataException($"SCHEMA line missing '{key}'.");
        }

        var text = NormalizeJsonValue(raw) as string
                   ?? throw new InvalidDataException($"SCHEMA line '{key}' is not a string.");
        return Convert.FromBase64String(text);
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
