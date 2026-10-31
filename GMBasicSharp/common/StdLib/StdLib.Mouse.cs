// FILE: Interpreter.StdLib.Mouse.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Mouse — namespace 'MOUSE'
 * 12 funzioni | 9 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (12):
 *   BUTTON
 *   CLICK
 *   DBLCLICK
 *   LEFT
 *   MIDDLE
 *   MOVE
 *   PRESENT
 *   RIGHT
 *   WHEEL
 *   X
 *   Y
 * COMANDI (9):
 *   CURSOR
 *   HIDE
 *   LIMIT
 *   OFF
 *   ON
 *   SET
 *   SHOW
 *   UNLIMIT
 *   WAIT
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- MOUSE NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'MOUSE'.
    /// Gestisce comandi di configurazione e funzioni di input.
    /// </summary>
    private void CreateMouseNamespace()
    {
        const string namespaceName = "MOUSE";
        const SymbolFlags publicFuncFlag =
            SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // --- Comandi (Statements) ---

        // Registrazione Legacy: MOUSE ON, MOUSE WAIT, ecc.
        RegisterCommand(TokenKind.Mouse, "MOUSE", namespaceName, Parser.DispatchMouseStatement);

        // Registrazione Qualificata: MOUSE.ON(), MOUSE.WAIT(), ecc.

        // --- Funzioni (Expressions) ---

        var procedures = new ProcedureDefinition[]
        {
            // X() - Coordinata X assoluta o colonna
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseX, "X"),
                [], new IntegerType(), TokenKind.MouseX),

            // Y() - Coordinata Y assoluta o riga
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseY, "Y"),
                [], new IntegerType(), TokenKind.MouseY),

            // BUTTON([id]) - Stato pulsanti (Bitmask o specifico)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseButton, "BUTTON"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "buttonId"), new IntegerType(),
                        SymbolFlags.IsOptional, null,false)
                ],
                new IntegerType(), TokenKind.MouseButton),

            // WHEEL() - Delta rotella
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseWheel, "WHEEL"),
                [], new IntegerType(), TokenKind.MouseWheel),

            // CLICK() - Rilevamento click singolo (booleano)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseClick, "CLICK"),
                [], new BitType(), TokenKind.MouseClick),

            // DBLCLICK() - Rilevamento doppio click
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseDblClick, "DBLCLICK"),
                [], new BitType(), TokenKind.MouseDblClick),

            // MOVE() - Rilevamento movimento
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseMove, "MOVE"),
                [], new BitType(), TokenKind.MouseMove),
            
            // PRESENT() - Verifica presenza hardware
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MousePresent, "PRESENT"), 
                [], new BitType(), TokenKind.MousePresent),

            // LEFT() - Stato tasto sinistro
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseLeft, "LEFT"), 
                [], new BitType(), TokenKind.MouseLeft),

            // RIGHT() - Stato tasto destro
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseRight, "RIGHT"), 
                [], new BitType(), TokenKind.MouseRight),

            // MIDDLE() - Stato tasto centrale
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseMiddle, "MIDDLE"), 
                [], new BitType(), TokenKind.MouseMiddle),

            // CLICK(x1, y1, x2, y2) - Click in area specifica (Overload)
            new NativeProcedureDefinition(-1, publicFuncFlag, T(TokenKind.MouseClick, "CLICK"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x1"), new IntegerType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y1"), new IntegerType(), noFlags, null,false),
                    new Parameter(-1, T(TokenKind.Identifier, "x2"), new IntegerType(), noFlags, null,false),
                    new Parameter(-1, T(TokenKind.Identifier, "y2"), new IntegerType(), noFlags, null, false)
                ], 
                new BitType(), TokenKind.MouseClick),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}