// FILE: Interpreter.Parser.Stmt.Window.cs

namespace Basic;

public sealed partial class Parser
{
    /// <summary>True se il token può essere il NOME di una finestra: un Identifier o una
    /// qualsiasi parola che non sia una keyword strutturale del comando WINDOW.</summary>
    private static bool IsWindowNameToken(Token t)
    {
        return t.Kind switch
        {
            TokenKind.Window or TokenKind.Define or TokenKind.Set or TokenKind.Select
                or TokenKind.Theme or TokenKind.Desktop or TokenKind.Frame or TokenKind.Shadow
                or TokenKind.From or TokenKind.To or TokenKind.Title or TokenKind.Screen
                or TokenKind.Viewport or TokenKind.Pixel => false,
            _ => !string.IsNullOrEmpty(t.Lexeme) && (char.IsLetter(t.Lexeme[0]) || t.Lexeme[0] == '_'),
        };
    }

    /// <summary>DOEVENTS — equivalente VB6: pompa UN frame di eventi non bloccante
    /// (WINDOW PUMP 0) e ritorna. In TEXT/headless è no-op; in GRAPHICS drena input,
    /// dispatcha gli eventi dei controlli e presenta il frame, senza attendere.</summary>
    public static Stmt ParseDoEventsStatement(Parser parser, string? ns)
    {
        var tok = parser.Consume(TokenKind.DoEvents, "Internal parser error: Expected 'DOEVENTS' token.");
        return new WindowPumpStmt(new NumberExpr(0, tok), tok);
    }

