// FILE: Basic.ISAM.IsamIndexPredicate
namespace Basic.ISAM;

/// <summary>
/// Valutatore MVP per predicati di indice parziale (es. "Score > 0").
/// </summary>
internal static class IsamIndexPredicate
{
    /// <summary>
    /// Restituisce true se il record deve essere incluso nell'indice (predicato null/vuoto = sempre).
    /// </summary>
    public static bool Matches(string? predicate, IReadOnlyDictionary<string, object?> fields)
    {
        if (string.IsNullOrWhiteSpace(predicate))
        {
            return true;
        }

        var trimmed = predicate.Trim();
        foreach (var op in new[] { ">=", "<=", "!=", ">", "<", "=" })
        {
            var idx = trimmed.IndexOf(op, StringComparison.Ordinal);
            if (idx <= 0)
            {
                continue;
            }

            var fieldName = trimmed[..idx].Trim();
            var rhsRaw = trimmed[(idx + op.Length)..].Trim();
            if (!fields.TryGetValue(fieldName, out var lhs))
            {
                return false;
            }

            if (!TryToInt64(lhs, out var lhsNum) || !long.TryParse(rhsRaw, out var rhsNum))
            {
                return false;
            }

            return op switch
            {
                ">" => lhsNum > rhsNum,
                ">=" => lhsNum >= rhsNum,
                "<" => lhsNum < rhsNum,
                "<=" => lhsNum <= rhsNum,
                "=" => lhsNum == rhsNum,
                "!=" => lhsNum != rhsNum,
                _ => false,
            };
        }

        return false;
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
}
