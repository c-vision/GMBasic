// FILE: Interpreter.Ast.Clock.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Comando del dominio <c>CLOCK</c> (tempo a passo fisso):
/// <c>CLOCK PAUSE | RESUME | SCALE secondi | FPS frequenza</c>.
/// <para>
/// Il verbo è il <see cref="TokenKind"/> del sotto-comando (<c>Pause</c>, <c>Resume</c>,
/// <c>Scale</c>, <c>Fps</c>): è la chiave di dispatch in tutti i backend, mai il lessema
/// (§12). <c>FPS</c> non è nel lessico — è una soft keyword che il parser normalizza nel
/// TokenKind dedicato una volta sola.
/// </para>
/// </summary>
/// <param name="Command">Token del verbo, con il Kind specifico del sotto-comando.</param>
/// <param name="Arguments">Argomenti del verbo (nessuno, oppure il valore da impostare).</param>
/// <param name="Token">Token iniziale (<c>CLOCK</c>) per posizione e diagnostica.</param>
public sealed record ClockCommandStmt(Token Command, ImmutableArray<Expr> Arguments, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>ID del simbolo del comando risolto nella SymbolTable (namespace CLOCK).</summary>
    public int CommandSymbolId { get; init; } = -1;

    public override string ToString() => $"CLOCK {Command.Lexeme} {string.Join(", ", Arguments)}";

    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new ClockCommandStmt((Token)Command.DeepClone(), clonedArguments, (Token)Token.DeepClone())
        {
            CommandSymbolId = CommandSymbolId,
            ScopeId = ScopeId
        };
    }
}
