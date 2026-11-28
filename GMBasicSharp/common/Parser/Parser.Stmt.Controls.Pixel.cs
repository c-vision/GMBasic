// FILE: Parser/Parser.Stmt.Controls.Pixel.cs
// Parser dei controlli PIXEL e del metodo CONTROL, registrati per NOME (NamedCommandRegistry).

namespace Basic;

public sealed partial class Parser
{
    private static readonly HashSet<string> PixelClauseKeys = new(StringComparer.OrdinalIgnoreCase)
    { "NAME", "TAG", "TOOLTIP", "VISIBLE", "ENABLED", "TABINDEX", "TABSTOP", "FOCUS", "STYLE", "ON", "ID", "USE",
      "EXPAND", "ANCHOR", "ALIGN" };

    /* Nomi di evento validi (per le clausole inline attaccate ON<EVENTO>). */
    private static readonly HashSet<string> PixelEvents = new(StringComparer.OrdinalIgnoreCase)
    { "CLICK", "DBLCLICK", "CHANGE", "GOTFOCUS", "LOSTFOCUS", "MOUSEDOWN", "MOUSEUP", "MOUSEMOVE",
      "KEYDOWN", "KEYPRESS", "KEYUP", "DRAGDROP", "DRAGOVER", "VALIDATE", "SCROLL", "DROPDOWN",
      "UPCLICK", "DOWNCLICK", "NODECLICK", "EXPAND", "COLLAPSE", "SELCHANGE", "ROWCOLCHANGE",
      "WHEN" };

    /// <summary>`ONCLICK`/`ONCHANGE`/... (keyword attaccata) → nome evento, altrimenti null.</summary>
    private static string? AttachedEvent(Token t)
    {
        var lx = t.Lexeme;
        if (lx == null || lx.Length <= 2) return null;
        if (!lx.StartsWith("ON", StringComparison.OrdinalIgnoreCase)) return null;
        var ev = lx.Substring(2);
        return PixelEvents.Contains(ev) ? ev.ToUpperInvariant() : null;
    }

    private static bool IsClauseKeyword(Token t) =>
        t.Lexeme != null && PixelClauseKeys.Contains(t.Lexeme);   /* per lexeme: TAG/TOOLTIP/VISIBLE... sono keyword */

    /// <summary>Confronto per LESSEMA (case-insensitive) — LEFT/RIGHT/... possono essere
    /// keyword o identificatori: la grafia è l'unica cosa che conta qui.</summary>
    private static bool LayoutWord(Token? t, params string[] words) =>
        t?.Lexeme != null && words.Any(w => string.Equals(t.Lexeme, w, StringComparison.OrdinalIgnoreCase));

    /// <summary>Clausola di LAYOUT del controllo (EXPAND/ANCHOR/ALIGN) → frammento CSS-like
    /// `chiave=valore` da fondere nella spec di stile. Il token della chiave è già consumato.
    /// Stessa grammatica del parser C (gmb_parser_stmt_controls.c): una sola semantica.</summary>
    private static string ParseLayoutClause(Parser p, string key)
    {
        if (key == "EXPAND")
        {
            var v = "both";
            var t = p.Peek();
            if (LayoutWord(t, "H", "HORIZONTAL")) { v = "h"; p.Advance(); }
            else if (LayoutWord(t, "V", "VERTICAL")) { v = "v"; p.Advance(); }
            else if (LayoutWord(t, "BOTH", "ALL")) { v = "both"; p.Advance(); }
            else if (LayoutWord(t, "NONE")) { v = "none"; p.Advance(); }
            return "expand=" + v;
        }
        if (key == "ANCHOR" || key == "ALIGN")
        {
            var parts = new List<string>();
            while (true)
            {
                var t = p.Peek();
                string? v;
                if (key == "ANCHOR")
                    v = LayoutWord(t, "LEFT") ? "left"
                        : LayoutWord(t, "RIGHT") ? "right"
                        : LayoutWord(t, "TOP") ? "top"
                        : LayoutWord(t, "BOTTOM") ? "bottom"
                        : LayoutWord(t, "ALL", "FILL") ? "all"
                        : LayoutWord(t, "NONE") ? "none" : null;
                else
                    v = LayoutWord(t, "LEFT", "START") ? "left"
                        : LayoutWord(t, "CENTER", "CENTRE") ? "center"
                        : LayoutWord(t, "RIGHT", "END") ? "right"
                        : LayoutWord(t, "TOP") ? "top"
                        : LayoutWord(t, "MIDDLE", "VCENTER") ? "middle"
                        : LayoutWord(t, "BOTTOM") ? "bottom" : null;
                if (v == null)
                {
                    if (parts.Count == 0)
                        throw CreateError(t, key == "ANCHOR"
                            ? "ANCHOR nel controllo: atteso LEFT/RIGHT/TOP/BOTTOM/ALL/NONE."
                            : "ALIGN nel controllo: atteso LEFT/CENTER/RIGHT/TOP/MIDDLE/BOTTOM.");
                    break;
                }
                parts.Add(v); p.Advance();
                if (p.Peek().Kind is TokenKind.Plus or TokenKind.Comma) { p.Advance(); continue; }
                break;
            }
            return (key == "ANCHOR" ? "anchor=" : "align=") + string.Join("+", parts);
        }
        return "";
    }

