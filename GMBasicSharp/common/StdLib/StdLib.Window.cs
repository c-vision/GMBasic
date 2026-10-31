// FILE: Interpreter.StdLib.StdLib.Window.cs

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Window — namespace 'WINDOWS'
 * 7 funzioni | 14 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (7):
 *   DESKTOP
 *   HEIGHT
 *   THEME
 *   WIDTH
 *   WINDOWCLOSED
 *   X
 *   Y
 * COMANDI (14):
 *   CLEAR
 *   DEFINE
 *   DESKTOP
 *   EDIT
 *   FRAME
 *   PRINT
 *   SCROLL
 *   SELECT
 *   SET
 *   SHADOW
 *   SHOW
 *   TEXT
 *   THEME
 *   VIEWPORT
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateWindowNamespace()
    {
        const string namespaceName = "WINDOWS";

        BuiltinNamespaceNames.Add(namespaceName);
        SymbolTable.BuiltinNamespaceNames.Add(namespaceName);

        // VIEWPORT [SCREEN] (x1, y1)-(x2, y2) — finestra di coordinate logiche (grafica).
        // Spostato qui da GRAPHICS (era WINDOW): il sistema di coordinate logiche appartiene
        // alle finestre, non al comando grafico WINDOW.
        RegisterCommand(TokenKind.Viewport, "VIEWPORT", namespaceName, Parser.ParseViewPortStatement);

        // WINDOW DEFINE ... | WINDOW name ACTION | WINDOW SET TITLE "t" | WINDOW [SCREEN] — finestre TUI (legacy).
        // I comandi WINDOWS usano SOLO la sintassi legacy a spazio: WINDOW DEFINE nome FROM ...,
        // WINDOW nome ACTIVATE|DEACTIVATE|RELEASE|CLOSE|MINIMIZE|RESTORE, WINDOW SET TITLE "titolo"
        // (non esiste piu' la forma qualificata WINDOWS.SETTITLE). Il dispatcher ritorna
        // List<Stmt> (un solo statement per forma).
        RegisterCommand(TokenKind.Window, "WINDOW", namespaceName,
            (p, ns) => Parser.ParseTuiWindowStatement(p, ns).First());

        // Funzioni di LETTURA finestre pixel (Fase W-Pixel): coordinate/dimensioni della
        // finestra attiva in pixel logici. Parentesi SEMPRE (sono funzioni).
        const SymbolFlags pub = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.Identifier, "WINDOWCLOSED"), [], new IntegerType(), TokenKind.Identifier),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowX, "X"), [], new IntegerType(), TokenKind.WindowX),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowY, "Y"), [], new IntegerType(), TokenKind.WindowY),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowWidth, "WIDTH"), [], new IntegerType(), TokenKind.WindowWidth),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowHeight, "HEIGHT"), [], new IntegerType(), TokenKind.WindowHeight),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowTheme, "THEME"), [], new StringType(), TokenKind.WindowTheme),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.WindowDesktop, "DESKTOP"), [], new IntegerType(), TokenKind.WindowDesktop),
        };
        RegisterBuiltinNamespace(namespaceName, procedures, new Dictionary<string, DataType>());
    }
}
