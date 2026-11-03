// FILE: Interpreter.StdLib.Math.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Math — namespace 'MATH'
 * 42 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (42):
 *   Abs
 *   Acos
 *   Angle2d
 *   Asin
 *   Atn
 *   Avg
 *   COSH
 *   Ceiling
 *   Clamp
 *   Comb
 *   Cos
 *   Deg2Rad
 *   Dist2d
 *   Dist3d
 *   Exp
 *   Fact
 *   Fix
 *   Floor
 *   Hypot
 *   Int
 *   IsNumeric
 *   Log
 *   Log10
 *   Max
 *   Min
 *   POLAR_X
 *   POLAR_Y
 *   Perm
 *   Pi
 *   Pythagoras
 *   Rad2Deg
 *   Range
 *   Rnd
 *   Round
 *   SINH
 *   Sgn
 *   Sin
 *   Sqr
 *   Sum
 *   TANH
 *   Tan
 *   Trunc
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- MATH NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'MATH'.
    /// </summary>
    private void CreateMathNamespace()
    {
        const string namespaceName = "MATH";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Rnd, "Rnd"),
                [new Parameter(-1, T(TokenKind.Identifier, "range"), new DoubleType(), optionalFlag, null, false)],
                new DoubleType(),
                TokenKind.Rnd),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Pi, "Pi"), [], new DoubleType(), TokenKind.Pi),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Abs, "Abs"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new InferredType(),
                TokenKind.Abs),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sgn, "Sgn"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new LongType(),
                TokenKind.Sgn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sqr, "Sqr"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Sqr),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Log, "Log"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Log),
            // `LN` REMOVED (decisione utente 2026-10-27): l'alias Clipper dell'logaritmo non e'
            // piu' una grafia di superficie. Resta SOLO `Log` (= MATH.LOG), che il generatore
            // scrive come nome enum canonico in gmb_fn_lookup.h. Una sola grafia per funzione.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Exp, "Exp"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Exp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sin, "Sin"),
                [new Parameter(-1, T(TokenKind.Identifier, "angleRadians"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Sin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cos, "Cos"),
                [new Parameter(-1, T(TokenKind.Identifier, "angleRadians"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Cos),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Tan, "Tan"),
                [new Parameter(-1, T(TokenKind.Identifier, "angleRadians"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Tan),
            /* ATN(value[, x]): un solo comando per le due arità, senza token "2".
             * ATN(y)    = arctan(y)                          → atan(y)
             * ATN(y, x) = angolo del vettore (x, y) in [-π, π] → atan2(y, x)
             * Il 2° parametro è OPZIONALE: stesso pattern di ROUND/TRUNC e delle altre
             * builtin con argomento opzionale (SymbolFlags.IsOptional). La tabella nativa
             * generata (tools/registry-export → gmb_registry.gen.h) ne deriva l'arity 1..2. */
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Atn, "Atn"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), optionalFlag, null, false)
            ],
            new DoubleType(),
            TokenKind.Atn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsNumeric, "IsNumeric"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new BitType(),
                TokenKind.IsNumeric),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Round, "Round"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "value"), new CurrencyType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "decimals"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Round),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Trunc, "Trunc"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "value"), new CurrencyType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "decimals"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Trunc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Fix, "Fix"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Fix),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Floor, "Floor"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Floor),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Int, "Int"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Int),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Ceiling, "Ceiling"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Ceiling),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Log10, "Log10"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Log10),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Asin, "Asin"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Asin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Acos, "Acos"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Acos),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sinh, "SINH"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Sinh),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cosh, "COSH"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Cosh),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Tanh, "TANH"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Tanh),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Deg2Rad, "Deg2Rad"),
                [new Parameter(-1, T(TokenKind.Identifier, "degrees"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Deg2Rad),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Rad2Deg, "Rad2Deg"),
                [new Parameter(-1, T(TokenKind.Identifier, "radians"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Rad2Deg),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Sum, "Sum"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new DoubleType(), TokenKind.Sum),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Avg, "Avg"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new DoubleType(), TokenKind.Avg, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Min, "Min"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new InferredType(), TokenKind.Min),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Max, "Max"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new InferredType(), TokenKind.Max),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Clamp, "Clamp"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "min"), new InferredType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "max"), new InferredType(), noFlags, null, false)
            ], new InferredType(), TokenKind.Clamp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Range, "Range"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "values"), new ArrayType(new InferredType()),
                        SymbolFlags.IsArray | SymbolFlags.IsParamArray, null, false)
                ],
                new ArrayType(new InferredType()), TokenKind.Range),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Hypot, "Hypot"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Hypot),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Pythagoras, "Pythagoras"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Pythagoras),
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
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Fact, "Fact"),
                [new Parameter(-1, T(TokenKind.Identifier, "n"), new UQuadType(), noFlags, null, false)],
                new UQuadType(),
                TokenKind.Fact),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Perm, "Perm"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Perm),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Comb, "Comb"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Comb)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}