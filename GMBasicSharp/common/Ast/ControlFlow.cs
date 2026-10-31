// FILE: Interpreter.Ast.ControlFlow.cs
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Istruzione incondizionata che salta a un'etichetta.
/// </summary>
public sealed record JumpStmt(Token TargetLabel) : Stmt(TargetLabel.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"JUMP -> {TargetLabel.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo JumpStmt.
    /// </summary>
    /// <returns>Una nuova istanza di JumpStmt con una copia del suo token TargetLabel.</returns>
    public override AstNode DeepClone()
    {
        return new JumpStmt((Token)TargetLabel.DeepClone());
    }
}

/// <summary>
/// Istruzione che esegue un salto a un'etichetta se una condizione è (o non è) soddisfatta.
/// È la base per IF, WHILE, DO...LOOP e la valutazione logica con short-circuiting.
/// </summary>
public sealed record ConditionalJumpStmt(Expr Condition, Token InitialTargetLabel, bool JumpWhenTrue)
    : Stmt(Condition.Position)
{
    /// <summary>
    /// Ottiene o imposta l'etichetta di destinazione per il salto.
    /// Resa modificabile per permettere il "backpatching" durante il parsing
    /// delle espressioni logiche complesse.
    /// </summary>
    public Token TargetLabel { get; set; } = InitialTargetLabel;

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"IF {(JumpWhenTrue ? "" : "NOT ")}{Condition} JUMP -> {TargetLabel.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConditionalJumpStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConditionalJumpStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCondition = (Expr)Condition.DeepClone();
        var clonedInitialTargetLabel = (Token)InitialTargetLabel.DeepClone();
        var clonedTargetLabel = (Token)TargetLabel.DeepClone();
        return new ConditionalJumpStmt(clonedCondition, clonedInitialTargetLabel, JumpWhenTrue)
            { TargetLabel = clonedTargetLabel };
    }
}

