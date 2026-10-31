// FILE: Basic.ISAM.IsamKeyExpressionEvaluator

using System.Text.Json;
using Basic.Databases;

namespace Basic.ISAM;

/// <summary>
/// Valutatore built-in per espressioni chiave indice ISAM (engine-side, senza GPM).
/// Supporta UPPER, JSON_VALUE, COMPOSITE; estendibile via <see cref="KeyExpressionEvaluatorCallback"/> esterno.
/// </summary>
internal static class IsamKeyExpressionEvaluator
{
    private const char CompositeSeparator = '\x1F';

    /// <summary>
    /// Tenta di valutare un'espressione chiave built-in. Restituisce false se serve il callback GPM/UDF.
    /// </summary>
    public static bool TryEvaluateBuiltIn(
        string expressionString,
        IReadOnlyDictionary<string, object?> recordContext,
        DataType targetType,
        out object? value)
    {
        value = null;
        var expr = expressionString.Trim();

        if (TryEvaluateUpper(expr, recordContext, out value))
        {
            return true;
        }

        if (TryEvaluateJsonValue(expr, recordContext, out value))
        {
            return true;
        }

        if (TryEvaluateComposite(expr, recordContext, out value))
        {
            return true;
        }

        if (recordContext.TryGetValue(expr, out value))
        {
            return true;
        }

        return false;
    }

    /// <summary>
    /// Catena built-in → callback utente (hook UDF / espressioni BASIC unsafe).
    /// </summary>
    public static KeyExpressionEvaluatorCallback Chain(KeyExpressionEvaluatorCallback? userCallback) =>
        (expression, context, targetType) =>
        {
            if (TryEvaluateBuiltIn(expression, context, targetType, out var builtIn))
            {
                return builtIn;
            }

            if (userCallback != null)
            {
                return userCallback(expression, context, targetType);
            }

            throw new InvalidOperationException(
                $"Espressione chiave ISAM non risolvibile: '{expression}'. Usare IsUnsafe=true e un KeyExpressionEvaluatorCallback GPM.");
        };

    private static bool TryEvaluateUpper(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
    {
        value = null;
        if (!expr.StartsWith("UPPER(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
        {
            return false;
        }

        var inner = expr[6..^1].Trim();
        if (!ctx.TryGetValue(inner, out var raw))
        {
            return false;
        }

        value = raw?.ToString()?.ToUpperInvariant() ?? string.Empty;
        return true;
    }

    private static bool TryEvaluateJsonValue(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
    {
        value = null;
        if (!expr.StartsWith("JSON_VALUE(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
        {
            return false;
        }

        var inner = expr[11..^1];
        var comma = inner.IndexOf(',');
        if (comma <= 0)
        {
            return false;
        }

        var fieldName = inner[..comma].Trim();
        var pathToken = inner[(comma + 1)..].Trim().Trim('\'', '"');
        if (!pathToken.StartsWith('$'))
        {
            pathToken = "$." + pathToken.TrimStart('.');
        }

        if (!ctx.TryGetValue(fieldName, out var jsonRaw) || jsonRaw is null)
        {
            value = string.Empty;
            return true;
        }

        value = ExtractJsonPath(jsonRaw.ToString() ?? "{}", pathToken) ?? string.Empty;
        return true;
    }

    private static bool TryEvaluateComposite(string expr, IReadOnlyDictionary<string, object?> ctx, out object? value)
    {
        value = null;
        if (!expr.StartsWith("COMPOSITE(", StringComparison.OrdinalIgnoreCase) || !expr.EndsWith(')'))
        {
            return false;
        }

        var inner = expr[10..^1];
        var parts = inner.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 2)
        {
            return false;
        }

        var segments = new string[parts.Length];
        for (var i = 0; i < parts.Length; i++)
        {
            if (!ctx.TryGetValue(parts[i], out var partVal))
            {
                segments[i] = string.Empty;
            }
            else
            {
                segments[i] = partVal?.ToString() ?? string.Empty;
            }
        }

        value = string.Join(CompositeSeparator, segments);
        return true;
    }

    internal static string? ExtractJsonPath(string json, string jsonPath)
    {
        if (string.IsNullOrWhiteSpace(json))
        {
            return null;
        }

        try
        {
            using var doc = JsonDocument.Parse(json);
            var element = doc.RootElement;
            var segments = jsonPath.TrimStart('$', '.').Split('.', StringSplitOptions.RemoveEmptyEntries);
            foreach (var segment in segments)
            {
                if (element.ValueKind != JsonValueKind.Object ||
                    !element.TryGetProperty(segment, out element))
                {
                    return null;
                }
            }

            return element.ValueKind switch
            {
                JsonValueKind.String => element.GetString(),
                JsonValueKind.Number => element.GetRawText(),
                JsonValueKind.True => "true",
                JsonValueKind.False => "false",
                JsonValueKind.Null => null,
                _ => element.GetRawText(),
            };
        }
        catch (JsonException)
        {
            return null;
        }
    }
}
