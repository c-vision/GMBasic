// FILE: Interpreter.StdLib.Conversions.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Conversions — namespace 'CONVERSIONS'
 * 11 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (11):
 *   Bin
 *   CByte
 *   CCur
 *   CDbl
 *   CInt
 *   CLng
 *   CSng
 *   CStr
 *   Hex
 *   Oct
 *   Val
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CONVERSIONS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'CONVERSIONS'.
    /// Questa versione completa definisce le procedure native per tutte le funzioni,
    /// abilitando la sintassi qualificata moderna (es. CONVERSIONS.CSTR(...)).
    /// </summary>
    private void CreateConversionsNamespace()
    {
        const string namespaceName = "CONVERSIONS";
        var noFlags = SymbolFlags.None;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CStr, "CStr"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new StringType(),
                TokenKind.CStr),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Val, "Val"),
                [new Parameter(-1, T(TokenKind.Identifier, "sourceString"), new StringType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.Val),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Hex, "Hex"),
                [new Parameter(-1, T(TokenKind.Identifier, "number"), new InferredType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Hex),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Oct, "Oct"),
                [new Parameter(-1, T(TokenKind.Identifier, "number"), new InferredType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Oct),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Bin, "Bin"),
                [new Parameter(-1, T(TokenKind.Identifier, "number"), new InferredType(), noFlags, null, false)],
                new StringType(),
                TokenKind.Bin),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CSng, "CSng"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.CSng),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CDbl, "CDbl"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.CDbl),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CCur, "CCur"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new CurrencyType(), TokenKind.CCur),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CByte, "CByte"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new ByteType(),
                TokenKind.CByte),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CLng, "CLng"),
                [new Parameter(-1, T(TokenKind.Identifier, "expression"), new InferredType(), noFlags, null, false)],
                new LongType(),
                TokenKind.CLng),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CInt, "CInt"),
                [new Parameter(-1, T(TokenKind.Identifier, "value"), new InferredType(), noFlags, null, false)],
                new IntegerType(),
                TokenKind.CInt)
        };
        
        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}