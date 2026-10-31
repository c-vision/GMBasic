// FILE: Interpreter.Parser.Stmt.Console.cs

using System.Collections.Immutable;

// ReSharper disable InconsistentNaming
// ReSharper disable UnusedParameter.Local
// ReSharper disable UnusedParameter.Global
namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO AI COMANDI DEL NAMESPACE 'CONSOLE'
------------------------------------------------------------------------------------------------------------------------
Questo file contiene i metodi di parsing per tutti i comandi che interagiscono
direttamente con la console di testo. Ogni comando può essere invocato in due modi:

1. Sintassi Legacy: La forma tradizionale, non qualificata.
   (es. `CLS`, `COLOR 15, 1`)
   Per utilizzare questa sintassi, il namespace CONSOLE deve essere importato
   con `IMPORT CONSOLE`.

2. Sintassi Qualificata Fusa: La forma moderna, che utilizza il nome del namespace
   e gli stessi argomenti della forma legacy, SENZA parentesi. Questa sintassi è
   sempre disponibile.
   (es. `CONSOLE.CLS`, `CONSOLE.COLOR 15, 1`)

------------------------------------------------------------------------------------------------------------------------
COMANDI DISPONIBILI:
------------------------------------------------------------------------------------------------------------------------

CLS / CCLS
Cancella lo schermo della console.

- Legacy:    CLS [colore]
- Qualificata fusa: CONSOLE.CLS [colore]

COLOR
Imposta i colori di primo piano e di sfondo per il testo successivo.

- Legacy:    COLOR [foreground] [, background]
- Qualificata fusa: CONSOLE.COLOR [foreground] [, background]

CPRINT
Stampa un'espressione sulla console, simile a PRINT ma specifico per la console.

- Legacy:    CPRINT espressione1; espressione2
- Qualificata fusa: CONSOLE.PRINT espressione1; espressione2

CLOCATE
Posiziona il cursore a una specifica riga e colonna della console.

- Legacy:    CLOCATE riga, colonna
- Qualificata fusa: CONSOLE.LOCATE riga, colonna

CURSOR
Controlla la visibilità e la forma del cursore.

- Legacy:    CURSOR ON|OFF
- Legacy:    CURSOR SHAPE stile$
- Qualificata fusa: CONSOLE.CURSOR ON|OFF | CONSOLE.CURSOR SHAPE stile$

GET
Legge un singolo carattere dalla tastiera senza attendere Invio.

- Legacy:    GET variabile$
- Qualificata fusa: CONSOLE.GET variabile$

INKEY
Controlla la tastiera e restituisce il tasto premuto, senza bloccare l'esecuzione.
Come funzione ritorna un valore: `INKEY([timeout])`.

- Legacy:    INKEY variabile$ [, timeout]
- Funzione:  CONSOLE.INKEY([timeout])  (con parentesi, restituisce il tasto premuto)

WAITKEY
Mette in pausa l'esecuzione e attende la pressione di un tasto qualsiasi.

- Legacy:    WAITKEY
- Qualificata fusa: CONSOLE.WAITKEY

INPUT
Legge l'input dell'utente dalla console e lo assegna a una o più variabili.

- Legacy:    INPUT ["prompt";] variabile1, variabile2, ...
- Qualificata fusa: CONSOLE.INPUT ["prompt";] variabile1, variabile2, ...

LINE INPUT
Legge un'intera riga di testo, inclusi spazi e virgole, in una singola variabile.

- Legacy:    LINE INPUT ["prompt";] variabile$
- Qualificata fusa: CONSOLE.LINEINPUT ["prompt";] variabile$

SET (attributi)
Attiva o disattiva un attributo di testo (grassetto, inverso, etc.).

