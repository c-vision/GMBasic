// FILE: Interpreter.StdLib.Chart.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Chart — namespace 'CHART'
 * 1 funzioni | 6 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   CREATE
 * COMANDI (6):
 *   CLEAR
 *   CLOSE
 *   FINANCIAL
 *   PLOT
 *   SAVE
 *   SHOW
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CHART NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'CHART'.
    /// </summary>
    private void CreateChartNamespace()
    {
        const string namespaceName = "CHART";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        
        RegisterCommand(TokenKind.Chart, "CHART", namespaceName, Parser.DispatchChartStatement);

        // --- I comandi CHART sono SOLO legacy (sintassi spazio: CHART PLOT, CHART SAVE).
        // Non esiste sintassi qualificata per i comandi CHART; l'unica funzione (CREATE,
        // che restituisce un handle) resta qualificata con parentesi — vedi procedures qui sotto.
        // Le registrazioni qualificate per nome (registro qualificato) sono state RIMOSSE.
        //
        var procedures = new ProcedureDefinition[]
        {
            // CREATE è l'unica funzione perché restituisce un handle.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Create, "CREATE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "Titolo"), new StringType(), SymbolFlags.None,
                        null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "XLabel"), new StringType(),
                        SymbolFlags.IsOptional, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "YLabel"), new StringType(),
                        SymbolFlags.IsOptional, null, false)
                ],
                new LongType(),
                TokenKind.ChartCreate),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}