    /// <summary>CHECK/RADIO/TRACK/DROPDOWN/PROGRESS/TEXTBOX/BUTTON/... + clausole
    /// (NAME/TAG/TOOLTIP/VISIBLE/ENABLED/TABINDEX/TABSTOP/FOCUS/STYLE/ON &lt;EV&gt; &lt;SUB&gt;).</summary>
    public static Stmt ParsePixelControlStandalone(Parser p, string? ns)
    {
        var tok = p.Peek();
        var kind = (tok.Lexeme ?? "").ToUpperInvariant();
        p.Advance();   // consuma il nome del controllo

        var args = new List<Expr>();
        Target? into = null;
        Target? id = null;
        var clauses = new List<PixelClause>();

        while (p.Peek().Kind != TokenKind.Newline && p.Peek().Kind != TokenKind.Eof)
        {
            if (p.Peek().Kind == TokenKind.Into) { p.Advance(); into = p.ParseTarget(); break; }
            if (IsClauseKeyword(p.Peek())) break;
            args.Add(p.ParseExpression());
            if (!p.Match(TokenKind.Comma)) break;
        }
        if (into == null && p.Peek().Kind == TokenKind.Into) { p.Advance(); into = p.ParseTarget(); }

        while (p.Peek().Kind != TokenKind.Newline && p.Peek().Kind != TokenKind.Eof)
        {
            var t = p.Peek();
            var att = AttachedEvent(t);
            if (att == null && !IsClauseKeyword(t)) break;
            var key = att != null ? "ON" : t.Lexeme!.ToUpperInvariant();
            var keyKind = key switch
            {
                "NAME" => TokenKind.PixelName,
                "TAG" => TokenKind.Tag,
                "TOOLTIP" => TokenKind.PixelTooltip,
                "VISIBLE" => TokenKind.PixelVisible,
                "ENABLED" => TokenKind.PixelEnabled,
                "TABINDEX" => TokenKind.PixelTabIndex,
                "TABSTOP" => TokenKind.PixelTabStop,
                "FOCUS" => TokenKind.Focus,
                "STYLE" => TokenKind.PixelStyle,
                _ => TokenKind.Identifier
            };
            p.Advance();
            if (key == "ON")
            {
                string ev;
                if (att != null) ev = att;
                else { ev = (p.Peek().Lexeme ?? "").ToUpperInvariant(); p.Advance(); }
                clauses.Add(p.ParseControlEventClause(ev));
            }
            else if (key == "FOCUS")
            {
                clauses.Add(new PixelClause("FOCUS", null, null, null, KeyKind: keyKind));
            }
            else if (key == "ID")
            {
                id = p.ParseTarget();
            }
            else if (key == "USE")
            {
                /* Spazio coordinate DICHIARATO dal controllo: USE CELLS → metric=1
                 * (CellsToPixel), USE PIXELS → metric=2 (PixelToCells). Fuso nella spec
                 * di stile, che i backend passano già al runtime (unica fonte C). */
                var v = (p.Peek().Lexeme ?? "").ToUpperInvariant();
                if (v == "CELLS") { p.Advance(); clauses.Add(new PixelClause("STYLE", new StringExpr("metric=1", t), null, null, KeyKind: TokenKind.PixelStyle)); }
                else if (v == "PIXELS") { p.Advance(); clauses.Add(new PixelClause("STYLE", new StringExpr("metric=2", t), null, null, KeyKind: TokenKind.PixelStyle)); }
                else throw CreateError(p.Peek(), "USE nel controllo: atteso CELLS o PIXELS.");
            }
            else if (key is "EXPAND" or "ANCHOR" or "ALIGN")
            {
                /* Clausole di LAYOUT: proprietà del controllo come NAME/TAG/STYLE.
                 * Il frammento `expand=…`/`anchor=…`/`align=…` è fuso nella STESSA spec
                 * di stile (unico canale verso il runtime C, che lo applica). */
                clauses.Add(new PixelClause("STYLE", new StringExpr(" " + ParseLayoutClause(p, key), t), null, null, KeyKind: TokenKind.PixelStyle));
            }
            else
            {
                var v = p.ParseExpression();
                clauses.Add(new PixelClause(key, v, null, null, KeyKind: keyKind));
            }
        }
        return new PixelControlStmt(kind, args, clauses, into, tok, id);
    }

