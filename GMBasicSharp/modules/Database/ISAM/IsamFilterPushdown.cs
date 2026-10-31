// FILE: Basic.ISAM.IsamFilterPushdown

using System.Text;

namespace Basic.ISAM;

/// <summary>
/// Rileva filtri SET FILTER con uguaglianze su campi indicizzati (seek pushdown).
/// ponytail: AND di <c>field = literal</c> su prefisso chiave indice attivo (SET ORDER);
/// OR, funzioni, campi non in chiave restano su scan sequenziale.
/// </summary>
internal static class IsamFilterPushdown
{
    /// <summary>
    /// Prova a estrarre <c>field = literal</c> da un AST filtro.
    /// </summary>
    public static bool TryParseEquality(Expr? expression, out string fieldName, out object? value)
    {
        fieldName = string.Empty;
        value = null;
        if (expression is not BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.ExactlyEqual } binary)
        {
            return false;
        }

        if (binary.Left is not QualifiedNameExpr { Parts.Length: 1 } qualified)
        {
            return false;
        }

        fieldName = qualified.Parts[0].Lexeme;
        if (string.IsNullOrEmpty(fieldName))
        {
            return false;
        }

        switch (binary.Right)
        {
            case NumberExpr number:
                value = number.Value == Math.Truncate(number.Value)
                    ? (object)(long)number.Value
                    : number.Value;
                return true;
            case StringExpr str:
                value = str.Value;
                return true;
            default:
                return false;
        }
    }

    /// <summary>
    /// Raccoglie uguaglianze da <c>field = literal</c> o AND di tali clausole.
    /// </summary>
    public static bool TryCollectEqualities(Expr? expression, Dictionary<string, object?> equalities)
    {
        equalities.Clear();
        if (expression is null)
        {
            return false;
        }

        return CollectEqualitiesRecursive(expression, equalities) && equalities.Count > 0;
    }

    /// <summary>
    /// Campi record referenziati dall'espressione chiave indice (semplice, UPPER, COMPOSITE).
    /// </summary>
    public static bool TryGetKeyFieldNames(string keyExpression, out string[] fieldNames)
    {
        fieldNames = [];
        var expr = keyExpression.Trim();

        if (expr.StartsWith("COMPOSITE(", StringComparison.OrdinalIgnoreCase) && expr.EndsWith(')'))
        {
            var inner = expr[10..^1];
            fieldNames = inner.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
            return fieldNames.Length >= 2;
        }

        if (expr.StartsWith("UPPER(", StringComparison.OrdinalIgnoreCase) && expr.EndsWith(')'))
        {
            var inner = expr[6..^1].Trim();
            if (string.IsNullOrEmpty(inner))
            {
                return false;
            }

            fieldNames = [inner];
            return true;
        }

        if (string.IsNullOrEmpty(expr))
        {
            return false;
        }

        fieldNames = [expr];
        return true;
    }

    /// <summary>
    /// Quanti campi iniziali della chiave sono coperti da uguaglianze nel filtro (prefisso).
    /// </summary>
    public static int CountMatchingPrefixFields(
        string[] keyFieldNames,
        IReadOnlyDictionary<string, object?> equalities)
    {
        var count = 0;
        foreach (var field in keyFieldNames)
        {
            if (!equalities.ContainsKey(field))
            {
                break;
            }

            count++;
        }

        return count;
    }

    /// <summary>
    /// Lunghezza byte (UTF-8 upper) da confrontare per pushdown prefisso; -1 = chiave intera serializzata.
    /// </summary>
    public static int ComputeMatchLength(object? keyValue, bool isFullKeyMatch)
    {
        if (isFullKeyMatch)
        {
            return -1;
        }

        return Encoding.UTF8.GetByteCount((keyValue?.ToString() ?? string.Empty).ToUpperInvariant());
    }

    private static bool CollectEqualitiesRecursive(Expr expression, Dictionary<string, object?> equalities)
    {
        if (TryParseEquality(expression, out var fieldName, out var value))
        {
            equalities[fieldName] = value;
            return true;
        }

        if (expression is BinaryExpr { Op.Kind: TokenKind.And } andExpr)
        {
            return CollectEqualitiesRecursive(andExpr.Left, equalities)
                   && CollectEqualitiesRecursive(andExpr.Right, equalities);
        }

        return false;
    }
}
