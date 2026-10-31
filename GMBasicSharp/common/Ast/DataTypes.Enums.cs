// FILE: Ast.DataTypes.Enums.cs

namespace Basic;

/// <summary>
/// Rappresenta un singolo membro di un'enumerazione.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questo membro.</param>
/// <param name="Name">Il token con il nome del membro.</param>
/// <param name="ValueExpr">L'espressione opzionale che definisce il valore esplicito.</param>
public sealed record EnumMember(
    int SymbolId,
    Token Name,
    Expr? ValueExpr) : Decl(Name.Position)
{
    public override string ToString() => ValueExpr != null ? $"{Name.Lexeme} = {ValueExpr}" : Name.Lexeme;

    public override AstNode DeepClone()
    {
        return new EnumMember(
            SymbolId,
            (Token)Name.DeepClone(),
            (Expr?)ValueExpr?.DeepClone());
    }
}

/// <summary>
/// Rappresenta la definizione di un'enumerazione (ENUM...END ENUM).
/// Ora è un semplice Decl per raggruppare i membri, non più un DataType.
/// </summary>
/// <param name="SymbolId">L'ID univoco del simbolo per questa enumerazione (che funge da alias di tipo).</param>
/// <param name="Name">Il token con il nome dell'enumerazione.</param>
/// <param name="UnderlyingType">Il tipo di dato sottostante (es. INTEGER).</param>
/// <param name="Flags">I flag semantici (es. visibilità).</param>
/// <param name="Members">La lista dei membri dell'enumerazione.</param>
public sealed record EnumDefinition(
    int SymbolId,
    Token Name,
    DataType UnderlyingType,
    SymbolFlags Flags,
    ImmutableArray<EnumMember> Members
) : Decl(Name.Position) // <-- CAMBIATO DA DataType A Decl
{
    public Token OriginalName { get; init; } = Name;

    public override string ToString() => $"ENUM {Name.Lexeme} AS {UnderlyingType} [{Flags}]";

    public override AstNode DeepClone()
    {
        return new EnumDefinition(
            SymbolId,
            (Token)Name.DeepClone(),
            (DataType)UnderlyingType.DeepClone(),
            Flags,
            [..Members.Select(m => (EnumMember)m.DeepClone())]
        )
        {
            OriginalName = (Token)OriginalName.DeepClone()
        };
    }
}