    private Token? _lastEvTargetToken;

    /// <summary>Azione di un evento di controllo: `GOSUB label` / `GOTO label` / `STOP` /
    /// `CALL sub()` / `<sub>()` / `<sub>` (alias nudo). Restituisce la clausola ON.</summary>
    private PixelClause ParseControlEventClause(string ev)
    {
        if (Match(TokenKind.Goto)) { var t = Consume(TokenKind.Identifier, TokenKind.Number); _lastEvTargetToken = t; return new PixelClause("ON", null, ev, null, "GOTO", t.Lexeme); }
        if (Match(TokenKind.Gosub)) { var t = Consume(TokenKind.Identifier, TokenKind.Number); _lastEvTargetToken = t; return new PixelClause("ON", null, ev, null, "GOSUB", t.Lexeme); }
        if (Match(TokenKind.Stop)) return new PixelClause("ON", null, ev, null, "STOP", null);
        Match(TokenKind.Call);
        var nameTok = Peek();
        var name = nameTok.Lexeme ?? "";
        Advance();
        _lastEvTargetToken = nameTok;
        if (Peek().Kind == TokenKind.LeftParen)
        {
            int depth = 0;
            while (Peek().Kind != TokenKind.Newline && Peek().Kind != TokenKind.Eof)
            {
                var k = Peek().Kind;
                if (k == TokenKind.LeftParen) depth++;
                else if (k == TokenKind.RightParen) depth--;
                Advance();
                if (depth <= 0) break;
            }
        }
        return new PixelClause("ON", null, ev, name, "CALL", null);
    }

    /// <summary>FIRE EVENT &lt;handle&gt;, "&lt;EVENTO&gt;" — comando: innalza un evento controllo.</summary>
    public static Stmt ParseFireStandalone(Parser p, string? ns)
    {
        var tok = p.Peek();
        p.Advance();   // FIRE
        if (!string.Equals(p.Peek().Lexeme, "EVENT", StringComparison.OrdinalIgnoreCase))
            throw Parser.CreateError(p.Peek(), "Atteso EVENT dopo FIRE.");
        p.Advance();   // EVENT
        var handle = p.ParseExpression();
        p.Match(TokenKind.Comma);
        var ev = p.ParseExpression();
        return new FireEventStmt(handle, ev, tok);
    }

    /// <summary>CONTROL SET "n","prop",val[,str$] | CONTROL GET "n","prop" INTO v |
    /// CONTROL GETS ... INTO v$ | CONTROL MOVE "n",x,y[,w,h] | CONTROL FOCUS "n" |
    /// CONTROL ZORDER "n",front.</summary>
    public static Stmt ParseControlMethodStandalone(Parser p, string? ns)
    {
        var tok = p.Peek();          // CONTROL (Identifier)
        p.Advance();
        var subTok = p.Peek();
        var sub = (subTok.Lexeme ?? "").ToUpperInvariant();  // SET/GET/GETS/MOVE/FOCUS/ZORDER
        p.Advance();
        var subKind = subTok.Kind != TokenKind.Identifier ? subTok.Kind : sub switch
        {
            "GETS" => TokenKind.ControlGets,
            "ZORDER" => TokenKind.ControlZorder,
            "INVALIDATE" => TokenKind.ControlInvalidate,
            _ => TokenKind.Identifier
        };

        var args = new List<Expr>();
        Target? into = null;
        while (p.Peek().Kind != TokenKind.Newline && p.Peek().Kind != TokenKind.Eof)
        {
            if (p.Peek().Kind == TokenKind.Into) { p.Advance(); into = p.ParseTarget(); break; }
            /* CONTROL ADD instance AT x, y SIZE w, h [PARENT container]:
             * AT/SIZE/PARENT sono keyword (TokenKind) — separatori, non argomenti. */
            if (p.Peek().Kind is TokenKind.At or TokenKind.Size or TokenKind.Parent) { p.Advance(); continue; }
            args.Add(p.ParseExpression());
            if (p.Match(TokenKind.Comma)) continue;
            if (p.Peek().Kind is TokenKind.At or TokenKind.Size or TokenKind.Parent) continue;
            break;
        }
        if (into == null && p.Peek().Kind == TokenKind.Into) { p.Advance(); into = p.ParseTarget(); }
        return new ControlMethodStmt(sub, args, into, tok, subKind);
    }
}
