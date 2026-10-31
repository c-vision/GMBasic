/* FILE: native/gmb_db_c/src/SPEED/QueryOptimizer.c */
/* Port 1:1 di modules/Database/SPEED/QueryOptimizer.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"
#include <unistd.h>

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.QueryOptimizer.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Minimal index selector for SPEED equality seeks and filtered scans.
///// ponytail: cost = SST file count + manifest row count when trusted; unique indexes win ties.
///// Upgrade path: per-column cardinality (ANALYZE) before join reordering.
///// </summary>
//internal static class QueryOptimizer
//{
//    /// <summary>Equality filter on a single indexed field.</summary>
//    internal readonly record struct EqualityPredicate(string FieldName, string? TableAlias = null);
//
//    /// <summary>Chosen index access path with a cheap SST-touch and row-count estimate.</summary>
//    internal sealed record IndexAccessPlan(string IndexName, bool IsPrimaryKey, int EstimatedSstFiles, long EstimatedRows);
//
//    /// <summary>Parsed <c>field = literal</c> from a simple WHERE expression.</summary>
//    internal sealed record ParsedEquality(string FieldName, string? TableAlias, string RawValue);
//
//    /// <summary>
//    /// Picks the lowest-cost index that satisfies an equality predicate, or null when no index matches.
//    /// </summary>
//    internal static IndexAccessPlan? SelectBestIndex(
//        TableMetadata tableMeta,
//        Manifest manifest,
//        IReadOnlyList<EqualityPredicate> predicates,
//        string? hintFieldName = null)
//    {
//        ArgumentNullException.ThrowIfNull(tableMeta);
//        ArgumentNullException.ThrowIfNull(manifest);
//        if (predicates.Count == 0) return null;
//
//        IndexAccessPlan? best = null;
//        var bestScore = long.MaxValue;
//
//        foreach (var candidate in EnumerateCandidates(tableMeta, manifest))
//        {
//            if (!MatchesAnyPredicate(candidate.Definition, predicates, hintFieldName)) continue;
//
//            var score = ScoreCandidate(candidate, manifest);
//            if (score >= bestScore) continue;
//            bestScore = score;
//            best = new IndexAccessPlan(
//                candidate.Definition.Name,
//                candidate.IsPrimaryKey,
//                candidate.SstFiles,
//                candidate.EstimatedRows);
//        }
//
//        return best;
//    }
//
//    /// <summary>
//    /// Resolves the index definition for a JOIN seek on <paramref name="joinFieldName"/>.
//    /// </summary>
//    internal static Config.SpeedIndexDefinition ResolveJoinIndex(
//        TableMetadata tableMeta,
//        Manifest manifest,
//        string joinFieldName)
//    {
//        ArgumentException.ThrowIfNullOrEmpty(joinFieldName);
//
//        var plan = SelectBestIndex(
//            tableMeta,
//            manifest,
//            [new EqualityPredicate(joinFieldName)],
//            joinFieldName);
//
//        if (plan is null)
//        {
//            throw new InvalidOperationException(
//                $"Indice per il campo '{joinFieldName}' non trovato nella tabella '{tableMeta.Name}'.");
//        }
//
//        return GetIndexDefinition(tableMeta.Schema, plan.IndexName)
//               ?? throw new InvalidOperationException(
//                   $"Indice '{plan.IndexName}' non trovato nella tabella '{tableMeta.Name}'.");
//    }
//
//    /// <summary>
//    /// ponytail: only top-level <c>alias.field = literal</c> or <c>field = literal</c> — no OR/functions.
//    /// </summary>
//    internal static ParsedEquality? TryParseSimpleEquality(string? expression, string? expectedTableAlias = null)
//    {
//        var list = TryParseCompoundAndEqualities(expression, expectedTableAlias);
//        return list is { Count: 1 } ? list[0] : null;
//    }
//
//    /// <summary>
//    /// ponytail: top-level AND of simple equalities only — no OR, no nested AND in literals.
//    /// </summary>
//    internal static IReadOnlyList<ParsedEquality>? TryParseCompoundAndEqualities(
//        string? expression,
//        string? expectedTableAlias = null)
//    {
//        if (string.IsNullOrWhiteSpace(expression)) return null;
//
//        var parts = SplitTopLevelAnd(expression.Trim());
//        if (parts.Count == 0) return null;
//
//        var result = new List<ParsedEquality>(parts.Count);
//        foreach (var part in parts)
//        {
//            if (!TryParseEqualityPart(part, expectedTableAlias, out var parsed))
//                return null;
//
//            result.Add(parsed);
//        }
//
//        return result;
//    }
//
//    /// <summary>Verifies a materialized row satisfies all parsed equalities.</summary>
//    internal static bool RowMatchesEqualities(
//        IReadOnlyDictionary<string, object?> row,
//        Config.SpeedSchema schema,
//        IReadOnlyList<ParsedEquality> equalities)
//    {
//        foreach (var eq in equalities)
//        {
//            if (!schema.Fields.Any(f => f.Name.Equals(eq.FieldName, StringComparison.OrdinalIgnoreCase)))
//                return false;
//
//            var field = schema.Fields.First(f => f.Name.Equals(eq.FieldName, StringComparison.OrdinalIgnoreCase));
//            if (!row.TryGetValue(eq.FieldName, out var actual) &&
//                !row.TryGetValue(field.Name, out actual))
//                return false;
//
//            var expected = CoerceLiteral(eq.RawValue, field.DataType);
//            if (expected is null) return false;
//
//            if (field.DataType is StringType)
//            {
//                var actualStr = actual?.ToString()?.TrimEnd() ?? string.Empty;
//                var expectedStr = expected.ToString()?.TrimEnd() ?? string.Empty;
//                if (!actualStr.Equals(expectedStr, StringComparison.Ordinal))
//                    return false;
//                continue;
//            }
//
//            if (actual?.Equals(expected) != true) return false;
//        }
//
//        return true;
//    }
//
//    private static bool TryParseEqualityPart(
//        string part,
//        string? expectedTableAlias,
//        out ParsedEquality parsed)
//    {
//        parsed = default!;
//        var eqIndex = FindTopLevelEquals(part);
//        if (eqIndex < 0) return false;
//
//        var left = part[..eqIndex].Trim();
//        var right = part[(eqIndex + 1)..].Trim();
//        if (left.Length == 0 || right.Length == 0) return false;
//
//        string fieldName;
//        string? alias = null;
//        var dot = left.IndexOf('.');
//        if (dot >= 0)
//        {
//            alias = left[..dot].Trim();
//            fieldName = left[(dot + 1)..].Trim();
//        }
//        else
//        {
//            fieldName = left;
//        }
//
//        if (string.IsNullOrEmpty(fieldName) || fieldName.Contains('(')) return false;
//
//        if (expectedTableAlias is not null && alias is not null &&
//            !alias.Equals(expectedTableAlias, StringComparison.OrdinalIgnoreCase))
//        {
//            return false;
//        }
//
//        parsed = new ParsedEquality(fieldName, alias, right);
//        return true;
//    }
//
//    private static List<string> SplitTopLevelAnd(string expression)
//    {
//        var parts = new List<string>();
//        var start = 0;
//        var inString = '\0';
//
//        for (var i = 0; i <= expression.Length - 4; i++)
//        {
//            var c = expression[i];
//            if (inString != '\0')
//            {
//                if (c == inString) inString = '\0';
//                continue;
//            }
//
//            if (c is '\'' or '"')
//            {
//                inString = c;
//                continue;
//            }
//
//            if (!expression.AsSpan(i, 4).Equals(" AND".AsSpan(), StringComparison.OrdinalIgnoreCase))
//                continue;
//
//            if (i > 0)
//            {
//                var before = expression[i - 1];
//                if (!char.IsWhiteSpace(before) && before is not '\'' and not '"')
//                    continue;
//            }
//
//            if (i + 4 < expression.Length)
//            {
//                var after = expression[i + 4];
//                if (!char.IsWhiteSpace(after) && !char.IsLetter(after))
//                    continue;
//            }
//
//            parts.Add(expression[start..i].Trim());
//            start = i + 4;
//            i = start - 1;
//        }
//
//        var tail = expression[start..].Trim();
//        if (tail.Length > 0)
//            parts.Add(tail);
//
//        return parts;
//    }
//
//    internal static bool PredicateMatchesIndex(string fieldName, Config.SpeedIndexDefinition indexDef) =>
//        fieldName.Equals(indexDef.Name, StringComparison.OrdinalIgnoreCase) ||
//        fieldName.Equals(indexDef.KeyExpression, StringComparison.OrdinalIgnoreCase);
//
//    internal static Config.SpeedIndexDefinition? GetIndexDefinition(Config.SpeedSchema schema, string indexName)
//    {
//        if (schema.PrimaryKey.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase))
//            return schema.PrimaryKey;
//
//        return schema.SecondaryKeys.FirstOrDefault(sk =>
//            sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase));
//    }
//
//    /// <summary>Coerces a WHERE literal into a CLR value for marshalling.</summary>
//      internal static object? CoerceLiteral(string rawValue, DataType dataType)
//    {
//        var trimmed = rawValue.Trim();
//        if (trimmed.Length == 0) return null;
//
//        if ((trimmed.StartsWith('\'') && trimmed.EndsWith('\'')) ||
//            (trimmed.StartsWith('"') && trimmed.EndsWith('"')))
//        {
//            return trimmed[1..^1];
//        }
//
//        return dataType switch
//        {
//            LongType => long.TryParse(trimmed, out var l) ? l : null,
//            IntegerType => int.TryParse(trimmed, out var i) ? i : null,
//            CurrencyType => decimal.TryParse(trimmed, out var d) ? d : null,
//            DoubleType => double.TryParse(trimmed, out var dbl) ? dbl : null,
//            StringType => trimmed,
//            _ => trimmed,
//        };
//    }
//
//    internal static int CountSstFiles(Manifest manifest, int tableId)
//    {
//        if (!manifest.TryGetTable(tableId, out var meta) || meta is null) return 0;
//        return meta.SstFilesByLevel.Values.Sum(files => files.Count);
//    }
//
//    internal static string GetIndexTableName(string mainTableName, string indexName) =>
//        $"_idx_{mainTableName}_{indexName}";
//
//    private readonly record struct IndexCandidate(
//        Config.SpeedIndexDefinition Definition,
//        bool IsPrimaryKey,
//        int SstFiles,
//        int TableId,
//        long EstimatedRows);
//
//    private static IEnumerable<IndexCandidate> EnumerateCandidates(TableMetadata tableMeta, Manifest manifest)
//    {
//        yield return new IndexCandidate(
//            tableMeta.Schema.PrimaryKey,
//            true,
//            CountSstFiles(manifest, tableMeta.Id),
//            tableMeta.Id,
//            manifest.GetRowCount(tableMeta.Id));
//
//        foreach (var secondary in tableMeta.Schema.SecondaryKeys)
//        {
//            var indexTableName = GetIndexTableName(tableMeta.Name, secondary.Name);
//            var indexTableId = -1;
//            var sstFiles = 0;
//            long rows = 0;
//            if (manifest.TryGetTable(indexTableName, out var indexMeta) && indexMeta is not null)
//            {
//                indexTableId = indexMeta.Id;
//                sstFiles = CountSstFiles(manifest, indexMeta.Id);
//                rows = manifest.GetRowCount(indexMeta.Id);
//            }
//
//            yield return new IndexCandidate(secondary, false, sstFiles, indexTableId, rows);
//        }
//    }
//
//    private static bool MatchesAnyPredicate(
//        Config.SpeedIndexDefinition indexDef,
//        IReadOnlyList<EqualityPredicate> predicates,
//        string? hintFieldName)
//    {
//        foreach (var predicate in predicates)
//        {
//            if (PredicateMatchesIndex(predicate.FieldName, indexDef)) return true;
//        }
//
//        return hintFieldName is not null && PredicateMatchesIndex(hintFieldName, indexDef);
//    }
//
//    private static long ScoreCandidate(IndexCandidate candidate, Manifest manifest)
//    {
//        // ponytail: fewer SST files first; trusted row count breaks ties; unique indexes win at equal cost.
//        var uniquePenalty = candidate.Definition.IsUnique ? 0L : 1_000L;
//        var rowPenalty = candidate.TableId >= 0 && manifest.IsRowCountTrusted(candidate.TableId)
//            ? candidate.EstimatedRows
//            : 0L;
//        return candidate.SstFiles * 1_000_000L + rowPenalty + uniquePenalty;
//    }
//
//    private static int FindTopLevelEquals(string expression)
//    {
//        var inString = '\0';
//        for (var i = 0; i < expression.Length; i++)
//        {
//            var c = expression[i];
//            if (inString != '\0')
//            {
//                if (c == inString) inString = '\0';
//                continue;
//            }
//
//            if (c is '\'' or '"')
//            {
//                inString = c;
//                continue;
//            }
//
//            if (c == '=')
//            {
//                var prev = i > 0 ? expression[i - 1] : '\0';
//                var next = i + 1 < expression.Length ? expression[i + 1] : '\0';
//                if (prev is '<' or '>' or '!' || next == '=') continue;
//                return i;
//            }
//        }
//
//        return -1;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

static void qo_trim_copy(const char* s, int start, int end, char* out, int cap)
{
    int n;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!s) return;
    while (start < end && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    n = end - start;
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    if (n > 0) memcpy(out, s + start, (size_t)n);
    out[n] = 0;
}

static int qo_find_top_level_equals(const char* expression)
{
    char inString = 0;
    int i, len;
    if (!expression) return -1;
    len = (int)strlen(expression);
    for (i = 0; i < len; i++) {
        char c = expression[i];
        if (inString != 0) {
            if (c == inString) inString = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            inString = c;
            continue;
        }
        if (c == '=') {
            char prev = i > 0 ? expression[i - 1] : 0;
            char next = i + 1 < len ? expression[i + 1] : 0;
            if (prev == '<' || prev == '>' || prev == '!' || next == '=') continue;
            return i;
        }
    }
    return -1;
}

static int qo_is_and_at(const char* s, int i, int len)
{
    char a, n, d;
    if (i + 4 > len) return 0;
    if (s[i] != ' ') return 0;
    a = s[i + 1];
    n = s[i + 2];
    d = s[i + 3];
    return (a == 'A' || a == 'a') && (n == 'N' || n == 'n') && (d == 'D' || d == 'd');
}

static int qo_is_or_at(const char* s, int i, int len)
{
    char o, r, after;
    if (i + 3 > len) return 0;
    if (s[i] != ' ') return 0;
    o = s[i + 1];
    r = s[i + 2];
    if (!((o == 'O' || o == 'o') && (r == 'R' || r == 'r'))) return 0;
    after = i + 3 < len ? s[i + 3] : ' ';
    if (after && !isspace((unsigned char)after)) return 0;
    return 1;
}

static int qo_has_top_level_or(const char* expression)
{
    int i, len;
    char inString = 0;
    if (!expression) return 0;
    len = (int)strlen(expression);
    for (i = 0; i <= len - 3; i++) {
        char c = expression[i];
        if (inString != 0) {
            if (c == inString) inString = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            inString = c;
            continue;
        }
        if (qo_is_or_at(expression, i, len)) return 1;
    }
    return 0;
}

static int qo_split_top_level_and(const char* expression, char parts[][256], int cap)
{
    int start = 0, n = 0, i, len;
    char inString = 0;
    if (!expression || cap <= 0) return 0;
    len = (int)strlen(expression);
    for (i = 0; i <= len - 4; i++) {
        char c = expression[i];
        if (inString != 0) {
            if (c == inString) inString = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            inString = c;
            continue;
        }
        if (!qo_is_and_at(expression, i, len)) continue;
        if (i > 0) {
            char before = expression[i - 1];
            if (!isspace((unsigned char)before) && before != '\'' && before != '"') continue;
        }
        if (i + 4 < len) {
            char after = expression[i + 4];
            if (!isspace((unsigned char)after) && !isalpha((unsigned char)after)) continue;
        }
        if (n >= cap) return n;
        qo_trim_copy(expression, start, i, parts[n], 256);
        if (parts[n][0]) n++;
        start = i + 4;
        i = start - 1;
    }
    if (n < cap) {
        qo_trim_copy(expression, start, len, parts[n], 256);
        if (parts[n][0]) n++;
    }
    return n;
}

static int qo_try_parse_equality_part(const char* part, const char* expectedTableAlias,
                                      QueryOptimizerParsedEquality* parsed)
{
    int eqIndex, len, dot;
    char left[128], right[256], fieldName[64], alias[64];
    if (!part || !parsed) return 0;
    memset(parsed, 0, sizeof(*parsed));
    eqIndex = qo_find_top_level_equals(part);
    if (eqIndex < 0) return 0;
    len = (int)strlen(part);
    qo_trim_copy(part, 0, eqIndex, left, (int)sizeof(left));
    qo_trim_copy(part, eqIndex + 1, len, right, (int)sizeof(right));
    if (!left[0] || !right[0]) return 0;
    alias[0] = 0;
    dot = -1;
    {
        int i;
        for (i = 0; left[i]; i++) {
            if (left[i] == '.') { dot = i; break; }
        }
    }
    if (dot >= 0) {
        qo_trim_copy(left, 0, dot, alias, (int)sizeof(alias));
        qo_trim_copy(left, dot + 1, (int)strlen(left), fieldName, (int)sizeof(fieldName));
    } else {
        snprintf(fieldName, sizeof(fieldName), "%s", left);
    }
    if (!fieldName[0] || strchr(fieldName, '(')) return 0;
    if (expectedTableAlias && alias[0] && gmb_strcasecmp(alias, expectedTableAlias) != 0)
        return 0;
    snprintf(parsed->FieldName, sizeof(parsed->FieldName), "%s", fieldName);
    snprintf(parsed->TableAlias, sizeof(parsed->TableAlias), "%s", alias);
    snprintf(parsed->RawValue, sizeof(parsed->RawValue), "%s", right);
    return 1;
}

int QueryOptimizer_PredicateMatchesIndex(const char* fieldName, const char* indexName, const char* keyExpression)
{
    if (!fieldName) return 0;
    if (indexName && gmb_strcasecmp(fieldName, indexName) == 0) return 1;
    if (keyExpression && gmb_strcasecmp(fieldName, keyExpression) == 0) return 1;
    return 0;
}

void QueryOptimizer_CoerceLiteral(const char* rawValue, char* out, int cap)
{
    int n;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!rawValue) return;
    while (*rawValue && isspace((unsigned char)*rawValue)) rawValue++;
    n = (int)strlen(rawValue);
    while (n > 0 && isspace((unsigned char)rawValue[n - 1])) n--;
    if (n >= 2 && ((rawValue[0] == '\'' && rawValue[n - 1] == '\'') ||
                   (rawValue[0] == '"' && rawValue[n - 1] == '"'))) {
        rawValue++;
        n -= 2;
    }
    if (n >= cap) n = cap - 1;
    if (n < 0) n = 0;
    memcpy(out, rawValue, (size_t)n);
    out[n] = 0;
}

int QueryOptimizer_TryParseCompoundAndEqualities(const char* expression, const char* expectedTableAlias,
                                                 QueryOptimizerParsedEquality* out, int cap, int* nOut)
{
    char parts[QueryOptimizer_MaxEqualities][256];
    char trimmed[1024];
    int i, nParts;
    if (nOut) *nOut = 0;
    if (!expression || !out || cap <= 0) return 0;
    qo_trim_copy(expression, 0, (int)strlen(expression), trimmed, (int)sizeof(trimmed));
    if (!trimmed[0]) return 0;
    if (qo_has_top_level_or(trimmed)) return 0;
    nParts = qo_split_top_level_and(trimmed, parts, QueryOptimizer_MaxEqualities);
    if (nParts == 0) return 0;
    if (nParts > cap) return 0;
    for (i = 0; i < nParts; i++) {
        if (!qo_try_parse_equality_part(parts[i], expectedTableAlias, &out[i]))
            return 0;
    }
    if (nOut) *nOut = nParts;
    return 1;
}

int QueryOptimizer_TryParseSimpleEquality(const char* expression, const char* expectedTableAlias,
                                          QueryOptimizerParsedEquality* out)
{
    QueryOptimizerParsedEquality list[QueryOptimizer_MaxEqualities];
    int n = 0;
    if (!QueryOptimizer_TryParseCompoundAndEqualities(expression, expectedTableAlias, list,
                                                      QueryOptimizer_MaxEqualities, &n))
        return 0;
    if (n != 1) return 0;
    if (out) *out = list[0];
    return 1;
}

static int qo_matches_any(const char* indexName, const char* keyExpression,
                          const QueryOptimizerParsedEquality* predicates, int predicateN,
                          const char* hintFieldName)
{
    int i;
    for (i = 0; i < predicateN; i++) {
        if (QueryOptimizer_PredicateMatchesIndex(predicates[i].FieldName, indexName, keyExpression))
            return 1;
    }
    return hintFieldName && QueryOptimizer_PredicateMatchesIndex(hintFieldName, indexName, keyExpression);
}

static int64_t qo_score(int sstFiles, int tableId, int64_t estimatedRows, int isUnique, const Manifest* manifest)
{
    int64_t uniquePenalty = isUnique ? 0 : 1000;
    int64_t rowPenalty = (tableId >= 0 && Manifest_IsRowCountTrusted(manifest, tableId)) ? estimatedRows : 0;
    return (int64_t)sstFiles * 1000000LL + rowPenalty + uniquePenalty;
}

int QueryOptimizer_SelectBestIndex(const Manifest* manifest, int tableId,
                                   const QueryOptimizerParsedEquality* predicates, int predicateN,
                                   const char* hintFieldName, QueryOptimizerIndexAccessPlan* out)
{
    int64_t bestScore = INT64_MAX;
    int found = 0, i, sst, secN;
    int64_t rows, score;
    char tableName[128];
    if (out) memset(out, 0, sizeof(*out));
    if (!manifest || !predicates || predicateN <= 0 || !out) return 0;
    if (!Manifest_TryGetTable(manifest, tableId, tableName, (int)sizeof(tableName))) return 0;

    sst = Manifest_CountSstFiles(manifest, tableId);
    rows = Manifest_GetRowCount(manifest, tableId);
    if (qo_matches_any("Id", "Id", predicates, predicateN, hintFieldName)) {
        score = qo_score(sst, tableId, rows, 1, manifest);
        if (score < bestScore) {
            bestScore = score;
            snprintf(out->IndexName, sizeof(out->IndexName), "Id");
            out->IsPrimaryKey = 1;
            out->EstimatedSstFiles = sst;
            out->EstimatedRows = rows;
            found = 1;
        }
    }

    secN = Manifest_SecondaryKeyCount(manifest, tableId);
    for (i = 0; i < secN; i++) {
        char iname[64], expr[128], idxTable[256];
        int idxId = -1, idxSst = 0;
        int64_t idxRows = 0;
        if (!Manifest_GetSecondaryKey(manifest, tableId, i, iname, (int)sizeof(iname),
                                      expr, (int)sizeof(expr), NULL))
            continue;
        if (!qo_matches_any(iname, expr, predicates, predicateN, hintFieldName)) continue;
        Manifest_IndexTableName(tableName, iname, idxTable, (int)sizeof(idxTable));
        if (Manifest_TryGetTableByName(manifest, idxTable, &idxId) && idxId > 0) {
            idxSst = Manifest_CountSstFiles(manifest, idxId);
            idxRows = Manifest_GetRowCount(manifest, idxId);
        }
        score = qo_score(idxSst, idxId, idxRows, 1, manifest);
        if (score >= bestScore && found) continue;
        bestScore = score;
        snprintf(out->IndexName, sizeof(out->IndexName), "%s", iname);
        out->IsPrimaryKey = 0;
        out->EstimatedSstFiles = idxSst;
        out->EstimatedRows = idxRows;
        found = 1;
    }
    return found;
}

int QueryOptimizer_ResolveJoinIndex(const Manifest* manifest, int tableId, const char* joinFieldName,
                                    char* indexNameOut, int cap)
{
    QueryOptimizerParsedEquality pred;
    QueryOptimizerIndexAccessPlan plan;
    if (!joinFieldName || !joinFieldName[0]) return 0;
    memset(&pred, 0, sizeof(pred));
    snprintf(pred.FieldName, sizeof(pred.FieldName), "%s", joinFieldName);
    if (!QueryOptimizer_SelectBestIndex(manifest, tableId, &pred, 1, joinFieldName, &plan))
        return 0;
    if (indexNameOut && cap > 0) snprintf(indexNameOut, (size_t)cap, "%s", plan.IndexName);
    return 1;
}

/* ponytail: seek verifies only the chosen index key; extra AND equalities
 * need schema Fields + Marshaller (RowMatchesEqualities). Upgrade: schema JSON 1:1. */
