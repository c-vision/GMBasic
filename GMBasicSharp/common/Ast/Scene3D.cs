// FILE: Interpreter.Ast.Scene3D.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Comando del dominio <c>SCENE</c> (motore 3D, renderer mesh):
/// <c>SCENE RESET|VIEW|TEXTURE|OBJECT|VERTEX|QUAD|BOX|OBJPOS|CAMERA|RENDER|SAVE</c>.
/// <para>
/// Il verbo è il <see cref="TokenKind"/> dedicato del sotto-comando (<c>SceneReset</c>,
/// <c>SceneView</c>, …): è la chiave di dispatch in tutti i backend, mai il lessema (§12 — il
/// parser normalizza il lessema una volta sola, come per <c>CLOCK FPS</c>). Gli argomenti sono le
/// espressioni che seguono il verbo, separate da virgola, nell'ordine della facciata
/// <c>gmb_scene_*</c>.
/// </para>
/// </summary>
/// <param name="Command">Token del verbo, con il Kind specifico del sotto-comando.</param>
/// <param name="Arguments">Argomenti del verbo, nell'ordine documentato.</param>
/// <param name="Token">Token iniziale (<c>SCENE</c>) per posizione e diagnostica.</param>
public sealed record SceneCommandStmt(Token Command, ImmutableArray<Expr> Arguments, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>ID del simbolo del comando risolto nella SymbolTable (namespace SCENE).</summary>
    public int CommandSymbolId { get; init; } = -1;

    public override string ToString() => $"SCENE {Command.Lexeme} {string.Join(", ", Arguments)}";

    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new SceneCommandStmt((Token)Command.DeepClone(), clonedArguments, (Token)Token.DeepClone())
        {
            CommandSymbolId = CommandSymbolId,
            ScopeId = ScopeId
        };
    }
}
