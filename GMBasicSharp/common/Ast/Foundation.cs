// FILE: Ast.Foundation.cs

namespace Basic;

/// <summary>
/// Rappresenta una posizione univoca nel codice sorgente,
/// combinando un ID di file con riga e colonna per una tracciabilità completa.
/// </summary>
public readonly struct SourcePosition(int fileId, int line, int column) : IEquatable<SourcePosition>
{
    /// <summary>L'ID del file sorgente, gestito da un SourceManager.</summary>
    public int FileId { get; } = fileId;

    /// <summary>Il numero di riga (1-based).</summary>
    public int Line { get; } = line;

    /// <summary>Il numero di colonna (1-based).</summary>
    public int Column { get; } = column;

    /// <summary>Rappresenta una posizione sconosciuta o non specificata.</summary>
    public static SourcePosition Unknown => new(-1, 0, 0);

    /// <summary>Restituisce una rappresentazione testuale della posizione.</summary>
    public override string ToString() => $"[File:{FileId} L:{Line}, C:{Column}]";

    /// <summary>
    /// Crea una copia di questa istanza. Poiché è uno struct, restituisce semplicemente se stessa.
    /// </summary>
    public SourcePosition DeepClone() => this;

    /// <inheritdoc />
    public override bool Equals(object? obj) => obj is SourcePosition other && Equals(other);

    /// <inheritdoc />
    public bool Equals(SourcePosition other) =>
        FileId == other.FileId && Line == other.Line && Column == other.Column;

    /// <inheritdoc />
    public override int GetHashCode() => HashCode.Combine(FileId, Line, Column);

    /// <summary>Operatore di uguaglianza.</summary>
    public static bool operator ==(SourcePosition left, SourcePosition right) => left.Equals(right);

    /// <summary>Operatore di disuguaglianza.</summary>
    public static bool operator !=(SourcePosition left, SourcePosition right) => !(left == right);
}

/// <summary>
/// Rappresenta una singola riga di codice sorgente, con informazioni complete sulla sua origine.
/// </summary>
public record SourceLine(int FileId, string FilePath, int OriginalLineNumber, string Text);

/// <summary>
/// Classe base astratta per tutti i nodi nell'Abstract Syntax Tree (AST).
/// </summary>
[JsonDerivedType(typeof(Token), nameof(Token))]
[JsonDerivedType(typeof(Expr), "Expr")]
[JsonDerivedType(typeof(Stmt), "Stmt")]
[JsonDerivedType(typeof(Decl), "Decl")]
[JsonDerivedType(typeof(SourceLine), nameof(SourceLine))]
public abstract record AstNode(SourcePosition Position)
{
    /// <summary>
    /// Ottiene o imposta l'indice della riga di codice sorgente nella SourceMap del programma.
    /// </summary>
    public int SourceLineIndex { get; init; } = -1;

    /// <summary>
    /// Ottiene l'ID dello scope lessicale in cui questo nodo è stato definito.
    /// </summary>
    public int ScopeId { get; init; } = -1;

    /// <summary>Numero di riga (convenienza).</summary>
    [JsonIgnore] public int Line => Position.Line;

    /// <summary>Numero di colonna (convenienza).</summary>
    [JsonIgnore] public int Column => Position.Column;

    /// <summary>Crea una copia profonda di questo nodo.</summary>
    public abstract AstNode DeepClone();
}

/// <summary>Classe base astratta per tutte le istruzioni nell'AST.</summary>
public abstract record Stmt(SourcePosition Position) : AstNode(Position);

/// <summary>§4.10 SEMAPRAX — semantic patch revision-bound (lato codice): sostituisce il corpo della
/// procedura <see cref="Target"/> con <see cref="Body"/> SE <see cref="ExpectedRev"/> coincide con la
/// revisione corrente del programma (altrimenti rifiutato). Il linker applica; lo stmt diventa no-op.</summary>
public sealed record SemanticPatchStmt(string Target, string ExpectedRev, ImmutableArray<Stmt> Body, Token Token)
    : Stmt(Token.Position)
{
    public override AstNode DeepClone() =>
        new SemanticPatchStmt(Target, ExpectedRev, [.. Body.Select(b => (Stmt)b.DeepClone())], (Token)Token.DeepClone());
}

