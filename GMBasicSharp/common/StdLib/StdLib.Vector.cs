// FILE: Interpreter.StdLib.Vector.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Vector — namespace 'VECTOR'
 * 13 funzioni | 9 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (13):
 *   Angle2d
 *   CHEBYSHEV
 *   COSINE
 *   CREATE
 *   Dist2d
 *   Dist3d
 *   EUCLIDEA
 *   MANHATTAN
 *   MINKOWSKI
 *   POLAR_X
 *   POLAR_Y
 *   PRODUCT
 * COMANDI (9):
 *   ADD
 *   ADD WRAP
 *   BOUNCE2D
 *   DIV
 *   FILL RANDOM
 *   FUNCTION
 *   MUL
 *   SET
 *   WRAP
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- VECTOR NAMESPACES ---
    // ========================================================================
    
    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'VECTOR'.
    /// Contiene comandi e funzioni per i vettori.
    /// </summary>
    private void CreateVectorNamespace()
    {
        const string namespaceName = "VECTOR";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;
        var xDataParam = new Parameter(-1, T(TokenKind.Identifier, "x_data"), new DoubleType(),
            SymbolFlags.IsArray, null, false);
        var yDataParam = new Parameter(-1, T(TokenKind.Identifier, "y_data"), new DoubleType(),
            SymbolFlags.IsArray, null, false);

        // --- Comandi (Sintassi Legacy) ---
        RegisterCommand(TokenKind.Vector, "VECTOR", namespaceName, Parser.DispatchVectorStatement);

        // --- Funzioni (Sintassi Moderna Qualificata: es. VECTOR.CREATE) ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LinAlgVectorCreateFunc, "CREATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "z"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "w"), new DoubleType(), optionalFlag, null, false)
                ],
                new LongType(), TokenKind.LinAlgVectorCreateFunc),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DotProduct, "PRODUCT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.DotProduct),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Norm, "EUCLIDEA"),
                [ new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false) ],
                new DoubleType(), TokenKind.Norm),

            // Metriche di distanza (scipy.spatial.distance) — tra due vettori (handle)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VectorManhattan, "MANHATTAN"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.VectorManhattan),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VectorChebyshev, "CHEBYSHEV"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.VectorChebyshev),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VectorCosineDistance, "COSINE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.VectorCosineDistance),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VectorMinkowski, "MINKOWSKI"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "handle1"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "handle2"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.VectorMinkowski),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VGet, "VGET"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "index"), new LongType(), noFlags, null, false)], new DoubleType(), TokenKind.VGet,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VAdd, "VADD"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VAdd),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VSub, "VSUB"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VSub),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VMul, "VMUL"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VMul),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VDiv, "VDIV"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VDiv),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VCross, "VCROSS"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VCross),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VMin, "VMIN"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VMin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VMax, "VMAX"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VMax),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VScale, "VSCALE"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "s"), new DoubleType(), noFlags, null, false)], new LongType(), TokenKind.VScale),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VNorm, "VNORM"),
                [new Parameter(-1, T(TokenKind.Identifier, "handle"), new LongType(), noFlags, null, false)], new LongType(), TokenKind.VNorm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VLerp, "VLERP"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "t"), new DoubleType(), noFlags, null, false)], new LongType(), TokenKind.VLerp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VAngle, "VANGLE"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new DoubleType(), TokenKind.VAngle,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.VDist, "VDIST"),
                [new Parameter(-1, T(TokenKind.Identifier, "a"), new LongType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "b"), new LongType(), noFlags, null, false)], new DoubleType(), TokenKind.VDist,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.EuclideanDistanceSquared, "EuclideanDistanceSquared"),
                [xDataParam, yDataParam], new DoubleType(), TokenKind.EuclideanDistanceSquared,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dist2d, "Dist2d"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "x2"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Dist2d),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Dist3d, "Dist3d"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "z1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "x2"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y2"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "z2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Dist3d),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PolarX, "POLAR_X"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "radius"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "angleDegrees"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.PolarX),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PolarY, "POLAR_Y"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "radius"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "angleDegrees"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.PolarY),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Angle2d, "Angle2d"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y1"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "x2"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y2"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Angle2d),
        };
        
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
