// FILE: Interpreter.Ast.Arrays.cs

// ReSharper disable InvalidXmlDocComment
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming
namespace Basic;

/// <summary>
/// Rappresenta l'istruzione AADD(array, valore) per aggiungere un elemento a un array.
/// </summary>
public sealed record AAddStmt(Target Array, Expr Value, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"AADD({Array}, {Value})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AAddStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AAddStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedValue = (Expr)Value.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new AAddStmt(clonedArray, clonedValue, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione AINS(array, posizione) per inserire un elemento vuoto.
/// </summary>
/// <param name="Array">L'array di destinazione.</param>
/// <param name="PositionExpr">L'espressione che indica la posizione in cui inserire.</param>
/// <param name="Token">Il token 'AINS' per riferimento.</param>
public sealed record AInsStmt(Target Array, Expr PositionExpr, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"AINS({Array}, {PositionExpr})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AInsStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AInsStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedPositionExpr = (Expr)PositionExpr.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new AInsStmt(clonedArray, clonedPositionExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ADEL(array, posizione) per eliminare un elemento.
/// </summary>
/// <param name="Array">L'array di destinazione.</param>
/// <param name="PositionExpr">L'espressione che indica la posizione da eliminare.</param>
/// <param name="Token">Il token 'ADEL' per riferimento.</param>
public sealed record ADelStmt(Target Array, Expr PositionExpr, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ADEL({Array}, {PositionExpr})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ADelStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ADelStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedPositionExpr = (Expr)PositionExpr.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ADelStmt(clonedArray, clonedPositionExpr, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ASORT(array, ...).
/// </summary>
public sealed record ASortStmt(Target Array, Expr? CompareFunction, Expr? Order, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ASORT({Array})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ASortStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ASortStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedCompareFunction = (Expr?)CompareFunction?.DeepClone();
        var clonedOrder = (Expr?)Order?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ASortStmt(clonedArray, clonedCompareFunction, clonedOrder, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione AEVAL(array, udf_azione$) per eseguire un'azione su ogni elemento.
/// </summary>
public sealed record AEvalStmt(Target Array, Expr ActionFunction, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"AEVAL({Array}, {ActionFunction})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AEvalStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AEvalStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedActionFunction = (Expr)ActionFunction.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new AEvalStmt(clonedArray, clonedActionFunction, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ASHUFFLE array (muta l'array originale in ordine casuale).
/// </summary>
public sealed record AShuffleStmt(Target Array, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"ASHUFFLE({Array})";

    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new AShuffleStmt(clonedArray, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione AFILL(array, valore [, start] [, count]).
/// </summary>
public sealed record AFillStmt(Target Array, Expr Value, Expr? Start, Expr? Count, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"AFILL({Array}, {Value})";

    /// <summary>
    /// Crea una copia profonda di questo nodo AFillStmt.
    /// </summary>
    /// <returns>Una nuova istanza di AFillStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedArray = (Target)Array.DeepClone();
        var clonedValue = (Expr)Value.DeepClone();
        var clonedStart = (Expr?)Start?.DeepClone();
        var clonedCount = (Expr?)Count?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new AFillStmt(clonedArray, clonedValue, clonedStart, clonedCount, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ACOPY(sorgente, destinazione [, start_sorg] [, count] [, start_dest]).
/// </summary>
public sealed record ACopyStmt(
    Target Source,
    Target Destination,
    Expr? SourceStart,
    Expr? Count,
    Expr? DestStart,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ACOPY({Source}, {Destination})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ACopyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ACopyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSource = (Target)Source.DeepClone();
        var clonedDestination = (Target)Destination.DeepClone();
        var clonedSourceStart = (Expr?)SourceStart?.DeepClone();
        var clonedCount = (Expr?)Count?.DeepClone();
        var clonedDestStart = (Expr?)DestStart?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ACopyStmt(clonedSource, clonedDestination, clonedSourceStart, clonedCount, clonedDestStart,
            clonedToken);
    }
}