- Legacy:    SET BOLD ON
- Qualificata: CONSOLE.SET BOLD ON  (La sintassi rimane la stessa)
*/
/// <summary>
///     Contiene i metodi di parsing per le istruzioni del namespace CONSOLE.
///     Ogni metodo è stato refattorizzato per gestire sia la sintassi legacy
///     che la nuova sintassi qualificata fusa (senza parentesi).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione SET per il namespace CONSOLE.
    /// Gestisce sia la sintassi legacy (`SET BOLD ON`) che quella qualificata (`CONSOLE.SET BOLD ON`).
    /// La sua unica responsabilità è identificare il sottocomando di attributo e delegare a
    /// <see cref="ParseSetAttributeStatement"/>.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto dal dispatcher ("CONSOLE").</param>
    /// <returns>Un nodo AST <see cref="SetAttributeStmt"/>.</returns>
    public static Stmt ParseConsoleSetStatement(Parser parser, string? ns)
    {
        // Sintassi legacy: il token corrente è "SET" (non consumato dal dispatcher a più
        // livelli). Lo consumiamo come token di riferimento del nodo AST.
        Token commandToken;
        if (parser.Peek().Kind == TokenKind.Set)
            commandToken = parser.Advance();
        else
            commandToken = parser.Previous();

        // Il prossimo token deve essere l'attributo (BOLD, etc.)
        var subCommandToken = parser.Peek();

        return subCommandToken.Kind switch
        {
            // --- Sottocomandi CONSOLE ---
            TokenKind.Bold or TokenKind.Inverse or TokenKind.Underline or TokenKind.Blink or
            TokenKind.Bell or TokenKind.Confirm or TokenKind.Cursor or
            TokenKind.Century =>
                ParseSetAttributeStatement(parser, "CONSOLE"),

            // --- SET SHELL ON|OFF (opt-in per SYSTEM.SHELL/EXEC) ---
            TokenKind.Shell => ParseSetAttributeStatement(parser, "SYSTEM"),

            // --- Valori stringa (non ON/OFF) ---
            TokenKind.Theme => ParseSetThemeStatement(parser, commandToken),
            TokenKind.Date or TokenKind.DateFormat => ParseSetDateFormatStatement(parser, commandToken),


            // --- SET legacy Clipper (2026-08-06): WRAP/DELIMITERS/ESCAPE/TYPEAHEAD/INTENSITY/
            // EPOCH/DECIMALS/FIXED/PRINTER/DEVICE/MESSAGE — stato Clipper (vedi ClipperSet). ---
            TokenKind.Wrap or TokenKind.Escape or TokenKind.Intensity or TokenKind.Fixed or
            TokenKind.Printer or TokenKind.Delimiters or TokenKind.Typeahead or TokenKind.Epoch or
            TokenKind.Decimals or TokenKind.Device or TokenKind.Message =>
                ParseSetClipperStatement(parser, commandToken),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando '{subCommandToken.Lexeme}' non riconosciuto per SET nel contesto CONSOLE. " +
                $"Attributi validi: BOLD, INVERSE, UNDERLINE, BLINK.")
        };
    }

    /// <summary>
    /// Parsa un'istruzione CCLS (Console Clear Screen) come comando autonomo.
    /// Gestisce entrambe le sintassi.
    /// </summary>
    /// <returns>Un nodo AST di tipo CCLSStmt.</returns>
    public static Stmt ParseCCLSStatement(Parser parser, string? ns)
    {
        var cclsToken = parser.Consume(TokenKind.CCLS, "Internal parser error: Expected 'CCLS' token.");
        Expr? colorExpr = null;

        // Sintassi unificata (legacy e qualificata fusa): CCLS [colore]
        if (!parser.IsAtEndOfStatement())
        {
            colorExpr = parser.ParseExpression();
        }

        return new CCLSStmt(colorExpr, cclsToken);
    }

    /// <summary>
    /// Parsa un'istruzione CSAVESCREEN (SAVE SCREEN forzato sulla console testuale),
    /// gestendo entrambe le sintassi.
    /// </summary>
    /// <returns>Un nodo AST di tipo CSaveScreenStmt.</returns>
    public static Stmt ParseCSaveScreenStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.CSaveScreen, "Internal parser error: Expected 'CSAVESCREEN' token.");

        // Sintassi unificata (legacy e qualificata fusa): CSAVESCREEN name
        var nameExpr = parser.ParseExpression();

        return new CSaveScreenStmt(nameExpr, token);
    }

    /// <summary>
    /// Parsa un'istruzione CRESTORESCREEN (RESTORE SCREEN forzato sulla console testuale),
    /// gestendo entrambe le sintassi.
    /// </summary>
    /// <returns>Un nodo AST di tipo CRestoreScreenStmt.</returns>
    public static Stmt ParseCRestoreScreenStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.CRestoreScreen, "Internal parser error: Expected 'CRESTORESCREEN' token.");

        // Sintassi unificata (legacy e qualificata fusa): CRESTORESCREEN name
        var nameExpr = parser.ParseExpression();

        return new CRestoreScreenStmt(nameExpr, token);
    }

    /// <summary>
    /// Parsa un'istruzione CPRINT, gestendo entrambe le sintassi.
    /// </summary>
    /// <returns>Un nodo AST di tipo CPrintStmt.</returns>
    public static CPrintStmt ParseCPrintStatement(Parser parser, string? ns)
    {
        var cprintToken = parser.Consume(TokenKind.CPrint, "Internal parser error: Expected 'CPRINT' token.");
        var arguments = parser.ParsePrintArguments();
        return new CPrintStmt(arguments, cprintToken);
    }

    /// <summary>
    /// Parsa un'istruzione CLOCATE r,c o CONSOLE.CLOCATE r,c (sintassi qualificata fusa, senza parentesi).
    /// </summary>
    public static Stmt ParseCLocateStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.CLocate, "Internal parser error: Expected 'CLOCATE' token.");

        // Sintassi unificata (legacy e qualificata fusa): CLOCATE line, column
        var line = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after line number.");
        var col = parser.ParseExpression();

        return new CLocateStmt(line, col, token);
    }

    /// <summary>
    /// Parsa un'istruzione CURSOR ON/OFF/SHAPE, gestendo la doppia sintassi.
    /// La forma qualificata fusa (CONSOLE.CURSOR ...) riusa le stesse clausole della
    /// forma legacy: `CURSOR SHAPE espressione` oppure `CURSOR ON|OFF`.
    /// </summary>
    public static Stmt ParseCursorStatement(Parser parser, string? ns)
    {
        var cursorToken = parser.Consume(TokenKind.Cursor, "Internal parser error: Expected 'CURSOR' token.");

        // Sintassi unificata (legacy e qualificata fusa)
        if (parser.Match(TokenKind.Shape))
        {
            var shapeExpression = parser.ParseExpression();
            return new CursorShapeStmt(shapeExpression, cursorToken);
        }

        var stateToken = parser.Advance();
        return stateToken.Kind is not (TokenKind.On or TokenKind.Off)
            ? throw CreateError(stateToken, "Expected ON, OFF, or SHAPE after CURSOR.")
            : new CursorStateStmt(stateToken, cursorToken);
    }

    /// <summary>
    /// Parsa la variante Console del comando GET.
    /// Questa versione corretta utilizza ParseTarget per supportare l'assegnazione
    /// a qualsiasi L-value valido (variabili, elementi di array, campi).
    /// </summary>
    private static GetKeyboardStmt ParseConsoleGetStatement(Parser parser, string? ns)
    {
        var getToken = parser.Consume(TokenKind.Get, "Internal parser error: Expected 'GET' token.");

        // Sintassi unificata (legacy e qualificata fusa): GET destination
        var destinationVariable = parser.ParseTarget();

        return new GetKeyboardStmt(destinationVariable, getToken);
    }

    /// <summary>
    /// Parsa un'istruzione INKEY, gestendo entrambe le sintassi.
    /// Questa versione corretta utilizza ParseTarget per supportare l'assegnazione
    /// a qualsiasi L-value valido.
    /// </summary>
    public static Stmt ParseInkeyStatement(Parser parser, string? ns)
    {
        var inkeyToken = parser.Consume(TokenKind.Inkey, "Internal parser error: Expected 'INKEY' token.");
        Target variable;
        Expr? timeoutExpr = null;
        variable = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            timeoutExpr = parser.ParseExpression();
        }

        return new InkeyStmt(variable, timeoutExpr, inkeyToken);
    }

    /// <summary>
    /// Parsa un'istruzione WAITKEY, gestendo entrambe le sintassi.
    /// </summary>
    public static Stmt ParseWaitKeyStatement(Parser parser, string? ns)
    {
        var waitKeyToken = parser.Consume(TokenKind.WaitKey, "Internal parser error: Expected 'WAITKEY' token.");
        return new WaitKeyStmt(waitKeyToken);
    }

    /// <summary>
    /// [FASE 1 - CORRETTO] Parsa un'istruzione LINE INPUT, gestendo la doppia sintassi e il nuovo timeout.
    /// La logica ora distingue correttamente tra la keyword a due parole 'LINE INPUT' (legacy)
    /// e l'identificatore 'LINEINPUT' (qualificata).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("CONSOLE").</param>
    /// <param name="qual">Il token del qualificatore ("CONSOLE"), se la chiamata è moderna.</param>
    /// <returns>Un nodo AST <see cref="LineInputStmt"/>.</returns>
    /// <exception cref="BasicException">Lanciata se la sintassi non è valida.</exception>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `LINE INPUT [prompt;] var$ [, timeout]`
    /// 
    /// <code>
    /// ' Input semplice
    /// LINE INPUT Risposta$
    /// 
    /// ' Input con prompt e timeout di 10 secondi
    /// LINE INPUT "Inserisci una frase: "; Frase$, 10000
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `CONSOLE.LINEINPUT [prompt;] var$ [, timeout]`
    /// 
    /// <code>
    /// ' Input con prompt, variabile e timeout
    /// CONSOLE.LINEINPUT "Nome completo: "; NomeCompleto$, 15000
    /// 
    /// ' Input solo con variabile e timeout
    /// CONSOLE.LINEINPUT InputUtente$, 5000
    /// </code>
    /// 
    /// </example>
    public static LineInputStmt ParseLineInputStatement(Parser parser, string? ns)
    {
        Token lineInputToken;
        Expr? prompt = null;
        Target variable;
        Expr? timeoutExpr = null;

        // Consumo del verbo: sintassi legacy a due parole 'LINE INPUT'.
        lineInputToken = parser.Consume(TokenKind.LineInput, "Internal parser error: Expected 'LINE INPUT' token.");

        // Sintassi unificata (legacy e qualificata fusa): LINE INPUT [prompt;] var$ [, timeout]
        if (parser.Check(TokenKind.StringLiteral))
        {
            prompt = parser.ParseExpression();
            if (!parser.Match(TokenKind.Semicolon, TokenKind.Comma))
                throw CreateError(parser.Peek(), "Expected ',' or ';' after prompt in LINE INPUT.");
        }

        variable = parser.ParseTarget();

        // Controllo per il timeout opzionale
        if (parser.Match(TokenKind.Comma))
        {
            timeoutExpr = parser.ParseExpression();
        }

        return new LineInputStmt(prompt, variable, timeoutExpr, null, lineInputToken);
    }

    /// <summary>
    /// Parsa un'istruzione INPUT, gestendo sia la sintassi legacy (con prompt e variabili multiple)
    /// sia la sintassi moderna qualificata, che introduce un parametro di timeout opzionale.
    /// La logica è stata resa robusta per distinguere l'argomento di timeout numerico opzionale
    /// da una lista di variabili.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("CONSOLE").</param>
    /// <param name="qual">Il token del qualificatore ("CONSOLE"), se la chiamata è moderna.</param>
    /// <returns>Un nodo AST <see cref="InputStmt"/>.</returns>
    /// <exception cref="BasicException">Lanciata se la sintassi non è valida.</exception>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `INPUT [prompt;] var1 [, var2, ...] [, timeout]`
    /// 
    /// <code>
    /// ' Input con prompt e più variabili
    /// INPUT "Nome e Cognome"; Nome$, Cognome$
    /// 
    /// ' Input con timeout di 5 secondi
    /// INPUT "Rispondi entro 5 secondi:"; Risposta, 5000
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `CONSOLE.INPUT [prompt;] var1 [, var2, ...] [, timeout]`
    /// 
    /// <code>
    /// ' Input con prompt e timeout
    /// CONSOLE.INPUT "Hai 5 secondi per rispondere:"; Risposta, 5000
    /// 
    /// ' Input per più variabili senza prompt, con timeout
    /// CONSOLE.INPUT Nome, Cognome, 10000
    /// </code>
    /// 
    /// </example>
    private static InputStmt ParseInputStatement(Parser parser, string? ns)
    {
        var inputToken = parser.Consume(TokenKind.Input, "Internal parser error: Expected 'INPUT' token.");
        Expr? prompt = null;
        var variables = ImmutableArray.CreateBuilder<Target>();
        Expr? timeoutExpr = null;

        // Sintassi unificata (legacy e qualificata fusa): INPUT [prompt;] var1, var2, ..., [timeout]
        if (parser.Check(TokenKind.StringLiteral))
        {
            prompt = parser.ParseExpression();
            if (!parser.Match(TokenKind.Semicolon, TokenKind.Comma))
                throw CreateError(parser.Peek(), "Expected ',' or ';' after prompt in INPUT.");
        }

        // Parsa la lista di variabili, fermandosi prima di un eventuale timeout.
        while (!parser.IsAtEndOfStatement())
        {
            // Lookahead: se il prossimo token è una virgola, seguita da un numero,
            // e quel numero è l'ultima cosa sulla riga, allora è un timeout.
            if (parser.Check(TokenKind.Comma) && parser.At(1).Kind == TokenKind.Number &&
                (parser.At(2).Kind is TokenKind.Newline or TokenKind.Colon or TokenKind.Eof))
            {
                parser.Advance(); // Consuma la virgola
                timeoutExpr = parser.ParseExpression();
                break; // Il timeout è sempre l'ultimo argomento.
            }

            variables.Add(parser.ParseTarget());

            // Se non c'è una virgola, la lista di argomenti è finita.
            if (!parser.Match(TokenKind.Comma)) break;

            // Dopo la virgola: se il prossimo è un numero a fine riga, è il timeout.
            if (parser.Check(TokenKind.Number) &&
                (parser.At(1).Kind is TokenKind.Newline or TokenKind.Colon or TokenKind.Eof))
            {
                timeoutExpr = parser.ParseExpression();
                break; // Il timeout è sempre l'ultimo argomento.
            }
        }

        if (variables.Count == 0)
            throw CreateError(inputToken, "INPUT statement requires at least one variable to store the value.");

        return new InputStmt(variables.ToImmutable(), prompt, timeoutExpr, null, inputToken);
    }

    /// <summary>
    /// Parsa un'istruzione SET attributo ON/OFF.
    /// Questo metodo viene chiamato dal dispatcher di `SET`.
    /// </summary>
    private static SetAttributeStmt ParseSetAttributeStatement(Parser parser, string? ns)
    {
        var attributeToken = parser.Advance(); // Consuma BOLD, INVERSE, etc.
        var setToken = attributeToken;
        var stateToken = parser.Advance();
        return stateToken.Kind is not (TokenKind.On or TokenKind.Off)
            ? throw CreateError(stateToken, "Expected ON or OFF after the attribute name.")
            : new SetAttributeStmt(attributeToken, stateToken, setToken);
    }

    /// <summary>
    /// Parsa un'istruzione CAPTION (CONSOLE.CAPTION "Nuovo Titolo" o legacy).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("CONSOLE").</param>
    /// <param name="qual">Il token del qualificatore ("CONSOLE").</param>
    /// <returns>Un nodo AST <see cref="ConsoleCaptionStmt"/>.</returns>
    public static Stmt ParseConsoleCaptionStatement(Parser parser, string? ns)
    {
        var captionToken = parser.Consume(TokenKind.Caption, "Internal parser error: Expected 'CAPTION' token.");

        // Sintassi unificata (legacy e qualificata fusa): CAPTION espressione
        var titleExpression = parser.ParseExpression();

        return new ConsoleCaptionStmt(titleExpression, captionToken);
    }

    /// <summary>
    /// Parsa un'istruzione `CONSOLE.SENDKEY` o `SENDKEY` per inviare testo al buffer di input della console.
    /// Questo comando è fondamentale per il testing automatico di programmi interattivi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("CONSOLE").</param>
    /// <param name="qual">Il token del qualificatore ("CONSOLE"), se la chiamata è moderna.</param>
    /// <returns>Un nodo AST <see cref="ConsoleSendKeyStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `SENDKEY espressione_stringa
    /// 
    /// <code>
    /// ' Invia una stringa letterale
    /// SENDKEY "MioNome" + CHR(13)  ' Invia il testo e poi un Invio
    /// 
    /// ' Invia il contenuto di una variabile
    /// DIM Risposta AS STRING = "S" + CHR(13)
    /// SENDKEY Risposta
    /// </code>
    /// 
    ///     --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `CONSOLE.SENDKEY espressione_stringa`
    /// 
    /// <code>
    /// ' Invia una stringa letterale
    /// CONSOLE.SENDKEY "12345" + CHR(13)
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseConsoleSendKeyStatement(Parser parser, string? ns)
    {
        var sendKeyToken = parser.Consume(TokenKind.SendKey, "Internal parser error: Expected 'SENDKEY' token.");

        // Sintassi unificata (legacy e qualificata fusa): SENDKEY espressione
        var keysExpression = parser.ParseExpression();

        return new ConsoleSendKeyStmt(keysExpression, sendKeyToken);
    }

    /// <summary>
    /// Parsa l'istruzione `CONSOLE.SCROLLMODE modo$` per commutare StandardConsole tra
    /// la modalità a buffer fisso (default) e quella "nativa" a scorrimento lineare che
    /// sfrutta lo scrollback reale del terminale.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("CONSOLE").</param>
    /// <param name="qual">Il token del qualificatore ("CONSOLE").</param>
    /// <returns>Un nodo AST <see cref="ConsoleScrollModeStmt"/>.</returns>
    /// <example>
    /// <code>
    /// CONSOLE.SCROLLMODE "NATIVE"   ' scorrimento lineare, scrollback reale del terminale
    /// CONSOLE.SCROLLMODE "BUFFERED" ' torna al buffer a griglia con supporto VIEW PRINT
    /// </code>
    /// </example>
    public static Stmt ParseConsoleScrollModeStatement(Parser parser, string? ns)
    {
        var scrollModeToken = parser.Consume(TokenKind.ScrollMode, "Internal parser error: Expected 'SCROLLMODE' token.");

        // Sintassi unificata (legacy e qualificata fusa): SCROLLMODE modo
        var modeExpression = parser.ParseExpression();

        return new ConsoleScrollModeStmt(modeExpression, scrollModeToken);
    }

    /// <summary>
    /// Parsa CPALETTE per operare sulla palette della console di testo.
    ///   CPALETTE idx, r, g, b  — modifica colore
    ///   CPALETTE USE "name"       — attiva palette per nome
    ///   CPALETTE DEFINE "name", arr
    ///   CPALETTE REPLACE "name", arr
    ///   CPALETTE SAVE "file"
    ///   CPALETTE LOAD "file"
    ///   CPALETTE.USE "name"
    ///   CPALETTE idx, r, g, b     (sintassi qualificata fusa, senza parentesi)
    /// </summary>
    public static Stmt ParseCPaletteStatement(Parser parser, string? ns)
    {
        var paletteToken = parser.Consume(TokenKind.CPalette, "Internal parser error: Expected 'CPALETTE' token.");

        // Sintassi unificata (legacy e qualificata fusa)
        // CPALETTE.USE(...) / CPALETTE.<sub> (sub-comando con dot)
        if (parser.Match(TokenKind.Dot))
            return ParseCPaletteQualified(parser, paletteToken);

        // CPALETTE USE / DEFINE / REPLACE / SAVE / LOAD (sub-comando con keyword)
        if (parser.Match(TokenKind.Use))
            return ParseCPaletteUseSub(parser, paletteToken);
        if (parser.Match(TokenKind.Define))
            return ParseCPaletteDefineSub(parser, paletteToken);
        if (parser.Match(TokenKind.Replace))
            return ParseCPaletteReplaceSub(parser, paletteToken);
        if (parser.Match(TokenKind.Save))
            return ParseCPaletteSaveSub(parser, paletteToken);
        if (parser.Match(TokenKind.Load))
            return ParseCPaletteLoadSub(parser, paletteToken);

        // CPALETTE idx, r, g, b
        Expr index, r, g, b;
        index = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after color index.");
        r = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after red component.");
        g = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after green component.");
        b = parser.ParseExpression();
        return new PaletteStmt(index, r, g, b, paletteToken);
    }

    private static Stmt ParseCPaletteQualified(Parser parser, Token paletteToken)
    {
        // Accetta keyword (USE, DEFINE, REPLACE, SAVE, LOAD) o Identifier come sub-comando
        Token sub;
        if (parser.Match(TokenKind.Use))
            sub = parser.Previous();
        else if (parser.Match(TokenKind.Define))
            sub = parser.Previous();
        else if (parser.Match(TokenKind.Replace))
            sub = parser.Previous();
        else if (parser.Match(TokenKind.Save))
            sub = parser.Previous();
        else if (parser.Match(TokenKind.Load))
            sub = parser.Previous();
        else
            sub = parser.Consume("Expected sub-command after CPALETTE.", TokenKind.Identifier);
        switch (sub.Kind)
        {
            case TokenKind.Use:
            {
                var name = parser.ParseExpression();
                return new PaletteUseStmt(name, paletteToken);
            }
            case TokenKind.Define:
            {
                var name = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
                var colors = parser.ParseExpression();
                return new PaletteDefineStmt(name, colors, paletteToken);
            }
            case TokenKind.Replace:
            {
                var name = parser.ParseExpression();
                parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
                var colors = parser.ParseExpression();
                return new PaletteReplaceStmt(name, colors, paletteToken);
            }
            case TokenKind.Save:
            {
                var fileName = parser.ParseExpression();
                return new PaletteSaveStmt(fileName, paletteToken);
            }
            case TokenKind.Load:
            {
                var fileName = parser.ParseExpression();
                return new PaletteLoadStmt(fileName, paletteToken);
            }
            default:
                throw new Exception($"Unknown CPALETTE sub-command '{sub.Lexeme}'.");
        }
    }

    private static Stmt ParseCPaletteUseSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        return new PaletteUseStmt(name, paletteToken);
    }

    private static Stmt ParseCPaletteDefineSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
        var colors = parser.ParseExpression();
        return new PaletteDefineStmt(name, colors, paletteToken);
    }

    private static Stmt ParseCPaletteReplaceSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
        var colors = parser.ParseExpression();
        return new PaletteReplaceStmt(name, colors, paletteToken);
    }

    private static Stmt ParseCPaletteSaveSub(Parser parser, Token paletteToken)
    {
        var fileName = parser.ParseExpression();
        return new PaletteSaveStmt(fileName, paletteToken);
    }

    private static Stmt ParseCPaletteLoadSub(Parser parser, Token paletteToken)
    {
        var fileName = parser.ParseExpression();
        return new PaletteLoadStmt(fileName, paletteToken);
    }

    /// <summary>
    /// Parsa @ row, col SAY expr [PICTURE pic] oppure @ row, col GET var [options...].
    /// </summary>
    public static Stmt ParseAtStatement(Parser parser)
    {
        parser.Advance(); // consuma '@'
        var rowToken = parser.Consume(TokenKind.Number, "Expected row number after @.");
        parser.Consume(TokenKind.Comma, "Expected ',' after row in @ statement.");
        var colToken = parser.Consume(TokenKind.Number, "Expected column number after row.");
        var keyword = parser.Consume("Expected SAY or GET after coordinates.", TokenKind.Say, TokenKind.Get);

        if (keyword.Kind == TokenKind.Get)
            return ParseAtGetTail(parser, rowToken, colToken);
        if (keyword.Kind == TokenKind.Say)
            return ParseAtSayTail(parser, rowToken, colToken);

        throw CreateError(keyword, $"Expected SAY or GET, found '{keyword.Lexeme}'.");
    }

    private static Stmt ParseAtSayTail(Parser parser, Token rowToken, Token colToken)
    {
        var row = new NumberExpr(Convert.ToDouble(rowToken.Lexeme), rowToken);
        var col = new NumberExpr(Convert.ToDouble(colToken.Lexeme), colToken);
        var expr = parser.ParseExpression();
        Expr? picture = null;
        var lookahead = parser.Peek();
        if (lookahead.Kind == TokenKind.Picture)
        {
            parser.Advance();
            picture = parser.ParseExpression();
        }
        return new AtSayStmt(row, col, expr, picture, rowToken);
    }

    private static Stmt ParseAtGetTail(Parser parser, Token rowToken, Token colToken)
    {
        var row = new NumberExpr(Convert.ToDouble(rowToken.Lexeme), rowToken);
        var col = new NumberExpr(Convert.ToDouble(colToken.Lexeme), colToken);

        var variable = parser.ParseTarget();

        // Optional clauses
        Expr? picture = null, valid = null, rangeLo = null, rangeHi = null, color = null;
        string? controlType = null;
        Expr? items = null, controlWidth = null, controlHeight = null;
        Expr? caption = null, stepValue = null, minValue = null, maxValue = null;
        Expr? whenClause = null, messageClause = null, errorClause = null;

        while (true)
        {
            var tok = parser.Peek();
            var kw = GetOptionalKeyword(tok);
            if (kw == null) break;

            switch (kw)
            {
                case "PICTURE":
                    parser.Advance();
                    picture = parser.ParseExpression();
                    break;
                case "VALID":
                    parser.Advance();
                    valid = parser.ParseExpression();
                    break;
                case "RANGE":
                    parser.Advance();
                    rangeLo = parser.ParseExpression();
                    if (parser.Peek().Kind == TokenKind.Comma)
                    {
                        parser.Advance();
                        rangeHi = parser.ParseExpression();
                    }
                    break;
                case "COLOR":
                    parser.Advance();
                    color = parser.ParseExpression();
                    break;
                case "TYPE":
                    parser.Advance();
                    var typeTok = parser.Consume("Expected control type after TYPE.", TokenKind.Identifier, TokenKind.StringLiteral);
                    // Lexeme di una StringLiteral include le virgolette ("CheckBox"): il
                    // valore letterale (senza virgolette) è in Literal. Senza il Trim, il
                    // controllo TYPE non era mai riconosciuto (Equals("CheckBox") falliva) e
                    // i tasti freccia/SPACE del controllo non funzionavano.
                    controlType = typeTok.Literal is string ts ? ts : typeTok.Lexeme.Trim('"');
                    break;
                case "ITEMS":
                    parser.Advance();
                    items = parser.ParseExpression();
                    break;
                case "SIZE":
                    parser.Advance();
                    controlWidth = parser.ParseExpression();
                    if (parser.Peek().Kind == TokenKind.Comma)
                    {
                        parser.Advance();
                        controlHeight = parser.ParseExpression();
                    }
                    break;
                case "CAPTION":
                    parser.Advance();
                    caption = parser.ParseExpression();
                    break;
                case "STEP":
                    parser.Advance();
                    stepValue = parser.ParseExpression();
                    break;
                case "MIN":
                    parser.Advance();
                    minValue = parser.ParseExpression();
                    break;
                case "MAX":
                    parser.Advance();
                    maxValue = parser.ParseExpression();
                    break;
                case "WHEN":
                    parser.Advance();
                    whenClause = parser.ParseExpression();
                    break;
                case "MESSAGE":
                    parser.Advance();
                    messageClause = parser.ParseExpression();
                    break;
                case "ERROR":
                    parser.Advance();
                    errorClause = parser.ParseExpression();
                    break;
                default:
                    return new AtGetStmt(row, col, variable, picture, valid, rangeLo, rangeHi,
                        color, controlType, items, controlWidth, controlHeight,
                        caption, stepValue, minValue, maxValue, row.Position, whenClause, messageClause, errorClause);
            }
        }

        return new AtGetStmt(row, col, variable, picture, valid, rangeLo, rangeHi,
            color, controlType, items, controlWidth, controlHeight,
            caption, stepValue, minValue, maxValue, row.Position, whenClause, messageClause, errorClause);
    }

    /// <summary>
    /// Parsa un'istruzione READ (loop interattivo per i GET accumulati).
    /// </summary>
    public static Stmt ParseClipperReadStatement(Parser parser)
    {
        parser.Advance(); // consuma 'READ'
        return new ClipperReadStmt(parser.Previous());
    }

    /// <summary>
    /// Restituisce il nome canonico della clausola opzionale GET se il token corrente
    /// e' un identificatore o una keyword nota (TYPE, CAPTION, RANGE, ...).
    /// Altrimenti null.
    /// </summary>
    private static string? GetOptionalKeyword(Token tok)
    {
        if (tok.Kind == TokenKind.Identifier)
            return tok.Lexeme.ToUpperInvariant();
        // Mappa keyword note a nomi clausola
        return tok.Kind switch
        {
            TokenKind.Type => "TYPE",
            TokenKind.Caption => "CAPTION",
            TokenKind.Step => "STEP",
            TokenKind.Min => "MIN",
            TokenKind.Max => "MAX",
            TokenKind.Color => "COLOR",
            TokenKind.Picture => "PICTURE",
            TokenKind.Range => "RANGE",
            TokenKind.Size => "SIZE",
            TokenKind.Items => "ITEMS",
            TokenKind.When => "WHEN",
            TokenKind.Message => "MESSAGE",
            TokenKind.Error => "ERROR",
            _ => null
        };
    }

    /// <summary>
    /// Parsa BOX top, left, bottom, right [SINGLE|DOUBLE]
    /// </summary>
    public static Stmt ParseBoxStatement(Parser parser)
    {
        var boxToken = parser.Consume(TokenKind.Box, "Expected BOX.");
        var top = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after top in BOX.");
        var left = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after left in BOX.");
        var bottom = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after bottom in BOX.");
        var right = parser.ParseExpression();
        Token? border = null;
        var peek = parser.Peek();
        if (peek.Kind is TokenKind.Single or TokenKind.Double)
            border = parser.Advance();
        return new BoxStmt(top, left, bottom, right, border, boxToken);
    }

    /// <summary>
    /// Parsa SCROLL top, left, bottom, right, n
    /// </summary>
    public static Stmt ParseScrollStatement(Parser parser)
    {
        var scrollToken = parser.Consume(TokenKind.Scroll, "Expected SCROLL.");
        var top = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after top in SCROLL.");
        var left = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after left in SCROLL.");
        var bottom = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after bottom in SCROLL.");
        var right = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after right in SCROLL.");
        var count = parser.ParseExpression();
        return new ScrollStmt(top, left, bottom, right, count, scrollToken);
    }

    /// <summary>Parsa ALERT "messaggio".</summary>
    /// <summary>
    /// Parsa SET THEME "name".
    /// </summary>
    public static Stmt ParseSetThemeStatement(Parser parser, Token commandToken)
    {
        parser.Consume(TokenKind.Theme, "Expected 'THEME'.");
        parser.Consume(TokenKind.To, "Expected 'TO'.");
        var themeName = parser.ParseExpression();
        return new SetThemeStmt(themeName, commandToken);
    }


    public static Stmt ParseSetDateFormatStatement(Parser parser, Token commandToken)
    {
        if (parser.Check(TokenKind.Date) || parser.Check(TokenKind.DateFormat)) parser.Advance();
        else parser.Consume(TokenKind.Date, "Expected 'DATE FORMAT'.");
        parser.Match(TokenKind.Format);
        parser.Consume(TokenKind.To, "Expected 'TO'.");
        var format = parser.ParseExpression();
        return new SetDateFormatStmt(format, commandToken);
    }


    /// <summary>
    /// Parsa un SET legacy Clipper: SET WRAP ON/OFF, SET DELIMITERS TO ",;",
    /// SET TYPEAHEAD n, SET MESSAGE TO riga, ecc. Il valore può essere ON/OFF
    /// (booleano) oppure un'espressione (dopo l'opzionale TO).
    /// </summary>
    public static Stmt ParseSetClipperStatement(Parser parser, Token commandToken)
    {
        var attrToken = parser.Advance(); // consuma WRAP/ESCAPE/...
        var name = attrToken.Lexeme.ToUpperInvariant();

        // ON/OFF
        if (parser.Match(TokenKind.On) || parser.Match(TokenKind.Off))
            return new SetClipperStmt(name, new BooleanExpr(parser.Previous().Kind == TokenKind.On, parser.Previous()), commandToken);

        // TO <expr> (opzionale: "SET TYPEAHEAD 16" == "SET TYPEAHEAD TO 16")
        if (parser.Match(TokenKind.To))
        {
            // SET DEVICE TO PRINTER|SCREEN: il valore è una KEYWORD (Printer/Screen) che il
            // parser di espressioni tratterebbe come VariableExpr inesistente (NIL) — emetti
            // la STRINGA del nome del dispositivo.
            if (name == "DEVICE" && parser.Peek().Kind is TokenKind.Printer or TokenKind.Screen)
            {
                var devTok = parser.Advance();
                return new SetClipperStmt(name, new StringExpr(devTok.Lexeme, devTok), commandToken);
            }
            return new SetClipperStmt(name, parser.ParseExpression(), commandToken);
        }

        // valore diretto senza TO
        return new SetClipperStmt(name, parser.ParseExpression(), commandToken);
    }
}
