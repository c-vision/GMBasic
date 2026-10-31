// FILE: Interpreter.Ast.Loops.cs

// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

/// <summary>
/// Istruzione di setup per un ciclo FOR...NEXT.
/// </summary>
/// <param name="Variable">La variabile contatore, rappresentata come destinazione di assegnazione (Target).</param>
/// <param name="Start">L'espressione per il valore iniziale del contatore.</param>
/// <param name="End">L'espressione per il valore finale del contatore.</param>
/// <param name="Step">L'espressione opzionale per il passo di incremento/decremento.</param>
public sealed record ForStmt(VariableTarget Variable, Expr Start, Expr End, Expr? Step) : Stmt(Variable.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"FOR {Variable} = {Start} TO {End}{(Step is not null ? $" STEP {Step}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ForStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ForStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedVariable = (VariableTarget)Variable.DeepClone();
        var clonedStart = (Expr)Start.DeepClone();
        var clonedEnd = (Expr)End.DeepClone();
        var clonedStep = (Expr?)Step?.DeepClone();
        return new ForStmt(clonedVariable, clonedStart, clonedEnd, clonedStep);
    }
}

/// <summary>
/// Istruzione che verifica la condizione di un ciclo FOR e salta alla fine se terminato.
/// </summary>
public sealed record ForCheckAndJumpStmt(Token Variable, Token ExitLabel) : Stmt(Variable.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CHECK {Variable.Lexeme} -> {ExitLabel.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ForCheckAndJumpStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ForCheckAndJumpStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedVariable = (Token)Variable.DeepClone();
        var clonedExitLabel = (Token)ExitLabel.DeepClone();
        return new ForCheckAndJumpStmt(clonedVariable, clonedExitLabel);
    }
}

/// <summary>
/// Istruzione NEXT che incrementa la variabile contatore di un ciclo FOR.
/// </summary>
public sealed record NextStmt(Token Variable) : Stmt(Variable.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"NEXT {Variable.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo NextStmt.
    /// </summary>
    /// <returns>Una nuova istanza di NextStmt con una copia del suo token Variable.</returns>
    public override AstNode DeepClone()
    {
        return new NextStmt((Token)Variable.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione `FOR EACH ... IN ... NEXT`.
/// Questo è il nodo AST di alto livello che verrà successivamente "appiattito"
/// dall'ottimizzatore in istruzioni a basso livello per la VM.
/// </summary>
/// <param name="IteratorVariables">La lista di una o due variabili di iterazione (es. 'elemento' o 'chiave, valore').</param>
/// <param name="DeclaredType">Il tipo di dato opzionale dichiarato con la clausola AS.</param>
/// <param name="Collection">L'espressione che restituisce la collezione su cui iterare.</param>
/// <param name="Body">Le istruzioni che costituiscono il corpo del ciclo.</param>
/// <param name="ForToken">Il token 'FOR' iniziale, per riferimento alla posizione.</param>
public sealed record ForEachStmt(
    ImmutableArray<Token> IteratorVariables,
    DataType? DeclaredType,
    Expr Collection,
    ImmutableArray<Stmt> Body,
    Token ForToken
) : Stmt(ForToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var vars = string.Join(", ", IteratorVariables.Select(v => v.Lexeme));
        return $"FOR EACH {vars} IN {Collection}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo ForEachStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ForEachStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ForEachStmt(
            [.. IteratorVariables.Select(t => (Token)t.DeepClone())],
            DeclaredType?.DeepClone() as DataType,
            (Expr)Collection.DeepClone(),
            [.. Body.Select(s => (Stmt)s.DeepClone())],
            (Token)ForToken.DeepClone()
        );
    }
}

/// <summary>
/// Istruzione di inizializzazione per un ciclo FOR EACH.
/// Crea e prepara l'enumeratore a partire da <see cref="CollectionExpr"/>.
/// Supporta una o più variabili di iterazione e (opzionalmente) una dichiarazione di tipo (AS ...).
/// 
/// Semantica attesa (fase esecuzione / lowering):
///   enumerator = GetEnumerator(CollectionExpr);
///   // eventuali controlli di compatibilità con DeclaredType.
/// 
/// NOTE:
/// - Le variabili sono token grezzi: il binding simbolico avverrà in fasi successive.
/// - La posizione AST del nodo è quella del token 'FOR', per coerenza con ForStmt.
/// </summary>
public sealed record ForEachInitStmt(
    Token ForToken,
    ImmutableArray<Token> Variables,
    DataType? DeclaredType,
    Expr CollectionExpr
) : Stmt(ForToken.Position)
{
    /// <summary>
    /// Crea una copia profonda del nodo.
    /// I Token (immutabili) sono riutilizzati; l'espressione di collezione viene clonata.
    /// Il tipo dichiarato (se immutabile) viene riutilizzato; se non lo fosse, aggiungere un clone esplicito.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedExpr = (Expr)CollectionExpr.DeepClone();
        // Se DataType richiede clonazione profonda: var clonedType = (DataType?)DeclaredType?.DeepClone();
        return new ForEachInitStmt(ForToken, Variables, DeclaredType, clonedExpr);
    }

    /// <summary>
    /// Rappresentazione compatta: FOR EACH INIT (var1, var2 AS Tipo) IN Expr
    /// </summary>
    public override string ToString()
    {
        var vars = Variables.IsDefaultOrEmpty
            ? "<none>"
            : string.Join(", ", Variables.Select(v => v.Lexeme));
        var typeStr = DeclaredType is null ? "" : $" AS {DeclaredType}";
        return $"FOR EACH INIT ({vars}{typeStr}) IN {CollectionExpr}";
    }
}