/// <summary>
/// Rappresenta l'istruzione GOSUB.
/// </summary>
public sealed record GosubStmt(Token Target, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Target.Lexeme;

    /// <summary>
    /// Crea una copia profonda di questo nodo GosubStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GosubStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new GosubStmt((Token)Target.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione GOTO.
/// </summary>
public sealed record GotoStmt(Token Target, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Target.Lexeme;

    /// <summary>
    /// Crea una copia profonda di questo nodo GotoStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GotoStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new GotoStmt((Token)Target.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione RETURN.
/// </summary>
public sealed record ReturnStmt(Expr? Value, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Value?.ToString() ?? string.Empty;

    /// <summary>
    /// Crea una copia profonda di questo nodo ReturnStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ReturnStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedValue = (Expr?)Value?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ReturnStmt(clonedValue, clonedToken);
    }
}

/// <summary>
/// Definisce il tipo di salto per un'istruzione ON...
/// </summary>
public enum JumpType
{
    Goto,
    Gosub
}

/// <summary>
/// Rappresenta un'istruzione ON ... GOTO/GOSUB ottimizzata.
/// Incapsula l'intera logica di salto calcolato in un unico nodo AST
/// per una valutazione efficiente da parte dell'Executor.
/// </summary>
/// <param name="IndexExpression">L'espressione numerica che determina a quale etichetta saltare.</param>
/// <param name="Type">Il tipo di salto da eseguire (GOTO o GOSUB).</param>
/// <param name="TargetLabels">La lista immutabile delle etichette di destinazione.</param>
/// <param name="Token">Il token 'ON' per riferimento alla posizione.</param>
public sealed record OnJumpStmt(Expr IndexExpression, JumpType Type, ImmutableArray<Token> TargetLabels, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var jumpKeyword = Type == JumpType.Goto ? "GOTO" : "GOSUB";
        return $"ON {IndexExpression} {jumpKeyword} {string.Join(", ", TargetLabels.Select(t => t.Lexeme))}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo OnJumpStmt.
    /// </summary>
    /// <returns>Una nuova istanza di OnJumpStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedIndexExpression = (Expr)IndexExpression.DeepClone();
        var clonedTargetLabels = TargetLabels.Select(t => (Token)t.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new OnJumpStmt(clonedIndexExpression, Type, clonedTargetLabels, clonedToken);
    }
}

/// <summary>
/// Rappresenta un'istruzione ON ERROR GOTO o ON ERROR GOSUB.
/// </summary>
public sealed record OnErrorStmt(Token? Keyword, Token? Target, Expr? ProcedureCall, Token OnToken)
    : Stmt(OnToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => ProcedureCall != null ? $"ERROR {ProcedureCall}" : $"ERROR {Keyword?.Lexeme} {Target?.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo OnErrorStmt.
    /// </summary>
    /// <returns>Una nuova istanza di OnErrorStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new OnErrorStmt(
            (Token?)Keyword?.DeepClone(),
            (Token?)Target?.DeepClone(),
            (Expr?)ProcedureCall?.DeepClone(),
            (Token)OnToken.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione THROW senza argomenti, utilizzata esclusivamente all'interno di un blocco CATCH
/// per ri-sollevare l'eccezione corrente, preservando lo StackTrace e le informazioni originali dell'errore (TODO #74).
/// Questo comando permette di intercettare un'eccezione per operazioni locali (es. logging) e poi 
/// propagarla intatta verso gestori di livello superiore.
/// </summary>
/// <param name="Token">Il token 'THROW' o 'RETHROW' per il riferimento alla posizione nel codice sorgente.</param>
/// <summary>
/// THROW con espressione: <c>THROW expr</c> o <c>THROW NEW Type(args)</c>.
/// L'espressione viene valutata e lanciata come eccezione personalizzata.
/// </summary>
public sealed record ThrowStmt(Expr Value, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"THROW {Value}";

    public override AstNode DeepClone()
    {
        return new ThrowStmt((Expr)Value.DeepClone(), (Token)Token.DeepClone());
    }
}

public sealed record RethrowStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale dell'istruzione per scopi di debug e visualizzazione dell'AST.
    /// </summary>
    /// <returns>Una stringa che identifica il comando come un'operazione di rethrow.</returns>
    public override string ToString() => "THROW (RETHROW)";

    /// <summary>
    /// Crea una copia profonda (deep copy) di questo nodo AST, garantendo che la nuova istanza
    /// sia completamente indipendente dall'originale per manipolazioni sicure dell'albero.
    /// </summary>
    /// <returns>Una nuova istanza di <see cref="RethrowStmt"/> con una copia del token originale.</returns>
    public override AstNode DeepClone()
    {
        var clonedToken = (Token)Token.DeepClone();
        return new RethrowStmt(clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ON ERROR RESUME NEXT per la gestione degli errori in linea.
/// </summary>
public sealed record OnErrorResumeStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "ERROR RESUME NEXT";

    /// <summary>
    /// Crea una copia profonda di questo nodo OnErrorResumeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di OnErrorResumeStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new OnErrorResumeStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione RESUME per continuare l'esecuzione dopo un errore.
/// </summary>
public sealed record ResumeStmt(Token ResumeToken, Token? Target) : Stmt(ResumeToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Target?.Lexeme ?? "NEXT";

    /// <summary>
    /// Crea una copia profonda di questo nodo ResumeStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ResumeStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedResumeToken = (Token)ResumeToken.DeepClone();
        var clonedTarget = (Token?)Target?.DeepClone();
        return new ResumeStmt(clonedResumeToken, clonedTarget);
    }
}

// FILE: Interpreter.Ast.ControlFlow.cs (Modifica richiesta)

/// <summary>
/// Rappresenta una singola clausola CATCH all'interno di un blocco TRY.
/// </summary>
/// <param name="Variable">La dichiarazione opzionale della variabile che cattura l'eccezione.</param>
/// <param name="WhenCondition">La condizione opzionale del filtro WHEN.</param>
/// <param name="Body">La lista di istruzioni nel corpo del CATCH.</param>
/// <param name="Token">Il token 'CATCH' per riferimento.</param>
public sealed record CatchClause(
    VariableDeclaration? Variable,
    Expr? WhenCondition,
    ImmutableArray<Stmt> Body,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"CATCH {(Variable is not null ? Variable.NameSuffixed.Lexeme : "")}{(WhenCondition is not null ? $" WHEN {WhenCondition}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CatchClause.
    /// </summary>
    /// <returns>Una nuova istanza di CatchClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedVariable = (VariableDeclaration?)Variable?.DeepClone();
        var clonedWhenCondition = (Expr?)WhenCondition?.DeepClone();
        var clonedBody = Body.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new CatchClause(clonedVariable, clonedWhenCondition, clonedBody, clonedToken);
    }
}

/// <summary>
/// Rappresenta la clausola FINALLY all'interno di un blocco TRY.
/// </summary>
/// <param name="Body">La lista di istruzioni nel corpo del FINALLY.</param>
/// <param name="Token">Il token 'FINALLY' per riferimento.</param>
public sealed record FinallyClause(ImmutableArray<Stmt> Body, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "FINALLY";

    /// <summary>
    /// Crea una copia profonda di questo nodo FinallyClause.
    /// </summary>
    /// <returns>Una nuova istanza di FinallyClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedBody = Body.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new FinallyClause(clonedBody, clonedToken);
    }
}

/// <summary>
/// Rappresenta un'intera struttura TRY...CATCH...FINALLY...END TRY.
/// </summary>
/// <param name="TryBody">Le istruzioni nel corpo del TRY.</param>
/// <param name="CatchClauses">La lista delle clausole CATCH.</param>
/// <param name="Finally">La clausola FINALLY opzionale.</param>
/// <param name="Token">Il token 'TRY' per riferimento.</param>
public sealed record TryCatchStmt(
    ImmutableArray<Stmt> TryBody,
    ImmutableArray<CatchClause> CatchClauses,
    FinallyClause? Finally,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "TRY ...";

    /// <summary>
    /// Crea una copia profonda di questo nodo TryCatchStmt.
    /// </summary>
    /// <returns>Una nuova istanza di TryCatchStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedTryBody = TryBody.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedCatchClauses = CatchClauses.Select(c => (CatchClause)c.DeepClone()).ToImmutableArray();
        var clonedFinally = (FinallyClause?)Finally?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new TryCatchStmt(clonedTryBody, clonedCatchClauses, clonedFinally, clonedToken);
    }
}

