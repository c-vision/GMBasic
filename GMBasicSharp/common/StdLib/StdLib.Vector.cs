// FILE: Interpreter.StdLib.Vector.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Vector — namespace 'VECTOR'
 * 7 funzioni | 9 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (7):
 *   CHEBYSHEV
 *   COSINE
 *   CREATE
 *   EUCLIDEA
 *   MANHATTAN
 *   MINKOWSKI
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
                new DoubleType(), TokenKind.VectorMinkowski)
        };
        
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
