// FILE: Interpreter.StdLib.Spatial.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Spatial — namespace 'SPATIAL'
 * 4 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (4):
 *   CDIST
 *   CONVEXHULL
 *   DISTANCEMATRIX
 *   PDIST
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SPATIAL NAMESPACE (scipy.spatial.distance + ConvexHull) ---
    // ========================================================================

    private void CreateSpatialNamespace()
    {
        const string namespaceName = "SPATIAL";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        var doubleArr = new ArrayType(new DoubleType());
        var doubleMat = new ArrayType(doubleArr);

        var XParam = new Parameter(-1, T(TokenKind.Identifier, "X"), doubleMat, arrayFlag, null, false);
        var YParam = new Parameter(-1, T(TokenKind.Identifier, "Y"), doubleMat, arrayFlag, null, false);
        var metricParam = new Parameter(-1, T(TokenKind.Identifier, "metric"), new StringType(), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            // scipy.spatial.distance.pdist — distanze condensate (triangolo superiore).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpatialPdist, "PDIST"),
                [XParam, metricParam], doubleArr, TokenKind.SpatialPdist),
            // scipy.spatial.distance.cdist — matrice di distanze X×Y.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpatialCdist, "CDIST"),
                [XParam, YParam, metricParam], new LongType(), TokenKind.SpatialCdist),
            // scipy.spatial.distance_matrix — distanze euclidee X×Y.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpatialDistanceMatrix, "DISTANCEMATRIX"),
                [XParam, YParam], new LongType(), TokenKind.SpatialDistanceMatrix),
            // scipy.spatial.ConvexHull — indici vertici inviluppo convesso (2D).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SpatialConvexHull, "CONVEXHULL"),
                [XParam], doubleArr, TokenKind.SpatialConvexHull)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
