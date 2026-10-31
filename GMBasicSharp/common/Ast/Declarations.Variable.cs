// FILE: Ast.Declarations.Variable.cs

namespace Basic;

/// <summary>
/// Rappresenta una singola dichiarazione di variabile all'interno di un'istruzione.
/// Contiene tutte le informazioni semantiche e sintattiche di una dichiarazione.
/// ORA CONTIENE ESPLICITAMENTE SIA IL NOME ORIGINALE CHE QUELLO NORMALIZZATO.
/// </summary>
public sealed record VariableDeclaration(
    int SymbolId,
    Token NameSuffixed,      // Il token originale, es. "MioVar$"
    Token NameCleaned,       // Il token normalizzato, es. "MioVar"
    ImmutableArray<Expr> Dimensions,
    DataType? DataType,
    Expr? InitialValue,
    bool IsImplicitArray,
    bool HasAsClause,
    SymbolFlags Flags = SymbolFlags.None,
    ImmutableArray<Token> Attributes = default) : Decl(NameSuffixed.Position)
{
    /// <summary>
    /// Ottiene le espressioni per i limiti inferiori delle dimensioni dell'array,
    /// se specificati con la sintassi `DIM A(lower TO upper)`.
    /// Se non specificato, l'array è 0-based e questo array sarà vuoto.
    /// </summary>
    public ImmutableArray<Expr> LowerBounds { get; init; } = ImmutableArray<Expr>.Empty;

    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var dimensionsStr = "";
        if (Dimensions.Any())
        {
            if (LowerBounds.Any())
            {
                var dims = Dimensions.Select((t, i) => i < LowerBounds.Length
                        ? $"{LowerBounds[i]} TO {t}"
                        : t.ToString())
                    .ToList();

                dimensionsStr = $"({string.Join(", ", dims)})";
            }
            else
            {
                dimensionsStr = $"({string.Join(", ", Dimensions)})";
            }
        }
        else if (IsImplicitArray)
        {
            dimensionsStr = "()";
        }

        var typeStr = DataType is not null and not InferredType ? $" AS {DataType}" : "";
        var flagsStr = Flags != SymbolFlags.None ? $" [{Flags}]" : "";

        // Mostra entrambi i nomi per un debug chiaro
        return
            $"{NameSuffixed.Lexeme} (as {NameCleaned.Lexeme})#{SymbolId}{dimensionsStr}{typeStr}{(InitialValue is not null ? $" = {InitialValue}" : "")}{flagsStr}";
    }

    /// <summary>
    /// Crea una copia profonda di questa dichiarazione di variabile.
    /// </summary>
    /// <returns>Una nuova istanza di VariableDeclaration con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedNameSuffixed = (Token)NameSuffixed.DeepClone();
        var clonedNameCleaned = (Token)NameCleaned.DeepClone();
        var clonedDimensions = Dimensions.Select(d => (Expr)d.DeepClone()).ToImmutableArray();
        var clonedDataType = DataType?.DeepClone() as DataType;
        var clonedInitialValue = (Expr?)InitialValue?.DeepClone();
        var clonedLowerBounds = LowerBounds.Select(d => (Expr)d.DeepClone()).ToImmutableArray();

        return new VariableDeclaration(
            SymbolId,
            clonedNameSuffixed,
            clonedNameCleaned,
            clonedDimensions,
            clonedDataType,
            clonedInitialValue,
            IsImplicitArray,
            HasAsClause,
            Flags
        )
        {
            LowerBounds = clonedLowerBounds
        };
    }
}