    public static List<Stmt> ParseTuiWindowStatement(Parser parser, string expectedNamespace)
    {
        // Consuma WINDOW (sintassi legacy a spazio)
        parser.Consume(TokenKind.Window, "Expected 'WINDOW'.");

        // WINDOW da solo o SCREEN → grafico
        if (parser.IsAtEndOfStatement() || parser.Check(TokenKind.Screen))
        {
            return [ParseViewPortStatement(parser, expectedNamespace)];
        }

        // WINDOW THEME "nome"
        if (parser.Match(TokenKind.Theme))
        {
            var themeExpr = parser.ParseExpression();
            return [new WindowThemeStmt(themeExpr, parser.Previous())];
        }

        // WINDOW DESKTOP n — cambia desktop virtuale attivo
        if (parser.Match(TokenKind.Desktop))
        {
            var desktopExpr = parser.ParseExpression();
            return [new WindowDesktopStmt(desktopExpr, parser.Previous())];
        }

        // WINDOW FRAME DOUBLE|SINGLE|ROUNDED — imposta frame globale
        if (parser.Match(TokenKind.Frame))
        {
            var style = WindowFrameStyle.Double;
            if (parser.Match(TokenKind.Double)) style = WindowFrameStyle.Double;
            else if (parser.Match(TokenKind.Single)) style = WindowFrameStyle.Single;
            else if (parser.Match(TokenKind.Rounded)) style = WindowFrameStyle.Rounded;
            else throw CreateError(parser.Peek(), "Expected DOUBLE, SINGLE, or ROUNDED after WINDOW FRAME.");
            return [new WindowFrameStmt(null, style, parser.Previous())];
        }

        // WINDOW SHADOW ON|OFF — imposta shadow globale
        if (parser.Match(TokenKind.Shadow))
        {
            var enable = parser.Match(TokenKind.On);
            if (!enable && !parser.Match(TokenKind.Off))
                throw CreateError(parser.Peek(), "Expected ON or OFF after WINDOW SHADOW.");
            return [new WindowShadowStmt(null, enable, parser.Previous())];
        }

        // WINDOW SHOW | WINDOW HIDE — visibilita' della finestra dello screen (globale).
        if (parser.Check(TokenKind.Show) || parser.Check(TokenKind.Hide))
        {
            bool show = parser.Match(TokenKind.Show);
            if (!show) parser.Consume(TokenKind.Hide, "Expected SHOW or HIDE.");
            return [new WindowVisibilityStmt(show, parser.Previous())];
        }

        // WINDOW SELECT target — finestra attiva (viewport corrente). target = nome | 0.
        if (parser.Check(TokenKind.Select))
        {
            parser.Advance();
            // Nome keyword (Log/Edit/View/Form) → stringa letterale (non variabile).
            if (IsWindowNameToken(parser.Peek()))
            {
                var nameTok = parser.Advance();
                return [new WindowSelectStmt(new StringExpr(nameTok.Lexeme, nameTok), nameTok)];
            }
            var target = parser.ParseExpression();
            return [new WindowSelectStmt(target, parser.Previous())];
        }

        // WINDOW PUMP <ms> — un frame del loop form (COMANDO a due token).
        if (parser.Peek().Lexeme != null && parser.Peek().Lexeme.Equals("PUMP", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var ms = parser.ParseExpression();
            return [new WindowPumpStmt(ms, parser.Previous())];
        }

        // WINDOW RESIZE <w>, <h> — ridimensiona la pagina logica e fa scattare il LAYOUT
        // dei controlli (EXPAND/ANCHOR/ALIGN). Comando a due argomenti separati da virgola.
        if (parser.Match(TokenKind.Resize))
        {
            var rw = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after WINDOW RESIZE width.");
            var rh = parser.ParseExpression();
            return [new WindowResizeStmt(rw, rh, parser.Previous())];
        }

        // WINDOW CLOSE — richiede la chiusura della finestra (COMANDO, senza nome = SDL).
        if (parser.Match(TokenKind.Close))
            return [new WindowRequestCloseStmt(parser.Previous())];

        // WINDOW DEFINE ...
        if (parser.Check(TokenKind.Define))
        {
            parser.Advance();
            return [ParseWindowDefineStatement(parser, expectedNamespace)];
        }

        // WINDOW SET TITLE "titolo" — titolo della finestra attiva.
        if (parser.Match(TokenKind.Set))
        {
            parser.Consume(TokenKind.Title, "Expected 'TITLE' after 'WINDOW SET'.");
            var title = parser.ParseExpression();
            return [new WindowSetTitleStmt(title, parser.Previous())];
        }

        // WINDOW name ACTION — il nome può essere un Identifier o keyword comuni usate come
        // nomi di finestra (Log, Edit, Help, Main, View, Status, ecc.).
        var nameToken = IsWindowNameToken(parser.Peek())
            ? parser.Advance()
            : throw CreateError(parser.Peek(), "Expected window name or DEFINE after WINDOW.");

        // ── Comandi di contenuto (buffer della finestra) ──
        if (parser.Match(TokenKind.Print))
        {
            var text = parser.ParseExpression();
            return [new WindowAppendLineStmt(nameToken, text, nameToken)];
        }
        if (parser.Match(TokenKind.Text))
        {
            var lines = parser.ParseExpression();
            return [new WindowSetContentStmt(nameToken, lines, nameToken)];
        }
        if (parser.Match(TokenKind.Clear))
        {
            return [new WindowClearStmt(nameToken, nameToken)];
        }
        if (parser.Match(TokenKind.Scroll))
        {
            var delta = parser.ParseExpression();
            return [new WindowScrollStmt(nameToken, delta, nameToken)];
        }
        if (parser.Match(TokenKind.Edit))
        {
            var enable = parser.Match(TokenKind.On);
            if (!enable && !parser.Match(TokenKind.Off))
                throw CreateError(parser.Peek(), "Expected ON or OFF after WINDOW EDIT.");
            return [new WindowEditStmt(nameToken, enable, nameToken)];
        }

        // ── Comandi per-finestra (Desktop, Frame, Shadow) ──
        if (parser.Match(TokenKind.Desktop))
        {
            var desktopExpr = parser.ParseExpression();
            return [new WindowMoveDesktopStmt(nameToken, desktopExpr, nameToken)];
        }
        if (parser.Match(TokenKind.Frame))
        {
            var style = WindowFrameStyle.Double;
            if (parser.Match(TokenKind.Double)) style = WindowFrameStyle.Double;
            else if (parser.Match(TokenKind.Single)) style = WindowFrameStyle.Single;
            else if (parser.Match(TokenKind.Rounded)) style = WindowFrameStyle.Rounded;
            else throw CreateError(parser.Peek(), "Expected DOUBLE, SINGLE, or ROUNDED after WINDOW name FRAME.");
            return [new WindowFrameStmt(nameToken, style, nameToken)];
        }
        if (parser.Match(TokenKind.Shadow))
        {
            var enable = parser.Match(TokenKind.On);
            if (!enable && !parser.Match(TokenKind.Off))
                throw CreateError(parser.Peek(), "Expected ON or OFF after WINDOW name SHADOW.");
            return [new WindowShadowStmt(nameToken, enable, nameToken)];
        }

        WindowAction action;
        if (parser.Check(TokenKind.Activate)) { parser.Advance(); action = WindowAction.Activate; }
        else if (parser.Check(TokenKind.Deactivate)) { parser.Advance(); action = WindowAction.Deactivate; }
        else if (parser.Check(TokenKind.Release)) { parser.Advance(); action = WindowAction.Release; }
        else if (parser.Check(TokenKind.Close)) { parser.Advance(); action = WindowAction.Close; }
        else if (parser.Check(TokenKind.Minimize)) { parser.Advance(); action = WindowAction.Minimize; }
        else if (parser.Check(TokenKind.Restore)) { parser.Advance(); action = WindowAction.Restore; }
        else throw CreateError(parser.Peek(), "Expected window action after name.");
        return [new WindowActionStmt(nameToken, action, nameToken)];
    }

    public static Stmt ParseWindowDefineStatement(Parser parser, string? expectedNamespace)
    {
        if (parser.Check(TokenKind.Define))
            parser.Advance();

        Expr? title = null; var isDouble = false; var isShadow = false; var isPixel = false;
        var style = WindowFrameStyle.Double;
        Expr? desktop = null;
        Token nameToken; Expr top, left, bottom, right;

        // Sintassi unificata (legacy e qualificata fusa): DEFINE name FROM top, left TO bottom, right
        nameToken = IsWindowNameToken(parser.Peek())
            ? parser.Advance()
            : throw CreateError(parser.Peek(), "Expected window name.");
        parser.Consume(TokenKind.From, "Expected FROM.");
        top = parser.ParseExpression(); parser.Consume(TokenKind.Comma, "Expected ','");
        left = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected TO.");
        bottom = parser.ParseExpression(); parser.Consume(TokenKind.Comma, "Expected ','");
        right = parser.ParseExpression();
        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Title)) title = parser.ParseExpression();
            else if (parser.Match(TokenKind.Double)) { isDouble = true; style = WindowFrameStyle.Double; }
            else if (parser.Match(TokenKind.Single)) { style = WindowFrameStyle.Single; }
            else if (parser.Match(TokenKind.Rounded)) { style = WindowFrameStyle.Rounded; }
            else if (parser.Match(TokenKind.Shadow)) isShadow = true;
            else if (parser.Match(TokenKind.Pixel)) isPixel = true;
            else if (parser.Match(TokenKind.Desktop)) desktop = parser.ParseExpression();
            else break;
        }
        return new DefineWindowStmt(nameToken, top, left, bottom, right, title, isDouble, isShadow, isPixel, nameToken, style, desktop);
    }

    public static Stmt ActivateWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Activate);
    public static Stmt DeactivateWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Deactivate);
    public static Stmt ReleaseWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Release);

    public static Stmt CloseWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Close);
    public static Stmt MinimizeWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Minimize);
    public static Stmt RestoreWindow(Parser p, string? ns) => ActionWindow(p, WindowAction.Restore);

    private static Stmt ActionWindow(Parser p, WindowAction action)
    {
        p.Advance(); // consuma il comando (ACTIVATE/DEACTIVATE/...)
        var n = p.ParseExpression();
        var nt = n is StringExpr se ? new Token(TokenKind.Identifier, se.Value, null, se.Position)
            : n is VariableExpr ve ? ve.NameCleaned : throw new InvalidOperationException("Expected window name.");
        return new WindowActionStmt(nt, action, p.Previous());
    }
}
