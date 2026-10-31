/* FILE: native/gmb_db_c/src/ISAM/IsamJoinStrategy.c */
/* Port 1:1 di modules/Database/ISAM/IsamJoinStrategy.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamJoinStrategy
//
//using Basic.Databases;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Pianifica l'esecuzione di equi-join ISAM: index-nested-loop, hash join o nested-loop.
///// </summary>
//internal static class IsamJoinStrategy
//{
//    /// <summary>ponytail: hash join solo se la tabella build ≤ 10k record; altrimenti nested-loop.</summary>
//    private const int HashJoinMaxBuildRecords = 10_000;
//
//    internal enum ExecutionMode
//    {
//        NestedLoop,
//        IndexNestedLoop,
//        HashJoin,
//    }
//
//    internal readonly record struct EquiJoinPlan(
//        string ProbeField,
//        string BuildField,
//        Config.IndexDefinition BuildIndex,
//        BPlusTreeManager BuildIndexManager,
//        ExecutionMode Mode,
//        bool BuildOnSecondary);
//
//    /// <summary>
//    /// Prova a estrarre <c>left = right</c> da un AST di join (campi qualificati semplici).
//    /// </summary>
//    internal static bool TryParseEquiJoin(Expr? expression, out string leftField, out string rightField)
//    {
//        leftField = string.Empty;
//        rightField = string.Empty;
//        if (expression is not BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.ExactlyEqual } binary)
//        {
//            return false;
//        }
//
//        if (binary.Left is not QualifiedNameExpr { Parts.Length: 1 } left
//            || binary.Right is not QualifiedNameExpr { Parts.Length: 1 } right)
//        {
//            return false;
//        }
//
//        leftField = left.Parts[0].Lexeme;
//        rightField = right.Parts[0].Lexeme;
//        return !string.IsNullOrEmpty(leftField) && !string.IsNullOrEmpty(rightField);
//    }
//
//  /// <summary>
//    /// Sceglie index-nested-loop, hash join o nested-loop per un equi-join.
//    /// </summary>
//    internal static bool TryPlanEquiJoin(
//        Engine primaryEngine,
//        Engine secondaryEngine,
//        string primaryField,
//        string secondaryField,
//        out EquiJoinPlan plan)
//    {
//        plan = default!;
//        if (TryResolveIndexOnField(secondaryEngine, secondaryField, out var indexDef, out var indexManager))
//        {
//            plan = new EquiJoinPlan(
//                primaryField, secondaryField, indexDef, indexManager,
//                ExecutionMode.IndexNestedLoop, BuildOnSecondary: true);
//            return true;
//        }
//
//        if (TryResolveIndexOnField(primaryEngine, primaryField, out indexDef, out indexManager))
//        {
//            plan = new EquiJoinPlan(
//                secondaryField, primaryField, indexDef, indexManager,
//                ExecutionMode.IndexNestedLoop, BuildOnSecondary: false);
//            return true;
//        }
//
//        var primaryRecords = EstimateRecordCount(primaryEngine);
//        var secondaryRecords = EstimateRecordCount(secondaryEngine);
//        var buildOnSecondary = secondaryRecords <= primaryRecords;
//        var buildRecords = buildOnSecondary ? secondaryRecords : primaryRecords;
//        if (buildRecords is > 0 and <= HashJoinMaxBuildRecords)
//        {
//            plan = buildOnSecondary
//                ? new EquiJoinPlan(
//                    primaryField, secondaryField, default!, default!,
//                    ExecutionMode.HashJoin, BuildOnSecondary: true)
//                : new EquiJoinPlan(
//                    secondaryField, primaryField, default!, default!,
//                    ExecutionMode.HashJoin, BuildOnSecondary: false);
//            return true;
//        }
//
//        return false;
//    }
//
//    internal static bool TryResolveIndexOnField(
//        Engine engine,
//        string fieldName,
//        out Config.IndexDefinition indexDef,
//        out BPlusTreeManager indexManager)
//    {
//        indexDef = default!;
//        indexManager = default!;
//
//        if (IndexCoversField(engine.Schema.PrimaryKey, fieldName))
//        {
//            indexDef = engine.Schema.PrimaryKey;
//            indexManager = engine.PrimaryIndexManager;
//            return true;
//        }
//
//        foreach (var secondary in engine.Schema.SecondaryKeys)
//        {
//            if (!IndexCoversField(secondary, fieldName))
//            {
//                continue;
//            }
//
//            if (engine.SecondaryIndexManagers.TryGetValue(secondary.Name, out var manager))
//            {
//                indexDef = secondary;
//                indexManager = manager;
//                return true;
//            }
//        }
//
//        return false;
//    }
//
//    internal static byte[] BuildSeekKey(
//        object? fieldValue,
//        Config.IndexDefinition indexDef,
//        KeyExpressionEvaluatorCallback keyEvaluator,
//        Engine buildEngine,
//        string buildField)
//    {
//        var context = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase)
//        {
//            [buildField] = fieldValue,
//        };
//        var targetType = buildEngine.GetFieldDataType(buildField) ?? new StringType(indexDef.KeyLength);
//        var keyValue = keyEvaluator(indexDef.KeyExpression, context, targetType);
//        return SchemaManager.SerializeKeyValue(keyValue, indexDef);
//    }
//
//    internal static object? GetFieldValue(UdtInstance record, string fieldName)
//    {
//        var field = record.Definition.Fields.FirstOrDefault(f =>
//            f.Name.Lexeme.Equals(fieldName, StringComparison.OrdinalIgnoreCase));
//        if (field is null)
//        {
//            return null;
//        }
//
//        record.Fields.TryGetValue(field.SymbolId, out var value);
//        return value;
//    }
//
//    internal static bool FieldValuesEqual(object? left, object? right)
//    {
//        if (left is null || right is null)
//        {
//            return left is null && right is null;
//        }
//
//        if (TryToInt64(left, out var leftLong) && TryToInt64(right, out var rightLong))
//        {
//            return leftLong == rightLong;
//        }
//
//        return string.Equals(left.ToString(), right.ToString(), StringComparison.OrdinalIgnoreCase);
//    }
//
//    private static bool IndexCoversField(Config.IndexDefinition indexDef, string fieldName) =>
//        IsamFilterPushdown.TryGetKeyFieldNames(indexDef.KeyExpression, out var names)
//        && names.Length == 1
//        && names[0].Equals(fieldName, StringComparison.OrdinalIgnoreCase);
//
//    private static int EstimateRecordCount(Engine engine)
//    {
//        var handle = engine.Handle;
//        var recordSize = handle.Header.RecordSize;
//        if (recordSize <= 0)
//        {
//            return 0;
//        }
//
//        var dataBytes = Math.Max(0, handle.GetStream().Length - handle.DataAreaStartOffset);
//        return (int)(dataBytes / recordSize);
//    }
//
//    private static bool TryToInt64(object? value, out long result)
//    {
//        result = 0;
//        switch (value)
//        {
//            case long l:
//                result = l;
//                return true;
//            case int i:
//                result = i;
//                return true;
//            case short s:
//                result = s;
//                return true;
//            case byte b:
//                result = b;
//                return true;
//            default:
//                return long.TryParse(value?.ToString(), out result);
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

int IsamJoinStrategy_IndexCoversField(const IsamIndexDefinition* def, const char* fieldName)
{
    char names[IsamFilterPushdown_MaxEq][IsamFilterPushdown_NameCap];
    int n = 0;
    const char* expr;
    if (!def || !fieldName || !fieldName[0]) return 0;
    expr = def->KeyExpression[0] ? def->KeyExpression : def->Name;
    if (!IsamFilterPushdown_TryGetKeyFieldNames(expr, names, IsamFilterPushdown_MaxEq, &n))
        return 0;
    return n == 1 && gmb_strcasecmp(names[0], fieldName) == 0;
}

int IsamJoinStrategy_FieldValuesEqual(const IsamFieldValue* a, int aText, const IsamFieldValue* b, int bText)
{
    const char *sa, *sb;
    if (!a || !b) return 0;
    if (!aText && !bText) return a->i64 == b->i64;
    sa = a->text ? a->text : (a->textOut ? a->textOut : "");
    sb = b->text ? b->text : (b->textOut ? b->textOut : "");
    if (!aText) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%lld", (long long)a->i64);
        return gmb_strcasecmp(tmp, sb) == 0;
    }
    if (!bText) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%lld", (long long)b->i64);
        return gmb_strcasecmp(sa, tmp) == 0;
    }
    return gmb_strcasecmp(sa, sb) == 0;
}

int IsamJoinStrategy_SerializeFieldKey(const IsamFieldValue* v, int isText, int keyLength, uint8_t* out)
{
    const char* t;
    if (!v || !out || keyLength <= 0) return GMB_DB_ERR;
    if (isText) {
        t = v->text ? v->text : (v->textOut ? v->textOut : "");
        return SchemaManager_SerializeKeyValueText(t, keyLength, out);
    }
    return SchemaManager_SerializeKeyValueI64(v->i64, keyLength, out);
}

void IsamJoinStrategy_HashKey(const IsamFieldValue* v, int isText, char* out, int cap)
{
    const char* t;
    int i;
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!v) return;
    if (isText) {
        t = v->text ? v->text : (v->textOut ? v->textOut : "");
        snprintf(out, (size_t)cap, "%s", t);
        for (i = 0; out[i]; i++) out[i] = (char)toupper((unsigned char)out[i]);
        return;
    }
    snprintf(out, (size_t)cap, "%lld", (long long)v->i64);
}
