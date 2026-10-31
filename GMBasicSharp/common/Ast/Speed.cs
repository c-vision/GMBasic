// FILE: Interpreter.Ast.Speed.cs

using System.Text;

// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

// ====================================================================================================================
// --- SEZIONE 1: NODI AST SPECIFICI PER SPEED (Comandi Base) ---
// ====================================================================================================================

#region Comandi Base Speed

/// <summary>
/// Rappresenta l'istruzione `SPEED OPEN "dbpath" ALIAS DbAlias`.
/// Apre un database (contenitore di tabelle). L'alias è obbligatorio.
/// </summary>
[JsonDerivedType(typeof(SpeedOpenStmt), nameof(SpeedOpenStmt))]
public sealed record SpeedOpenStmt(
    Expr DatabasePath,
    Token OriginalAliasToken, // Obbligatorio
    Token Token
) : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED OPEN {DatabasePath} ALIAS {OriginalAliasToken.Lexeme}";

    public override AstNode DeepClone()
    {
        return new SpeedOpenStmt(
            (Expr)DatabasePath.DeepClone(),
            (Token)OriginalAliasToken.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED CLOSE [alias | DATABASES]`.
/// Chiude il database SPEED corrente / per alias, oppure tutti (`DATABASES`).
/// </summary>
[JsonDerivedType(typeof(SpeedCloseStmt), nameof(SpeedCloseStmt))]
public sealed record SpeedCloseStmt(
    bool AllDatabases,
    Token? AliasToken,
    Token Token
) : Stmt(Token.Position)
{
    public override string ToString() => AllDatabases
        ? "SPEED CLOSE DATABASES"
        : $"SPEED CLOSE {AliasToken?.Lexeme}";

    public override AstNode DeepClone() => new SpeedCloseStmt(
        AllDatabases,
        AliasToken is null ? null : (Token)AliasToken.DeepClone(),
        (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione `SPEED USE "tableName" FROM DbAlias OF TypeName ALIAS TableAlias [INDEXED BY ...]`.
/// Apre una tabella da un database aperto. L'alias della tabella è obbligatorio.
/// </summary>
[JsonDerivedType(typeof(SpeedUseStmt), nameof(SpeedUseStmt))]
public sealed record SpeedUseStmt(
    Expr TableName,
    Expr DatabaseReference, // Riferimento all'alias del database (non più handle numerico)
    int TypeSymbolId,
    Token OriginalTypeName,
    IsamIndexedByClause? IndexedBy,
    int AliasSymbolId, // ID del simbolo per l'alias della tabella
    Token OriginalAliasToken, // Obbligatorio
    bool IsUnsafe,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder(
            $"SPEED USE {TableName} FROM {DatabaseReference} OF {OriginalTypeName.Lexeme}#{TypeSymbolId} ALIAS {OriginalAliasToken.Lexeme}");
        if (IndexedBy != null) sb.Append($" {IndexedBy}");
        if (IsUnsafe) sb.Append(" UNSAFE");
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo SpeedUseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SpeedUseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SpeedUseStmt(
            (Expr)TableName.DeepClone(),
            (Expr)DatabaseReference.DeepClone(),
            TypeSymbolId,
            (Token)OriginalTypeName.DeepClone(),
            IndexedBy?.DeepClone(),
            AliasSymbolId,
            (Token)OriginalAliasToken.DeepClone(),
            IsUnsafe,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED CREATE INDEX "indexName" ON "alias" KEY (expression)`.
/// </summary>
[JsonDerivedType(typeof(SpeedCreateIndexStmt), nameof(SpeedCreateIndexStmt))]
public sealed record SpeedCreateIndexStmt(Expr IndexName, Expr Alias, Expr KeyExpression, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED CREATE INDEX {IndexName} ON {Alias} KEY ({KeyExpression})";

    public override AstNode DeepClone()
    {
        return new SpeedCreateIndexStmt((Expr)IndexName.DeepClone(), (Expr)Alias.DeepClone(),
            (Expr)KeyExpression.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED DROP INDEX "indexName" ON "alias"`.
/// </summary>
[JsonDerivedType(typeof(SpeedDropIndexStmt), nameof(SpeedDropIndexStmt))]
public sealed record SpeedDropIndexStmt(Expr IndexName, Expr Alias, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED DROP INDEX {IndexName} ON {Alias}";

    public override AstNode DeepClone()
    {
        return new SpeedDropIndexStmt((Expr)IndexName.DeepClone(), (Expr)Alias.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `GET alias, udtVar` per il motore Speed.
/// </summary>
[JsonDerivedType(typeof(SpeedGetStmt), nameof(SpeedGetStmt))]
public sealed record SpeedGetStmt(Expr Alias, Target Destination, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED GET {Alias}, {Destination}";

    public override AstNode DeepClone()
    {
        return new SpeedGetStmt((Expr)Alias.DeepClone(), (Target)Destination.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `PUT alias [, udtVar]` per il motore Speed.
/// </summary>
[JsonDerivedType(typeof(SpeedPutStmt), nameof(SpeedPutStmt))]
public sealed record SpeedPutStmt(Expr Alias, Expr? Source, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED PUT {Alias}" + (Source != null ? $", {Source}" : "");

    public override AstNode DeepClone()
    {
        return new SpeedPutStmt((Expr)Alias.DeepClone(), (Expr?)Source?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DELETE alias [, udtVar]` per il motore Speed.
/// </summary>
[JsonDerivedType(typeof(SpeedDeleteStmt), nameof(SpeedDeleteStmt))]
public sealed record SpeedDeleteStmt(Expr Alias, Expr? Source, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED DELETE {Alias}" + (Source != null ? $", {Source}" : "");

    public override AstNode DeepClone()
    {
        return new SpeedDeleteStmt((Expr)Alias.DeepClone(), (Expr?)Source?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SEEK alias, keyValue [USING "indexName"]`.
/// </summary>
[JsonDerivedType(typeof(SpeedSeekStmt), nameof(SpeedSeekStmt))]
public sealed record SpeedSeekStmt(Expr Alias, Expr KeyValue, Expr? IndexName, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        $"SPEED SEEK {Alias}, {KeyValue}" + (IndexName != null ? $" USING {IndexName}" : "");

    public override AstNode DeepClone()
    {
        return new SpeedSeekStmt((Expr)Alias.DeepClone(), (Expr)KeyValue.DeepClone(), (Expr?)IndexName?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `GO alias, TOP|BOTTOM`.
/// </summary>
[JsonDerivedType(typeof(SpeedGoStmt), nameof(SpeedGoStmt))]
public sealed record SpeedGoStmt(Expr Alias, Token Location, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED GO {Alias}, {Location.Lexeme}";

    public override AstNode DeepClone()
    {
        return new SpeedGoStmt((Expr)Alias.DeepClone(), (Token)Location.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SKIP alias [, n]`.
/// </summary>
[JsonDerivedType(typeof(SpeedSkipStmt), nameof(SpeedSkipStmt))]
public sealed record SpeedSkipStmt(Expr Alias, Expr? Count, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED SKIP {Alias}" + (Count != null ? $", {Count}" : "");

    public override AstNode DeepClone()
    {
        return new SpeedSkipStmt((Expr)Alias.DeepClone(), (Expr?)Count?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET ORDER TO "indexName" ON alias`.
/// </summary>
[JsonDerivedType(typeof(SpeedSetOrderStmt), nameof(SpeedSetOrderStmt))]
public sealed record SpeedSetOrderStmt(Expr Alias, Expr IndexName, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED SET ORDER TO {IndexName} ON {Alias}";

    public override AstNode DeepClone()
    {
        return new SpeedSetOrderStmt((Expr)Alias.DeepClone(), (Expr)IndexName.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET FILTER TO [condizione] ON alias`.
/// </summary>
[JsonDerivedType(typeof(SpeedSetFilterStmt), nameof(SpeedSetFilterStmt))]
public sealed record SpeedSetFilterStmt(Expr Alias, Expr? Condition, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        $"SPEED SET FILTER TO {(Condition != null ? Condition.ToString() : "")} ON {Alias}";

    public override AstNode DeepClone()
    {
        return new SpeedSetFilterStmt((Expr)Alias.DeepClone(), (Expr?)Condition?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET RELATION ON parentAlias TO keyExpr INTO childAlias`.
/// </summary>
[JsonDerivedType(typeof(SpeedSetRelationStmt), nameof(SpeedSetRelationStmt))]
public sealed record SpeedSetRelationStmt(Expr ParentAlias, Expr? KeyExpression, Expr? ChildAlias, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString()
    {
        var sb = new StringBuilder($"SPEED SET RELATION ON {ParentAlias}");
        if (KeyExpression != null && ChildAlias != null)
        {
            sb.Append($" TO {KeyExpression} INTO {ChildAlias}");
        }

        return sb.ToString();
    }

    public override AstNode DeepClone()
    {
        return new SpeedSetRelationStmt((Expr)ParentAlias.DeepClone(), (Expr?)KeyExpression?.DeepClone(),
            (Expr?)ChildAlias?.DeepClone(), (Token)Token.DeepClone());
    }
}

#endregion

// ====================================================================================================================
// --- SEZIONE 2: NODI AST PER SPEED QUERY LANGUAGE (CORRETTI) ---
// ====================================================================================================================

#region Speed Query Language

/// <summary>
/// Rappresenta un campo nella clausola SELECT, che può avere un alias.
/// </summary>
[JsonDerivedType(typeof(SelectField), nameof(SelectField))]
public sealed record SelectField(Expr Expression, Token? Alias)
{
    public override string ToString() => Expression + (Alias != null ? $" AS {Alias.Lexeme}" : "");
}

/// <summary>
/// Rappresenta la clausola `FROM alias [AS token]`.
/// </summary>
[JsonDerivedType(typeof(FromClause), nameof(FromClause))]
public sealed record FromClause(Token TableAlias, Token? AsAlias)
{
    public override string ToString() => $"FROM {TableAlias.Lexeme}" + (AsAlias != null ? $" AS {AsAlias.Lexeme}" : "");
}

/// <summary>
/// Rappresenta la clausola `JOIN alias [AS token] ON condizione`.
/// </summary>
[JsonDerivedType(typeof(JoinClause), nameof(JoinClause))]
public sealed record JoinClause(Token TableAlias, Token? AsAlias, Expr Condition)
{
    public override string ToString() => $"JOIN {TableAlias.Lexeme}" +
                                         (AsAlias != null ? $" AS {AsAlias.Lexeme}" : "") + $" ON {Condition}";
}

/// <summary>
/// Rappresenta la clausola `ORDER BY campo [ASC|DESC]`.
/// </summary>
[JsonDerivedType(typeof(OrderByField), nameof(OrderByField))]
public sealed record OrderByField(Expr Expression, bool Descending)
{
    public override string ToString() => Expression + (Descending ? " DESC" : " ASC");
}

/// <summary>
/// Rappresenta l'istruzione completa `SPEED SELECT ... INTO array_variable`.
/// Questa versione è aggiornata per includere il SymbolId del tipo di destinazione e il token originale del nome del tipo.
/// </summary>
[JsonDerivedType(typeof(SpeedSelectStmt), nameof(SpeedSelectStmt))]
public sealed record SpeedSelectStmt(
    ImmutableArray<SelectField>? SelectFields,
    Target? IntoTarget,
    int DestinationUdtSymbolId,
    Token? DestinationUdtToken,
    FromClause? From,
    JoinClause? Join,
    Expr? Where,
    ImmutableArray<Expr>? GroupBy,
    Expr? Having,
    ImmutableArray<OrderByField>? OrderBy,
    Expr? QueryExpression,
    Token Token)
    : Stmt(Token.Position)
{
    public override string ToString()
    {
        if (QueryExpression != null) return $"SPEED.SELECT({QueryExpression})";

        // NOTA: SelectFields/GroupBy/OrderBy sono ImmutableArray<T>? (struct nullable).
        // string.Join(sep, SelectFields) senza .Value non trova un overload IEnumerable<T>
        // che accetti un ImmutableArray<T>? direttamente: il compilatore ripiega su
        // string.Join(string, object[]) (via boxing di un singolo argomento), stampando
        // il ToString() dell'intero struct nullable ("System.Collections.Immutable.
        // ImmutableArray`1[...]") invece di enumerarne gli elementi — testo non
        // re-parsabile (contiene persino un backtick, interpretato dal lexer come inizio
        // di una raw string mai chiusa) usato altrove per rigenerare la query da UDT VIEW.
        var sb = new StringBuilder("SPEED SELECT ");
        if (SelectFields != null) sb.Append(string.Join(", ", SelectFields.Value));
        if (IntoTarget != null)
            sb.Append(
                $" INTO {IntoTarget} OF {(DestinationUdtToken != null ? DestinationUdtToken.Lexeme : $"TYPE#{DestinationUdtSymbolId}")} ");
        if (From != null) sb.Append(From);
        if (Join != null) sb.Append($" {Join}");
        if (Where != null) sb.Append($" WHERE {Where}");
        if (GroupBy != null && GroupBy.Value.Any()) sb.Append($" GROUP BY {string.Join(", ", GroupBy.Value)}");
        if (Having != null) sb.Append($" HAVING {Having}");
        if (OrderBy != null && OrderBy.Value.Any()) sb.Append($" ORDER BY {string.Join(", ", OrderBy.Value)}");
        return sb.ToString();
    }

    public override AstNode DeepClone()
    {
        ImmutableArray<SelectField>? clonedSelectFields = null;
        if (SelectFields.HasValue)
        {
            clonedSelectFields =
            [
                ..SelectFields.Value
                    .Select(f => new SelectField((Expr)f.Expression.DeepClone(), (Token?)f.Alias?.DeepClone()))
            ];
        }

        ImmutableArray<Expr>? clonedGroupBy = null;
        if (GroupBy.HasValue)
        {
            clonedGroupBy =
            [
                ..GroupBy.Value
                    .Select(g => (Expr)g.DeepClone())
            ];
        }

        ImmutableArray<OrderByField>? clonedOrderBy = null;
        if (OrderBy.HasValue)
        {
            clonedOrderBy =
            [
                ..OrderBy.Value
                    .Select(o => o with { Expression = (Expr)o.Expression.DeepClone() })
            ];
        }

        return new SpeedSelectStmt(
            clonedSelectFields,
            (Target?)IntoTarget?.DeepClone(),
            DestinationUdtSymbolId,
            (Token?)DestinationUdtToken?.DeepClone(),
            From is null ? null : new FromClause((Token)From.TableAlias.DeepClone(), (Token?)From.AsAlias?.DeepClone()),
            Join is null
                ? null
                : new JoinClause((Token)Join.TableAlias.DeepClone(), (Token?)Join.AsAlias?.DeepClone(),
                    (Expr)Join.Condition.DeepClone()),
            (Expr?)Where?.DeepClone(),
            clonedGroupBy,
            (Expr?)Having?.DeepClone(),
            clonedOrderBy,
            (Expr?)QueryExpression?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED CREATE VIEW "viewName" OF TypeName AS SELECT ...`.
/// Questa versione è aggiornata per usare SymbolId per il tipo e per la vista stessa.
/// </summary>
[JsonDerivedType(typeof(SpeedCreateViewStmt), nameof(SpeedCreateViewStmt))]
public sealed record SpeedCreateViewStmt(
    int ViewSymbolId,
    Token OriginalViewName,
    int OfTypeSymbolId,
    Token OriginalTypeName,
    SpeedSelectStmt? SelectStatement,
    Expr? QueryExpression,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() =>
        $"SPEED CREATE VIEW {OriginalViewName.Lexeme}#{ViewSymbolId} OF {OriginalTypeName.Lexeme}#{OfTypeSymbolId} " +
        $"AS {SelectStatement?.ToString() ?? QueryExpression?.ToString()}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SpeedCreateViewStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SpeedCreateViewStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SpeedCreateViewStmt(
            ViewSymbolId,
            (Token)OriginalViewName.DeepClone(),
            OfTypeSymbolId,
            (Token)OriginalTypeName.DeepClone(),
            (SpeedSelectStmt?)SelectStatement?.DeepClone(),
            (Expr?)QueryExpression?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED REFRESH VIEW "viewName"`.
/// </summary>
[JsonDerivedType(typeof(SpeedRefreshViewStmt), nameof(SpeedRefreshViewStmt))]
public sealed record SpeedRefreshViewStmt(Token ViewName, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED REFRESH VIEW {ViewName.Lexeme}";

    public override AstNode DeepClone()
    {
        return new SpeedRefreshViewStmt((Token)ViewName.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SPEED DROP VIEW "viewName"`.
/// </summary>
[JsonDerivedType(typeof(SpeedDropViewStmt), nameof(SpeedDropViewStmt))]
public sealed record SpeedDropViewStmt(Token ViewName, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"SPEED DROP VIEW {ViewName.Lexeme}";

    public override AstNode DeepClone()
    {
        return new SpeedDropViewStmt((Token)ViewName.DeepClone(), (Token)Token.DeepClone());
    }
}

#endregion