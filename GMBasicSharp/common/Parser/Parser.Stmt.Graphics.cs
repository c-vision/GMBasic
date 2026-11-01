// FILE: Interpreter.Parser.Stmt.Graphics.cs

using System.Collections.Immutable;

// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault

// ReSharper disable UnusedParameter.Local
namespace Basic;
/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO AI COMANDI DEL NAMESPACE 'GRAPHICS'
------------------------------------------------------------------------------------------------------------------------
Questo file contiene i metodi di parsing per i comandi che operano sulla
schermata grafica. Ogni comando può essere invocato in due modi:

1. Sintassi Legacy: La forma tradizionale, non qualificata.
   (es. `SCREEN 12`, `LINE (0,0)-(100,100)`)
   Per utilizzare questa sintassi, il namespace GRAPHICS deve essere importato
   con `IMPORT GRAPHICS`.

2. Sintassi Qualificata Fusa: La forma moderna, che utilizza il nome del namespace
   e gli STESSI argomenti della forma legacy, SENZA parentesi (direttiva AGENTS.md §2.2).
   Questa sintassi è sempre disponibile.
   (es. `GRAPHICS.SCREEN 12`, `GRAPHICS.LINE (0,0)-(100,100)`)

------------------------------------------------------------------------------------------------------------------------
COMANDI DISPONIBILI:
------------------------------------------------------------------------------------------------------------------------

SCREEN
Inizializza, modifica o chiude la modalità grafica.

- Legacy:    SCREEN mode [, [switch]] [, [active]] [, [visible]] [, [repl]]
- Qualificata fusa: GRAPHICS.SCREEN mode [, [switch]] [, [active]] [, [visible]] [, [repl]]

PSET / PRESET
Disegna un pixel con il colore di primo piano (PSET) o di sfondo (PRESET).

- Legacy:    PSET (x, y) [, color]
- Qualificata fusa: GRAPHICS.PSET (x, y) [, color]
- Legacy:    PRESET (x, y)
- Qualificata fusa: GRAPHICS.PRESET (x, y)

LINE
Disegna una linea o un rettangolo.

- Legacy:    LINE [(x1, y1)]-(x2, y2) [, color] [, style]
- Qualificata fusa: GRAPHICS.LINE [(x1, y1)]-(x2, y2) [, color] [, style]

CIRCLE
Disegna un cerchio, un'ellisse o un arco.

- Legacy:    CIRCLE (x, y), radius [, color] [, start] [, end] [, aspect]
- Qualificata fusa: GRAPHICS.CIRCLE (x, y), radius [, color] [, start] [, end] [, aspect]

PAINT
Riempie un'area chiusa con un colore.

- Legacy:    PAINT (x, y) [, fill] [, border]
- Qualificata fusa: GRAPHICS.PAINT (x, y) [, fill] [, border]

DRAW
Disegna una forma complessa usando un linguaggio di comandi stringa.

- Legacy:    DRAW command$
- Qualificata fusa: GRAPHICS.DRAW command$

FONT
Imposta il font di default per le operazioni di testo.

- Legacy:    FONT path$, size
- Qualificata fusa: GRAPHICS.FONT path$, size

PCOPY
Copia il contenuto di una pagina grafica in un'altra.

- Legacy:    PCOPY source, destination
- Qualificata fusa: GRAPHICS.PCOPY source, destination

IMAGEGET / IMAGEPUT / IMAGEPUTS
Gestisce il salvataggio e il disegno di aree grafiche in array.

- Legacy:    IMAGEGET (x1,y1)-(x2,y2), arrayVar
- Qualificata fusa: GRAPHICS.IMAGEGET (x1,y1)-(x2,y2), arrayVar
- Legacy:    IMAGEPUT (x,y), arrayVar [, mode]
- Qualificata fusa: GRAPHICS.IMAGEPUT (x,y), arrayVar [, mode]

SPRITE / MATRIX
Comandi per la gestione di sprite 2D e matrici di trasformazione. La sintassi
è multi-parola (es. `SPRITE LOAD "a.png", id%`). La versione qualificata fusa
riusa le stesse clausole: `GRAPHICS.SPRITE LOAD "a.png", id%`.
*/

