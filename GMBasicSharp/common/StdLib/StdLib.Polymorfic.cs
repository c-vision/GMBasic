// FILE: Interpreter.StdLib.Polymorphic.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Polymorfic — namespace '—'
 * 0 funzioni | 16 comandi | 0 tipi | 0 costanti
 *
 * COMANDI (16):
 *   CLS
 *   COLOR
 *   DOEVENTS
 *   GET
 *   INPUT
 *   LINE INPUT
 *   LOCATE
 *   PRINT
 *   CLS
 *   COLOR
 *   DOEVENTS
 *   LOCATE
 *   PRINT
 *   GET
 *   INPUT
 *   LINE INPUT
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- POLYMORPHIC COMMANDS ---
    // ========================================================================

    /// <summary>
    /// Centralizza la registrazione dei comandi polimorfici che sono condivisi tra più namespace.
    /// Questa versione è stata aggiornata per riflettere la nuova architettura del parser,
    /// rimuovendo i dispatcher obsoleti e mantenendo quelli ancora necessari.
    /// </summary>
    private void CreatePolymorphicCommands()
    {
        // Comandi condivisi tra CONSOLE e GRAPHICS (ambiguità semantica, non sintattica)
        RegisterCommand(TokenKind.Cls, "CLS", "CONSOLE", Parser.DispatchClsStatement);
        RegisterCommand(TokenKind.Cls, "CLS", "GRAPHICS", Parser.DispatchClsStatement);
        RegisterCommand(TokenKind.Color, "COLOR", "CONSOLE", Parser.DispatchColorStatement);
        RegisterCommand(TokenKind.Color, "COLOR", "GRAPHICS", Parser.DispatchColorStatement);
        RegisterCommand(TokenKind.Locate, "LOCATE", "CONSOLE", Parser.DispatchLocateStatement);
        RegisterCommand(TokenKind.Locate, "LOCATE", "GRAPHICS", Parser.DispatchLocateStatement);
        // DOEVENTS — equivalente VB6: un frame eventi non bloccante (WINDOW PUMP 0).
        // Polimorfico come PRINT: disponibile senza IMPORT sia su CONSOLE sia su GRAPHICS.
        RegisterCommand(TokenKind.DoEvents, "DOEVENTS", "CONSOLE", Parser.ParseDoEventsStatement);
        RegisterCommand(TokenKind.DoEvents, "DOEVENTS", "GRAPHICS", Parser.ParseDoEventsStatement);

        RegisterCommand(TokenKind.Print, "PRINT", "CONSOLE", Parser.DispatchPrintStatement);
        RegisterCommand(TokenKind.Print, "PRINT", "GRAPHICS", Parser.DispatchPrintStatement);

        // Comandi condivisi tra IO e CONSOLE - ambiguità risolta da lookahead
        RegisterCommand(TokenKind.Input, "INPUT", "IO", Parser.DispatchInputStatement);
        RegisterCommand(TokenKind.Input, "INPUT", "CONSOLE", Parser.DispatchInputStatement);
        RegisterCommand(TokenKind.LineInput, "LINE INPUT", "IO", Parser.DispatchLineInputStatement);
        RegisterCommand(TokenKind.LineInput, "LINE INPUT", "CONSOLE", Parser.DispatchLineInputStatement);
        // Forme qualificate fuse (direttiva §2.2): CONSOLE.INPUT/IO.INPUT senza parentesi
        RegisterCommand(TokenKind.Get, "GET", "CONSOLE", Parser.DispatchGetStatement);
        RegisterCommand(TokenKind.Get, "GET", "IO", Parser.DispatchGetStatement);
    }
}