/// <summary>
/// Rappresenta l'istruzione ERROR per simulare un errore di runtime,
/// ora con una descrizione opzionale.
/// </summary>
public sealed record ErrorStmt(Expr ErrorCode, Expr? Description, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ERROR {ErrorCode}{(Description is not null ? $", {Description}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ErrorStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ErrorStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedErrorCode = (Expr)ErrorCode.DeepClone();
        var clonedDescription = (Expr?)Description?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new ErrorStmt(clonedErrorCode, clonedDescription, clonedToken);
    }
}

/// <summary>
/// Classe base astratta per l'azione da eseguire in un'istruzione ON KEY.
/// </summary>
[JsonDerivedType(typeof(GotoAction), nameof(GotoAction))]
[JsonDerivedType(typeof(GosubAction), nameof(GosubAction))]
[JsonDerivedType(typeof(StopAction), nameof(StopAction))]
public abstract record OnKeyAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione ON KEY.
    /// </summary>
    /// <returns>Una nuova istanza di OnKeyAction che è una copia completa dell'originale.</returns>
    public abstract OnKeyAction DeepClone();
}

/// <summary>
/// Rappresenta l'azione CALL, con i dettagli della chiamata alla procedura.
/// Utilizza Expr per rappresentare la chiamata, che verrà validata come FunctionExpr
/// durante la fase di riscrittura.
/// </summary>
public sealed record CallAction(Expr Call) : OnKeyAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione CallAction.
    /// </summary>
    /// <returns>Una nuova istanza di CallAction con una copia della sua espressione di chiamata figlia.</returns>
    public override OnKeyAction DeepClone()
    {
        return new CallAction((Expr)Call.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione GOTO, con la sua etichetta di destinazione.
/// </summary>
public sealed record GotoAction(Token TargetLabel) : OnKeyAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione GotoAction.
    /// </summary>
    /// <returns>Una nuova istanza di GotoAction con una copia del suo token TargetLabel.</returns>
    public override OnKeyAction DeepClone()
    {
        return new GotoAction((Token)TargetLabel.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione GOSUB, con la sua etichetta di destinazione.
/// </summary>
public sealed record GosubAction(Token TargetLabel) : OnKeyAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione GosubAction.
    /// </summary>
    /// <returns>Una nuova istanza di GosubAction con una copia del suo token TargetLabel.</returns>
    public override OnKeyAction DeepClone()
    {
        return new GosubAction((Token)TargetLabel.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'azione STOP, che disabilita l'hook per un tasto.
/// </summary>
public sealed record StopAction : OnKeyAction
{
    /// <summary>
    /// Crea una copia profonda di questa azione StopAction.
    /// </summary>
    /// <returns>Una nuova istanza di StopAction.</returns>
    public override OnKeyAction DeepClone()
    {
        return this with { }; // StopAction non ha campi, quindi una shallow copy è sufficiente e safe.
    }
}

/// <summary>
/// Rappresenta l'istruzione ON KEY... per registrare un hook da tastiera.
/// </summary>
/// <summary>
/// ON SPRITE COLLISION <handler> / ON SPRITE COLLISIONS <handler>: registra un handler
/// chiamato dal motore quando due sprite collidono (SYNC). Il singolare riceve (x, y)
/// della collisione; il plurale riceve l'array degli id degli sprite collidenti.
/// </summary>
public sealed record OnSpriteCollisionStmt(string HandlerName, bool IsPlural, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => IsPlural
        ? $"ON SPRITE COLLISIONS {HandlerName}"
        : $"ON SPRITE COLLISION {HandlerName}";

    public override AstNode DeepClone() => new OnSpriteCollisionStmt(HandlerName, IsPlural, (Token)Token.DeepClone());
}
public sealed record OnKeyStmt(Expr? KeyExpression, OnKeyAction Action, Token OnToken)
    : Stmt(OnToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var keyExprStr = KeyExpression?.ToString() ?? "ANY";
        var actionStr = Action switch
        {
            GotoAction g => $"GOTO {g.TargetLabel.Lexeme}",
            GosubAction g => $"GOSUB {g.TargetLabel.Lexeme}",
            StopAction => "STOP",
            _ => "(unknown action)"
        };
        return $"ON KEY ({keyExprStr}) {actionStr}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo OnKeyStmt.
    /// </summary>
    /// <returns>Una nuova istanza di OnKeyStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedKeyExpression = (Expr?)KeyExpression?.DeepClone();
        var clonedAction = Action.DeepClone();
        var clonedOnToken = (Token)OnToken.DeepClone();
        return new OnKeyStmt(clonedKeyExpression, clonedAction, clonedOnToken);
    }
}