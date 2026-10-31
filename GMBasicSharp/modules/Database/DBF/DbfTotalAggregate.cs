// FILE: Basic.DBF.DbfTotalAggregate

using Basic.Databases;

namespace Basic.DBF;

/// <summary>
/// One numeric aggregate target for <see cref="DbfEngine.TotalOn"/> (field ref or simple arithmetic).
/// </summary>
/// <param name="Expression">AST evaluated per record via <see cref="KeyExpressionEvaluatorCallback"/>.</param>
/// <param name="StorageFieldName">Physical DBF column that receives the running sum.</param>
public readonly record struct DbfTotalAggregate(Expr Expression, string StorageFieldName);

/// <summary>
/// Builds and evaluates TOTAL ON FIELDS expressions (ponytail: + - * / on field refs).
/// </summary>
public static class DbfTotalExpressions
{
    private static readonly HashSet<TokenKind> ArithmeticOps =
    [
        TokenKind.Plus,
        TokenKind.Minus,
        TokenKind.Multiply,
        TokenKind.Divide
    ];

    /// <summary>
    /// Maps a FIELDS expression to an aggregate, or returns false if unsupported.
    /// </summary>
    public static bool TryCreate(Expr expression, out DbfTotalAggregate aggregate)
    {
        aggregate = default;
        if (!IsSupportedNumericExpression(expression))
        {
            return false;
        }

        var storage = ResolveStorageFieldName(expression);
        if (string.IsNullOrEmpty(storage))
        {
            return false;
        }

        aggregate = new DbfTotalAggregate(expression, storage);
        return true;
    }

    /// <summary>
    /// Plain expression text for the key/aggregate evaluator (no symbol ids).
    /// </summary>
    public static string ToPlainString(Expr expression) => ExprPlain.From(expression);

    /// <summary>
    /// Field names referenced by an expression (depth-first, left before right).
    /// </summary>
    public static IEnumerable<string> CollectReferencedFieldNames(Expr expression)
    {
        switch (expression)
        {
            case VariableExpr v:
                yield return v.NameCleaned.Lexeme;
                yield break;

            case FieldAccessExpr f:
                yield return f.Field.Lexeme;
                yield break;

            case BinaryExpr b:
                foreach (var name in CollectReferencedFieldNames(b.Left))
                {
                    yield return name;
                }

                foreach (var name in CollectReferencedFieldNames(b.Right))
                {
                    yield return name;
                }

                yield break;

            case GroupingExpr g:
                foreach (var name in CollectReferencedFieldNames(g.Expression))
                {
                    yield return name;
                }

                break;
        }
    }

    /// <summary>
    /// Field names used by the TOTAL ON key expression.
    /// </summary>
    public static HashSet<string> CollectKeyFieldNames(Expr keyExpression)
    {
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var name in CollectReferencedFieldNames(keyExpression))
        {
            names.Add(name);
        }

        return names;
    }

    /// <summary>
    /// Evaluates a numeric aggregate for one record.
    /// </summary>
    public static decimal EvaluateNumeric(
        KeyExpressionEvaluatorCallback evaluator,
        Expr expression,
        IReadOnlyDictionary<string, object?> recordValues)
    {
        var plain = ToPlainString(expression);
        var result = evaluator(plain, recordValues, new DoubleType());
        return Convert.ToDecimal(result);
    }

    private static bool IsSupportedNumericExpression(Expr expression) =>
        expression switch
        {
            VariableExpr or FieldAccessExpr => true,
            BinaryExpr b when ArithmeticOps.Contains(b.Op.Kind) =>
                IsSupportedNumericExpression(b.Left) && IsSupportedNumericExpression(b.Right),
            GroupingExpr g => IsSupportedNumericExpression(g.Expression),
            _ => false
        };

    // ponytail: computed totals land in the leftmost referenced field (upgrade: alias / new column)
    private static string ResolveStorageFieldName(Expr expression) =>
        CollectReferencedFieldNames(expression).FirstOrDefault() ?? string.Empty;
}
