// FILE: Basic.ISAM.IsamQueryPredicate
namespace Basic.ISAM;

/// <summary>
/// Parser MVP per predicati stringa IsamQuery (uguaglianza campo = letterale).
/// </summary>
internal static class IsamQueryPredicate
{
    /// <summary>
    /// Prova a interpretare <c>Field == value</c> o <c>Field = value</c>.
    /// </summary>
    public static bool TryParseEquality(string predicate, out string fieldName, out object? value)
    {
        fieldName = string.Empty;
        value = null;
        var trimmed = predicate.Trim();
        foreach (var op in new[] { "==", "=" })
        {
            var idx = trimmed.IndexOf(op, StringComparison.Ordinal);
            if (idx <= 0)
            {
                continue;
            }

            fieldName = trimmed[..idx].Trim();
            if (string.IsNullOrEmpty(fieldName))
            {
                return false;
            }

            value = ParseLiteral(trimmed[(idx + op.Length)..].Trim());
            return true;
        }

        return false;
    }

    /// <summary>
    /// Espressione chiave indice MVP pushdown: identificatore semplice uguale al campo filtro.
    /// </summary>
    public static bool IsSimpleFieldKeyExpression(string keyExpression, string fieldName) =>
        keyExpression.Trim().Equals(fieldName, StringComparison.OrdinalIgnoreCase);

    private static object? ParseLiteral(string rhs)
    {
        if (rhs.Length >= 2 &&
            ((rhs[0] == '"' && rhs[^1] == '"') || (rhs[0] == '\'' && rhs[^1] == '\'')))
        {
            return rhs[1..^1];
        }

        if (long.TryParse(rhs, out var number))
        {
            return number;
        }

        return rhs;
    }
}
