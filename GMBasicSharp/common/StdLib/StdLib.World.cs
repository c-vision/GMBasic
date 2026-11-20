// FILE: Interpreter.StdLib.World.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: World — namespace 'WORLD' (motore 3D)
 * 5 funzioni | 1 comando | 0 tipi | 0 costanti
 *
 * FUNZIONI (5): MOVING, LIGHTNOW, TIME, FLOORZ, CEILZ
 * COMANDO (1): WORLD (LOAD | CLOSE | TICK | SECMOVE | HEIGHT | ANIM | SCROLL | LIGHTFX | LAMP)
 * ==== GMB-STDLIB-AUDIT-END ==== */

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- WORLD NAMESPACE (motore 3D) ---
    // ========================================================================

    /// <summary>
    /// Registra il dominio <c>WORLD</c>: un comando legacy (tutti i verbi, dispatcher nel parser)
    /// e le cinque LETTURE qualificate. Non c'è logica qui: ogni lettura è un builtin che i
    /// backend inoltrano alla facciata <c>gmb_world_*</c> del runtime C (CommonLib.World3DNative
    /// per le gambe gestite, chiamata diretta per VM/C e C99Native) — una sola sorgente di stato.
    /// </summary>
    private void CreateWorldNamespace()
    {
        const string namespaceName = "WORLD";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // --- comando: WORLD LOAD|CLOSE|TICK|SECMOVE|HEIGHT|ANIM|SCROLL|LIGHTFX|LAMP ---
        // I verbi sono sottomandi del dispatcher: il comando è UNO solo (nessuna parentesi, §1).
        RegisterCommand(TokenKind.World, "WORLD", namespaceName, Parser.DispatchWorldStatement);

        // --- letture (funzioni: leggono e basta, §1) ---
        var procedures = new ProcedureDefinition[]
        {
            // MOVING(settore): −1 mentre il settore si muove, 0 quando è fermo (T-SECMOVE).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Moving, "MOVING"),
                [new Parameter(-1, T(TokenKind.Identifier, "sector"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.Moving),

            // LIGHTNOW(settore): livello di luce corrente dopo LIGHTFX/LAMP.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LightNow, "LIGHTNOW"),
                [new Parameter(-1, T(TokenKind.Identifier, "sector"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.LightNow),

            // TIME(): orologio del mondo in secondi (passo fisso 1/60 con accumulatore).
            // TokenKind dedicato: `Time` è già la funzione omonima TIME(), e un solo token non può
            // significare due cose (§12 — il dispatch è per TokenKind).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.WorldTime, "TIME"),
                [],
                new DoubleType(), TokenKind.WorldTime),

            // FLOORZ(settore) / CEILZ(settore): quote attuali, in SOLA LETTURA — non fermano il
            // mover (a differenza di WORLD HEIGHT, che è un comando).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FloorZ, "FLOORZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "sector"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.FloorZ),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CeilZ, "CEILZ"),
                [new Parameter(-1, T(TokenKind.Identifier, "sector"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.CeilZ)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
