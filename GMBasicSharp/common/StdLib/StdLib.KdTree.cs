// FILE: Interpreter.StdLib.KdTree.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: KdTree — namespace 'KDTREE'
 * 4 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   CREATE
 *   QUERY
 *   QUERYBALL
 *   QUERYDIST
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- KDTREE NAMESPACE (scipy.spatial.KDTree) ---
    // ========================================================================

    private void CreateKdTreeNamespace()
    {
        const string namespaceName = "KDTREE";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        var doubleArr = new ArrayType(new DoubleType());
        var doubleMat = new ArrayType(doubleArr);

        var pointsParam = new Parameter(-1, T(TokenKind.Identifier, "points"), doubleArr, arrayFlag, null, false);
        var mParam = new Parameter(-1, T(TokenKind.Identifier, "m"), new LongType(), noFlags, null, false);
        var handleParam = new Parameter(-1, T(TokenKind.Identifier, "h"), new LongType(), noFlags, null, false);
        var queryParam = new Parameter(-1, T(TokenKind.Identifier, "x"), doubleArr, arrayFlag, null, false);
        var kParam = new Parameter(-1, T(TokenKind.Identifier, "k"), new LongType(), noFlags, null, false);
        var rParam = new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KdTreeCreate, "CREATE"),
                [pointsParam, mParam], new LongType(), TokenKind.KdTreeCreate),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KdTreeQuery, "QUERY"),
                [handleParam, queryParam, kParam], doubleArr, TokenKind.KdTreeQuery),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KdTreeQueryDist, "QUERYDIST"),
                [handleParam, queryParam, kParam], doubleArr, TokenKind.KdTreeQueryDist),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KdTreeQueryBall, "QUERYBALL"),
                [handleParam, queryParam, rParam], doubleArr, TokenKind.KdTreeQueryBall),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
