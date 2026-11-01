// FILE: Interpreter.Parser.Stmt.Controls.cs

namespace Basic;

public sealed partial class Parser
{
    private static ControlEventStmt MakeCes(string ev, PixelClause cl, Target id, Token tok, Token? targetTok)
    {
        var tgt = (cl.ActionKind ?? "CALL").ToUpperInvariant() == "CALL" ? cl.SubName : cl.LabelName;
        return new ControlEventStmt(ev.ToUpperInvariant(), ConvertTargetToExpr(id), cl.ActionKind ?? "CALL", tgt, tok, targetTok);
    }

    /// <summary>Clausole proprieta' dei controlli CONTROLS: NAME/TAG/TOOLTIP "v",
    /// VISIBLE/ENABLED/TABSTOP e, TABINDEX n. Aggiorna `id` per la clausola ID.</summary>
    private static ControlProps? ParseControlProps(Parser p, ref Target? id, out List<ControlEventStmt>? events, Token tok)
    {
        Expr? name=null,tag=null,tip=null,vis=null,en=null,ti=null,ts=null,sty=null; bool foc=false;
        Expr? color=null,colorfocus=null,colorsel=null,colorerr=null,haserr=null,upper=null;
        events = null;
        while (p.Peek().Kind != TokenKind.Newline && p.Peek().Kind != TokenKind.Eof)
        {
            var t = p.Peek();
            var att = Parser.AttachedEvent(t);
            if (att != null) { p.Advance(); var cl = p.ParseControlEventClause(att); if (id != null) (events ??= new()).Add(MakeCes(att, cl, id, tok, p._lastEvTargetToken)); continue; }
            var key = (t.Lexeme ?? "").ToUpperInvariant();
            if (key == "ON") { p.Advance(); var ev = (p.Peek().Lexeme ?? "").ToUpperInvariant(); p.Advance(); var cl = p.ParseControlEventClause(ev); if (id != null) (events ??= new()).Add(MakeCes(ev, cl, id, tok, p._lastEvTargetToken)); continue; }
            if (key == "ID") { p.Advance(); id = p.ParseTarget(); continue; }
            if (key == "FOCUS") { p.Advance(); foc = true; continue; }
            if (key == "STYLE") { p.Advance(); sty = p.ParseExpression(); continue; }
            if (key == "NAME" || key == "TAG" || key == "TOOLTIP" || key == "VISIBLE" || key == "ENABLED" || key == "TABINDEX" || key == "TABSTOP"
                || key == "COLOR" || key == "COLORFOCUS" || key == "COLORSEL" || key == "COLORERROR" || key == "HASERROR" || key == "UPPERCASE")
            {
                p.Advance();
                var v = p.ParseExpression();
                switch (key) { case "NAME": name=v; break; case "TAG": tag=v; break; case "TOOLTIP": tip=v; break;
                    case "VISIBLE": vis=v; break; case "ENABLED": en=v; break; case "TABINDEX": ti=v; break; case "TABSTOP": ts=v; break;
                    case "COLOR": color=v; break; case "COLORFOCUS": colorfocus=v; break; case "COLORSEL": colorsel=v; break;
                    case "COLORERROR": colorerr=v; break; case "HASERROR": haserr=v; break; case "UPPERCASE": upper=v; break; }
                continue;
            }
            break;
        }
        if (name==null&&tag==null&&tip==null&&vis==null&&en==null&&ti==null&&ts==null&&!foc&&sty==null
            &&color==null&&colorfocus==null&&colorsel==null&&colorerr==null&&haserr==null&&upper==null) return null;
        return new ControlProps(name, tag, tip, vis, en, ti, ts, foc, sty, color, colorfocus, colorsel, colorerr, haserr, upper);
    }

