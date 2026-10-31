// FILE: Basic.Databases.Speed.QueryOptimizer.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Minimal index selector for SPEED equality seeks and filtered scans.
/// ponytail: cost = SST file count + manifest row count when trusted; unique indexes win ties.
/// Upgrade path: per-column cardinality (ANALYZE) before join reordering.
/// </summary>
internal static class QueryOptimizer
{
    /// <summary>Equality filter on a single indexed field.</summary>
    internal readonly record struct EqualityPredicate(string FieldName, string? TableAlias = null);

    /// <summary>Chosen index access path with a cheap SST-touch and row-count estimate.</summary>
    internal sealed record IndexAccessPlan(string IndexName, bool IsPrimaryKey, int EstimatedSstFiles, long EstimatedRows);

    /// <summary>Parsed <c>field = literal</c> from a simple WHERE expression.</summary>
    internal sealed record ParsedEquality(string FieldName, string? TableAlias, string RawValue);

    /// <summary>
    /// Picks the lowest-cost index that satisfies an equality predicate, or null when no index matches.
    /// </summary>
    internal static IndexAccessPlan? SelectBestIndex(
        TableMetadata tableMeta,
        Manifest manifest,
        IReadOnlyList<EqualityPredicate> predicates,
        string? hintFieldName = null)
    {
        ArgumentNullException.ThrowIfNull(tableMeta);
        ArgumentNullException.ThrowIfNull(manifest);
        if (predicates.Count == 0) return null;

        IndexAccessPlan? best = null;
        var bestScore = long.MaxValue;

        foreach (var candidate in EnumerateCandidates(tableMeta, manifest))
        {
            if (!MatchesAnyPredicate(candidate.Definition, predicates, hintFieldName)) continue;

            var score = ScoreCandidate(candidate, manifest);
            if (score >= bestScore) continue;
            bestScore = score;
            best = new IndexAccessPlan(
                candidate.Definition.Name,
                candidate.IsPrimaryKey,
                candidate.SstFiles,
                candidate.EstimatedRows);
        }

        return best;
    }

    /// <summary>
    /// Resolves the index definition for a JOIN seek on <paramref name="joinFieldName"/>.
    /// </summary>
    internal static Config.SpeedIndexDefinition ResolveJoinIndex(
        TableMetadata tableMeta,
        Manifest manifest,
        string joinFieldName)
    {
        ArgumentException.ThrowIfNullOrEmpty(joinFieldName);

        var plan = SelectBestIndex(
            tableMeta,
            manifest,
            [new EqualityPredicate(joinFieldName)],
            joinFieldName);

        if (plan is null)
        {
            throw new InvalidOperationException(
                $"Indice per il campo '{joinFieldName}' non trovato nella tabella '{tableMeta.Name}'.");
        }

        return GetIndexDefinition(tableMeta.Schema, plan.IndexName)
               ?? throw new InvalidOperationException(
                   $"Indice '{plan.IndexName}' non trovato nella tabella '{tableMeta.Name}'.");
    }

    /// <summary>
    /// ponytail: only top-level <c>alias.field = literal</c> or <c>field = literal</c> — no OR/functions.
    /// </summary>
    internal static ParsedEquality? TryParseSimpleEquality(string? expression, string? expectedTableAlias = null)
    {
        var list = TryParseCompoundAndEqualities(expression, expectedTableAlias);
        return list is { Count: 1 } ? list[0] : null;
    }

    /// <summary>
    /// ponytail: top-level AND of simple equalities only — no OR, no nested AND in literals.
    /// </summary>
    internal static IReadOnlyList<ParsedEquality>? TryParseCompoundAndEqualities(
        string? expression,
        string? expectedTableAlias = null)
    {
        if (string.IsNullOrWhiteSpace(expression)) return null;

        var parts = SplitTopLevelAnd(expression.Trim());
        if (parts.Count == 0) return null;

        var result = new List<ParsedEquality>(parts.Count);
        foreach (var part in parts)
        {
            if (!TryParseEqualityPart(part, expectedTableAlias, out var parsed))
                return null;

            result.Add(parsed);
        }

        return result;
    }

