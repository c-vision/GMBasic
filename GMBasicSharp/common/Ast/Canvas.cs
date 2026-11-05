// FILE: Interpreter.Ast.Canvas.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Comando <c>CANVAS</c>: canvas offscreen RGBA8.
///
/// <code>
/// CANVAS NEW w, h INTO c          CANVAS USE c
/// CANVAS CLEAR c [, col]          CANVAS SET c, x, y, col
/// CANVAS GET c, x, y INTO col     CANVAS BLIT c [, dx, dy]
/// CANVAS SAVE c, "file.png"       CANVAS LOAD "file.png" INTO c
/// CANVAS WIDTH c INTO w           CANVAS HEIGHT c INTO h
/// CANVAS END                      CANVAS FREE c
/// </code>
///
/// La forma CONTROLS <c>CANVAS x, y, w, h [NAME …]</c> NON arriva qui: il dispatcher la
/// rimanda a <c>ParsePixelControlStandalone</c> (stessa disambiguazione di CHART).
/// </summary>
public sealed record CanvasStmt(string Sub, ImmutableArray<Expr> Args, Target? Into, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CANVAS {Sub} {string.Join(", ", Args)}";
    public override AstNode DeepClone() => new CanvasStmt(Sub,
        [.. Args.Select(a => (Expr)a.DeepClone())],
        (Target?)Into?.DeepClone(), (Token)Token.DeepClone());
}
