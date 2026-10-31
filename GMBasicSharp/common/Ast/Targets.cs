// FILE: Interpreter.Ast.Targets.cs



// Aggiunto per la serializzazione polimorfica

// ReSharper disable InvalidXmlDocComment
// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming
namespace Basic;

/// <summary>
/// Classe base astratta per una destinazione di assegnazione (es. una variabile o un elemento di array).
/// </summary>
[JsonDerivedType(typeof(VariableTarget), nameof(VariableTarget))]
[JsonDerivedType(typeof(FieldTarget), nameof(FieldTarget))]
[JsonDerivedType(typeof(ArrayElementTarget), nameof(ArrayElementTarget))]
[JsonDerivedType(typeof(IndexedTarget), nameof(IndexedTarget))]
[JsonDerivedType(typeof(MacroTarget), nameof(MacroTarget))] 
public abstract record Target(SourcePosition Position) : AstNode(Position);

/// <summary>
/// Rappresenta una variabile come destinazione di un'assegnazione.
/// Contiene sia il nome originale (con suffisso) per la deduzione del tipo,
/// sia il nome pulito per il linking.
/// </summary>
public sealed record VariableTarget(int SymbolId, Token NameSuffixed, Token NameCleaned) 
    : Target(NameSuffixed.Position)
{
    /// <summary>
    /// Il tipo di dato della variabile, risolto durante la fase di riscrittura.
    /// </summary>
    public DataType? ResolvedType { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{NameSuffixed.Lexeme}#{SymbolId}";

    /// <summary>
    /// Crea una copia profonda di questo nodo VariableTarget.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new VariableTarget(
            SymbolId, 
            (Token)NameSuffixed.DeepClone(), 
            (Token)NameCleaned.DeepClone())
        {
            ResolvedType = ResolvedType?.DeepClone() as DataType
        };
    }
}

/// <summary>
/// Rappresenta un target di assegnazione specificato tramite una macro di sostituzione.
/// Es. in `REPLACE &cCampo WITH 10`, `&cCampo` diventa un `MacroTarget`.
/// </summary>
/// <param name="Expression">L'espressione macro (`MacroExpr`) che, una volta valutata, restituirà il nome del campo.</param>
public sealed record MacroTarget(MacroExpr Expression) : Target(Expression.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Expression.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo MacroTarget.
    /// </summary>
    /// <returns>Una nuova istanza di MacroTarget con una copia della sua espressione macro figlia.</returns>
    public override AstNode DeepClone()
    {
        return new MacroTarget((MacroExpr)Expression.DeepClone());
    }
}

/// <summary>
/// Rappresenta un campo di una struttura come destinazione di un'assegnazione (es. P.Nome = ...).
/// Questa versione è stata aggiornata per utilizzare un SymbolId per identificare il campo,
/// allineandosi all'architettura ID-based.
/// </summary>
public sealed record FieldTarget(Target? Structure, int FieldSymbolId, Token OriginalToken)
    : Target(Structure?.Position ?? OriginalToken.Position)
{
    /// <summary>
    /// Il tipo di dato del campo, risolto durante la fase di riscrittura.
    /// </summary>
    public DataType? ResolvedType { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"{Structure}.{OriginalToken.Lexeme}#{FieldSymbolId}";

    /// <summary>
    /// Crea una copia profonda di questo nodo FieldTarget.
    /// </summary>
    /// <returns>Una nuova istanza di FieldTarget con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedStructure = (Target)Structure.DeepClone();
        var clonedOriginalToken = (Token)OriginalToken.DeepClone();
        return new FieldTarget(clonedStructure, FieldSymbolId, clonedOriginalToken)
        {
            ResolvedType = ResolvedType?.DeepClone() as DataType
        };
    }
}

/// <summary>
/// Rappresenta un elemento di un array come destinazione di un'assegnazione.
/// Contiene sia il nome originale (con suffisso) per la deduzione del tipo,
/// sia il nome pulito per il linking.
/// </summary>
public sealed record ArrayElementTarget(int SymbolId, Token NameSuffixed, Token NameCleaned, ImmutableArray<Expr> Indices)
    : Target(NameSuffixed.Position)
{
    /// <summary>
    /// Il tipo di dato dell'array, risolto durante la fase di riscrittura.
    /// </summary>
    public DataType? ResolvedType { get; init; }

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{NameSuffixed.Lexeme}#{SymbolId}({string.Join(", ", Indices)})";

    /// <summary>
    /// Crea una copia profonda di questo nodo ArrayElementTarget.
    /// </summary>
    /// <returns>Una nuova istanza di ArrayElementTarget con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedNameSuffixed = (Token)NameSuffixed.DeepClone();
        var clonedNameCleaned = (Token)NameCleaned.DeepClone();
        var clonedIndices = Indices.Select(i => (Expr)i.DeepClone()).ToImmutableArray();
        return new ArrayElementTarget(SymbolId, clonedNameSuffixed, clonedNameCleaned, clonedIndices)
        {
            ResolvedType = ResolvedType?.DeepClone() as DataType
        };
    }
}

/// <summary>
/// Rappresenta una destinazione di assegnazione indicizzata.
/// </summary>
public sealed record IndexedTarget(Expr Base, ImmutableArray<Expr> Indices) : Target(Base.Position)
{
    /// <summary>
    /// Il tipo di dato della collezione, risolto durante la fase di riscrittura.
    /// </summary>
    public DataType? ResolvedType { get; init; }

    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new IndexedTarget((Expr)Base.DeepClone(), [..Indices.Select(i => (Expr)i.DeepClone())])
        {
            ResolvedType = ResolvedType?.DeepClone() as DataType
        };
    }
}

