// FILE: Interpreter.StdLib.Arrays.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Arrays — namespace 'ARRAYS'
 * 26 funzioni | 8 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (26):
 *   AADD
 *   ABINARYSEARCH
 *   ACHUNK
 *   ACLONE
 *   ACOPY
 *   ACOUNT
 *   ADEL
 *   AEVAL
 *   AFILL
 *   AFILTER
 *   AFLAT
 *   AINS
 *   AJOIN
 *   AMAP
 *   AMAX
 *   AMIN
 *   APAD
 *   AREDUCE
 *   AREVERSE
 *   ASCAN
 *   ASHUFFLE
 *   ASORT
 *   ASUM
 *   ATOKENS
 *   AUNIQUE
 *   AZIP
 * COMANDI (8):
 *   AADD
 *   ACOPY
 *   ADEL
 *   AEVAL
 *   AFILL
 *   AINS
 *   ASHUFFLE
 *   ASORT
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- ARRAYS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'ARRAYS'.
    /// VERSIONE CORRETTA: Aggiunge le registrazioni qualificate per i comandi.
    /// Aggiornate le firme per accettare puntatori a funzione (callback) ove necessario.
    /// </summary>
    private void CreateArraysNamespace()
    {
        const string namespaceName = "ARRAYS";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags flagsArrayByRef =
            SymbolFlags.IsByRef | SymbolFlags.IsArray;

        // Tipo generico per callback (accetta qualsiasi cosa)
        var anyFunctionType = new FunctionPointerType(ImmutableArray<DataType>.Empty, new InferredType());

        // --- Registrazione dei Comandi (per sintassi legacy) ---
        RegisterCommand(TokenKind.AAdd, "AADD", namespaceName, Parser.ParseAAddStatement);
        RegisterCommand(TokenKind.AIns, "AINS", namespaceName, Parser.ParseAInsStatement);
        RegisterCommand(TokenKind.ADel, "ADEL", namespaceName, Parser.ParseADelStatement);
        RegisterCommand(TokenKind.AFill, "AFILL", namespaceName, Parser.ParseAFillStatement);
        RegisterCommand(TokenKind.ACopy, "ACOPY", namespaceName, Parser.ParseACopyStatement);
        RegisterCommand(TokenKind.ASort, "ASORT", namespaceName, Parser.ParseASortStatement);
        RegisterCommand(TokenKind.AEval, "AEVAL", namespaceName, Parser.ParseAEvalStatement);
        RegisterCommand(TokenKind.AShuffle, "ASHUFFLE", namespaceName, Parser.ParseAShuffleStatement);

        // --- Registrazione per la sintassi qualificata dei COMANDI ---

        // --- Funzioni Native ---
        var procedures = new ProcedureDefinition[]
        {
            // --- Nuove definizioni per supportare l'uso in espressioni (Expression Style) ---
            
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ASort, "ASORT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "compareFunc"), anyFunctionType, SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "order"), new LongType(), SymbolFlags.IsOptional, null, false)
            ], new ArrayType(new InferredType()), TokenKind.ASort),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AEval, "AEVAL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "actionFunc"), anyFunctionType, SymbolFlags.None, null, false)
            ], new ArrayType(new InferredType()), TokenKind.AEval),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AAdd, "AADD"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), SymbolFlags.None, null, false)
            ], new ArrayType(new InferredType()), TokenKind.AAdd),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AIns, "AINS"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "position"), new LongType(), SymbolFlags.None, null, false)
            ], new ArrayType(new InferredType()), TokenKind.AIns),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ADel, "ADEL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "position"), new LongType(), SymbolFlags.None, null, false)
            ], new ArrayType(new InferredType()), TokenKind.ADel),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AFill, "AFILL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "start"), new LongType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), SymbolFlags.IsOptional, null, false)
            ], new ArrayType(new InferredType()), TokenKind.AFill),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ACopy, "ACOPY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "source"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "destination"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "srcStart"), new LongType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "count"), new LongType(), SymbolFlags.IsOptional, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "destStart"), new LongType(), SymbolFlags.IsOptional, null, false)
            ], new ArrayType(new InferredType()), TokenKind.ACopy),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AClone, "ACLONE"),
                [new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)],
                new ArrayType(new InferredType()),
                TokenKind.AClone),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AMap, "AMAP"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mapFunc"), anyFunctionType, SymbolFlags.None,
                    null, false)
            ], new ArrayType(new InferredType()), TokenKind.AMap),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AFilter, "AFILTER"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "filterFunc"), anyFunctionType, SymbolFlags.None,
                    null, false)
            ], new ArrayType(new InferredType()), TokenKind.AFilter),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AUnique, "AUNIQUE"),
                [new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)],
                new ArrayType(new InferredType()),
                TokenKind.AUnique),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AShuffle, "ASHUFFLE"),
                [new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)],
                new ArrayType(new InferredType()), TokenKind.AShuffle),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AReverse, "AREVERSE"),
                [new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)],
                new ArrayType(new InferredType()), TokenKind.AReverse),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AChunk, "ACHUNK"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "chunkSize"), new LongType(), SymbolFlags.None,
                    null, false)
            ], new ArrayType(new InferredType()), TokenKind.AChunk),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AFlat, "AFLAT"),
                [new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)],
                new ArrayType(new InferredType()),
                TokenKind.AFlat),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.APad, "APAD"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "newLength"), new LongType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "padValue"), new InferredType(),
                    SymbolFlags.IsOptional, null, false)
            ], new ArrayType(new InferredType()), TokenKind.APad),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AZip, "AZIP"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array1"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "array2"), new InferredType(), flagsArrayByRef, null, false)
            ], new ArrayType(new InferredType()), TokenKind.AZip),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ATokens, "ATOKENS"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "sourceString"), new StringType(),
                    SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "delimiters"), new StringType(),
                    SymbolFlags.IsOptional, null, false)
            ], new ArrayType(new StringType()), TokenKind.ATokens),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AScan, "ASCAN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "start"), new LongType(), SymbolFlags.IsOptional,
                    null, false)
            ], new LongType(), TokenKind.AScan),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ACount, "ACOUNT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "conditionFunc"), anyFunctionType,
                    SymbolFlags.None, null, false)
            ], new LongType(), TokenKind.ACount),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AReduce, "AREDUCE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "reduceFunc"), anyFunctionType, SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "initialValue"), new InferredType(),
                    SymbolFlags.IsOptional, null, false)
            ], new InferredType(), TokenKind.AReduce),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ABinarySearch, "ABINARYSEARCH"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), SymbolFlags.None,
                    null, false)
            ], new LongType(), TokenKind.ABinarySearch),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AJoin, "AJOIN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "separator"), new StringType(),
                    SymbolFlags.IsOptional, null, false)
            ], new StringType(), TokenKind.AJoin),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AMax, "AMAX"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)
            ], new DoubleType(), TokenKind.AMax),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AMin, "AMIN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)
            ], new DoubleType(), TokenKind.AMin),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ASum, "ASUM"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "array"), new InferredType(), flagsArrayByRef, null, false)
            ], new DoubleType(), TokenKind.ASum)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}