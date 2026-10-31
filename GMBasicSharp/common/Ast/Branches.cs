// FILE: Interpreter.Ast.Branches.cs

namespace Basic;

/// <summary>
/// Classe base astratta per una singola condizione all'interno di una clausola CASE.
/// </summary>
[JsonDerivedType(typeof(CaseValue), nameof(CaseValue))]
[JsonDerivedType(typeof(CaseRange), nameof(CaseRange))]
[JsonDerivedType(typeof(CaseComparison), nameof(CaseComparison))]
[JsonDerivedType(typeof(CaseTypePattern), nameof(CaseTypePattern))]
[JsonDerivedType(typeof(CaseValuePattern), nameof(CaseValuePattern))]
public abstract record CaseTest(SourcePosition Position) : AstNode(Position);

/// <summary>
/// Rappresenta un test per un valore discreto (es. CASE 1, CASE "A").
/// </summary>
/// <param name="Value">L'espressione che rappresenta il valore da confrontare.</param>
public sealed record CaseValue(Expr Value) : CaseTest(Value.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Value.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseValue.
    /// </summary>
    /// <returns>Una nuova istanza di CaseValue con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new CaseValue((Expr)Value.DeepClone());
    }
}

/// <summary>
/// Rappresenta un test per un intervallo di valori (es. CASE 1 TO 10).
/// </summary>
/// <param name="Start">L'espressione che definisce l'inizio dell'intervallo.</param>
/// <param name="End">L'espressione che definisce la fine dell'intervallo.</param>
public sealed record CaseRange(Expr Start, Expr End) : CaseTest(Start.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Start} TO {End}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseRange.
    /// </summary>
    /// <returns>Una nuova istanza di CaseRange con copie di tutte le sue espressioni figlie.</returns>
    public override AstNode DeepClone()
    {
        return new CaseRange((Expr)Start.DeepClone(), (Expr)End.DeepClone());
    }
}

/// <summary>
/// Rappresenta un test di comparazione (es. CASE IS > 100).
/// </summary>
/// <param name="Operator">Il token dell'operatore di confronto (es. >).</param>
/// <param name="Value">L'espressione con cui confrontare il valore del SELECT.</param>
public sealed record CaseComparison(Token Operator, Expr Value) : CaseTest(Operator.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"IS {Operator.Lexeme} {Value}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseComparison.
    /// </summary>
    /// <returns>Una nuova istanza di CaseComparison con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new CaseComparison((Token)Operator.DeepClone(), (Expr)Value.DeepClone());
    }
}

/// <summary>
/// Rappresenta un pattern matching sul tipo (es. CASE IS Tipo [AS Variabile]).
/// </summary>
/// <param name="TargetType">Il tipo di dato da confrontare.</param>
/// <param name="VariableName">Il token della variabile in cui acquisire il valore del tipo (es. 's' in 'CASE IS STRING s').</param>
/// <param name="WhenCondition">La condizione opzionale 'WHEN' da valutare se il tipo corrisponde.</param>
public sealed record CaseTypePattern(DataType TargetType, Token? VariableName, Expr? WhenCondition)
    : CaseTest(TargetType.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"IS {TargetType}{(VariableName != null ? $" AS {VariableName.Lexeme}" : "")}{(WhenCondition != null ? $" WHEN {WhenCondition}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseTypePattern.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new CaseTypePattern((DataType)TargetType.DeepClone(), (Token?)VariableName?.DeepClone(), (Expr?)WhenCondition?.DeepClone());
    }
}

