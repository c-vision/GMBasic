// FILE: Interpreter.StdLib.Clock.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Clock — namespace 'CLOCK' (tempo a passo fisso)
 * 4 funzioni | 1 comando | 0 tipi | 0 costanti
 *
 * FUNZIONI (4): DT, ALPHA, FPS, SCALE
 * COMANDO (1): CLOCK (PAUSE | RESUME | SCALE | FPS)
 * ==== GMB-STDLIB-AUDIT-END ==== */

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CLOCK NAMESPACE (tempo a passo fisso) ---
    // ========================================================================

    /// <summary>
    /// Registra il dominio <c>CLOCK</c>: il comando che governa l'orologio a passo fisso del
    /// ciclo grafico (PAUSE/RESUME/SCALE/FPS) e le quattro letture qualificate. Non c'è logica
    /// qui: lo stato vive nel runtime C (<c>include/gmb_clock.h</c>) e ogni backend lo raggiunge
    /// con la facciata <c>gmb_clock_frame_*</c> — P/Invoke per IL/PCodeVM (CommonLib.ClockNative),
    /// chiamata diretta per VM/C e C99Native. Una sola sorgente di tempo (§0bis).
    /// </summary>
    private void CreateClockNamespace()
    {
        const string namespaceName = "CLOCK";

        // --- comando: CLOCK PAUSE | RESUME | SCALE s | FPS f ---
        // UN comando solo, verbi a spazio (§1, nessuna parentesi). Il verbo FPS è una SOFT
        // keyword normalizzata nel parser: non entra nel lessico, così `fps` resta un nome
        // di variabile per i programmi esistenti.
        RegisterCommand(TokenKind.Clock, "CLOCK", namespaceName, Parser.DispatchClockStatement);

        // --- letture (funzioni: leggono e basta, §1) ---
        var procedures = new ProcedureDefinition[]
        {
            // DT(): durata di UN passo di simulazione, in secondi (= 1 / FPS).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic | SymbolFlags.IsFunction,
                T(TokenKind.ClockDt, "DT"), [], new DoubleType(), TokenKind.ClockDt),

            // ALPHA(): parte di passo rimasta nella coda, 0..1 (per interpolare il disegno).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic | SymbolFlags.IsFunction,
                T(TokenKind.ClockAlpha, "ALPHA"), [], new DoubleType(), TokenKind.ClockAlpha),

            // FPS(): passi di simulazione al secondo (frequenza nominale del passo fisso).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic | SymbolFlags.IsFunction,
                T(TokenKind.ClockFps, "FPS"), [], new DoubleType(), TokenKind.ClockFps),

            // SCALE(): moltiplicatore del tempo reale in ingresso (1 = normale, 0.5 = lento).
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic | SymbolFlags.IsFunction,
                T(TokenKind.ClockScale, "SCALE"), [], new DoubleType(), TokenKind.ClockScale)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
