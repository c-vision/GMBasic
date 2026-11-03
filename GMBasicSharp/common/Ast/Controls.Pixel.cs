// FILE: Ast/Controls.Pixel.cs
// Controlli PIXEL (grafica) + metodo CONTROL, per le gambe C# (IL/PCode/VM-C).
// Il front-end li parsea per NOME/LESSEMA (NamedCommandRegistry), senza TokenKind dedicati.

namespace Basic;

/// <summary>Una clausola di un controllo pixel:
/// NAME/TAG/TOOLTIP/VISIBLE/ENABLED/TABINDEX/TABSTOP/FOCUS/STYLE (Value) oppure
/// ON &lt;EVENTO&gt; &lt;azione&gt; (EventName + ActionKind/SubName/LabelName).
/// ActionKind: "CALL" (SubName = sub/call target), "GOSUB"/"GOTO" (LabelName = label),
/// "STOP" (deregistra). Default "CALL" preserva la semantica storica (bare sub).</summary>
public sealed record PixelClause(
    string Key, Expr? Value, string? EventName, string? SubName,
    string ActionKind = "CALL", string? LabelName = null);

/// <summary>Controllo pixel: CHECK/RADIO/TRACK/DROPDOWN/PROGRESS/TOGGLE/SPINNER/TAB/
/// LISTBOX/COLORPICKER/GRID/TREEVIEW/STATUSBAR/DATEPICKER/FILEDIALOG/MENU/TBROWSE/
/// BUTTON/TEXTBOX. Args = argomenti posizionali; Into = variabile target (opzionale).</summary>
[JsonDerivedType(typeof(PixelControlStmt), nameof(PixelControlStmt))]
public sealed record PixelControlStmt(
    string Kind, List<Expr> Args, List<PixelClause> Clauses, Target? Into, Token Token, Target? Id = null) : Stmt(Token.Position)
{
    public override string ToString() => $"{Kind} [{string.Join(", ", Args)}]";
    public override AstNode DeepClone() => new PixelControlStmt(
        Kind,
        Args.Select(a => (Expr)a.DeepClone()).ToList(),
        Clauses.Select(c => new PixelClause(c.Key, (Expr?)c.Value?.DeepClone(), c.EventName, c.SubName, c.ActionKind, c.LabelName)).ToList(),
        (Target?)Into?.DeepClone(),
        (Token)Token.DeepClone(),
        (Target?)Id?.DeepClone());
}

/// <summary>Evento controllo raggruppato: `ON <EVENTO> CONTROL <id> <azione>`.
/// Registra (o rimuove con STOP) un gestore su un controllo per handle. Sovrascrive l'inline.</summary>
[JsonDerivedType(typeof(ControlEventStmt), nameof(ControlEventStmt))]
public sealed record ControlEventStmt(string Event, Expr Id, string ActionKind, string? Target, Token Token, Token? TargetToken = null) : Stmt(Token.Position)
{
    public override string ToString() => $"ON {Event} CONTROL {Id} {ActionKind} {Target}";
    public override AstNode DeepClone() => new ControlEventStmt(
        Event, (Expr)Id.DeepClone(), ActionKind, Target, (Token)Token.DeepClone(), TargetToken);
}

/// <summary>Comando `FIRE EVENT <handle>, "<EVENTO>"`: innalza programmaticamente un evento
/// controllo (due token, niente parentesi: e' un COMANDO).</summary>
[JsonDerivedType(typeof(FireEventStmt), nameof(FireEventStmt))]
public sealed record FireEventStmt(Expr Handle, Expr Event, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"FIRE EVENT {Handle}, {Event}";
    public override AstNode DeepClone() => new FireEventStmt((Expr)Handle.DeepClone(), (Expr)Event.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Metodo a runtime per nome: CONTROL SET/GET/GETS/MOVE/FOCUS/ZORDER.
/// Args = argomenti; Into = variabile target (GET/GETS).</summary>
[JsonDerivedType(typeof(ControlMethodStmt), nameof(ControlMethodStmt))]
public sealed record ControlMethodStmt(
    string Sub, List<Expr> Args, Target? Into, Token Token,
    TokenKind SubKind = TokenKind.Identifier) : Stmt(Token.Position)
{
    public override string ToString() => $"CONTROL {Sub} [{string.Join(", ", Args)}]";

    /// <summary>TokenKind del sottocomando: quello risolto dal parser, con fallback sulla
    /// grafia per le AST costruite dal linker (SubKind = Identifier).</summary>
    public TokenKind ResolveSubKind() => SubKind != TokenKind.Identifier
        ? SubKind
        : Sub.ToUpperInvariant() switch
        {
            "SET" => TokenKind.Set,
            "GET" => TokenKind.Get,
            "GETS" => TokenKind.ControlGets,
            "MOVE" => TokenKind.Move,
            "FOCUS" => TokenKind.Focus,
            "INVALIDATE" => TokenKind.ControlInvalidate,
            "REFRESH" => TokenKind.Refresh,
            "ZORDER" => TokenKind.ControlZorder,
            "ADD" => TokenKind.Add,
            "REMOVE" => TokenKind.Remove,
            "DRAW" => TokenKind.Draw,
            _ => TokenKind.Identifier
        };

    public override AstNode DeepClone() => new ControlMethodStmt(
        Sub,
        Args.Select(a => (Expr)a.DeepClone()).ToList(),
        (Target?)Into?.DeepClone(),
        (Token)Token.DeepClone(),
        SubKind);
}