/// <summary>
/// Contiene le versioni dei metodi di parsing per le istruzioni grafiche,
/// specifiche per la Fase 1 (Parsing Puro).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Parsa un'istruzione SCREEN.
    /// </summary>
    public static Stmt ParseScreenStatement(Parser parser, string? ns)
    {
        var screenToken = parser.Consume(TokenKind.Screen, "Internal parser error: Expected 'SCREEN' token.");

        Expr? mode;
        Expr? activePage = null;
        Expr? visiblePage = null;
        Expr? closeRepl = null;

        // Sintassi unificata (legacy e qualificata fusa): SCREEN mode [, switch] [, active] [, visible] [, repl]
        mode = parser.ParseExpression();
        if (mode is NumberExpr _smode) parser.GuiMode = (_smode.Value != 0);

        if (!parser.Match(TokenKind.Comma))
            return new ScreenStmt(mode, activePage, visiblePage, closeRepl, screenToken);
        activePage = parser.ParseOptionalExpression(); // Riutilizzabile
        if (!parser.Match(TokenKind.Comma))
            return new ScreenStmt(mode, activePage, visiblePage, closeRepl, screenToken);
        visiblePage = parser.ParseOptionalExpression(); // Riutilizzabile
        if (parser.Match(TokenKind.Comma))
        {
            closeRepl = parser.ParseOptionalExpression(); // Riutilizzabile
        }

        return new ScreenStmt(mode, activePage, visiblePage, closeRepl, screenToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione TEXTMODE (selezione modalita' console).
    /// </summary>
    public static Stmt ParseTextModeStatement(Parser parser, string? ns)
    {
        var tok = parser.Consume(TokenKind.TextMode, "Internal parser error: Expected 'TEXTMODE' token.");
        var mode = parser.ParseExpression();
        return new TextModeStmt(mode, tok);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PSET.
    /// </summary>
    public static Stmt ParsePSetStatement(Parser parser, string? ns)
    {
        var psetToken = parser.Consume(TokenKind.PSet, "Internal parser error: Expected 'PSET' token.");
        Expr x, y;
        Expr? color = null;

        // Sintassi unificata (legacy e qualificata fusa): PSET (x, y) [, color]
        parser.Consume(TokenKind.LeftParen, "Expected '(' after PSET.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after coordinates.");
        if (parser.Match(TokenKind.Comma)) color = parser.ParseOptionalExpression(); // Riutilizzabile

        return new PSetStmt(x, y, color, psetToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PRESET.
    /// </summary>
    public static Stmt ParsePresetStatement(Parser parser, string? ns)
    {
        var presetToken = parser.Consume(TokenKind.Preset, "Internal parser error: Expected 'PRESET' token.");
        Expr x, y;

        // Sintassi unificata (legacy e qualificata fusa): PRESET (x, y)
        parser.Consume(TokenKind.LeftParen, "Expected '(' after PRESET.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after coordinates.");

        return new PresetStmt(x, y, presetToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione LINE.
    /// </summary>
    public static Stmt ParseLineStatement(Parser parser, string? ns)
    {
        var lineToken = parser.Consume(TokenKind.Line, "Internal parser error: Expected 'LINE' token.");
        Expr? x1 = null, y1 = null, color = null, style = null;
        Expr x2, y2;

        // Sintassi unificata (legacy e qualificata fusa): LINE [(x1, y1)]-(x2, y2) [, color] [, style]
        if (!parser.Check(TokenKind.Minus))
        {
            parser.Consume(TokenKind.LeftParen, "Expected '(' for first coordinate.");
            x1 = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
            y1 = parser.ParseExpression();
            parser.Consume(TokenKind.RightParen, "Expected ')' after first coordinate.");
        }

        parser.Consume(TokenKind.Minus, "Expected '-' separating coordinate pairs.");
        parser.Consume(TokenKind.LeftParen, "Expected '(' for second coordinate.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after second coordinate.");
        if (!parser.Match(TokenKind.Comma)) return new LineStmt(x1, y1, x2, y2, color, style, lineToken);
        if (!parser.Check(TokenKind.Comma)) color = parser.ParseOptionalExpression(); // Riutilizzabile
        if (parser.Match(TokenKind.Comma)) style = parser.ParseOptionalExpression(); // Riutilizzabile

        return new LineStmt(x1, y1, x2, y2, color, style, lineToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione CIRCLE.
    /// </summary>
    public static Stmt ParseCircleStatement(Parser parser, string? ns)
    {
        var circleToken = parser.Consume(TokenKind.Circle, "Internal parser error: Expected 'CIRCLE' token.");
        Expr x, y, radius;
        Expr? color = null, start = null, end = null, aspect = null;

        // Sintassi unificata (legacy e qualificata fusa): CIRCLE (x, y), radius [, color] [, start] [, end] [, aspect]
        parser.Consume(TokenKind.LeftParen, "Expected '(' after CIRCLE.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after coordinates.");
        parser.Consume(TokenKind.Comma, "Expected ',' after coordinates.");
        radius = parser.ParseExpression();
        if (!parser.Match(TokenKind.Comma))
            return new CircleStmt(x, y, radius, color, start, end, aspect, circleToken);
        if (!parser.Check(TokenKind.Comma)) color = parser.ParseOptionalExpression();
        if (!parser.Match(TokenKind.Comma))
            return new CircleStmt(x, y, radius, color, start, end, aspect, circleToken);
        if (!parser.Check(TokenKind.Comma)) start = parser.ParseOptionalExpression();
        if (!parser.Match(TokenKind.Comma))
            return new CircleStmt(x, y, radius, color, start, end, aspect, circleToken);
        if (!parser.Check(TokenKind.Comma)) end = parser.ParseOptionalExpression();
        if (parser.Match(TokenKind.Comma))
        {
            aspect = parser.ParseOptionalExpression();
        }

        return new CircleStmt(x, y, radius, color, start, end, aspect, circleToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PAINT.
    /// </summary>
    public static Stmt ParsePaintStatement(Parser parser, string? ns)
    {
        var paintToken = parser.Consume(TokenKind.Paint, "Internal parser error: Expected 'PAINT' token.");
        Expr x, y;
        Expr? fill = null, border = null;

        // Sintassi unificata (legacy e qualificata fusa): PAINT (x, y) [, fill] [, border]
        parser.Consume(TokenKind.LeftParen, "Expected '(' after PAINT.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after coordinates.");
        if (!parser.Match(TokenKind.Comma)) return new PaintStmt(x, y, fill, border, paintToken);
        if (!parser.Check(TokenKind.Comma)) fill = parser.ParseOptionalExpression();
        if (parser.Match(TokenKind.Comma)) border = parser.ParseOptionalExpression();

        return new PaintStmt(x, y, fill, border, paintToken);
    }

/// <summary>
    /// [FASE 1] Parsa un'istruzione LOAD TEXTURE, ora con supporto opzionale per `CELL(w, h)`.
    /// VERSIONE CORRETTA: Gestisce `array()` come un riferimento all'intero array (VariableTarget).
    /// </summary>
    public static Stmt ParseLoadStatement(Parser parser, string? ns)
    {
        if (parser.At(1).Kind == TokenKind.Agent)
            return ParseAiAgentLoadStatement(parser, ns ?? "AI");

        var loadToken = parser.Consume(TokenKind.Load, "Internal parser error: Expected 'LOAD' token.");
        Target destination;
        Expr fileName;
        Expr? cellWidth = null;
        Expr? cellHeight = null;

        // Sintassi unificata (legacy e qualificata fusa): LOAD TEXTURE file INTO array [CELL(w, h)]
        parser.Consume(TokenKind.Texture, "Expected 'TEXTURE' after 'LOAD'.");
        fileName = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected 'INTO' keyword.");

        // --- INIZIO BLOCCO DI CORREZIONE ---
        // Parsiamo il nome della variabile.
        var destNameToken = parser.ConsumeProcedureName();
        // Se è seguito da "()", lo consumiamo ma lo ignoriamo per creare un VariableTarget.
        if (parser.Match(TokenKind.LeftParen))
        {
            parser.Consume(TokenKind.RightParen, "Expected ')' after array name.");
        }
        // Creiamo sempre un VariableTarget, che è il tipo corretto per un'assegnazione a un array.
        var (baseName, _) = SplitNameAndSuffix(destNameToken.Lexeme);
        var cleanToken = destNameToken with { Lexeme = baseName };
        destination = new VariableTarget(-1, destNameToken, cleanToken);
        // --- FINE BLOCCO DI CORREZIONE ---

        parser.Match(TokenKind.Comma);

        if (parser.Match(TokenKind.Cell))
        {
            parser.Consume(TokenKind.LeftParen, "Expected '(' after 'CELL'.");
            cellWidth = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' separating cell dimensions.");
            cellHeight = parser.ParseExpression();
            parser.Consume(TokenKind.RightParen, "Expected ')' after cell dimensions.");
        }

        return new LoadTextureStmt(destination, fileName, cellWidth, cellHeight, loadToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione DRAW, agendo come un dispatcher per le sue varianti.
    /// Questo metodo gestisce il polimorfismo del comando DRAW, distinguendo tra la sintassi
    /// standard `DRAW stringa` e la sintassi per il rendering in batch `DRAW TEXTURES ...`.
    /// La forma qualificata fusa (`GRAPHICS.DRAW ...`) riusa le stesse clausole della legacy.
    ///
    /// MEMORIA ARCHITETTURALE (NON DIMENTICARE):
    /// Questo metodo è il punto di ingresso unico per qualsiasi istruzione `DRAW` qualificata o meno.
    /// La sua responsabilità è eseguire un "lookahead" sul token successivo per decidere
    /// quale variante del comando parsare.
    ///
    /// Logica di Dispatching Interna (lookahead, identica per entrambe le sintassi):
    /// Se il token successivo è `TEXTURES`, la responsabilità viene delegata a
    /// `ParseDrawTexturesStatement`; se è `TEXTURE` a `ParseDrawTextureStatement`; se è l'identificatore
    /// `TEXT` a `ParseDrawTextStatement`. In tutti gli altri casi, si assume che sia la versione
    /// standard e si parsa una singola espressione.
    ///
    /// Questo approccio centralizza la logica di disambiguazione all'interno del parser specifico per il
    /// comando, mantenendo la registrazione in `StdLib` semplice.
    /// </summary>
    public static Stmt ParseDrawCommandStatement(Parser parser, string? ns)
    {
        var drawToken = parser.Consume(TokenKind.Draw, "Internal parser error: Expected 'DRAW' token.");

        // Sintassi unificata (legacy e qualificata fusa): il lookahead decide la variante.
        if (parser.Check(TokenKind.Textures))
        {
            return ParseDrawTexturesStatement(parser, drawToken);
        }
        if (parser.Check(TokenKind.Texture))
        {
            return ParseDrawTextureStatement(parser, drawToken);
        }
        if (parser.Match(TokenKind.Text))
        {
            return ParseDrawTextStatement(parser, drawToken);
        }

        var command = parser.ParseExpression();
        return new DrawStmt(command, drawToken);
    }

    /// <summary>
    /// [FASE 1 - NUOVO E COMPLETO] Parsa un'istruzione DRAW TEXTURE.
    /// Gestisce la sintassi unificata (legacy e qualificata fusa).
    /// Legacy: DRAW TEXTURE id, x, y [, rot] [, scaleX] [, scaleY]
    /// Qualificata fusa: GRAPHICS.DRAW TEXTURE id, x, y [, rot] [, scaleX] [, scaleY]
    /// </summary>
    private static Stmt ParseDrawTextStatement(Parser parser, Token drawToken)
    {
        var text = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after text.");
        var x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after x.");
        var y = parser.ParseExpression();

        Expr? rotation = null;
        var align = TextAlign.Left;
        Expr? color = null;

        if (parser.Check(TokenKind.Comma))
        {
            parser.Advance();
            if (!parser.Check(TokenKind.Newline))
            {
                var next = parser.Peek();
                if (next.Kind == TokenKind.AlignLeft || next.Kind == TokenKind.AlignCenter || next.Kind == TokenKind.AlignRight)
                {
                    parser.Advance();
                    align = next.Kind == TokenKind.AlignCenter ? TextAlign.Center
                         : next.Kind == TokenKind.AlignRight ? TextAlign.Right
                         : TextAlign.Left;
                }
                else
                {
                    rotation = parser.ParseExpression();
                }
            }
        }
        if (parser.Check(TokenKind.Comma))
        {
            parser.Advance();
            if (!parser.Check(TokenKind.Newline))
            {
                var next = parser.Peek();
                if (next.Kind == TokenKind.AlignLeft || next.Kind == TokenKind.AlignCenter || next.Kind == TokenKind.AlignRight)
                {
                    parser.Advance();
                    align = next.Kind == TokenKind.AlignCenter ? TextAlign.Center
                         : next.Kind == TokenKind.AlignRight ? TextAlign.Right
                         : TextAlign.Left;
                }
                else
                {
                    color = parser.ParseExpression();
                }
            }
        }

        return new DrawTextStmt(text, x, y, rotation, align, color, drawToken);
    }

    private static Stmt ParseDrawTextureStatement(Parser parser, Token commandToken)
    {
        Expr textureId, x, y;
        Expr? rotation = null, scaleX = null, scaleY = null;

        // Sintassi unificata (legacy e qualificata fusa): DRAW TEXTURE ...
        // 'DRAW' è già stato consumato dal chiamante, il token corrente è 'TEXTURE'.
        parser.Consume(TokenKind.Texture, "Expected 'TEXTURE' after 'DRAW'.");
        textureId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after texture ID.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after X coordinate.");
        y = parser.ParseExpression();

        if (parser.Match(TokenKind.Comma))
        {
            rotation = parser.ParseOptionalExpression();
            if (parser.Match(TokenKind.Comma))
            {
                scaleX = parser.ParseOptionalExpression();
                if (parser.Match(TokenKind.Comma))
                {
                    scaleY = parser.ParseOptionalExpression();
                }
            }
        }

        return new DrawTextureStmt(textureId, x, y, rotation, scaleX, scaleY, commandToken);
    }

/// <summary>
    /// [FASE 1 - CORRETTO] Parsa la sintassi `DRAW TEXTURES`, gestendo entrambe le varianti.
    /// </summary>
    private static Stmt ParseDrawTexturesStatement(Parser parser, Token commandToken)
    {
        parser.Consume(TokenKind.Textures, "Expected 'TEXTURES' after 'DRAW'.");

        // --- Logica di Disambiguazione ---
        // Parsiamo il primo argomento come un Target, dato che in entrambi i casi
        // (ID texture o array di Sprite) si tratta sintatticamente di un L-value.
        var firstArg = parser.ParseTarget();

        // Se dopo il primo target NON c'è una virgola, è la nuova sintassi a singolo array.
        if (!parser.Match(TokenKind.Comma))
        {
            return new DrawTexturesStmt(commandToken, SpritesArray: firstArg, null, null, null, null, null);
        }

        // Se c'è una virgola, è la vecchia sintassi. Il primo argomento deve essere trattato come un'espressione (l'ID).
        var textureId = ConvertTargetToExpr(firstArg);
        var xArray = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after X coordinates array.");
        var yArray = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after Y coordinates array.");
        var rotArray = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after rotations array.");
        var scaleArray = parser.ParseTarget();

        return new DrawTexturesStmt(commandToken, null, textureId, xArray, yArray, rotArray, scaleArray);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione FONT.
    /// </summary>
    public static Stmt ParseFontStatement(Parser parser, string? ns)
    {
        var fontToken = parser.Consume(TokenKind.Font, "Internal parser error: Expected 'FONT' token.");
        Expr path, size;

        // Sintassi unificata (legacy e qualificata fusa): FONT path$, size
        path = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after font path.");
        size = parser.ParseExpression();

        return new FontStmt(path, size, fontToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PCOPY.
    /// </summary>
    public static Stmt ParsePCopyStatement(Parser parser, string? ns)
    {
        var pcopyToken = parser.Consume(TokenKind.PCopy, "Internal parser error: Expected 'PCOPY' token.");
        Expr source, dest;

        // Sintassi unificata (legacy e qualificata fusa): PCOPY source, destination
        source = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after source page.");
        dest = parser.ParseExpression();

        return new PCopyStmt(source, dest, pcopyToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione SCREENSET.
    /// </summary>
    public static Stmt ParseScreenSetStatement(Parser parser, string? ns)
    {
        var screensetToken = parser.Consume(TokenKind.ScreenSet, "Internal parser error: Expected 'SCREENSET' token.");
        Expr? visible = null;
        Expr? active = null;

        // Sintassi unificata (legacy e qualificata fusa): SCREENSET [visible] [, active]
        if (!parser.Check(TokenKind.Comma) && !parser.IsAtEndOfStatement())
            visible = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma)) active = parser.ParseOptionalExpression();
        if (visible is null && active is null && !parser.IsAtEndOfStatement())
            visible = parser.ParseExpression();

        return new ScreenSetStmt(visible, active, screensetToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PALETTE.
    /// </summary>
    public static Stmt ParsePaletteStatement(Parser parser, string? ns)
    {
        var paletteToken = parser.Consume(TokenKind.Palette, "Internal parser error: Expected 'PALETTE' token.");

        // --- PALETTE.USE(...) (sub-comando introdotto da punto, sintassi unificata) ---
        if (parser.Match(TokenKind.Dot))
            return ParsePaletteQualified(parser, paletteToken);

        // --- Sub-comando con keyword (PALETTE USE, PALETTE DEFINE, ecc.) ---
        if (parser.Match(TokenKind.Use))
            return ParsePaletteUseSub(parser, paletteToken);
        if (parser.Match(TokenKind.Define))
            return ParsePaletteDefineSub(parser, paletteToken);
        if (parser.Match(TokenKind.Replace))
            return ParsePaletteReplaceSub(parser, paletteToken);
        if (parser.Match(TokenKind.Save))
            return ParsePaletteSaveSub(parser, paletteToken);
        if (parser.Match(TokenKind.Load))
            return ParsePaletteLoadSub(parser, paletteToken);

        // --- Sintassi unificata (legacy e qualificata fusa): PALETTE index, r, g, b ---
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

    private static Stmt ParsePaletteQualified(Parser parser, Token paletteToken)
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
            sub = parser.Consume("Expected sub-command after PALETTE.", TokenKind.Identifier);
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
                throw new Exception($"Unknown PALETTE sub-command '{sub.Lexeme}'.");
        }
    }

    private static Stmt ParsePaletteUseSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        return new PaletteUseStmt(name, paletteToken);
    }

    private static Stmt ParsePaletteDefineSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
        var colors = parser.ParseExpression();
        return new PaletteDefineStmt(name, colors, paletteToken);
    }

    private static Stmt ParsePaletteReplaceSub(Parser parser, Token paletteToken)
    {
        var name = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after palette name.");
        var colors = parser.ParseExpression();
        return new PaletteReplaceStmt(name, colors, paletteToken);
    }

    private static Stmt ParsePaletteSaveSub(Parser parser, Token paletteToken)
    {
        var fileName = parser.ParseExpression();
        return new PaletteSaveStmt(fileName, paletteToken);
    }

    private static Stmt ParsePaletteLoadSub(Parser parser, Token paletteToken)
    {
        var fileName = parser.ParseExpression();
        return new PaletteLoadStmt(fileName, paletteToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione IMAGEGET.
    /// </summary>
    public static Stmt ParseImageGetStatement(Parser parser, string? ns)
    {
        var imageGetToken = parser.Consume(TokenKind.ImageGet, "Internal parser error: Expected 'IMAGEGET' token.");
        Expr x2, y2;
        Target array;

        // Sintassi unificata (legacy e qualificata fusa): IMAGEGET (x1,y1)-(x2,y2), arrayVar
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y1 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Minus, "Expected '-'.");
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Comma, "Expected ','.");
        array = parser.ParseTarget();

        return new ImageGetStmt(x1, y1, x2, y2, array, imageGetToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione IMAGEPUT.
    /// </summary>
    public static Stmt ParseImagePutStatement(Parser parser, string? ns)
    {
        var imagePutToken = parser.Consume(TokenKind.ImagePut, "Internal parser error: Expected 'IMAGEPUT' token.");
        Target array;
        Expr? mode = null;

        // Sintassi unificata (legacy e qualificata fusa): IMAGEPUT (x,y), arrayVar [, mode]
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Comma, "Expected ','.");
        array = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma)) mode = parser.ParseOptionalExpression();

        return new ImagePutStmt(x, y, array, mode, imagePutToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione IMAGEPUTS.
    /// </summary>
    public static Stmt ParseImagePutsStatement(Parser parser, string? ns)
    {
        var imagePutsToken = parser.Consume(TokenKind.ImagePuts, "Internal parser error: Expected 'IMAGEPUTS' token.");
        Target spriteArray, positionsArray;
        Expr? mode = null;

        // Sintassi unificata (legacy e qualificata fusa): IMAGEPUTS spriteArray, positionsArray [, mode]
        spriteArray = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after sprite array in IMAGEPUTS.");
        positionsArray = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma)) mode = parser.ParseOptionalExpression();

        return new ImagePutsStmt(spriteArray, positionsArray, mode, imagePutsToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione SAVE SCREEN.
    /// </summary>
    public static Stmt ParseSaveScreenStatement(Parser parser, string? ns)
    {
        var saveScreenToken =
            parser.Consume(TokenKind.SaveScreen, "Internal parser error: Expected 'SAVE SCREEN' token.");
        Expr nameExpr;

        // Sintassi unificata (legacy e qualificata fusa): SAVESCREEN name
        nameExpr = parser.ParseExpression();

        return new SaveScreenStmt(nameExpr, saveScreenToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione RESTORE SCREEN.
    /// </summary>
    public static Stmt ParseRestoreScreenStatement(Parser parser, string? ns)
    {
        var restoreScreenToken =
            parser.Consume(TokenKind.RestoreScreen, "Internal parser error: Expected 'RESTORE SCREEN' token.");
        Expr nameExpr;

        // Sintassi unificata (legacy e qualificata fusa): RESTORESCREEN name
        nameExpr = parser.ParseExpression();

        return new RestoreScreenStmt(nameExpr, restoreScreenToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione BSAVE.
    /// </summary>
    public static Stmt ParseBSaveStatement(Parser parser, string? ns)
    {
        var bsaveToken = parser.Consume(TokenKind.BSave, "Internal parser error: Expected 'BSAVE' token.");
        Expr x2, y2;
        Target buffer;

        // Sintassi unificata (legacy e qualificata fusa): BSAVE (x1,y1)-(x2,y2), buffer
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y1 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Minus, "Expected '-'.");
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Comma, "Expected ','.");
        buffer = parser.ParseTarget();

        return new BSaveStmt(x1, y1, x2, y2, buffer, bsaveToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione BLOAD.
    /// </summary>
    public static Stmt ParseBLoadStatement(Parser parser, string? ns)
    {
        var bloadToken = parser.Consume(TokenKind.BLoad, "Internal parser error: Expected 'BLOAD' token.");
        Expr buffer;

        // Sintassi unificata (legacy e qualificata fusa): BLOAD (x,y), buffer
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Comma, "Expected ','.");
        buffer = parser.ParseExpression();

        return new BLoadStmt(x, y, buffer, bloadToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione CLS.
    /// </summary>
    private static ClsStmt ParseClsStatement(Parser parser, string? ns)
    {
        var clsToken = parser.Consume(TokenKind.Cls, "Internal parser error: Expected 'CLS' token.");
        Expr? color = null;

        // Sintassi unificata (legacy e qualificata fusa): CLS [color]
        if (!parser.IsAtEndOfStatement())
        {
            color = parser.ParseExpression();
        }

        return new ClsStmt(color, clsToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione COLOR.
    /// </summary>
    private static ColorStmt ParseColorStatement(Parser parser, string? ns)
    {
        var colorToken = parser.Consume(TokenKind.Color, "Internal parser error: Expected 'COLOR' token.");
        Expr? foreground = null;
        Expr? background = null;

        // Sintassi unificata (legacy e qualificata fusa): COLOR [foreground] [, background]
        if (parser.IsAtEndOfStatement()) return new ColorStmt(foreground, background, colorToken);
        if (parser.Match(TokenKind.Comma))
        {
            background = parser.ParseExpression();
        }
        else
        {
            foreground = parser.ParseExpression();
            if (parser.Match(TokenKind.Comma) && !parser.IsAtEndOfStatement())
            {
                background = parser.ParseExpression();
            }
        }

        return new ColorStmt(foreground, background, colorToken);
    }

    /// <summary>
    /// [FASE 1 - CORRETTO v3] Parsa gli argomenti per un'istruzione PRINT o CPRINT.
    /// La logica del loop è stata resa robusta per fermarsi quando incontra una keyword che inizia una nuova
    /// istruzione o un delimitatore di blocco.
    /// </summary>
    /// <returns>Un array immutabile di <see cref="PrintArg"/>.</returns>
    private ImmutableArray<PrintArg> ParsePrintArguments()
    {
        var argsBuilder = ImmutableArray.CreateBuilder<PrintArg>();
        // Le parentesi appartengono all'ESPRESSIONE, non alla lista PRINT:
        // `PRINT (1); 2` deve produrre un'espressione seguita dal separatore, non
        // consumare '(' come wrapper e lasciare ';' come nuova istruzione.
        // `PRINT (a, b)` non e' una forma canonica (il parser C la rifiuta): la
        // lista canonica usa `;`/`,` FUORI dalle parentesi.
        var hasParentheses = false;

        // Loop principale per consumare argomenti e separatori
        while (true)
        {
            // --- Condizioni di Uscita Robuste ---
            // 1. Delimitatori di blocco (es. '}' in una lambda)
            if (Check(TokenKind.RightBrace) || Check(TokenKind.RightBracket)) break;

            // 2. Fine della lista tra parentesi
            if (hasParentheses && Check(TokenKind.RightParen)) break;

            // 3. Fine della riga logica o inizio di una clausola (ELSE) per istruzioni non tra parentesi
            if (!hasParentheses &&
                (Check(TokenKind.Colon) || Check(TokenKind.Newline) || Check(TokenKind.Else) || IsAtEnd())) break;

            // 4. Inizio di una nuova istruzione (es. PRINT "valore" IF …) — ma una keyword
            //    STRUTTURALE seguita da qualcosa che può CONTINUARE un'espressione
            //    (operatore, '.', '(', separatore, fine istruzione) è una VARIABILE
            //    (parità col parser C): `PRINT step`, `PRINT step + x`. Vedi
            //    Bugs/(open) - Parser-STEP-Variabile-….
            if (IsStructuralKeyword(Peek().Kind) && !CanContinueExpression(At(1).Kind)) break;

            // --- Parsing di Separatori ---
            // Se il prossimo token è un separatore, lo consumiamo e continuiamo il loop.
            // Questo gestisce correttamente i casi di separatori multipli o finali.
            if (Match(TokenKind.Semicolon))
            {
                argsBuilder.Add(new PrintArg(null, ";"));
                continue;
            }

            if (Match(TokenKind.Comma))
            {
                argsBuilder.Add(new PrintArg(null, ","));
                continue;
            }

            // --- Parsing di Espressioni ---
            // Se siamo arrivati qui, ci deve essere un'espressione. Se non c'è, è la fine.
            if ((hasParentheses && Check(TokenKind.RightParen)) || (!hasParentheses && IsAtEndOfStatement()))
            {
                break;
            }

            var parsedExpr = ParseExpression();
            argsBuilder.Add(new PrintArg(parsedExpr, null));

            // Dopo un'espressione, ci si aspetta un separatore o la fine.
            // Se il prossimo token non è un separatore, il parsing degli argomenti è terminato.
            if (!Check(TokenKind.Semicolon) && !Check(TokenKind.Comma))
            {
                break;
            }
        }

        if (hasParentheses)
        {
            Consume(TokenKind.RightParen, "Expected ')' to close argument list.");
        }

        return argsBuilder.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione PRINT.
    /// </summary>
    private static PrintStmt ParsePrintStatement(Parser parser, string? ns)
    {
        // Accetta sia Print che QuestionMark
        var printToken = parser.Consume(TokenKind.Print, TokenKind.QuestionMark);
        var arguments = parser.ParsePrintArguments();
        return new PrintStmt(arguments, printToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione LOCATE.
    /// </summary>
    private static LocateStmt ParseLocateStatement(Parser parser, string? ns)
    {
        var locateToken = parser.Consume(TokenKind.Locate, "Internal parser error: Expected 'LOCATE' token.");
        Expr lineExpr, columnExpr;

        // Sintassi unificata (legacy e qualificata fusa): LOCATE row, column
        lineExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after row expression.");
        columnExpr = parser.ParseExpression();

        return new LocateStmt(lineExpr, columnExpr, locateToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione CLEAR (rettangolo).
    /// </summary>
    public static Stmt ParseClearRectStatement(Parser parser, string? ns)
    {
        var clearToken = parser.Consume(TokenKind.Clear, "Internal parser error: Expected 'CLEAR' token.");

        // Sintassi unificata (legacy e qualificata fusa): CLEAR (x1,y1)-(x2,y2)
        parser.Consume(TokenKind.LeftParen, "Expected '('.");
        var x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y1 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        parser.Consume(TokenKind.Minus, "Expected '-'.");
        parser.Consume(TokenKind.LeftParen, "Expected '('.");

        var x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        return new ClearRectStmt(x1, y1, x2, y2, clearToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione POINT.
    /// </summary>
    public static Stmt ParsePointStatement(Parser parser, string? ns)
    {
        var pointToken = parser.Consume(TokenKind.Point, "Internal parser error: Expected 'POINT' token.");
        Expr x, y;
        Expr? color = null;
        parser.Consume(TokenKind.LeftParen, "Expected '(' after POINT.");
        x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' separating coordinates.");
        y = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");
        if (parser.Match(TokenKind.Comma)) color = parser.ParseOptionalExpression();

        return new PointStmt(x, y, color, pointToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione SYNC.
    /// </summary>
    public static Stmt ParseSyncStatement(Parser parser, string? ns)
    {
        var syncToken = parser.Consume(TokenKind.Sync, "Internal parser error: Expected 'SYNC' token.");
        return new SyncStmt(syncToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un blocco TICK...END TICK.
    /// TICK [rate] ... END TICK — game loop sincronizzato.
    /// Si decompone in istruzioni atomiche: EnterLoopStmt, LabelStmt, TickStmt,
    /// corpo, TickEndStmt, JumpStmt, ExitLoopStmt, LabelStmt.
    /// </summary>
    public static List<Stmt> ParseTickStatement(Parser parser, ProcedureDefinition? procedureContext)
    {
        var tickToken = parser.Consume(TokenKind.Tick, "Expected 'TICK'.");

        // Optional rate (FPS), default 60
        int tickRate = 60;
        if (!parser.Check(TokenKind.Newline) && !parser.Check(TokenKind.EndTick) && !parser.IsAtEnd())
        {
            var rateExpr = parser.ParseExpression();
            if (rateExpr is NumberExpr numExpr)
                tickRate = Math.Max(1, (int)numExpr.Value);
        }
        parser.Consume(TokenKind.Newline, "Expected newline after TICK.");

        var loopStartLabel = parser.GenerateLoopLabel("TICK_START", tickToken.Position);
        var loopExitLabel = parser.GenerateLoopLabel("TICK_EXIT", tickToken.Position);

        parser._loopKindTokens.Push(tickToken);
        parser._loopStartLabels.Push(loopStartLabel);
        parser._loopContinueLabels.Push(loopStartLabel);
        parser._loopExitLabels.Push(loopExitLabel);

        var body = parser.ParseBlockBodyUntil(procedureContext, TokenKind.EndTick);

        parser.Consume(TokenKind.EndTick, "Expected 'END TICK' to close the block.");

        parser._loopKindTokens.Pop();
        parser._loopStartLabels.Pop();
        parser._loopContinueLabels.Pop();
        parser._loopExitLabels.Pop();

        return
        [
            new EnterLoopStmt(tickToken, loopStartLabel, loopExitLabel),
            new LabelStmt(loopStartLabel),
            new TickStmt(tickRate, tickToken),
            ..body,
            new TickEndStmt(tickRate, tickToken),
            new JumpStmt(loopStartLabel),
            new ExitLoopStmt(tickToken),
            new LabelStmt(loopExitLabel)
        ];
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione SPRITE, che è un comando multi-parola (es. `SPRITE LOAD`, `SPRITE SET POSITION`).
    /// Questo metodo agisce come un mini-dispatcher che analizza il sottocomando e costruisce il nodo AST appropriato.
    ///
    /// MEMORIA ARCHITETTURALE (NON DIMENTICARE):
    /// Il parser gestisce la sintassi unificata (legacy e qualificata fusa): il token `SPRITE` è seguito
    /// dal token del sottocomando (`LOAD`, `SET`, etc.), con le STESSE clausole in entrambe le forme
    /// (es. `GRAPHICS.SPRITE LOAD "a.png", id%`). Per i comandi composti come `SET POSITION`, consuma
    /// entrambi i token e crea un token combinato.
    ///
    /// Struttura del Nodo AST (`SpriteCommandStmt`):
    /// Il risultato è un `SpriteCommandStmt` che contiene il token `SPRITE` originale come riferimento principale,
    /// un `SubCommandToken` che identifica l'operazione specifica (es. `LOAD`, `SET POSITION`),
    /// e una lista di espressioni (`Expr`) per gli argomenti.
    /// </summary>
    public static Stmt ParseSpriteStatement(Parser parser, string? ns)
    {
        var spriteToken = parser.Consume(TokenKind.Sprite, "Internal parser error: Expected 'SPRITE' token.");
        Token subCommandToken;
        ImmutableArray<Expr> arguments;

        // --- SINTASSI UNIFICATA (legacy e qualificata fusa): SPRITE COMANDO ... ---
        var firstSubToken = parser.Advance(); // Consuma il primo token del sottocomando (es. LOAD, SET)

        // Gestisce i sottocomandi composti che definiscono il tipo di nodo
        switch (firstSubToken.Kind)
        {
            case TokenKind.Load:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpriteLoad, Lexeme = "LOAD" };
                break;
            case TokenKind.Set:
            {
                var secondSubToken = parser.Peek();
                switch (secondSubToken.Kind)
                {
                    // Usa un blocco switch/case tradizionale per eseguire Advance()
                    case TokenKind.Position:
                        parser.Advance(); // Consuma 'POSITION'
                        subCommandToken = firstSubToken with
                        {
                            Kind = TokenKind.SpriteSetPosition, Lexeme = "SET POSITION"
                        };
                        break;
                    case TokenKind.Matrix:
                        parser.Advance(); // Consuma 'MATRIX'
                        subCommandToken = firstSubToken with
                        {
                            Kind = TokenKind.SpriteSetMatrix, Lexeme = "SET MATRIX"
                        };
                        break;
                    // SPRITE SET SCALE id, sx, sy — ROTATION non è una keyword: si riconosce
                    // per lessema (ROTATION o ROTATE) e si emette con il TokenKind contestuale
                    // già esistente (MAI inventare TokenKind: il valore è la chiave canonica).
                    case TokenKind.Scale:
                        parser.Advance(); // Consuma 'SCALE'
                        subCommandToken = firstSubToken with
                        {
                            Kind = TokenKind.Scale, Lexeme = "SET SCALE"
                        };
                        break;
                    default:
                        if (secondSubToken.Lexeme != null
                            && (secondSubToken.Lexeme.Equals("ROTATION", StringComparison.OrdinalIgnoreCase)
                                || secondSubToken.Lexeme.Equals("ROTATE", StringComparison.OrdinalIgnoreCase)))
                        {
                            parser.Advance(); // Consuma 'ROTATION'
                            subCommandToken = firstSubToken with
                            {
                                Kind = TokenKind.Rotate, Lexeme = "SET ROTATION"
                            };
                            break;
                        }
                        throw CreateError(secondSubToken,
                            "Expected 'POSITION', 'MATRIX', 'SCALE' or 'ROTATION' after 'SPRITE SET'.");
                }

                break;
            }
            case TokenKind.On:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpriteOn, Lexeme = "ON" };
                break;
            case TokenKind.Off:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpriteOff, Lexeme = "OFF" };
                break;
            case TokenKind.Priority:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpritePriority, Lexeme = "PRIORITY" };
                break;
            case TokenKind.Transform:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpriteTransform, Lexeme = "TRANSFORM" };
                break;
            case TokenKind.SpriteGetPixels:
                subCommandToken = firstSubToken with { Kind = TokenKind.SpriteGetPixels, Lexeme = "GETPIXELS" };
                break;
            default:
                throw CreateError(firstSubToken,
                    $"Sottocomando '{firstSubToken.Lexeme}' non riconosciuto per SPRITE.");
        }

        // Parsa il resto della riga come una lista di argomenti, separati da virgola o spazio.
        arguments = parser.ParseExpressionList(TokenKind.Newline);

        return new SpriteCommandStmt(subCommandToken, arguments, spriteToken);
    }

    /// <summary>
    /// Parsa l'istruzione VIEW. Gestisce sia la sintassi con coordinate che quella senza per il reset.
    /// Sintassi unificata (legacy e qualificata fusa): VIEW [SCREEN] [(x1, y1)-(x2, y2)] [,[color] [,border]]
    /// </summary>
    public static Stmt ParseViewStatement(Parser parser, string? ns)
    {
        var viewToken = parser.Consume(TokenKind.View, "Internal parser error: Expected 'VIEW' token.");
        var screen = parser.Match(TokenKind.Screen);

        // Se non ci sono argomenti, è un reset.
        if (parser.IsAtEndOfStatement())
        {
            return new ViewStmt(screen, new NumberExpr(0, viewToken), new NumberExpr(0, viewToken),
                new NumberExpr(0, viewToken), new NumberExpr(0, viewToken), null, null, viewToken);
        }

        Expr x1, y1, x2, y2;
        Expr? color = null, border = null;

        parser.Consume(TokenKind.LeftParen, "Expected '(' for first coordinate pair.");
        x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y1 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        parser.Consume(TokenKind.Minus, "Expected '-' between coordinate pairs.");

        parser.Consume(TokenKind.LeftParen, "Expected '(' for second coordinate pair.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        if (parser.Match(TokenKind.Comma))
        {
            if (!parser.Check(TokenKind.Comma) && !parser.IsAtEndOfStatement())
            {
                color = parser.ParseExpression();
            }

            if (parser.Match(TokenKind.Comma))
            {
                border = parser.ParseOptionalExpression();
            }
        }

        return new ViewStmt(screen, x1, y1, x2, y2, color, border, viewToken);
    }

    /// <summary>
    /// Parsa l'istruzione WINDOW. Gestisce sia la sintassi con coordinate che quella senza per il reset.
    /// Sintassi unificata (legacy e qualificata fusa): WINDOW [SCREEN] [(x1, y1)-(x2, y2)]
    /// </summary>
    public static Stmt ParseViewPortStatement(Parser parser, string? ns)
    {
        // Il comando è VIEWPORT (nel namespace WINDOWS): accetta sia Viewport che Window
        // per retrocompatibilità durante la migrazione.
        var winToken = parser.Check(TokenKind.Viewport)
            ? parser.Advance()
            : parser.Consume(TokenKind.Window, "Internal parser error: Expected 'VIEWPORT' token.");
        var screen = parser.Match(TokenKind.Screen);

        // Se non ci sono argomenti, è un reset.
        if (parser.IsAtEndOfStatement())
        {
            return new ViewPortStmt(screen, new NumberExpr(0, winToken), new NumberExpr(0, winToken),
                new NumberExpr(0, winToken), new NumberExpr(0, winToken), winToken);
        }

        Expr x1, y1, x2, y2;

        parser.Consume(TokenKind.LeftParen, "Expected '(' for first coordinate pair.");
        x1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y1 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        parser.Consume(TokenKind.Minus, "Expected '-' between coordinate pairs.");

        parser.Consume(TokenKind.LeftParen, "Expected '(' for second coordinate pair.");
        x2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        y2 = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')'.");

        return new ViewPortStmt(screen, x1, y1, x2, y2, winToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione POLYGON.
    /// Sintassi unificata (legacy e qualificata fusa): POLYGON points_array, color
    /// </summary>
    public static Stmt ParsePolygonStatement(Parser parser, string? ns)
    {
        var polygonToken = parser.Consume(TokenKind.Polygon, "Internal parser error: Expected 'POLYGON' token.");
        Target pointsArray;
        Expr color;

        pointsArray = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array variable.");
        color = parser.ParseExpression();

        return new PolygonStmt(pointsArray, color, polygonToken);
    }

    /// <summary>
    /// Analizza il comando TILEMAP con i suoi sottocomandi.
    /// Sintassi unificata (legacy e qualificata fusa):
    ///     TILEMAP LOAD texId%, tileW%, tileH%, tilesPerRow%, mapW%, mapH%
    ///     TILEMAP SET row%, col%, tileIndex%
    ///     TILEMAP OFFSET x%, y%
    ///     TILEMAP SCROLL dx%, dy%
    ///     TILEMAP DRAW
    /// La forma qualificata fusa (es. `GRAPHICS.TILEMAP LOAD ...`) riusa le stesse clausole.
    /// </summary>
    public static Stmt ParseTilemapStatement(Parser parser, string? ns)
    {
        Token tilemapToken;
        Token subCommandToken;
        ImmutableArray<Expr> arguments;

        // Sintassi unificata: TILEMAP COMANDO ...
        tilemapToken = parser.Consume(TokenKind.Tilemap,
            "Internal parser error: Expected 'TILEMAP' token.");

        var firstSubToken = parser.Advance();

        subCommandToken = firstSubToken.Kind switch
        {
            TokenKind.Load => firstSubToken with { Kind = TokenKind.TilemapLoad, Lexeme = "LOAD" },
            TokenKind.Set => firstSubToken with { Kind = TokenKind.TilemapSet, Lexeme = "SET" },
            TokenKind.Scroll => firstSubToken with { Kind = TokenKind.TilemapScroll, Lexeme = "SCROLL" },
            TokenKind.Draw => firstSubToken with { Kind = TokenKind.TilemapDraw, Lexeme = "DRAW" },
            TokenKind.Offset => firstSubToken with { Kind = TokenKind.TilemapOffset, Lexeme = "OFFSET" },
            _ => throw CreateError(firstSubToken,
                $"Sottocomando TILEMAP sconosciuto: '{firstSubToken.Lexeme}'. " +
                "Attesi: LOAD, SET, OFFSET, SCROLL, DRAW.")
        };

        if (subCommandToken.Kind == TokenKind.TilemapDraw)
        {
            arguments = ImmutableArray<Expr>.Empty;
        }
        else
        {
            arguments = parser.ParseExpressionList(TokenKind.Newline);
        }

        return new TilemapCommandStmt(subCommandToken, arguments, tilemapToken);
    }

    /// <summary>
    /// Analizza SPRITEDEF START id%
    ///   "row1"
    ///   "row2"
    /// SPRITEDEF END
    /// </summary>
    public static Stmt ParseSpritedefStatement(Parser parser, string? ns)
    {
        var spritedefToken = parser.Consume(TokenKind.Spritedef,
            "Internal parser error: Expected 'SPRITEDEF' token.");

        parser.Consume(TokenKind.Start, "Expected 'START' after SPRITEDEF.");

        var spriteId = parser.ParseExpression();

        parser.Consume(TokenKind.Newline, "Expected newline after SPRITEDEF START arguments.");

        var rows = ImmutableArray.CreateBuilder<string>();

        while (!parser.Check(TokenKind.EndSpritedef) && !parser.IsAtEnd())
        {
            if (parser.Match(TokenKind.StringLiteral))
            {
                rows.Add(parser.Previous().Lexeme);
            }

            if (parser.Check(TokenKind.Newline))
                parser.Advance();
        }

        if (parser.Check(TokenKind.EndSpritedef))
            parser.Advance();

        return new SpritedefStmt(spriteId, rows.ToImmutable(), spritedefToken);
    }

    public static Stmt ParseUnloadStatement(Parser parser)
    {
        var unloadToken = parser.Consume(TokenKind.Unload, "Expected 'UNLOAD'.");

        if (parser.Match(TokenKind.Texture))
        {
            var handle = parser.ParseExpression();
            return new UnloadResourceStmt(UnloadResourceKind.Texture, handle, unloadToken);
        }
        if (parser.Match(TokenKind.Font))
        {
            var handle = parser.ParseExpression();
            return new UnloadResourceStmt(UnloadResourceKind.Font, handle, unloadToken);
        }

        throw Parser.CreateError(parser.Peek(),
            "Expected 'TEXTURE' or 'FONT' after 'UNLOAD'.");
    }

    /// <summary>[FASE 1] PIXEL BUFFER w, h, buffer — comando legacy (GRAFX-03, §2.0: niente
    /// parentesi, niente forma qualificata). Sostituisce la funzione PIXELBUFFER(w,h,px).</summary>
    public static Stmt ParsePixelBufferStatement(Parser parser, string? ns)
    {
        var pixelToken = parser.Consume(TokenKind.Pixel, "Internal parser error: Expected 'PIXEL' token.");
        parser.Consume(TokenKind.Buffer, "Expected 'BUFFER' after 'PIXEL' (comando legacy PIXEL BUFFER w, h, buffer).");
        var width = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after width in PIXEL BUFFER.");
        var height = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after height in PIXEL BUFFER.");
        var buffer = parser.ParseExpression();
        return new PixelBufferStmt(width, height, buffer, pixelToken);
    }
}