int Database_TrySeekIndexedEquality(Database* self, int tableId, const char* whereClause,
                                    uint8_t** value, int* valueLen)
{
    QueryOptimizerParsedEquality eqs[QueryOptimizer_MaxEqualities];
    QueryOptimizerIndexAccessPlan plan;
    Manifest* man;
    char tableName[128], coerced[256], idxName[64], idxExpr[128], idxTable[256];
    int n = 0, i, match = -1, found, pkLen = 0, idxId = 0, keyLen, secN, got;
    uint8_t* pk = NULL;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !whereClause) return 0;
    man = Database_GetManifest(self);
    if (!man) return 0;
    if (!QueryOptimizer_TryParseCompoundAndEqualities(whereClause, NULL, eqs,
                                                      QueryOptimizer_MaxEqualities, &n) || n <= 0)
        return 0;
    if (!QueryOptimizer_SelectBestIndex(man, tableId, eqs, n, eqs[0].FieldName, &plan))
        return 0;
    idxName[0] = 0;
    idxExpr[0] = 0;
    if (plan.IsPrimaryKey) {
        snprintf(idxName, sizeof(idxName), "Id");
        snprintf(idxExpr, sizeof(idxExpr), "Id");
    } else {
        secN = Manifest_SecondaryKeyCount(man, tableId);
        got = 0;
        for (i = 0; i < secN; i++) {
            if (!Manifest_GetSecondaryKey(man, tableId, i, idxName, (int)sizeof(idxName),
                                          idxExpr, (int)sizeof(idxExpr), NULL))
                continue;
            if (gmb_strcasecmp(idxName, plan.IndexName) == 0) { got = 1; break; }
        }
        if (!got) return 0;
    }
    for (i = 0; i < n; i++) {
        if (QueryOptimizer_PredicateMatchesIndex(eqs[i].FieldName, idxName, idxExpr)) {
            match = i;
            break;
        }
    }
    if (match < 0) return 0;
    QueryOptimizer_CoerceLiteral(eqs[match].RawValue, coerced, (int)sizeof(coerced));
    if (!coerced[0]) return 0;
    keyLen = (int)strlen(coerced);
    if (plan.IsPrimaryKey) {
        found = Database_Get(self, tableId, (const uint8_t*)coerced, keyLen, value, valueLen);
        return found ? 1 : -1;
    }
    if (!Manifest_TryGetTable(man, tableId, tableName, (int)sizeof(tableName))) return 0;
    Manifest_IndexTableName(tableName, plan.IndexName, idxTable, (int)sizeof(idxTable));
    if (!Manifest_TryGetTableByName(man, idxTable, &idxId) || idxId <= 0) return 0;
    found = Database_Get(self, idxId, (const uint8_t*)coerced, keyLen, &pk, &pkLen);
    if (!found || !pk || pkLen <= 0) {
        free(pk);
        return -1;
    }
    found = Database_Get(self, tableId, pk, pkLen, value, valueLen);
    free(pk);
    return found ? 1 : -1;
}