/// <summary>Classe base astratta per tutti i nodi di espressione nell'AST.</summary>
public abstract record Expr(SourcePosition Position) : AstNode(Position);

/// <summary>
/// Classe base astratta per un elemento di dichiarazione che definisce un simbolo.
/// </summary>
[JsonDerivedType(typeof(Parameter), nameof(Parameter))]
[JsonDerivedType(typeof(PropertyDefinition), nameof(PropertyDefinition))]
[JsonDerivedType(typeof(TypeField), nameof(TypeField))]
[JsonDerivedType(typeof(ProcedureDefinition), nameof(ProcedureDefinition))]
public abstract record Decl(SourcePosition Position) : AstNode(Position)
{
    /// <summary>Namespace dichiarato per questa dichiarazione.</summary>
    public string? DeclaredNamespace { get; init; }
}

/// <summary>Rappresenta un singolo token lessicale del sorgente BASIC.</summary>
public record Token(TokenKind Kind, string Lexeme, object? Literal, SourcePosition Position) : AstNode(Position)
{
    /// <inheritdoc />
    public override string ToString() => $"<{Kind}:'{Lexeme}'>";

    /// <inheritdoc />
    public override AstNode DeepClone() => this with { };
}

public enum ImportMode
{
    Namespace,
    AllMembers,
    SpecificMembers
}

public record ImportedMember(Token OriginalName, Token? Alias)
{
    public override string ToString() =>
        Alias != null ? $"{OriginalName.Lexeme} AS {Alias.Lexeme}" : OriginalName.Lexeme;
}

public record ImportStmt(
    Token NamespaceName,
    Token? Alias,
    ImportMode Mode,
    ImmutableArray<ImportedMember> SpecificMembers
) : Stmt(NamespaceName.Position)
{
    public override string ToString()
    {
        return Mode switch
        {
            ImportMode.Namespace => Alias != null
                ? $"IMPORT {NamespaceName.Lexeme} AS {Alias.Lexeme}"
                : $"IMPORT {NamespaceName.Lexeme} WITH ALIAS",
            ImportMode.AllMembers => $"IMPORT {NamespaceName.Lexeme}",
            ImportMode.SpecificMembers =>
                $"IMPORT {NamespaceName.Lexeme}.{{{string.Join(", ", SpecificMembers)}}}",
            _ => "IMPORT (invalid)"
        };
    }

    public override AstNode DeepClone()
    {
        var clonedNamespaceName = (Token)NamespaceName.DeepClone();
        var clonedAlias = (Token?)Alias?.DeepClone();
        var clonedSpecificMembers = SpecificMembers
            .Select(m => new ImportedMember((Token)m.OriginalName.DeepClone(), (Token?)m.Alias?.DeepClone()))
            .ToImmutableArray();
        return new ImportStmt(clonedNamespaceName, clonedAlias, Mode, clonedSpecificMembers);
    }
}

[JsonDerivedType(typeof(RequireStmt), nameof(RequireStmt))]
public record RequireStmt(StringExpr PackageName, StringExpr? VersionSpecifier, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        VersionSpecifier is not null
            ? $"REQUIRE {PackageName} @ {VersionSpecifier}"
            : $"REQUIRE {PackageName}";

    public override AstNode DeepClone()
    {
        var clonedPackageName = (StringExpr)PackageName.DeepClone();
        var clonedVersionSpecifier = (StringExpr?)VersionSpecifier?.DeepClone();
        var clonedToken = (Token)Token.DeepClone();
        return new RequireStmt(clonedPackageName, clonedVersionSpecifier, clonedToken);
    }
}

/// <summary>
/// Rappresenta una singola assegnazione 'destinazione = espressione'.
/// </summary>
public record Assignment(Target Target, Expr Value)
{
    public override string ToString() => $"{Target} = {Value}";
}
