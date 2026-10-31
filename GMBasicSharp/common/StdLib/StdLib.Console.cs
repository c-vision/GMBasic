// FILE: Interpreter.StdLib.Console.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Console — namespace 'CONSOLE'
 * 14 funzioni | 10 comandi | 1 tipi | 0 costanti
 *
 * FUNZIONI (14):
 *   ALERT
 *   BGRGB
 *   CELL
 *   CELLSTR
 *   CONFIRM
 *   FGRGB
 *   GET
 *   GETCURSORPOS
 *   GETPALETTECOLOR
 *   GETTEXTMETRICS
 *   INKEY
 *   MAKE
 *   MESSAGE
 *   Point
 * COMANDI (10):
 *   CAPTION
 *   CLIP
 *   CURSOR
 *   INKEY
 *   RESTORESCREEN
 *   SAVESCREEN
 *   SCROLLMODE
 *   SENDKEY
 *   SET
 *   WAITKEY
 * TIPI (1):
 *   KEY
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- CONSOLE NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'CONSOLE'.
    /// </summary>
    private void CreateConsoleNamespace()
    {
        const string namespaceName = "CONSOLE";
        const SymbolFlags
            publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        // Comandi Specifici di CONSOLE - Puntano ai parser statici.
        // NOTA (2026-08-06): le varianti FORZATE console C* (CCLS/CLOCATE/CPRINT/CSAVESCREEN/
        // CRESTORESCREEN/CPALETTE) sono registrate SOLO in GRAPHICS — servono a forzare il
        // comportamento console quando lo screen grafico è attivo (stessa regola: i comandi
        // polimorfici/ibridi stanno in CONSOLE, le varianti forzate in GRAPHICS).

        RegisterCommand(TokenKind.Cursor, "CURSOR", namespaceName, Parser.ParseCursorStatement);

        RegisterCommand(TokenKind.Inkey, "INKEY", namespaceName, Parser.ParseInkeyStatement);

        RegisterCommand(TokenKind.WaitKey, "WAITKEY", namespaceName, Parser.ParseWaitKeyStatement);

        RegisterCommand(TokenKind.Caption, "CAPTION", namespaceName, Parser.ParseConsoleCaptionStatement);
        
        RegisterCommand(TokenKind.SendKey, "SENDKEY", namespaceName, Parser.ParseConsoleSendKeyStatement);

        RegisterCommand(TokenKind.ScrollMode, "SCROLLMODE", namespaceName, Parser.ParseConsoleScrollModeStatement);

        // CPALETTE spostato in GRAPHICS (2026-08-06) — variante forzata console di PALETTE,
        // come le altre varianti C* (CCLS/CLOCATE/CPRINT/CSAVESCREEN/CRESTORESCREEN).

        // SAVESCREEN/RESTORESCREEN — polimorfici (console buffer, o texture se schermo grafico attivo).
        // (Erano in GRAPHICS; spostati qui per coerenza. Le varianti forzate grafica C* restano in Graphics.)
        RegisterCommand(TokenKind.SaveScreen, "SAVESCREEN", namespaceName, Parser.ParseSaveScreenStatement);
        RegisterCommand(TokenKind.RestoreScreen, "RESTORESCREEN", namespaceName, Parser.ParseRestoreScreenStatement);

        // Widget TUI standalone (CHECKBOX/LISTBOX/.../TBROWSE) — namespace proprio 'TUI'
        // (non default: la forma nuda richiede `IMPORT TUI`). Vedi CreateTuiNamespace
        // in StdLib.Tui.cs (decisione utente 2026-09-17, opzione B).

        // Il comando SET è polimorfico e viene gestito dal suo parser specifico
        RegisterCommand(TokenKind.Set, "SET", namespaceName, Parser.ParseConsoleSetStatement);
        
        // --- DEFINIZIONE DELLE FUNZIONI NATIVE ---
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Point, "Point"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "x"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new IntegerType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.Point),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Cell, "CELL"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "row"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "column"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "mode"), new IntegerType(), noFlags, null, false)
            ], new LongType(), TokenKind.Cell),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CellStr, "CELLSTR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "row"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "column"), new IntegerType(), noFlags, null, false)
            ], new StringType(), TokenKind.CellStr),

            // (SCREENS/SCREENINFO/TEXTWIDTH/TEXTHEIGHT/SCREEN restano nel namespace GRAPHICS
            // in GM-Basic — già polimorfici via GraphicsManager.IsScreenOn; vedi StdLib.Graphics.)
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Inkey, "INKEY"),
                [new Parameter(-1, T(TokenKind.Identifier, "timeout"), new LongType(), optionalFlag, null, false)],
                new StringType(), TokenKind.Inkey),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FgRgb, "FGRGB"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "text"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "r"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "g"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new IntegerType(), noFlags, null, false)
            ], new StringType(), TokenKind.FgRgb),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BgRgb, "BGRGB"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "text"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "r"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "g"), new IntegerType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new IntegerType(), noFlags, null, false)
            ], new StringType(), TokenKind.BgRgb),

            // Comandi CONSOLE ibridi TUI/GUI (2026-08-06): SETWINDOWTITLE / GETCURSORPOS /
            // GETTEXTMETRICS / GETPALETTECOLOR. In TUI agiscono sulla console (IConsole); con lo
            // screen grafico attivo delegano al GPM. Same logica dei polimorfici CLS/COLOR.

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GetCursorPos, "GETCURSORPOS"),
            [], new ArrayType(new DoubleType()), TokenKind.GetCursorPos),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GetTextMetrics, "GETTEXTMETRICS"),
            [], new ArrayType(new DoubleType()), TokenKind.GetTextMetrics),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GetPaletteColor, "GETPALETTECOLOR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "index"), new IntegerType(), noFlags, null, false)
            ], new ArrayType(new DoubleType()), TokenKind.GetPaletteColor),
            // MESSAGE() — getter Clipper (testo corrente del messaggio/prompt).
            // TokenKind.Message riusato (comando TEAM.MESSAGE e' statement-only).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Message, "MESSAGE"),
            [new Parameter(-1, T(TokenKind.Identifier, "text"), new StringType(), optionalFlag, null, false)],
            new StringType(), TokenKind.Message),

            // ALERT/CONFIRM come FUNZIONI nel namespace CONSOLE (NON globali):
            // scelta = CONSOLE.ALERT('msg'[, width[, align[, fg[, bg[, border]]]]])
            // scelta = CONSOLE.CONFIRM('msg?'[, width[, align[, fg[, bg[, border[, buttons]]]]]])
            // Tutti i parametri dopo msg sono opzionali → supportano i named arguments
            // (es. CONSOLE.CONFIRM("Salvare?", buttons := %YESNO, border := %DOUBLE)).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Alert, "ALERT"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "msg"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "width"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "align"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fg"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bg"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "border"), new IntegerType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.Alert),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Confirm, "CONFIRM"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "msg"), new StringType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "width"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "align"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "fg"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bg"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "border"), new IntegerType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "buttons"), new IntegerType(), optionalFlag, null, false)
            ], new LongType(), TokenKind.Confirm),

        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }

    /// <summary>
    /// Crea il modulo virtuale per il namespace 'CLIP'.
    /// VERSIONE CORRETTA E DEFINITIVA: Separa nettamente la funzione dai comandi.
    /// 1.  Definisce `GET` come `NativeProcedureDefinition`, classificandolo come una
    ///     funzione che restituisce un valore e può essere usata in espressioni.
    /// 2.  Registra la keyword `CLIP` come un comando, il cui parser gestirà
    ///     esclusivamente i sotto-comandi `SET` e `CLEAR`.
    /// Questo design impone la sintassi qualificata e distingue correttamente tra
    /// l'uso in espressioni (`var = CLIP.GET()`) e come istruzioni (`CLIP.SET ...`).
    /// </summary>
    /// <returns>Una tupla contenente il nome del namespace e il modulo virtuale creato.</returns>
    private void CreateClipNamespace()
    {
        const string namespaceName = "CLIP";

        // --- PARTE 1: REGISTRAZIONE DEI COMANDI (Statements) ---
        // Registra la keyword 'CLIP' per attivare un parser che gestisce
        // SOLO le istruzioni che non restituiscono un valore (SET e CLEAR).
        RegisterCommand(TokenKind.Clip, "CLIP", namespaceName, Parser.ParseClipStatement);

        // NOTA 2026-09-17: i widget TUI standalone (CHECKBOX/LISTBOX/.../TBROWSE) erano
        // registrati qui sotto CLIP — spostati nel namespace proprio 'TUI' (non default:
        // la forma nuda richiede `IMPORT TUI`; vedi CreateTuiNamespace in StdLib.Tui.cs).
        // CLIP resta solo clipboard.

        // --- PARTE 2: DEFINIZIONE DELLE FUNZIONI (Expressions) ---
        // Definisce GET come una procedura nativa. Questo permette al parser di
        // espressioni di riconoscerla e usarla in assegnazioni (es. a = CLIP.GET()).
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Get, "GET"), [],
                new StringType(), TokenKind.Get)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'ONKEY'.
    /// Include la classe statica 'KEY' che funge da enumerazione per i codici tasto.
    /// </summary>
    private void CreateOnKeyNamespace()
    {
        const string namespaceName = "ONKEY";

        // Costruisce la classe statica 'KEY' che conterrà le costanti e il metodo MAKE.
        var keyClassMembers = ImmutableArray.CreateBuilder<ClassMember>();

        // --- Definizione Costanti Tasti (Valori reali di ConsoleKey) ---
        AddKeyConstant("BACKSPACE", 8);
        AddKeyConstant("TAB", 9);
        AddKeyConstant("CLEAR", 12);
        AddKeyConstant("ENTER", 13);
        AddKeyConstant("PAUSE", 19);
        AddKeyConstant("CAPSLOCK", 20);
        AddKeyConstant("ESC", 27);
        AddKeyConstant("SPACE", 32);
        AddKeyConstant("PAGEUP", 33);
        AddKeyConstant("PAGEDOWN", 34);
        AddKeyConstant("END", 35);
        AddKeyConstant("HOME", 36);
        AddKeyConstant("LEFT", 37);
        AddKeyConstant("UP", 38);
        AddKeyConstant("RIGHT", 39);
        AddKeyConstant("DOWN", 40);
        AddKeyConstant("SELECT", 41);
        AddKeyConstant("PRINT", 42);
        AddKeyConstant("EXECUTE", 43);
        AddKeyConstant("PRINTSCREEN", 44);
        AddKeyConstant("INSERT", 45);
        AddKeyConstant("DELETE", 46);
        AddKeyConstant("HELP", 47);

        // Numeri
        AddKeyConstant("D0", 48);
        AddKeyConstant("D1", 49);
        AddKeyConstant("D2", 50);
        AddKeyConstant("D3", 51);
        AddKeyConstant("D4", 52);
        AddKeyConstant("D5", 53);
        AddKeyConstant("D6", 54);
        AddKeyConstant("D7", 55);
        AddKeyConstant("D8", 56);
        AddKeyConstant("D9", 57);

        // Lettere
        AddKeyConstant("A", 65);
        AddKeyConstant("B", 66);
        AddKeyConstant("C", 67);
        AddKeyConstant("D", 68);
        AddKeyConstant("E", 69);
        AddKeyConstant("F", 70);
        AddKeyConstant("G", 71);
        AddKeyConstant("H", 72);
        AddKeyConstant("I", 73);
        AddKeyConstant("J", 74);
        AddKeyConstant("K", 75);
        AddKeyConstant("L", 76);
        AddKeyConstant("M", 77);
        AddKeyConstant("N", 78);
        AddKeyConstant("O", 79);
        AddKeyConstant("P", 80);
        AddKeyConstant("Q", 81);
        AddKeyConstant("R", 82);
        AddKeyConstant("S", 83);
        AddKeyConstant("T", 84);
        AddKeyConstant("U", 85);
        AddKeyConstant("V", 86);
        AddKeyConstant("W", 87);
        AddKeyConstant("X", 88);
        AddKeyConstant("Y", 89);
        AddKeyConstant("Z", 90);

        AddKeyConstant("LWIN", 91);
        AddKeyConstant("RWIN", 92);
        AddKeyConstant("APPS", 93);
        AddKeyConstant("SLEEP", 95);

        // Tastierino Numerico
        AddKeyConstant("NUMPAD0", 96);
        AddKeyConstant("NUMPAD1", 97);
        AddKeyConstant("NUMPAD2", 98);
        AddKeyConstant("NUMPAD3", 99);
        AddKeyConstant("NUMPAD4", 100);
        AddKeyConstant("NUMPAD5", 101);
        AddKeyConstant("NUMPAD6", 102);
        AddKeyConstant("NUMPAD7", 103);
        AddKeyConstant("NUMPAD8", 104);
        AddKeyConstant("NUMPAD9", 105);
        AddKeyConstant("MULTIPLY", 106);
        AddKeyConstant("ADD", 107);
        AddKeyConstant("SEPARATOR", 108);
        AddKeyConstant("SUBTRACT", 109);
        AddKeyConstant("DECIMAL", 110);
        AddKeyConstant("DIVIDE", 111);

        // Tasti Funzione
        AddKeyConstant("F1", 112);
        AddKeyConstant("F2", 113);
        AddKeyConstant("F3", 114);
        AddKeyConstant("F4", 115);
        AddKeyConstant("F5", 116);
        AddKeyConstant("F6", 117);
        AddKeyConstant("F7", 118);
        AddKeyConstant("F8", 119);
        AddKeyConstant("F9", 120);
        AddKeyConstant("F10", 121);
        AddKeyConstant("F11", 122);
        AddKeyConstant("F12", 123);
        AddKeyConstant("F13", 124);
        AddKeyConstant("F14", 125);
        AddKeyConstant("F15", 126);
        AddKeyConstant("F16", 127);
        AddKeyConstant("F17", 128);
        AddKeyConstant("F18", 129);
        AddKeyConstant("F19", 130);
        AddKeyConstant("F20", 131);
        AddKeyConstant("F21", 132);
        AddKeyConstant("F22", 133);
        AddKeyConstant("F23", 134);
        AddKeyConstant("F24", 135);

        AddKeyConstant("NUMLOCK", 144);
        AddKeyConstant("SCROLL", 145);

        // --- Modificatori (Bitmask standard di ConsoleModifiers) ---
        AddKeyConstant("MOD_ALT", 1);
        AddKeyConstant("MOD_SHIFT", 2);
        AddKeyConstant("MOD_CTRL", 4);

        // --- Speciali ---
        AddKeyConstant("CTRL_C", -1); // ETX
        AddKeyConstant("CTRL_BREAK", -1); // Mappato come Ctrl+C per semplicità, o usare Pause

        // --- Metodo Statico MAKE ---
        var makeMethod = new NativeProcedureDefinition(
            -1,
            SymbolFlags.IsPublic | SymbolFlags.IsStatic | SymbolFlags.IsMethod |
            SymbolFlags.IsFunction,
            T(TokenKind.Make, "MAKE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "key"), new LongType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "modifiers"), new LongType(), SymbolFlags.None,
                    null, false)
            ],
            new LongType(), // Restituisce un intero che l'Executor tratterà come KeyCombination
            TokenKind.Make
        );
        keyClassMembers.Add(new MethodMember(makeMethod, AccessModifier.Public));

        // Costruzione della ClassDefinition "KEY"
        var keyClassDef = new ClassDefinition(
            -1,
            T(TokenKind.Identifier, "KEY"),
            SymbolFlags.IsPublic | SymbolFlags.IsClass | SymbolFlags.IsSealed,
            null,
            null,
            ImmutableArray<Token>.Empty, // Aggiunto argomento mancante per le interfacce
            keyClassMembers.ToImmutable()
        );

        // Registrazione dei tipi nel namespace
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "KEY", new ClassType(keyClassDef) }
        };

        // Nessuna procedura "libera" (tutto è nella classe KEY)
        var procedures = ImmutableArray<ProcedureDefinition>.Empty;

        RegisterBuiltinNamespace(namespaceName, procedures, types);
        return;

        // Helper per creare una costante nella classe
        void AddKeyConstant(string name, long value)
        {
            var token = new Token(TokenKind.Identifier, name, null, SourcePosition.Unknown);
            var constDecl = new ConstDeclaration(
                -1,
                token,
                new LongType(),
                new NumberExpr(value,
                    new Token(TokenKind.Number, value.ToString(), (double)value, SourcePosition.Unknown)),
                SymbolFlags.IsPublic | SymbolFlags.IsStatic | SymbolFlags.IsConst
            );

            // Usiamo un FieldMember per esporre la costante come membro statico della classe
            var varDecl = new VariableDeclaration(
                -1,
                token, // NameSuffixed
                token, // NameCleaned
                ImmutableArray<Expr>.Empty,
                new LongType(),
                constDecl.InitialValue,
                false,
                true,
                constDecl.Flags
            );

            keyClassMembers.Add(new FieldMember(varDecl, AccessModifier.Public));
        }
    }
}