/// <summary>
/// Rappresenta un pattern matching sul valore con acquisizione di variabile (es. CASE 10 AS v).
/// </summary>
/// <param name="PatternValue">L'espressione che definisce il valore da confrontare.</param>
/// <param name="VariableName">Il token della variabile in cui acquisire il valore (es. 'v' in 'CASE 10 AS v').</param>
/// <param name="WhenCondition">La condizione opzionale 'WHEN' da valutare se il valore corrisponde.</param>
public sealed record CaseValuePattern(Expr PatternValue, Token? VariableName, Expr? WhenCondition)
    : CaseTest(PatternValue.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"{PatternValue}{(VariableName != null ? $" AS {VariableName.Lexeme}" : "")}{(WhenCondition != null ? $" WHEN {WhenCondition}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseValuePattern.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new CaseValuePattern((Expr)PatternValue.DeepClone(), (Token?)VariableName?.DeepClone(), (Expr?)WhenCondition?.DeepClone());
    }
}

/// <summary>
/// Rappresenta una clausola CASE all'interno di un blocco SELECT.
/// </summary>
/// <param name="Tests">La lista delle condizioni di test per questa clausola.</param>
/// <param name="Statements">Le istruzioni da eseguire se una delle condizioni è soddisfatta.</param>
/// <param name="Token">Il token 'CASE' per riferimento.</param>
public sealed record CaseClause(ImmutableArray<CaseTest> Tests, ImmutableArray<Stmt> Statements, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CASE {string.Join(", ", Tests)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseClause.
    /// </summary>
    /// <returns>Una nuova istanza di CaseClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedTests = Tests.Select(t => (CaseTest)t.DeepClone()).ToImmutableArray();
        var clonedStatements = Statements.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new CaseClause(clonedTests, clonedStatements, clonedToken);
    }
}

/// <summary>
/// Rappresenta una clausola CASE ELSE all'interno di un blocco SELECT.
/// </summary>
public sealed record CaseElseClause(ImmutableArray<Stmt> Statements, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "CASE ELSE";

    /// <summary>
    /// Crea una copia profonda di questo nodo CaseElseClause.
    /// </summary>
    /// <returns>Una nuova istanza di CaseElseClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedStatements = Statements.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new CaseElseClause(clonedStatements, clonedToken);
    }
}