    /// <summary>Verifies a materialized row satisfies all parsed equalities.</summary>
    internal static bool RowMatchesEqualities(
        IReadOnlyDictionary<string, object?> row,
        Config.SpeedSchema schema,
        IReadOnlyList<ParsedEquality> equalities)
    {
        foreach (var eq in equalities)
        {
            if (!schema.Fields.Any(f => f.Name.Equals(eq.FieldName, StringComparison.OrdinalIgnoreCase)))
                return false;

            var field = schema.Fields.First(f => f.Name.Equals(eq.FieldName, StringComparison.OrdinalIgnoreCase));
            if (!row.TryGetValue(eq.FieldName, out var actual) &&
                !row.TryGetValue(field.Name, out actual))
                return false;

            var expected = CoerceLiteral(eq.RawValue, field.DataType);
            if (expected is null) return false;

            if (field.DataType is StringType)
            {
                var actualStr = actual?.ToString()?.TrimEnd() ?? string.Empty;
                var expectedStr = expected.ToString()?.TrimEnd() ?? string.Empty;
                if (!actualStr.Equals(expectedStr, StringComparison.Ordinal))
                    return false;
                continue;
            }

            if (actual?.Equals(expected) != true) return false;
        }

        return true;
    }

    private static bool TryParseEqualityPart(
        string part,
        string? expectedTableAlias,
        out ParsedEquality parsed)
    {
        parsed = default!;
        var eqIndex = FindTopLevelEquals(part);
        if (eqIndex < 0) return false;

        var left = part[..eqIndex].Trim();
        var right = part[(eqIndex + 1)..].Trim();
        if (left.Length == 0 || right.Length == 0) return false;

        string fieldName;
        string? alias = null;
        var dot = left.IndexOf('.');
        if (dot >= 0)
        {
            alias = left[..dot].Trim();
            fieldName = left[(dot + 1)..].Trim();
        }
        else
        {
            fieldName = left;
        }

        if (string.IsNullOrEmpty(fieldName) || fieldName.Contains('(')) return false;

        if (expectedTableAlias is not null && alias is not null &&
            !alias.Equals(expectedTableAlias, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        parsed = new ParsedEquality(fieldName, alias, right);
        return true;
    }

    private static List<string> SplitTopLevelAnd(string expression)
    {
        var parts = new List<string>();
        var start = 0;
        var inString = '\0';

        for (var i = 0; i <= expression.Length - 4; i++)
        {
            var c = expression[i];
            if (inString != '\0')
            {
                if (c == inString) inString = '\0';
                continue;
            }

            if (c is '\'' or '"')
            {
                inString = c;
                continue;
            }

            if (!expression.AsSpan(i, 4).Equals(" AND".AsSpan(), StringComparison.OrdinalIgnoreCase))
                continue;

            if (i > 0)
            {
                var before = expression[i - 1];
                if (!char.IsWhiteSpace(before) && before is not '\'' and not '"')
                    continue;
            }

            if (i + 4 < expression.Length)
            {
                var after = expression[i + 4];
                if (!char.IsWhiteSpace(after) && !char.IsLetter(after))
                    continue;
            }

            parts.Add(expression[start..i].Trim());
            start = i + 4;
            i = start - 1;
        }

        var tail = expression[start..].Trim();
        if (tail.Length > 0)
            parts.Add(tail);

        return parts;
    }

    internal static bool PredicateMatchesIndex(string fieldName, Config.SpeedIndexDefinition indexDef) =>
        fieldName.Equals(indexDef.Name, StringComparison.OrdinalIgnoreCase) ||
        fieldName.Equals(indexDef.KeyExpression, StringComparison.OrdinalIgnoreCase);

    internal static Config.SpeedIndexDefinition? GetIndexDefinition(Config.SpeedSchema schema, string indexName)
    {
        if (schema.PrimaryKey.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase))
            return schema.PrimaryKey;

        return schema.SecondaryKeys.FirstOrDefault(sk =>
            sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>Coerces a WHERE literal into a CLR value for marshalling.</summary>
      internal static object? CoerceLiteral(string rawValue, DataType dataType)
    {
        var trimmed = rawValue.Trim();
        if (trimmed.Length == 0) return null;

        if ((trimmed.StartsWith('\'') && trimmed.EndsWith('\'')) ||
            (trimmed.StartsWith('"') && trimmed.EndsWith('"')))
        {
            return trimmed[1..^1];
        }

        return dataType switch
        {
            LongType => long.TryParse(trimmed, out var l) ? l : null,
            IntegerType => int.TryParse(trimmed, out var i) ? i : null,
            CurrencyType => decimal.TryParse(trimmed, out var d) ? d : null,
            DoubleType => double.TryParse(trimmed, out var dbl) ? dbl : null,
            StringType => trimmed,
            _ => trimmed,
        };
    }

    internal static int CountSstFiles(Manifest manifest, int tableId)
    {
        if (!manifest.TryGetTable(tableId, out var meta) || meta is null) return 0;
        return meta.SstFilesByLevel.Values.Sum(files => files.Count);
    }

    internal static string GetIndexTableName(string mainTableName, string indexName) =>
        $"_idx_{mainTableName}_{indexName}";

    private readonly record struct IndexCandidate(
        Config.SpeedIndexDefinition Definition,
        bool IsPrimaryKey,
        int SstFiles,
        int TableId,
        long EstimatedRows);

    private static IEnumerable<IndexCandidate> EnumerateCandidates(TableMetadata tableMeta, Manifest manifest)
    {
        yield return new IndexCandidate(
            tableMeta.Schema.PrimaryKey,
            true,
            CountSstFiles(manifest, tableMeta.Id),
            tableMeta.Id,
            manifest.GetRowCount(tableMeta.Id));

        foreach (var secondary in tableMeta.Schema.SecondaryKeys)
        {
            var indexTableName = GetIndexTableName(tableMeta.Name, secondary.Name);
            var indexTableId = -1;
            var sstFiles = 0;
            long rows = 0;
            if (manifest.TryGetTable(indexTableName, out var indexMeta) && indexMeta is not null)
            {
                indexTableId = indexMeta.Id;
                sstFiles = CountSstFiles(manifest, indexMeta.Id);
                rows = manifest.GetRowCount(indexMeta.Id);
            }

            yield return new IndexCandidate(secondary, false, sstFiles, indexTableId, rows);
        }
    }

    private static bool MatchesAnyPredicate(
        Config.SpeedIndexDefinition indexDef,
        IReadOnlyList<EqualityPredicate> predicates,
        string? hintFieldName)
    {
        foreach (var predicate in predicates)
        {
            if (PredicateMatchesIndex(predicate.FieldName, indexDef)) return true;
        }

        return hintFieldName is not null && PredicateMatchesIndex(hintFieldName, indexDef);
    }

    private static long ScoreCandidate(IndexCandidate candidate, Manifest manifest)
    {
        // ponytail: fewer SST files first; trusted row count breaks ties; unique indexes win at equal cost.
        var uniquePenalty = candidate.Definition.IsUnique ? 0L : 1_000L;
        var rowPenalty = candidate.TableId >= 0 && manifest.IsRowCountTrusted(candidate.TableId)
            ? candidate.EstimatedRows
            : 0L;
        return candidate.SstFiles * 1_000_000L + rowPenalty + uniquePenalty;
    }

    private static int FindTopLevelEquals(string expression)
    {
        var inString = '\0';
        for (var i = 0; i < expression.Length; i++)
        {
            var c = expression[i];
            if (inString != '\0')
            {
                if (c == inString) inString = '\0';
                continue;
            }

            if (c is '\'' or '"')
            {
                inString = c;
                continue;
            }

            if (c == '=')
            {
                var prev = i > 0 ? expression[i - 1] : '\0';
                var next = i + 1 < expression.Length ? expression[i + 1] : '\0';
                if (prev is '<' or '>' or '!' || next == '=') continue;
                return i;
            }
        }

        return -1;
    }
}
