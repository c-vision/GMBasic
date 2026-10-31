// FILE: Interpreter.StdLib.Trading.Options.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading.Options — namespace 'TRADING.OPTIONS'
 * 16 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (16):
 *   BinomialOption
 *   BsCall
 *   BsPut
 *   Charm
 *   Delta
 *   DeltaSpeed
 *   Gamma
 *   GammaColor
 *   MertonCall
 *   MertonPut
 *   Rho
 *   Theta
 *   Vanna
 *   Vega
 *   Volga
 *   Zomma
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING.OPTIONS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TRADING.OPTIONS'.
    /// Contiene funzioni specifiche per la valutazione di opzioni.
    /// </summary>
    private void CreateTradingOptionsNamespace()
    {
        const string namespaceName = "TRADING.OPTIONS";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var optionalFlag = SymbolFlags.IsOptional;

        // --- Dichiarazione dei parametri prima dell'uso ---
        var S = new Parameter(-1, T(TokenKind.Identifier, "S"), new DoubleType(), noFlags, null, false); // Prezzo Sottostante
        var K = new Parameter(-1, T(TokenKind.Identifier, "K"), new DoubleType(), noFlags, null, false); // Strike Price
        var T_param =
            new Parameter(-1, T(TokenKind.Identifier, "T"), new DoubleType(), noFlags,
                null, false); // Tempo a scadenza (anni) - Rinominato per evitare conflitto con 'T' generico
        var r = new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false); // Tasso risk-free
        var v = new Parameter(-1, T(TokenKind.Identifier, "vol"), new DoubleType(), noFlags, null, false); // Volatilità
        var q = new Parameter(-1, T(TokenKind.Identifier, "q"), new DoubleType(), optionalFlag, null, false); // Dividend yield
        var type = new Parameter(-1, T(TokenKind.Identifier, "type"), new StringType(), noFlags,
            null, false); // "CALL" o "PUT"

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BsCall, "BsCall"),
                [S, K, T_param, r, v, q], new DoubleType(),
                TokenKind.BsCall, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BsPut, "BsPut"),
                [S, K, T_param, r, v, q], new DoubleType(),
                TokenKind.BsPut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MertonCall, "MertonCall"),
                [S, K, T_param, r, v, q], new DoubleType(),
                TokenKind.MertonCall, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MertonPut, "MertonPut"),
                [S, K, T_param, r, v, q], new DoubleType(),
                TokenKind.MertonPut, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BinomialOption, "BinomialOption"),
                [
                    S, K, T_param, r, v,
                    new Parameter(-1, T(TokenKind.Identifier, "steps"), new IntegerType(), noFlags, null, false), type
                ], new DoubleType(),
                TokenKind.BinomialOption, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Delta, "Delta"),
                [S, K, T_param, r, v, type], new DoubleType(),
                TokenKind.Delta, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Gamma, "Gamma"),
                [S, K, T_param, r, v], new DoubleType(),
                TokenKind.Gamma, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Theta, "Theta"),
                [S, K, T_param, r, v, type], new DoubleType(),
                TokenKind.Theta, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Vega, "Vega"),
                [S, K, T_param, r, v], new DoubleType(), TokenKind.Vega, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Rho, "Rho"),
                [S, K, T_param, r, v, type], new DoubleType(), TokenKind.Rho, ExecutionKind: NativeExecutionKind.Pure),
            // --- Greche avanzate per opzioni ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Vanna, "Vanna"),
                [S, K, T_param, r, v], new DoubleType(), TokenKind.Vanna, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Volga, "Volga"),
                [S, K, T_param, r, v], new DoubleType(), TokenKind.Volga, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Charm, "Charm"),
                [S, K, T_param, r, v, type], new DoubleType(), TokenKind.Charm, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Zomma, "Zomma"),
                [S, K, T_param, r, v], new DoubleType(), TokenKind.Zomma),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DeltaSpeed, "DeltaSpeed"),
                [S, K, T_param, r, v], new DoubleType(),
                TokenKind.DeltaSpeed, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GammaColor, "GammaColor"),
                [S, K, T_param, r, v], new DoubleType(),
                TokenKind.GammaColor, ExecutionKind: NativeExecutionKind.Pure)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}