    public static Stmt ParseCheckBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var cb = ParseControlArgs(p, ("AT", 2), ("TEXT", 1));
        var row = cb[0] ?? new NumberExpr(0, tok);
        var col = cb[1] ?? new NumberExpr(0, tok);
        var caption = cb[2] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after CHECKBOX.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new CheckBoxStmt(variable, row, col, caption, tok, id, props, events);
    }

    public static Stmt ParseListBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, items) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after LISTBOX.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ListBoxStmt(variable, row, col, width, height, items, tok, id, props, events);
    }

    public static Stmt ParseComboBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var cbb = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("ITEMS", 1));
        var row = cbb[0] ?? new NumberExpr(0, tok);
        var col = cbb[1] ?? new NumberExpr(0, tok);
        var width = cbb[2] ?? new NumberExpr(0, tok);
        var items = cbb[3] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after COMBOBOX.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ComboBoxStmt(variable, row, col, width, items, tok, id, props, events);
    }

    public static Stmt ParseMenuStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var mn = ParseControlArgs(p, ("AT", 2), ("ITEMS", 1));
        var row = mn[0] ?? new NumberExpr(0, tok);
        var col = mn[1] ?? new NumberExpr(0, tok);
        var items = mn[2] ?? new StringExpr("", tok);
        // INTO è OPZIONALE: un MENU senza destinazione è display-only (nessun valore da
        // leggere) — come SPINNER. Con INTO la variabile riceve l'indice della voce scelta.
        Target? variable = p.Match(TokenKind.Into) ? p.ParseTarget() : null;
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new MenuStmt(variable, row, col, items, tok, id, props, events);
    }

    public static Stmt ParseStatusBarStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: STATUSBAR "testo"  |  Verbosa: STATUSBAR TEXT "testo". */
        var a = ParseControlArgs(p, ("TEXT", 1));
        var text = a[0] ?? new StringExpr("", tok);
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new StatusBarStmt(text, tok, id, props, events);
    }

    public static Stmt ParseFileDialogStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: FILEDIALOG "*.txt", "Titolo" INTO v  |
         * Verbosa: FILEDIALOG MASK "*.txt" TITLE "Titolo" INTO v. */
        var a = ParseControlArgs(p, ("MASK", 1), ("TITLE", 1));
        var mask = a[0] ?? new StringExpr("*.*", tok);
        var title = a[1] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after FILEDIALOG.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new FileDialogStmt(variable, mask, title, tok, id, props, events);
    }

    public static Stmt ParsePrinterDialogStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: PRINTERDIALOG "Titolo" INTO p$, copie, duplex |
         * Verbosa: PRINTERDIALOG TITLE "Titolo" INTO p$, copie, duplex. */
        var a = ParseControlArgs(p, ("TITLE", 1));
        Expr? title = a[0];
        p.Consume(TokenKind.Into, "Expected 'INTO' after PRINTERDIALOG.");
        var printerVar = p.ParseTarget();
        p.Consume(TokenKind.Comma, "Expected ','");
        var copiesVar = p.ParseTarget();
        p.Consume(TokenKind.Comma, "Expected ','");
        var duplexVar = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new PrinterDialogStmt(printerVar, copiesVar, duplexVar, title, tok, id, props, events);
    }

    public static Stmt ParseToggleStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var tg = ParseControlArgs(p, ("AT", 2), ("TEXT", 1));
        var row = tg[0] ?? new NumberExpr(0, tok);
        var col = tg[1] ?? new NumberExpr(0, tok);
        var caption = tg[2] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after TOGGLE.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ToggleStmt(variable, row, col, caption, tok, id, props, events);
    }

    public static Stmt ParseRadioGroupStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var rg = ParseControlArgs(p, ("AT", 2), ("ITEMS", 1));
        var row = rg[0] ?? new NumberExpr(0, tok);
        var col = rg[1] ?? new NumberExpr(0, tok);
        var items = rg[2] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after RADIOGROUP.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new RadioGroupStmt(variable, row, col, items, tok, id, props, events);
    }

    public static Stmt ParseSliderStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var sl = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("MIN", 1), ("MAX", 1), ("STEP", 1));
        var row = sl[0] ?? new NumberExpr(0, tok);
        var col = sl[1] ?? new NumberExpr(0, tok);
        var length = sl[2] ?? new NumberExpr(0, tok);
        var min = sl[3] ?? new NumberExpr(0, tok);
        var max = sl[4] ?? new NumberExpr(0, tok);
        var step = sl[5] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after SLIDER.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new SliderStmt(variable, row, col, length, min, max, step, tok, id, props, events);
    }

    public static Stmt ParseNumericUpDownStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var nu = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("MIN", 1), ("MAX", 1), ("STEP", 1));
        var row = nu[0] ?? new NumberExpr(0, tok);
        var col = nu[1] ?? new NumberExpr(0, tok);
        var length = nu[2] ?? new NumberExpr(0, tok);
        var min = nu[3] ?? new NumberExpr(0, tok);
        var max = nu[4] ?? new NumberExpr(0, tok);
        var step = nu[5] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after NUMERICUPDOWN.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new NumericUpDownStmt(variable, row, col, length, min, max, step, tok, id, props, events);
    }

    public static Stmt ParseStarRatingStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var sr = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("MAX", 1), ("VALUE", 1));
        var row = sr[0] ?? new NumberExpr(0, tok);
        var col = sr[1] ?? new NumberExpr(0, tok);
        var length = sr[2] ?? new NumberExpr(0, tok);
        var max = sr[3] ?? new NumberExpr(0, tok);
        var value = sr[4] ?? new NumberExpr(0, tok);
        // INTO è OPZIONALE: STARRATING senza destinazione è display-only (come SPINNER/MENU).
        Target? variable = p.Match(TokenKind.Into) ? p.ParseTarget() : null;
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new StarRatingStmt(variable, row, col, length, max, value, tok, id, props, events);
    }

    public static Stmt ParseScrollBarStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var sb = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("MIN", 1), ("MAX", 1), ("VALUE", 1));
        var row = sb[0] ?? new NumberExpr(0, tok);
        var col = sb[1] ?? new NumberExpr(0, tok);
        var length = sb[2] ?? new NumberExpr(0, tok);
        var min = sb[3] ?? new NumberExpr(0, tok);
        var max = sb[4] ?? new NumberExpr(0, tok);
        var value = sb[5] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after SCROLLBAR.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ScrollBarStmt(variable, row, col, length, min, max, value, tok, id, props, events);
    }

    public static Stmt ParseScrollViewStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, content) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after SCROLLVIEW.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ScrollViewStmt(variable, row, col, width, height, content, tok, id, props, events);
    }

    /// <summary>Argomenti di un controllo: sintassi VELOCE (posizionale, virgole) o VERBOSA
    /// (chiave: AT x,y | SIZE w,h | TEXT e | VALUE e | MIN e | MAX e | STEP e | CAPTION e).
    /// Normalizza in un array PIATTO in ordine posizionale secondo `order` (Key,Count):
    /// ogni voce consuma Count espressioni dal nome-chiave o, in mancanza, dalla posizione.</summary>
    public static Expr?[] ParseControlArgs(Parser p, params (string Key, int Count)[] order)
    {
        var named = new Dictionary<string, List<Expr>>(StringComparer.OrdinalIgnoreCase);
        var pos = new List<Expr>();
        while (p.Peek().Kind != TokenKind.Newline && p.Peek().Kind != TokenKind.Eof)
        {
            var k = p.Peek().Kind;
            string? key = k switch
            {
                TokenKind.At => "AT", TokenKind.Size => "SIZE", TokenKind.Text => "TEXT",
                TokenKind.Value => "VALUE", TokenKind.Min => "MIN", TokenKind.Max => "MAX",
                TokenKind.Step => "STEP", TokenKind.Caption => "CAPTION",
                TokenKind.Width => "WIDTH", TokenKind.Height => "HEIGHT", TokenKind.Items => "ITEMS",
                TokenKind.Title => "TITLE", TokenKind.Mask => "MASK",
                _ => null
            };
            if (key != null)
            {
                p.Advance();
                int n = (key == "AT" || key == "SIZE") ? 2 : 1;
                var list = new List<Expr>();
                for (int i = 0; i < n; i++) { list.Add(p.ParseExpression()); if (i < n - 1) p.Match(TokenKind.Comma); }
                named[key] = list;
                p.Match(TokenKind.Comma);
                continue;
            }
            if (p.Peek().Kind == TokenKind.Into || IsClauseKeyword(p.Peek())) break;
            pos.Add(p.ParseExpression());
            if (!p.Match(TokenKind.Comma)) break;
        }
        var result = new List<Expr?>();
        int pi = 0;
        foreach (var (key, count) in order)
        {
            if (named.TryGetValue(key, out var nv))
                for (int i = 0; i < count; i++) result.Add(i < nv.Count ? nv[i] : null);
            else
                for (int i = 0; i < count; i++) result.Add(pi < pos.Count ? pos[pi++] : null);
        }
        return result.ToArray();
    }

    /// <summary>Helper per i controlli "griglia": posizione + size + contenuto.
    /// Veloce: r,c,w,h,content | Verbosa: AT r,c SIZE w,h ITEMS/TEXT content.</summary>
    private static (Expr Row, Expr Col, Expr Width, Expr Height, Expr Content) ParseGrid(Parser p, Token tok, bool numericContent = false)
    {
        var a = ParseControlArgs(p, ("AT", 2), ("SIZE", 2), ("ITEMS", 1));
        return (a[0] ?? new NumberExpr(0, tok), a[1] ?? new NumberExpr(0, tok),
                a[2] ?? new NumberExpr(0, tok), a[3] ?? new NumberExpr(0, tok),
                a[4] ?? (numericContent ? new NumberExpr(0, tok) : new StringExpr("", tok)));
    }

    public static Stmt ParseLabelStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: LABEL r, c, w, "text"  |  Verbosa: LABEL AT r, c WIDTH w TEXT "text". */
        var a = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1), ("TEXT", 1));
        var row = a[0] ?? new NumberExpr(0, tok);
        var col = a[1] ?? new NumberExpr(0, tok);
        var width = a[2] ?? new NumberExpr(0, tok);
        var text = a[3] ?? new StringExpr("", tok);
        // INTO v OPZIONALE: il Label è display-only (come SPINNER) — senza INTO il
        // valore di ritorno viene scartato.
        Target? variable = null;
        if (p.Match(TokenKind.Into))
            variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new LabelControlStmt(row, col, width, text, variable, tok, id, props, events);
    }

    public static Stmt ParseDataListStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: DATALIST r,c,w,h,items INTO v | Verbosa: DATALIST AT r,c SIZE w,h ITEMS items INTO v. */
        var a = ParseControlArgs(p, ("AT", 2), ("SIZE", 2), ("ITEMS", 1));
        var row = a[0] ?? new NumberExpr(0, tok);
        var col = a[1] ?? new NumberExpr(0, tok);
        var width = a[2] ?? new NumberExpr(0, tok);
        var height = a[3] ?? new NumberExpr(0, tok);
        var items = a[4] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after DATALIST.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new DataListStmt(variable, row, col, width, height, items, tok, id, props, events);
    }

    public static Stmt ParsePropertyGridStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, items) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after PROPERTYGRID.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new PropertyGridStmt(variable, row, col, width, height, items, tok, id, props, events);
    }

    // SPLITTER <row>, <col>, <width>, <height>, <orient> INTO var  (orient 0=verticale, 1=orizzontale)
    public static Stmt ParseSplitterStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var a = ParseControlArgs(p, ("AT", 2), ("SIZE", 2), ("ORIENT", 1));
        var row = a[0] ?? new NumberExpr(0, tok);
        var col = a[1] ?? new NumberExpr(0, tok);
        var width = a[2] ?? new NumberExpr(0, tok);
        var height = a[3] ?? new NumberExpr(0, tok);
        var orient = a[4] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after SPLITTER.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new SplitterStmt(variable, row, col, width, height, orient, tok, id, props, events);
    }

    // CONTEXTMENU <row>, <col>, <width>, <height>, items() INTO var  (items: "a|b|--|c", "--"=separatore)
    public static Stmt ParseContextMenuStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, items) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after CONTEXTMENU.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ContextMenuStmt(variable, row, col, width, height, items, tok, id, props, events);
    }

    public static Stmt ParseLogViewStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, records) = ParseGrid(p, tok);
        // INTO opzionale: senza destinazione il controllo è display-only.
        Target? variable = p.Match(TokenKind.Into) ? p.ParseTarget() : null;
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new LogViewStmt(variable, row, col, width, height, records, tok, id, props, events);
    }

    public static Stmt ParseHexViewStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, data) = ParseGrid(p, tok);
        // INTO opzionale: senza destinazione il controllo è display-only.
        Target? variable = p.Match(TokenKind.Into) ? p.ParseTarget() : null;
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new HexViewStmt(variable, row, col, width, height, data, tok, id, props, events);
    }

    public static Stmt ParseBigTextStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: BIGTEXT r, c, text | Verbosa: BIGTEXT AT r, c TEXT text. */
        var bg = ParseControlArgs(p, ("AT", 2), ("TEXT", 1));
        var row = bg[0] ?? new NumberExpr(0, tok);
        var col = bg[1] ?? new NumberExpr(0, tok);
        var text = bg[2] ?? new StringExpr("", tok);
        // INTO v OPZIONALE: BigText è display-only (come LABEL/SPINNER).
        Target? variable = null;
        if (p.Match(TokenKind.Into))
            variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new BigTextStmt(row, col, text, variable, tok, id, props, events);
    }

    public static Stmt ParseCodeAreaStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, lines) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after CODEAREA.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new CodeAreaStmt(variable, row, col, width, height, lines, tok, id, props, events);
    }

    public static Stmt ParseReorderListBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, items) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after REORDERLISTBOX.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ReorderListBoxStmt(variable, row, col, width, height, items, tok, id, props, events);
    }

    public static Stmt ParseAlertBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, text) = ParseGrid(p, tok);
        // INTO v OPZIONALE: AlertBox è display-only (come LABEL).
        Target? variable = null;
        if (p.Match(TokenKind.Into))
            variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new AlertBoxStmt(row, col, width, height, text, variable, tok, id, props, events);
    }

    public static Stmt ParseConfirmationBoxStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, text) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after CONFIRMATIONBOX.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ConfirmationBoxStmt(variable, row, col, width, height, text, tok, id, props, events);
    }

    public static Stmt ParseFileExplorerStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, path) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after FILEEXPLORER.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new FileExplorerStmt(variable, row, col, width, height, path, tok, id, props, events);
    }

    public static Stmt ParseDashboardStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, metrics) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after DASHBOARD.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new DashboardStmt(variable, row, col, width, height, metrics, tok, id, props, events);
    }

    public static Stmt ParseTabStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, tabs) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after TAB.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new TabStmt(variable, row, col, width, height, tabs, tok, id, props, events);
    }

    public static Stmt ParseGridStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: GRID r,c,w,h,headers,data INTO v | Verbosa: GRID AT r,c SIZE w,h ITEMS headers TEXT data INTO v. */
        var ga = ParseControlArgs(p, ("AT", 2), ("SIZE", 2), ("ITEMS", 1), ("TEXT", 1));
        var row = ga[0] ?? new NumberExpr(0, tok);
        var col = ga[1] ?? new NumberExpr(0, tok);
        var width = ga[2] ?? new NumberExpr(0, tok);
        var height = ga[3] ?? new NumberExpr(0, tok);
        var headers = ga[4] ?? new StringExpr("", tok);
        var data = ga[5] ?? new StringExpr("", tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after GRID.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new GridStmt(variable, row, col, width, height, headers, data, tok, id, props, events);
    }

    public static Stmt ParseDatePickerStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var dp = ParseControlArgs(p, ("AT", 2));
        var row = dp[0] ?? new NumberExpr(0, tok);
        var col = dp[1] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after DATEPICKER.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new DatePickerStmt(variable, row, col, tok, id, props, events);
    }

    public static Stmt ParseColorPickerStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var cp = ParseControlArgs(p, ("AT", 2), ("WIDTH", 1));
        var row = cp[0] ?? new NumberExpr(0, tok);
        var col = cp[1] ?? new NumberExpr(0, tok);
        var columns = cp[2] ?? new NumberExpr(0, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after COLORPICKER.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new ColorPickerStmt(variable, row, col, columns, tok, id, props, events);
    }

    public static Stmt ParseTreeViewStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var (row, col, width, height, tree) = ParseGrid(p, tok);
        p.Consume(TokenKind.Into, "Expected 'INTO' after TREEVIEW.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new TreeViewStmt(variable, row, col, width, height, tree, tok, id, props, events);
    }

    public static Stmt ParseSpinnerStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        var sp = ParseControlArgs(p, ("AT", 2), ("TEXT", 1));
        var row = sp[0] ?? new NumberExpr(0, tok);
        var col = sp[1] ?? new NumberExpr(0, tok);
        var preset = sp[2] ?? new StringExpr("", tok);
        // INTO v OPZIONALE: lo Spinner espone il preset selezionato (o la stringa vuota
        // per il preset di default) — display-only quando INTO è omesso (come i sample
        // ClipperPCodeTest/ConsoleUiControls esistenti).
        Target? variable = null;
        if (p.Match(TokenKind.Into))
            variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new SpinnerStmt(row, col, preset, variable, tok, id, props, events);
    }

    public static Stmt ParseTBrowseStandalone(Parser p, string? ns)
    {
        var tok = p.Peek(); p.Advance();
        /* Veloce: TBROWSE r,c,w,h,cols,data[,opts] | Verbosa: AT r,c SIZE w,h ITEMS cols TEXT data. */
        var a = ParseControlArgs(p, ("AT", 2), ("SIZE", 2), ("ITEMS", 1), ("TEXT", 1));
        var row = a[0] ?? new NumberExpr(0, tok);
        var col = a[1] ?? new NumberExpr(0, tok);
        var width = a[2] ?? new NumberExpr(0, tok);
        var height = a[3] ?? new NumberExpr(0, tok);
        var columns = a[4] ?? new StringExpr("", tok);
        var data = a[5] ?? new StringExpr("", tok);
        Expr? options = null;
        if (!p.Check(TokenKind.Into) && p.Peek().Kind == TokenKind.Comma)
        {
            p.Consume(TokenKind.Comma, "Expected ','");
            options = p.ParseExpression();
        }
        p.Consume(TokenKind.Into, "Expected 'INTO' after TBROWSE.");
        var variable = p.ParseTarget();
        Target? id = null;
        var props = ParseControlProps(p, ref id, out var events, tok);
        return new TBrowseStmt(variable, row, col, width, height, columns, data, options, tok, id, props, events);
    }
}