/// <summary>
/// Istruzione di avanzamento e assegnazione per ciclo FOR EACH.
/// Semantica attesa:
///   if (!enumerator.MoveNext()) goto ExitLabel;
///   // altrimenti: esegui mapping di enumerator.Current su tutte le variabili in <see cref="Variables"/>.
/// 
/// NOTE:
/// - Non contiene espressioni figlie, solo metainformazione su variabili e label di uscita.
/// - La posizione AST si riferisce a <see cref="ExitLabel"/> (etichetta generata dal parser).
/// </summary>
public sealed record ForEachAdvanceAndAssignStmt(
    ImmutableArray<Token> Variables,
    Token ExitLabel
) : Stmt(ExitLabel.Position)
{
    /// <summary>
    /// Clonazione profonda: solo riproduzione dei token (immutabili). Nessuna Expr da clonare.
    /// </summary>
    public override AstNode DeepClone()
        => new ForEachAdvanceAndAssignStmt(Variables, ExitLabel);

    /// <summary>
    /// Rappresentazione compatta: FOREACH ADVANCE+ASSIGN (var1, var2) OR JUMP ExitLabel
    /// </summary>
    public override string ToString()
    {
        var vars = Variables.IsDefaultOrEmpty
            ? "<none>"
            : string.Join(", ", Variables.Select(v => v.Lexeme));
        return $"FOREACH ADVANCE+ASSIGN ({vars}) OR JUMP {ExitLabel.Lexeme}";
    }
}

/// <summary>
/// Rappresenta l'istruzione CONTINUE per passare all'iterazione successiva di un ciclo.
/// </summary>
public sealed record ContinueStmt(Token Token, Token TargetLabel)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CONTINUE -> {TargetLabel.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ContinueStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ContinueStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedToken = (Token)Token.DeepClone();
        var clonedTargetLabel = (Token)TargetLabel.DeepClone();
        return new ContinueStmt(clonedToken, clonedTargetLabel);
    }
}

/// <summary>
/// Rappresenta l'istruzione EXIT per uscire da un ciclo o da una procedura.
/// </summary>
public sealed record ExitStmt(Token ExitToken, Token TargetLoop, Token? TargetLabel)
    : Stmt(ExitToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"EXIT {TargetLoop.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ExitStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ExitStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedExitToken = (Token)ExitToken.DeepClone();
        var clonedTargetLoop = (Token)TargetLoop.DeepClone();
        var clonedTargetLabel = (Token?)TargetLabel?.DeepClone();
        return new ExitStmt(clonedExitToken, clonedTargetLoop, clonedTargetLabel);
    }
}

/// <summary>
/// Istruzione che entra in un contesto di ciclo.
/// </summary>
public sealed record EnterLoopStmt(Token StartToken, Token ContinueLabel, Token ExitLabel)
    : Stmt(StartToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ENTER {StartToken.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo EnterLoopStmt.
    /// </summary>
    /// <returns>Una nuova istanza di EnterLoopStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedStartToken = (Token)StartToken.DeepClone();
        var clonedContinueLabel = (Token)ContinueLabel.DeepClone();
        var clonedExitLabel = (Token)ExitLabel.DeepClone();
        return new EnterLoopStmt(clonedStartToken, clonedContinueLabel, clonedExitLabel);
    }
}

/// <summary>
/// Istruzione che esce da un contesto di ciclo.
/// </summary>
public sealed record ExitLoopStmt(Token EndToken)
    : Stmt(EndToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"EXIT LOOP ({EndToken.Lexeme})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ExitLoopStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ExitLoopStmt con una copia del suo token EndToken.</returns>
    public override AstNode DeepClone()
    {
        return new ExitLoopStmt((Token)EndToken.DeepClone());
    }
}