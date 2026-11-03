// FILE: Interpreter.Ast.Plotting.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

// ========================================================================
// --- NUOVI NODI STMT PER IL SOTTOSISTEMA DI PLOTTING ---
// ========================================================================
/// <summary>
/// Rappresenta il comando CHART.PLOT.
/// </summary>
public sealed record ChartPlotStmt(ImmutableArray<Expr> Args, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.PLOT {string.Join(", ", Args)}";
    public override AstNode DeepClone() => new ChartPlotStmt([..Args.Select(a => (Expr)a.DeepClone())],
        (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta il comando CHART.FINANCIAL.
/// </summary>
public sealed record ChartFinancialStmt(ImmutableArray<Expr> Args, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.FINANCIAL {string.Join(", ", Args)}";
    public override AstNode DeepClone() =>
        new ChartFinancialStmt([..Args.Select(a => (Expr)a.DeepClone())], (Token)Token.DeepClone());
}

/// <summary>CHART SET handle, "prop", num [, "str"] | CHART GET handle, "prop" INTO var |
/// CHART GETS handle, "prop" INTO var$. Configurazione del grafico da BASIC (rendering,
/// view window per zoom/pan, colori, titolo).</summary>
[JsonDerivedType(typeof(ChartConfigStmt), nameof(ChartConfigStmt))]
public sealed record ChartConfigStmt(Expr Handle, string Sub, Expr Prop, Expr? Num, Expr? Str, Target? Into, Token Token,
    TokenKind SubKind = TokenKind.Identifier)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.{Sub} {Handle}, {Prop}";
    public override AstNode DeepClone() => new ChartConfigStmt(
        (Expr)Handle.DeepClone(), Sub, (Expr)Prop.DeepClone(), (Expr?)Num?.DeepClone(),
        (Expr?)Str?.DeepClone(), (Target?)Into?.DeepClone(), (Token)Token.DeepClone(), SubKind);
}

/// <summary>CHART DRAW handle, "op"[, args...] [INTO id] — oggetti di disegno (trendline,
/// orizzontale, verticale, rettangolo, Fibonacci) + move/remove/clear/count/hit.
/// args in coordinate DATI. INTO riceve l'id del nuovo oggetto (o count/hit).</summary>
[JsonDerivedType(typeof(ChartDrawStmt), nameof(ChartDrawStmt))]
public sealed record ChartDrawStmt(Expr Handle, Expr Op, ImmutableArray<Expr> Args, Target? Into, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.DRAW {Handle}, {Op}";
    public override AstNode DeepClone() => new ChartDrawStmt(
        (Expr)Handle.DeepClone(), (Expr)Op.DeepClone(),
        [..Args.Select(a => (Expr)a.DeepClone())], (Target?)Into?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta il comando CHART.CLEAR.
/// </summary>
public sealed record ChartClearStmt(Expr Handle, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.CLEAR {Handle}";
    public override AstNode DeepClone() => new ChartClearStmt((Expr)Handle.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta il comando CHART.CLOSE.
/// </summary>
public sealed record ChartCloseStmt(Expr Handle, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.CLOSE {Handle}";
    public override AstNode DeepClone() => new ChartCloseStmt((Expr)Handle.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta i comandi di rendering CHART.SAVE e CHART.SHOW.
/// </summary>
public sealed record ChartRenderStmt(bool IsSave, ImmutableArray<Expr> Args, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CHART.{(IsSave ? "SAVE" : "SHOW")} {string.Join(", ", Args)}";
    public override AstNode DeepClone() => new ChartRenderStmt(IsSave,
        [..Args.Select(a => (Expr)a.DeepClone())], (Token)Token.DeepClone());
}