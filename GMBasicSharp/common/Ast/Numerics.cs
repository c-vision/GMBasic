// FILE: Interpreter.Ast.Numerics.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Rappresenta un comando generico relativo alle matrici.
/// </summary>
/// <param name="Command">Il token del comando con il Kind specifico (es. CreateMatrix, TranslateMatrix).</param>
/// <param name="Arguments">La lista di argomenti che seguono il comando.</param>
public sealed record MatrixCommandStmt(Token Command, ImmutableArray<Expr> Arguments, Token Token) 
    : Stmt(Token.Position)
{
    /// <summary>
    /// ID del simbolo del comando risolto nella SymbolTable.
    /// </summary>
    public int CommandSymbolId { get; init; } = -1;

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Command.Lexeme} MATRIX {string.Join(", ", Arguments)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo MatrixCommandStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new MatrixCommandStmt((Token)Command.DeepClone(), clonedArguments, (Token)Token.DeepClone()) 
        { 
            CommandSymbolId = this.CommandSymbolId,
            ScopeId = this.ScopeId 
        };
    }
}

/// <summary>
/// Rappresenta un comando vettoriale generico o un comando grafico ad alte prestazioni.
/// </summary>
public sealed record VectorCommandStmt(Token Command, ImmutableArray<Expr> Arguments)
    : Stmt(Command.Position)
{
    /// <summary>
    /// ID del simbolo del comando risolto nella SymbolTable.
    /// </summary>
    public int CommandSymbolId { get; init; } = -1;

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Command.Lexeme} {string.Join(", ", Arguments)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo VectorCommandStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedArguments = Arguments.Select(a => (Expr)a.DeepClone()).ToImmutableArray();
        return new VectorCommandStmt((Token)Command.DeepClone(), clonedArguments) 
        { 
            CommandSymbolId = this.CommandSymbolId,
            ScopeId = this.ScopeId 
        };
    }
}
