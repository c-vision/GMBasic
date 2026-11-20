// FILE: Interpreter.Ast.World3D.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Comando del dominio <c>WORLD</c> (motore 3D): <c>WORLD LOAD|CLOSE|TICK|SECMOVE|HEIGHT|ANIM|SCROLL|LIGHTFX|LAMP</c>.
/// <para>
/// Il verbo è il <see cref="TokenKind"/> del sotto-comando (<c>Load</c>, <c>SecMove</c>, …): è la
/// chiave di dispatch in tutti i backend, mai il lessema (§12 — TokenKind canonico). Gli
/// argomenti sono le espressioni che seguono il verbo, separate da virgola, nell'ordine della
/// specifica §2.4.
/// </para>
/// </summary>
/// <param name="Command">Token del verbo, con il Kind specifico del sotto-comando.</param>
/// <param name="Arguments">Argomenti del verbo, nell'ordine documentato.</param>
/// <param name="Token">Token iniziale (<c>WORLD</c>) per posizione e diagnostica.</param>
public sealed record WorldCommandStmt(Token Command, ImmutableArray<Expr> Arguments, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>ID del simbolo del comando risolto nella SymbolTable (namespace WORLD).</summary>
    public int CommandSymbolId { get; init; } = -1;

    public override string ToString() => $"WORLD {Command.Lexeme} {string.Join(", ", Arguments)}";

    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new WorldCommandStmt((Token)Command.DeepClone(), clonedArguments, (Token)Token.DeepClone())
        {
            CommandSymbolId = CommandSymbolId,
            ScopeId = ScopeId
        };
    }
}