/// <summary>
/// Rappresenta una clausola CASE all'interno di un blocco MATCH.
/// </summary>
public sealed record MatchCaseClause(
    Expr? NullPattern,           // Per CASE IS NOTHING
    CaseTypePattern? TypePattern, // Per CASE IS TYPE var WHEN cond
    CaseValuePattern? ValuePattern, // Per CASE value AS var WHEN cond
    CaseValue? ValueTest,        // Per CASE value (semplice)
    CaseRange? RangeTest,        // Per CASE start TO end
    CaseComparison? ComparisonTest, // Per CASE IS > val
    Token WildcardToken,         // Per CASE _ (default)
    ImmutableArray<Stmt> Statements,
    Token CaseToken)
    : Stmt(CaseToken.Position)
{
    public override string ToString()
    {
        if (NullPattern != null) return "CASE IS NOTHING";
        if (TypePattern != null) return $"CASE IS {TypePattern}";
        if (ValuePattern != null) return $"CASE {ValuePattern}";
        if (ValueTest != null) return $"CASE {ValueTest}";
        if (RangeTest != null) return $"CASE {RangeTest}";
        if (ComparisonTest != null) return $"CASE {ComparisonTest}";
        if (WildcardToken != null) return "CASE _";
        return "CASE";
    }

    public override AstNode DeepClone()
    {
        return new MatchCaseClause(
            (Expr?)NullPattern?.DeepClone(),
            (CaseTypePattern?)TypePattern?.DeepClone(),
            (CaseValuePattern?)ValuePattern?.DeepClone(),
            (CaseValue?)ValueTest?.DeepClone(),
            (CaseRange?)RangeTest?.DeepClone(),
            (CaseComparison?)ComparisonTest?.DeepClone(),
            (Token)WildcardToken.DeepClone(),
            Statements.Select(s => (Stmt)s.DeepClone()).ToImmutableArray(),
            (Token)CaseToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'intera struttura MATCH...END MATCH (pattern matching stile Rust).
/// </summary>
public sealed record MatchStmt(
    Expr Expression,
    ImmutableArray<MatchCaseClause> Cases,
    Token MatchToken)
    : Stmt(MatchToken.Position)
{
    public override string ToString() => $"MATCH {Expression} ...";

    public override AstNode DeepClone()
    {
        return new MatchStmt(
            (Expr)Expression.DeepClone(),
            Cases.Select(c => (MatchCaseClause)c.DeepClone()).ToImmutableArray(),
            (Token)MatchToken.DeepClone());
    }
}

/// <summary>
/// Rappresenta una clausola IF...THEN o ELSEIF...THEN.
/// </summary>
public sealed record IfClause(Expr Condition, ImmutableArray<Stmt> Statements, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"IF {Condition} THEN ...";

    /// <summary>
    /// Crea una copia profonda di questo nodo IfClause.
    /// </summary>
    /// <returns>Una nuova istanza di IfClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCondition = (Expr)Condition.DeepClone();
        var clonedStatements = Statements.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new IfClause(clonedCondition, clonedStatements, clonedToken);
    }
}

/// <summary>
/// Rappresenta una clausola ELSE.
/// </summary>
public sealed record ElseClause(ImmutableArray<Stmt> Statements, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "ELSE ...";

    /// <summary>
    /// Crea una copia profonda di questo nodo ElseClause.
    /// </summary>
    /// <returns>Una nuova istanza di ElseClause con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedStatements = Statements.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new ElseClause(clonedStatements, clonedToken);
    }
}

/// <summary>
/// Rappresenta un'istruzione IF...THEN...ELSEIF...ELSE...ENDIF.
/// </summary>
public sealed record IfStmt(ImmutableArray<IfClause> IfClauses, ElseClause? ElseClause, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"{IfClauses[0].Condition}{(ElseClause is not null ? " ... ELSE ..." : " ...")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IfStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IfStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedIfClauses = IfClauses.Select(c => (IfClause)c.DeepClone()).ToImmutableArray();
        var clonedElseClause = (ElseClause?)ElseClause?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new IfStmt(clonedIfClauses, clonedElseClause, clonedToken);
    }
}

/// <summary>
/// Rappresenta un'istruzione SELECT CASE...END SELECT.
/// </summary>
public sealed record SelectStmt(
    Expr Expression,
    ImmutableArray<CaseClause> CaseClauses,
    CaseElseClause? ElseClause,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CASE {Expression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SelectStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SelectStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedExpression = (Expr)Expression.DeepClone();
        var clonedCaseClauses = CaseClauses.Select(c => (CaseClause)c.DeepClone()).ToImmutableArray();
        var clonedElseClause = (CaseElseClause?)ElseClause?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new SelectStmt(clonedExpression, clonedCaseClauses, clonedElseClause, clonedToken);
    }
}

/// <summary>
/// Rappresenta un blocco WITH...END WITH.
/// WITH expression
///   .Member = value  ' risolve su expression.Member
/// END WITH
/// </summary>
public sealed record WithStmt(
    Expr Expression,
    ImmutableArray<Stmt> Body,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"WITH {Expression} ... END WITH";

    public override AstNode DeepClone()
    {
        var clonedExpression = (Expr)Expression.DeepClone();
        var clonedBody = Body.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new WithStmt(clonedExpression, clonedBody, clonedToken);
    }
}

public sealed record UsingStmt(
    Target ResourceTarget,
    Expr ResourceInit,
    ImmutableArray<Stmt> Body,
    Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"USING ... END USING";

    public override AstNode DeepClone()
    {
        var clonedTarget = (Target)ResourceTarget.DeepClone();
        var clonedInit = (Expr)ResourceInit.DeepClone();
        var clonedBody = Body.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        var clonedToken = (Token)Token.DeepClone();
        return new UsingStmt(clonedTarget, clonedInit, clonedBody, clonedToken);
    }
}