int QueryOptimizer_SelfCheck(void)
{
    QueryOptimizerParsedEquality eq, eqs[QueryOptimizer_MaxEqualities];
    QueryOptimizerIndexAccessPlan plan;
    char dir[] = "native/gmb_db_c/build/speedqo_XXXXXX";
    char coerced[64];
    Database* db;
    Manifest* man;
    int tableId, n = 0, rc, vlen;
    uint8_t* val;

    if (!QueryOptimizer_TryParseSimpleEquality("Name = 'alice'", NULL, &eq) ||
        strcmp(eq.FieldName, "Name") != 0 || strcmp(eq.RawValue, "'alice'") != 0)
        return -80;
    QueryOptimizer_CoerceLiteral(eq.RawValue, coerced, (int)sizeof(coerced));
    if (strcmp(coerced, "alice") != 0) return -81;
    if (!QueryOptimizer_TryParseCompoundAndEqualities("a.Name = \"bob\" AND Id = 1", "a", eqs,
                                                      QueryOptimizer_MaxEqualities, &n) || n != 2)
        return -82;
    if (strcmp(eqs[0].FieldName, "Name") != 0 || strcmp(eqs[1].FieldName, "Id") != 0)
        return -83;
    if (QueryOptimizer_TryParseCompoundAndEqualities("Name = 'x' OR Id = 1", NULL, eqs,
                                                     QueryOptimizer_MaxEqualities, &n))
        return -84;

    if (!mkdtemp(dir)) return -85;
    db = Database_Open(dir);
    if (!db) return -86;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -87; }
    if (Database_Set(db, tableId, (const uint8_t*)"k001", 4, (const uint8_t*)"alice", 5) != GMB_DB_OK) {
        Database_Dispose(db); return -88;
    }
    if (Database_Set(db, tableId, (const uint8_t*)"k002", 4, (const uint8_t*)"bobxx", 5) != GMB_DB_OK) {
        Database_Dispose(db); return -88;
    }
    if (Database_CreateSecondaryIndex(db, tableId, "ByName", "Name", 0) <= 0) {
        Database_Dispose(db); return -89;
    }
    man = Database_GetManifest(db);
    if (!QueryOptimizer_TryParseSimpleEquality("Name = 'alice'", NULL, &eq)) {
        Database_Dispose(db); return -90;
    }
    if (!QueryOptimizer_SelectBestIndex(man, tableId, &eq, 1, "Name", &plan) ||
        plan.IsPrimaryKey || strcmp(plan.IndexName, "ByName") != 0) {
        Database_Dispose(db); return -91;
    }
    if (!QueryOptimizer_TryParseSimpleEquality("Id = 'k001'", NULL, &eq) ||
        !QueryOptimizer_SelectBestIndex(man, tableId, &eq, 1, "Id", &plan) ||
        !plan.IsPrimaryKey) {
        Database_Dispose(db); return -92;
    }
    rc = Database_TrySeekIndexedEquality(db, tableId, "Name = 'alice'", &val, &vlen);
    if (rc != 1 || vlen != 5 || memcmp(val, "alice", 5) != 0) {
        free(val); Database_Dispose(db); return -93;
    }
    free(val);
    rc = Database_TrySeekIndexedEquality(db, tableId, "Name = 'nobody'", &val, &vlen);
    if (rc != -1) { free(val); Database_Dispose(db); return -94; }
    rc = Database_TrySeekIndexedEquality(db, tableId, "Id = 'k002'", &val, &vlen);
    if (rc != 1 || memcmp(val, "bobxx", 5) != 0) {
        free(val); Database_Dispose(db); return -95;
    }
    free(val);
    rc = Database_TrySeekIndexedEquality(db, tableId, "Unknown = 'x'", &val, &vlen);
    if (rc != 0) { free(val); Database_Dispose(db); return -96; }
    rc = Database_TrySeekIndexedEquality(db, tableId, "Name > 1", &val, &vlen);
    if (rc != 0) { free(val); Database_Dispose(db); return -97; }
    Database_Dispose(db);
    return GMB_DB_OK;
}
