// FILE: Ast.Declarations.Core.cs

namespace Basic;

/// <summary>
/// Definisce un singolo parametro in una firma di FUNCTION o SUB.
/// </summary>
public sealed record Parameter(
    int SymbolId,
    Token Name,
    DataType DataType,
    SymbolFlags Flags,
    Expr? DefaultValue,
    bool HasAsClause) : Decl(Name.Position)
{
    /// <summary>Nome originale del parametro nel sorgente.</summary>
    public Token OriginalName { get; init; } = Name;

    /// <inheritdoc />
    public override string ToString()
    {
        var isOptional = Flags.HasFlag(SymbolFlags.IsOptional);
        var isByRef = Flags.HasFlag(SymbolFlags.IsByRef);
        var isArray = Flags.HasFlag(SymbolFlags.IsArray);

        return
            $"{(isOptional ? "OPTIONAL " : "")}{(isByRef ? "BYREF " : "")}{OriginalName.Lexeme}{(isArray ? "()" : "")} " +
            $"AS {DataType}{(DefaultValue is not null ? $" = {DefaultValue}" : "")} [{Flags}]";
    }

    /// <inheritdoc />
    public override AstNode DeepClone()
    {
        var clonedName = (Token)Name.DeepClone();
        var clonedOriginalName = (Token)OriginalName.DeepClone();
        var clonedDataType = (DataType)DataType.DeepClone();
        var clonedDefaultValue = (Expr?)DefaultValue?.DeepClone();

        return new Parameter(SymbolId, clonedName, clonedDataType, Flags, clonedDefaultValue, HasAsClause)
        {
            OriginalName = clonedOriginalName
        };
    }
}

/// <summary>
/// Rappresenta una singola dichiarazione di costante all'interno di un'istruzione CONST.
/// </summary>
public sealed record ConstDeclaration(
    int SymbolId,
    Token OriginalToken,
    DataType? DataType,
    Expr InitialValue,
    SymbolFlags Flags = SymbolFlags.None)
    : Decl(OriginalToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() =>
        $"{OriginalToken.Lexeme}#{SymbolId}{(DataType is not null ? $" AS {DataType}" : "")} = {InitialValue} [{Flags}]";

    /// <summary>
    /// Crea una copia profonda di questa dichiarazione di costante.
    /// </summary>
    /// <returns>Una nuova istanza di ConstDeclaration con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedOriginalToken = (Token)OriginalToken.DeepClone();
        var clonedInitialValue = (Expr)InitialValue.DeepClone();
        var clonedDataType = DataType?.DeepClone() as DataType;
        return new ConstDeclaration(SymbolId, clonedOriginalToken, clonedDataType, clonedInitialValue, Flags);
    }
}




/// <summary>
/// Rappresenta un'assegnazione di tupla: (x, y, z) = tupleExpr (swap/multi-assignment).
/// </summary>
public sealed record TupleAssignmentStmt(
    ImmutableArray<Token> VariableNames,
    Expr TupleExpression,
    Token OpenParen)
    : Stmt(OpenParen.Position)
{
    public override string ToString() => $"({string.Join(", ", VariableNames.Select(v => v.Lexeme))}) = {TupleExpression}";

    public override AstNode DeepClone()
    {
        return new TupleAssignmentStmt(
            VariableNames.Select(v => (Token)v.DeepClone()).ToImmutableArray(),
            (Expr)TupleExpression.DeepClone(),
            (Token)OpenParen.DeepClone());
    }
}

/// <summary>
/// Rappresenta una distrutturazione di tupla: DIM (x, y, z) = tupleExpr.
/// </summary>
public sealed record TupleDestructuringStmt(
    ImmutableArray<Token> VariableNames,
    Expr TupleExpression,
    Token KeywordToken)
    : Stmt(KeywordToken.Position)
{
    public override string ToString() => $"DIM ({string.Join(", ", VariableNames.Select(v => v.Lexeme))}) = {TupleExpression}";

    public override AstNode DeepClone()
    {
        return new TupleDestructuringStmt(
            VariableNames.Select(v => (Token)v.DeepClone()).ToImmutableArray(),
            (Expr)TupleExpression.DeepClone(),
            (Token)KeywordToken.DeepClone());
    }
}


