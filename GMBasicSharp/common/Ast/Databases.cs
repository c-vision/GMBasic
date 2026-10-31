// FILE: Interpreter.Ast.Databases.cs

using System.Collections.Immutable;
using System.Text;

// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

// ====================================================================================================================
// --- SEZIONE 1: ENUM E CLAUSOLE COMUNI ---
// ====================================================================================================================
// Nota di Progettazione: Questa sezione contiene elementi sintattici condivisi
// tra più comandi di database, come i livelli di isolamento e le clausole di scopo.

/// <summary>
/// Rappresenta i livelli di isolamento delle transazioni specificabili nella sintassi del linguaggio.
/// </summary>
public enum TransactionIsolationLevel
{
    Default,
    ReadCommitted,
    RepeatableRead,
    Serializable
}

/// <summary>
/// Rappresenta una clausola di scopo per comandi come REPLACE, DELETE, etc.
/// </summary>
[JsonDerivedType(typeof(DbfForClause), nameof(DbfForClause))]
[JsonDerivedType(typeof(DbfWhileClause), nameof(DbfWhileClause))]
[JsonDerivedType(typeof(DbfNextClause), nameof(DbfNextClause))]
[JsonDerivedType(typeof(DbfRecordClause), nameof(DbfRecordClause))]
[JsonDerivedType(typeof(DbfRestClause), nameof(DbfRestClause))]
[JsonDerivedType(typeof(DbfAllClause), nameof(DbfAllClause))]
public abstract record DbfScopeClause(SourcePosition Position) : AstNode(Position)
{
    /// <summary>
    /// Costruttore di default per clausole senza una posizione specifica (es. DbfAllClause).
    /// </summary>
    protected DbfScopeClause() : this(SourcePosition.Unknown)
    {
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo AstNode.
    /// Questo metodo è astratto e deve essere implementato dalle classi derivate.
    /// </summary>
    /// <returns>Una nuova istanza di AstNode che è una copia completa dell'originale.</returns>
    public abstract override AstNode DeepClone();
}

/// <summary>
/// Rappresenta la clausola FOR condition.
/// </summary>
public sealed record DbfForClause(Expr Condition) : DbfScopeClause(Condition.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola FOR.
    /// </summary>
    public override string ToString() => $"FOR {Condition}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfForClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfForClause con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new DbfForClause((Expr)Condition.DeepClone());
    }
}

/// <summary>
/// Rappresenta la clausola WHILE condition.
/// </summary>
public sealed record DbfWhileClause(Expr Condition) : DbfScopeClause(Condition.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola WHILE.
    /// </summary>
    public override string ToString() => $"WHILE {Condition}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfWhileClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfWhileClause con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new DbfWhileClause((Expr)Condition.DeepClone());
    }
}

/// <summary>
/// Rappresenta la clausola NEXT n.
/// </summary>
public sealed record DbfNextClause(Expr Count) : DbfScopeClause(Count.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola NEXT.
    /// </summary>
    public override string ToString() => $"NEXT {Count}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfNextClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfNextClause con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new DbfNextClause((Expr)Count.DeepClone());
    }
}

/// <summary>
/// Rappresenta la clausola ALL.
/// </summary>
public sealed record DbfAllClause : DbfScopeClause
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola ALL.
    /// </summary>
    public override string ToString() => "ALL";

    /// <summary>
    /// Inizializza una nuova istanza di DbfAllClause.
    /// </summary>
    public DbfAllClause() // Chiama il costruttore protetto di DbfScopeClause
    {
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfAllClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfAllClause.</returns>
    public override AstNode DeepClone()
    {
        return new DbfAllClause();
    }
}

/// <summary>
/// Rappresenta la clausola RECORD n — un singolo record specifico.
/// </summary>
public sealed record DbfRecordClause(Expr RecordNumber) : DbfScopeClause(RecordNumber.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola RECORD.
    /// </summary>
    public override string ToString() => $"RECORD {RecordNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfRecordClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfRecordClause con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new DbfRecordClause((Expr)RecordNumber.DeepClone());
    }
}

/// <summary>
/// Rappresenta la clausola REST — dal record corrente alla fine.
/// </summary>
public sealed record DbfRestClause : DbfScopeClause
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola REST.
    /// </summary>
    public override string ToString() => "REST";

    /// <summary>
    /// Inizializza una nuova istanza di DbfRestClause.
    /// </summary>
    public DbfRestClause()
    {
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfRestClause.
    /// </summary>
    /// <returns>Una nuova istanza di DbfRestClause.</returns>
    public override AstNode DeepClone()
    {
        return new DbfRestClause();
    }
}

/// <summary>
/// Rappresenta un singolo campo nella clausola ON di un'istruzione SORT.
/// Questa versione è aggiornata per utilizzare un SymbolId per identificare il campo
/// e supportare un'espressione (es. Macro) per la definizione dinamica.
/// </summary>
public sealed record DbfSortField(int FieldSymbolId, Token OriginalToken, bool Descending, Expr? FieldExpression = null)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del campo di ordinamento per il debug.
    /// </summary>
    public override string ToString() =>
        FieldExpression != null
            ? $"{FieldExpression}{(Descending ? " DESC" : "")}"
            : $"{OriginalToken.Lexeme}#{FieldSymbolId}{(Descending ? " DESC" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo campo di ordinamento.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSortField con copie dei suoi figli.</returns>
    public DbfSortField DeepClone()
    {
        return this with
        {
            OriginalToken = (Token)OriginalToken.DeepClone(),
            FieldExpression = (Expr?)FieldExpression?.DeepClone()
        };
    }
}

/// <summary>
/// Rappresenta un singolo campo nella clausola INDEXED BY di un'istruzione OPEN ISAM o USE ISAM,
/// oppure un campo di ordinamento in un'istruzione ISAM SORT TO.
/// Ora supporta espressioni complesse per la chiave e il verso di ordinamento.
/// </summary>
[JsonDerivedType(typeof(IsamIndexField), nameof(IsamIndexField))]
public sealed record IsamIndexField(Expr KeyExpression, bool IsUnsafe, bool IsDescending)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder();
        sb.Append(KeyExpression);
        if (IsUnsafe)
        {
            sb.Append(" UNSAFE");
        }

        // Aggiunge DESC solo se specificato, per coerenza con la sintassi SQL-like
        if (IsDescending)
        {
            sb.Append(" DESC");
        }

        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo oggetto IsamIndexField.
    /// </summary>
    /// <returns>Una nuova istanza di IsamIndexField con una copia della sua espressione chiave.</returns>
    public IsamIndexField DeepClone()
    {
        return this with { KeyExpression = (Expr)KeyExpression.DeepClone() };
    }
}

/// <summary>
/// Rappresenta la clausola `INDEXED BY` nell'istruzione `USE ISAM` o `OPEN ISAM`.
/// </summary>
[JsonDerivedType(typeof(IsamIndexedByClause), nameof(IsamIndexedByClause))]
public sealed record IsamIndexedByClause(ImmutableArray<IsamIndexField> KeyFields)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola.
    /// </summary>
    public override string ToString() =>
        $"INDEXED BY {string.Join(", ", KeyFields.Select(f => f.ToString()))}";

    /// <summary>
    /// Crea una copia profonda di questa clausola.
    /// </summary>
    /// <returns>Una nuova istanza di IsamIndexedByClause con copie di tutti i suoi campi di chiave.</returns>
    public IsamIndexedByClause DeepClone()
    {
        return new IsamIndexedByClause([..KeyFields.Select(f => f.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta una singola clausola `FIELD WITH value` in un'istruzione REPLACE.
/// Questa versione è aggiornata per utilizzare un SymbolId per identificare il campo.
/// </summary>
[JsonDerivedType(typeof(DbfReplacement), nameof(DbfReplacement))]
public sealed record DbfReplacement(int FieldSymbolId, Token NameSuffixed, Token NameCleaned, Expr Value)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della sostituzione per il debug.
    /// </summary>
    public override string ToString() => $"{NameSuffixed.Lexeme}#{FieldSymbolId} WITH {Value}";

    /// <summary>
    /// Crea una copia profonda di questo oggetto DbfReplacement.
    /// </summary>
    /// <returns>Una nuova istanza di DbfReplacement con copie di tutti i suoi figli.</returns>
    public DbfReplacement DeepClone()
    {
        return new DbfReplacement(
            FieldSymbolId,
            (Token)NameSuffixed.DeepClone(),
            (Token)NameCleaned.DeepClone(),
            (Expr)Value.DeepClone());
    }
}

/// <summary>
/// Rappresenta la clausola `FIELDS lista_campi`.
/// Questa versione è stata aggiornata per contenere espressioni (Expr) invece di token,
/// permettendo al parser di gestire nomi qualificati come "Tabella.Campo".
/// </summary>
[JsonDerivedType(typeof(DbfFieldsClause), nameof(DbfFieldsClause))]
public sealed record DbfFieldsClause(ImmutableArray<Expr> FieldExpressions)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della clausola FIELDS per il debug.
    /// </summary>
    public override string ToString() => $"FIELDS {string.Join(", ", FieldExpressions)}";

    /// <summary>
    /// Crea una copia profonda di questa clausola.
    /// </summary>
    /// <returns>Una nuova istanza di DbfFieldsClause con copie di tutte le sue espressioni.</returns>
    public DbfFieldsClause DeepClone()
    {
        var clonedExpressions = FieldExpressions.Select(e => (Expr)e.DeepClone()).ToImmutableArray();
        return new DbfFieldsClause(clonedExpressions);
    }
}

// ====================================================================================================================
// --- SEZIONE 2: NODI AST COMUNI A ISAM E DBF ---
// ====================================================================================================================

/// <summary>
/// Rappresenta l'istruzione BEGIN TRANSACTION.
/// </summary>
[JsonDerivedType(typeof(BeginTransactionStmt), nameof(BeginTransactionStmt))]
public sealed record BeginTransactionStmt(TransactionIsolationLevel Level, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        "BEGIN TRANSACTION" + (Level != TransactionIsolationLevel.Default ? $" LEVEL {Level}" : "");

    /// <summary>
    /// Crea una copia profonda di questo nodo BeginTransactionStmt.
    /// </summary>
    /// <returns>Una nuova istanza di BeginTransactionStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new BeginTransactionStmt(Level, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `COMMIT`.
/// </summary>
[JsonDerivedType(typeof(CommitStmt), nameof(CommitStmt))]
public sealed record CommitStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "COMMIT";

    /// <summary>
    /// Crea una copia profonda di questo nodo CommitStmt.
    /// </summary>
    /// <returns>Una nuova istanza di CommitStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new CommitStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `ROLLBACK`.
/// </summary>
[JsonDerivedType(typeof(RollbackStmt), nameof(RollbackStmt))]
public sealed record RollbackStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "ROLLBACK";

    /// <summary>
    /// Crea una copia profonda di questo nodo RollbackStmt.
    /// </summary>
    /// <returns>Una nuova istanza di RollbackStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new RollbackStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione `EXPORT Engine Area TO "file" AS CSV|JSON`.
/// </summary>
[JsonDerivedType(typeof(ExportToFileStmt), nameof(ExportToFileStmt))]
public sealed record ExportToFileStmt(
    Token EngineType,
    Expr Area,
    Expr FilePath,
    Token Format,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"EXPORT {EngineType.Lexeme} {Area} TO {FilePath} AS {Format.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ExportToFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ExportToFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ExportToFileStmt((Token)EngineType.DeepClone(), (Expr)Area.DeepClone(), (Expr)FilePath.DeepClone(),
            (Token)Format.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione `IMPORT Engine Area FROM "file" AS CSV|JSON`.
/// </summary>
[JsonDerivedType(typeof(ImportFromFileStmt), nameof(ImportFromFileStmt))]
public sealed record ImportFromFileStmt(
    Token EngineType,
    Expr Area,
    Expr FilePath,
    Token Format,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"IMPORT {EngineType.Lexeme} {Area} FROM {FilePath} AS {Format.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ImportFromFileStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ImportFromFileStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ImportFromFileStmt((Token)EngineType.DeepClone(), (Expr)Area.DeepClone(), (Expr)FilePath.DeepClone(),
            (Token)Format.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `CONVERT DBF "source.dbf" TO ISAM "dest.isdb" TYPE udtName KEY "pkField" LEN len [UNSAFE]`.
/// Questa versione è aggiornata per utilizzare i SymbolId per il tipo e il campo chiave.
/// </summary>
[JsonDerivedType(typeof(ConvertDbfToIsamStmt), nameof(ConvertDbfToIsamStmt))]
public sealed record ConvertDbfToIsamStmt(
    Expr SourceFile,
    Expr DestinationFile,
    int TypeSymbolId,
    Token OriginalTypeName,
    int PkFieldSymbolId,
    Token OriginalPkFieldName,
    Expr KeyLength,
    bool IsUnsafe,
    Token Token
) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString() => $"CONVERT DBF {SourceFile} TO ISAM {DestinationFile}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ConvertDbfToIsamStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ConvertDbfToIsamStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ConvertDbfToIsamStmt(
            (Expr)SourceFile.DeepClone(),
            (Expr)DestinationFile.DeepClone(),
            TypeSymbolId,
            (Token)OriginalTypeName.DeepClone(),
            PkFieldSymbolId,
            (Token)OriginalPkFieldName.DeepClone(),
            (Expr)KeyLength.DeepClone(),
            IsUnsafe,
            (Token)Token.DeepClone());
    }
}

// ====================================================================================================================
// --- SEZIONE 3: NODI AST SPECIFICI PER ISAM ---
// ====================================================================================================================

/// <summary>
/// Rappresenta l'istruzione specificamente dedicata a `ISAM OPEN`, gestendo
/// tutte le sue clausole (handle, alias, locking, etc.) in un unico nodo AST.
/// </summary>
[JsonDerivedType(typeof(IsamOpenStmt), nameof(IsamOpenStmt))]
public sealed record IsamOpenStmt(
    Expr FileName,
    Token OriginalTypeName,
    Token OriginalAliasToken,
    DatabaseLockingMode Locking,
    bool IsUnsafe,
    IsamIndexedByClause? IndexedBy,
    Expr? EncodingExpr,
    Token Token
) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder("ISAM OPEN ");
        sb.Append(FileName);
        if (Locking != DatabaseLockingMode.Default) sb.Append($" {Locking.ToString().ToUpper()}");
        if (IsUnsafe) sb.Append(" UNSAFE");
        sb.Append($" OF {OriginalTypeName.Lexeme}");
        sb.Append($" ALIAS {OriginalAliasToken.Lexeme}");
        if (IndexedBy != null) sb.Append($" {IndexedBy}");
        if (EncodingExpr != null) sb.Append($" ENCODING {EncodingExpr}");
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamOpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamOpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamOpenStmt(
            (Expr)FileName.DeepClone(),
            (Token)OriginalTypeName.DeepClone(),
            (Token)OriginalAliasToken.DeepClone(),
            Locking,
            IsUnsafe,
            IndexedBy?.DeepClone(),
            (Expr?)EncodingExpr?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `USE "file" OF TypeName [SHARED|EXCLUSIVE] [UNSAFE] [INDEXED BY ...] [ALIAS ...]`.
/// Questa versione è aggiornata per utilizzare un SymbolId per il tipo e per l'alias.
/// </summary>
[JsonDerivedType(typeof(IsamUseStmt), nameof(IsamUseStmt))]
public sealed record IsamUseStmt(
    Expr FileName,
    int TypeSymbolId,
    Token OriginalTypeName,
    DatabaseLockingMode Locking,
    bool IsUnsafe,
    IsamIndexedByClause? IndexedBy,
    int AliasSymbolId, // Reso non-nullo
    Token OriginalAliasToken, // Reso non-nullo
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        return $"USE {FileName} OF {OriginalTypeName.Lexeme}#{TypeSymbolId} ALIAS {OriginalAliasToken.Lexeme}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamUseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamUseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamUseStmt(
            (Expr)FileName.DeepClone(),
            TypeSymbolId,
            (Token)OriginalTypeName.DeepClone(),
            Locking,
            IsUnsafe,
            IndexedBy?.DeepClone(),
            AliasSymbolId,
            (Token)OriginalAliasToken.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SELECT area` per una work area ISAM.
/// </summary>
[JsonDerivedType(typeof(IsamSelectWorkAreaStmt), nameof(IsamSelectWorkAreaStmt))]
public sealed record IsamSelectWorkAreaStmt(Expr WorkArea, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SELECT {WorkArea}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSelectWorkAreaStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSelectWorkAreaStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSelectWorkAreaStmt((Expr)WorkArea.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `GO #filenum, TOP|BOTTOM`.
/// </summary>
[JsonDerivedType(typeof(IsamRecordStmt), nameof(IsamRecordStmt))]
public sealed record IsamRecordStmt(Expr FileNumber, Token Location, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"GO #{FileNumber}, {Location.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamRecordStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamRecordStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamRecordStmt((Expr)FileNumber.DeepClone(), (Token)Location.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SKIP #channel [, n]`.
/// </summary>
[JsonDerivedType(typeof(IsamSkipStmt), nameof(IsamSkipStmt))]
public sealed record IsamSkipStmt(Expr FileNumber, Expr? RecordCount, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SKIP #{FileNumber}{(RecordCount != null ? $", {RecordCount}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSkipStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSkipStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSkipStmt((Expr)FileNumber.DeepClone(), (Expr?)RecordCount?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SEEK #filenum, keyExpression` per i file ISAM.
/// </summary>
[JsonDerivedType(typeof(IsamSeekStmt), nameof(IsamSeekStmt))]
public sealed record IsamSeekStmt(Expr FileNumber, Expr PositionExpr, Expr? IndexName, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"SEEK #{FileNumber}, {PositionExpr}{(IndexName != null ? $" USING {IndexName}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSeekStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSeekStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSeekStmt(
            (Expr)FileNumber.DeepClone(),
            (Expr)PositionExpr.DeepClone(),
            (Expr?)IndexName?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DELETE #channel [SCOPE]`.
/// </summary>
[JsonDerivedType(typeof(IsamDeleteStmt), nameof(IsamDeleteStmt))]
public sealed record IsamDeleteStmt(Expr FileNumber, DbfScopeClause? Scope, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"DELETE #{FileNumber}{(Scope != null ? $" {Scope}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamDeleteStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamDeleteStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamDeleteStmt((Expr)FileNumber.DeepClone(), (DbfScopeClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `RECALL #channel [SCOPE]`.
/// </summary>
[JsonDerivedType(typeof(IsamRecallStmt), nameof(IsamRecallStmt))]
public sealed record IsamRecallStmt(Expr FileNumber, DbfScopeClause? Scope, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"RECALL #{FileNumber}{(Scope != null ? $" {Scope}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamRecallStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamRecallStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamRecallStmt((Expr)FileNumber.DeepClone(), (DbfScopeClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `PACK #channel`.
/// </summary>
[JsonDerivedType(typeof(IsamPackStmt), nameof(IsamPackStmt))]
public sealed record IsamPackStmt(Expr FileNumber, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"PACK #{FileNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamPackStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamPackStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamPackStmt((Expr)FileNumber.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `REINDEX #channel`.
/// </summary>
[JsonDerivedType(typeof(IsamReindexStmt), nameof(IsamReindexStmt))]
public sealed record IsamReindexStmt(Expr FileNumber, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"REINDEX #{FileNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamReindexStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamReindexStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamReindexStmt((Expr)FileNumber.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `REPAIR #channel`.
/// </summary>
[JsonDerivedType(typeof(IsamRepairStmt), nameof(IsamRepairStmt))]
public sealed record IsamRepairStmt(Expr FileNumber, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"REPAIR #{FileNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamRepairStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamRepairStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamRepairStmt((Expr)FileNumber.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `BACKUP #channel TO "filename"`.
/// </summary>
[JsonDerivedType(typeof(IsamBackupStmt), nameof(IsamBackupStmt))]
public sealed record IsamBackupStmt(Expr FileNumber, Expr DestinationFile, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"BACKUP #{FileNumber} TO {DestinationFile}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamBackupStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamBackupStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamBackupStmt((Expr)FileNumber.DeepClone(), (Expr)DestinationFile.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `APPEND #channel, udtVar`.
/// </summary>
[JsonDerivedType(typeof(IsamAppendStmt), nameof(IsamAppendStmt))]
public sealed record IsamAppendStmt(Expr FileNumber, Expr Source, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"APPEND #{FileNumber}, {Source}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamAppendStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamAppendStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamAppendStmt((Expr)FileNumber.DeepClone(), (Expr)Source.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `PUT alias [, udtVar]` per il motore ISAM — modifica il record
/// corrente. Prima il parser produceva un <see cref="PutFileStmt"/> (I/O file) per `ISAM PUT`,
/// che l'emitter IL traduceva in `PutFile` (handle file invalido). Questo nodo dedicato
/// permette all'emitter IL di delegare a <c>IlRuntime.IsamPut</c> come la PCodeVM (ISAM_PUT).
/// </summary>
[JsonDerivedType(typeof(IsamPutStmt), nameof(IsamPutStmt))]
public sealed record IsamPutStmt(Expr Alias, Expr? Source, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ISAM PUT {Alias}" + (Source != null ? $", {Source}" : "");

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamPutStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamPutStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamPutStmt((Expr)Alias.DeepClone(), (Expr?)Source?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `LOCK #channel [, RECORD]`.
/// </summary>
[JsonDerivedType(typeof(IsamLockStmt), nameof(IsamLockStmt))]
public sealed record IsamLockStmt(Expr FileNumber, bool IsRecordLock, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LOCK #{FileNumber}{(IsRecordLock ? " RECORD" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamLockStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamLockStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamLockStmt((Expr)FileNumber.DeepClone(), IsRecordLock, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `UNLOCK #channel [, RECORD]`.
/// </summary>
[JsonDerivedType(typeof(IsamUnlockStmt), nameof(IsamUnlockStmt))]
public sealed record IsamUnlockStmt(Expr FileNumber, bool IsRecordLock, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UNLOCK #{FileNumber}{(IsRecordLock ? " RECORD" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamUnlockStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamUnlockStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamUnlockStmt((Expr)FileNumber.DeepClone(), IsRecordLock, (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET ORDER ON #filenum, "nomeIndice"` specifica per il namespace ISAM.
/// </summary>
[JsonDerivedType(typeof(IsamSetOrderStmt), nameof(IsamSetOrderStmt))]
public sealed record IsamSetOrderStmt(Expr FileNumber, Expr IndexName, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET ORDER ON #{FileNumber} TO {IndexName}"
            : $"{Token.Lexeme}({FileNumber}, {IndexName})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSetOrderStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSetOrderStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSetOrderStmt((Expr)FileNumber.DeepClone(), (Expr)IndexName.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET RELATION ON #filenum TO espressione INTO alias`.
/// </summary>
[JsonDerivedType(typeof(IsamSetRelationStmt), nameof(IsamSetRelationStmt))]
public sealed record IsamSetRelationStmt(Expr FileNumber, Expr? KeyExpression, Token? ChildAlias, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        // Gestisce sia la sintassi qualificata (basata su Lexeme) che quella legacy
        if (Token.Lexeme.Equals("SETRELATION", StringComparison.OrdinalIgnoreCase))
        {
            // Sintassi Qualificata
            return KeyExpression == null
                ?
                // Rimozione della relazione: ISAM.SETRELATION(handle)
                $"{Token.Lexeme}({FileNumber})"
                :
                // Creazione della relazione: ISAM.SETRELATION(handle, key, "alias")
                $"{Token.Lexeme}({FileNumber}, {KeyExpression}, \"{ChildAlias?.Lexeme ?? "n/a"}\")";
        }

        // Sintassi Legacy
        return KeyExpression == null
            ?
            // Rimozione della relazione: SET RELATION ON #handle TO
            $"SET RELATION ON #{FileNumber} TO"
            :
            // Creazione della relazione: SET RELATION ON #handle TO key INTO alias
            $"SET RELATION ON #{FileNumber} TO {KeyExpression} INTO {ChildAlias?.Lexeme ?? "n/a"}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSetRelationStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSetRelationStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSetRelationStmt((Expr)FileNumber.DeepClone(), (Expr?)KeyExpression?.DeepClone(),
            (Token?)ChildAlias?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione CREATE INDEX per un file ISAM.
/// </summary>
[JsonDerivedType(typeof(IsamCreateIndexStmt), nameof(IsamCreateIndexStmt))]
public sealed record IsamCreateIndexStmt(
    Expr FileNumber,
    Expr IndexName,
    Expr KeyExpression,
    Expr KeyLength,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CREATE INDEX {IndexName} ON #{FileNumber} KEY {KeyExpression}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamCreateIndexStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamCreateIndexStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamCreateIndexStmt((Expr)FileNumber.DeepClone(), (Expr)IndexName.DeepClone(),
            (Expr)KeyExpression.DeepClone(), (Expr)KeyLength.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DROP INDEX "indexName" ON #filenum`.
/// </summary>
[JsonDerivedType(typeof(IsamDropIndexStmt), nameof(IsamDropIndexStmt))]
public sealed record IsamDropIndexStmt(Expr FileNumber, Expr IndexName, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"DROP INDEX {IndexName} ON #{FileNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamDropIndexStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamDropIndexStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamDropIndexStmt((Expr)FileNumber.DeepClone(), (Expr)IndexName.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `ISAM VALIDATE #filenum`.
/// </summary>
[JsonDerivedType(typeof(IsamValidateStmt), nameof(IsamValidateStmt))]
public sealed record IsamValidateStmt(Expr FileNumber, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ISAM VALIDATE #{FileNumber}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamValidateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamValidateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamValidateStmt((Expr)FileNumber.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `ISAM COPY TO "destinationFile" ON #fileNumber [FOR condition]`.
/// </summary>
[JsonDerivedType(typeof(IsamCopyToStmt), nameof(IsamCopyToStmt))]
public sealed record IsamCopyToStmt(
    Expr FileNumber,
    Expr DestinationFile,
    DbfForClause? Scope,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"ISAM COPY TO {DestinationFile} ON #{FileNumber}{(Scope != null ? $" {Scope}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamCopyToStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamCopyToStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamCopyToStmt((Expr)FileNumber.DeepClone(), (Expr)DestinationFile.DeepClone(),
            (DbfForClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `ISAM JOIN WITH alias TO "destinationFile" ON #fileNumber FOR condition`.
/// </summary>
[JsonDerivedType(typeof(IsamJoinWithStmt), nameof(IsamJoinWithStmt))]
public sealed record IsamJoinWithStmt(
    Expr FileNumber,
    int SecondaryAliasSymbolId,
    Token SecondaryAliasToken,
    Expr DestinationFile,
    DbfForClause Condition,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"ISAM JOIN WITH {SecondaryAliasToken.Lexeme} TO {DestinationFile} ON #{FileNumber} FOR {Condition.Condition}";

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamJoinWithStmt.
    /// </summary>
    public override AstNode DeepClone() =>
        new IsamJoinWithStmt(
            (Expr)FileNumber.DeepClone(),
            SecondaryAliasSymbolId,
            (Token)SecondaryAliasToken.DeepClone(),
            (Expr)DestinationFile.DeepClone(),
            (DbfForClause)Condition.DeepClone(),
            (Token)Token.DeepClone());
}

/// <summary>
/// Rappresenta l'istruzione `ISAM SORT TO "destinationFile" ON #fileNumber ON fieldList [FOR condition]`.
/// Ora supporta espressioni complesse per i campi di ordinamento.
/// </summary>
[JsonDerivedType(typeof(IsamSortToStmt), nameof(IsamSortToStmt))]
public sealed record IsamSortToStmt(
    Expr FileNumber,
    Expr DestinationFile,
    ImmutableArray<IsamIndexField> SortFields,
    DbfForClause? Scope,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder("ISAM SORT TO ");
        sb.Append(DestinationFile);
        sb.Append(" ON #");
        sb.Append(FileNumber);
        sb.Append(" ON ");
        sb.Append(string.Join(", ", SortFields.Select(f => f.ToString())));
        if (Scope is null) return sb.ToString();
        sb.Append(' ');
        sb.Append(Scope);
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSortToStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSortToStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSortToStmt(
            (Expr)FileNumber.DeepClone(),
            (Expr)DestinationFile.DeepClone(),
            [..SortFields.Select(f => f.DeepClone())],
            (DbfForClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET FILTER TO [condizione] ON #filenum`.
/// </summary>
[JsonDerivedType(typeof(IsamSetFilterStmt), nameof(IsamSetFilterStmt))]
public sealed record IsamSetFilterStmt(Expr FileNumber, Expr? Condition, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var conditionStr = Condition != null ? Condition.ToString() : "(none)";
        if (Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase))
        {
            return $"SET FILTER ON #{FileNumber} TO {conditionStr}";
        }

        return Condition != null ? $"{Token.Lexeme}({FileNumber}, {conditionStr})" : $"{Token.Lexeme}({FileNumber})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo IsamSetFilterStmt.
    /// </summary>
    /// <returns>Una nuova istanza di IsamSetFilterStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new IsamSetFilterStmt((Expr)FileNumber.DeepClone(), (Expr?)Condition?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

// ====================================================================================================================
// --- SEZIONE 4: NODI AST SPECIFICI PER DBF ---
// ====================================================================================================================

/// <summary>
/// Rappresenta l'istruzione `USE "file" [SHARED|EXCLUSIVE] [UNSAFE] [INDEX ...] [ALIAS ...] [AS TypeName]`.
/// Questa versione è aggiornata per utilizzare un SymbolId per il tipo e per l'alias.
/// </summary>
[JsonDerivedType(typeof(DbfUseStmt), nameof(DbfUseStmt))]
public sealed record DbfUseStmt(
    Expr FileName,
    DatabaseLockingMode Locking,
    bool IsUnsafe,
    bool IsNew, // NUOVO FLAG
    ImmutableArray<Expr> IndexPaths,
    int? AliasSymbolId, // RESO NULLABLE
    Token? OriginalAliasToken, // RESO NULLABLE
    int TypeSymbolId,
    Token? OriginalTypeName,
    /// <summary>Maps TYPE field name (key) to physical DBF column name (value).</summary>
    ImmutableDictionary<string, string> FieldMappings,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder("USE ");
        sb.Append(FileName);
        if (Locking != DatabaseLockingMode.Default) sb.Append($" {Locking.ToString().ToUpper()}");
        if (IsUnsafe) sb.Append(" UNSAFE");
        if (IsNew) sb.Append(" NEW");
        if (IndexPaths.Any()) sb.Append(" INDEX ...");
        if (OriginalAliasToken != null) sb.Append($" ALIAS {OriginalAliasToken.Lexeme}");
        if (OriginalTypeName != null) sb.Append($" AS {OriginalTypeName.Lexeme}#{TypeSymbolId}");
        if (FieldMappings.Count > 0)
        {
            sb.Append(" MAP { ");
            sb.Append(string.Join(", ", FieldMappings.Select(kvp => $"{kvp.Value} -> {kvp.Key}")));
            sb.Append(" }");
        }

        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfUseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfUseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedIndexPaths = IndexPaths.Select(p => (Expr)p.DeepClone()).ToImmutableArray();
        return new DbfUseStmt(
            (Expr)FileName.DeepClone(),
            Locking,
            IsUnsafe,
            IsNew,
            clonedIndexPaths,
            AliasSymbolId,
            (Token?)OriginalAliasToken?.DeepClone(),
            TypeSymbolId,
            (Token?)OriginalTypeName?.DeepClone(),
            FieldMappings,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SELECT area`.
/// </summary>
[JsonDerivedType(typeof(DbfSelectWorkAreaStmt), nameof(DbfSelectWorkAreaStmt))]
public sealed record DbfSelectWorkAreaStmt(Expr WorkArea, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SELECT {WorkArea}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSelectWorkAreaStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSelectWorkAreaStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSelectWorkAreaStmt((Expr)WorkArea.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `GET [variabile]` per i file DBF.
/// `Destination` è nullable per supportare `GET` senza argomenti (stile Clipper).
/// </summary>
[JsonDerivedType(typeof(DbfGetRecordStmt), nameof(DbfGetRecordStmt))]
public sealed record DbfGetRecordStmt(Target? Destination, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Destination != null ? $"GET {Destination}" : "GET";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfGetRecordStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfGetRecordStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfGetRecordStmt((Target?)Destination?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `REPLACE ... [SCOPE]`.
/// </summary>
[JsonDerivedType(typeof(DbfReplaceStmt), nameof(DbfReplaceStmt))]
public sealed record DbfReplaceStmt(ImmutableArray<Assignment> Replacements, DbfScopeClause? Scope, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"REPLACE {string.Join(", ", Replacements)} {(Scope != null ? Scope.ToString() : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfReplaceStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        var clonedReplacements = Replacements
            .Select(r => new Assignment((Target)r.Target.DeepClone(), (Expr)r.Value.DeepClone()))
            .ToImmutableArray();
        return new DbfReplaceStmt(clonedReplacements, (DbfScopeClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DELETE [SCOPE]` per la work area DBF.
/// </summary>
[JsonDerivedType(typeof(DbfDeleteStmt), nameof(DbfDeleteStmt))]
public sealed record DbfDeleteStmt(DbfScopeClause? Scope, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"DELETE {(Scope != null ? Scope.ToString() : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfDeleteStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfDeleteStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfDeleteStmt((DbfScopeClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `RECALL [SCOPE]` per la work area DBF.
/// </summary>
[JsonDerivedType(typeof(DbfRecallStmt), nameof(DbfRecallStmt))]
public sealed record DbfRecallStmt(DbfScopeClause? Scope, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"RECALL {(Scope != null ? Scope.ToString() : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfRecallStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfRecallStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfRecallStmt((DbfScopeClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SORT TO file ON fieldList [FOR cond]`.
/// </summary>
[JsonDerivedType(typeof(DbfSortStmt), nameof(DbfSortStmt))]
public sealed record DbfSortStmt(
    Expr FileName,
    ImmutableArray<DbfSortField> SortFields,
    DbfForClause? Scope,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SORT TO {FileName} ON {SortFields.FirstOrDefault()}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSortStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSortStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedSortFields = SortFields.Select(f => f.DeepClone()).ToImmutableArray();
        return new DbfSortStmt((Expr)FileName.DeepClone(), clonedSortFields, (DbfForClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `UPDATE ON key FROM alias REPLACE ...`.
/// Questa versione è aggiornata per utilizzare un SymbolId per l'alias.
/// </summary>
[JsonDerivedType(typeof(DbfUpdateStmt), nameof(DbfUpdateStmt))]
public sealed record DbfUpdateStmt(
    Expr KeyExpression,
    int AliasSymbolId,
    Token OriginalAliasToken,
    ImmutableArray<DbfReplacement> Replacements,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        // Sintassi Qualificata: DBF.UPDATEON("key", "alias", {"field": "value_expr_string"})
        if (Token.Lexeme.Equals("UPDATEON", StringComparison.OrdinalIgnoreCase))
        {
            // Ricostruisce l'aspetto della chiamata a funzione, mostrando le espressioni come stringhe
            var replacementsStr =
                string.Join(", ", Replacements.Select(r => $"\"{r.NameSuffixed.Lexeme}\": \"{r.Value}\""));
            return $"UPDATEON(\"{KeyExpression}\", \"{OriginalAliasToken.Lexeme}\", {{ {replacementsStr} }})";
        }

        // Sintassi Legacy: UPDATE ON key FROM alias REPLACE field WITH value_expr
        var legacyReplacements =
            string.Join(", ", Replacements.Select(r => $"{r.NameSuffixed.Lexeme} WITH {r.Value}"));
        return $"UPDATE ON {KeyExpression} FROM {OriginalAliasToken.Lexeme} REPLACE {legacyReplacements}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfUpdateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfUpdateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedReplacements = Replacements
            .Select(r => r.DeepClone())
            .ToImmutableArray();
        return new DbfUpdateStmt(
            (Expr)KeyExpression.DeepClone(),
            AliasSymbolId,
            (Token)OriginalAliasToken.DeepClone(),
            clonedReplacements,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `TOTAL ON keyExpr TO fileName [FIELDS ...] [FOR ...]`.
/// </summary>
[JsonDerivedType(typeof(DbfTotalOnStmt), nameof(DbfTotalOnStmt))]
public sealed record DbfTotalOnStmt(
    Expr KeyExpression,
    Expr FileName,
    DbfFieldsClause? Fields,
    DbfForClause? Scope,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"TOTAL ON {KeyExpression} TO {FileName}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfTotalOnStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfTotalOnStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFields = Fields?.DeepClone();
        return new DbfTotalOnStmt((Expr)KeyExpression.DeepClone(), (Expr)FileName.DeepClone(), clonedFields,
            (DbfForClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `PACK` per la work area DBF.
/// </summary>
[JsonDerivedType(typeof(DbfPackStmt), nameof(DbfPackStmt))]
public sealed record DbfPackStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "PACK";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfPackStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfPackStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new DbfPackStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `ZAP` per la work area DBF.
/// </summary>
[JsonDerivedType(typeof(DbfZapStmt), nameof(DbfZapStmt))]
public sealed record DbfZapStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "ZAP";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfZapStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfZapStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new DbfZapStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `GO TOP`, `GO BOTTOM`, o `GO recordNumber` per i file DBF.
/// </summary>
[JsonDerivedType(typeof(DbfGoStmt), nameof(DbfGoStmt))]
public sealed record DbfGoStmt(Expr Destination, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"GO {Destination}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfGoStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfGoStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfGoStmt((Expr)Destination.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SKIP [n]` per i file DBF.
/// </summary>
[JsonDerivedType(typeof(DbfSkipStmt), nameof(DbfSkipStmt))]
public sealed record DbfSkipStmt(Expr? RecordCount, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "SKIP";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSkipStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSkipStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSkipStmt((Expr?)RecordCount?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SEEK valore` per i file DBF.
/// </summary>
[JsonDerivedType(typeof(DbfSeekStmt), nameof(DbfSeekStmt))]
public sealed record DbfSeekStmt(Expr Value, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SEEK {Value}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSeekStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSeekStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSeekStmt((Expr)Value.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `LIST` per visualizzare i record di un database DBF.
/// </summary>
[JsonDerivedType(typeof(DbfListStmt), nameof(DbfListStmt))]
public sealed record DbfListStmt(
    DbfScopeClause? Scope,
    DbfFieldsClause? Fields,
    Expr? Destination,
    bool ToPrint,
    Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        var sb = new StringBuilder("LIST");
        if (Fields != null) sb.Append($" {Fields}");
        if (Scope != null) sb.Append($" {Scope}");
        if (ToPrint) sb.Append(" TO PRINT");
        else if (Destination != null) sb.Append($" TO FILE {Destination}");
        return sb.ToString();
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfListStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfListStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfListStmt(
            (DbfScopeClause?)Scope?.DeepClone(),
            Fields?.DeepClone(),
            (Expr?)Destination?.DeepClone(),
            ToPrint,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `CLOSE DATABASES` o `CLOSE alias` specifica per DBF.
/// Se WorkAreas è vuoto, significa "chiudi tutto".
/// </summary>
[JsonDerivedType(typeof(DbfCloseStmt), nameof(DbfCloseStmt))]
public sealed record DbfCloseStmt(ImmutableArray<Expr> WorkAreas, Token Token, bool All = false) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        !WorkAreas.IsEmpty ? $"CLOSE {string.Join(", ", WorkAreas)}"
        : All ? "CLOSE DATABASES" : "CLOSE";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfCloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfCloseStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfCloseStmt(
            [.. WorkAreas.Select(wa => (Expr)wa.DeepClone())],
            (Token)Token.DeepClone(),
            All
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET DELETED ON|OFF`.
/// </summary>
[JsonDerivedType(typeof(DbfSetDeletedStmt), nameof(DbfSetDeletedStmt))]
public sealed record DbfSetDeletedStmt(Token State, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET DELETED {State.Lexeme}"
            : $"{Token.Lexeme}({State.Lexeme.ToUpper()})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetDeletedStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetDeletedStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetDeletedStmt((Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET FILTER TO condizione`.
/// </summary>
[JsonDerivedType(typeof(DbfSetFilterStmt), nameof(DbfSetFilterStmt))]
public sealed record DbfSetFilterStmt(Expr? Condition, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SET FILTER TO {Condition}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetFilterStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetFilterStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetFilterStmt((Expr?)Condition?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `COPY TO file [FOR condizione]`.
/// </summary>
[JsonDerivedType(typeof(DbfCopyToStmt), nameof(DbfCopyToStmt))]
public sealed record DbfCopyToStmt(Expr FileName, DbfForClause? Scope, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"COPY TO {FileName} {Scope}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfCopyToStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfCopyToStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfCopyToStmt((Expr)FileName.DeepClone(), (DbfForClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `REINDEX` per la work area DBF.
/// </summary>
[JsonDerivedType(typeof(DbfReindexStmt), nameof(DbfReindexStmt))]
public sealed record DbfReindexStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "REINDEX";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfReindexStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfReindexStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new DbfReindexStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `APPEND RECORD variabile`.
/// </summary>
[JsonDerivedType(typeof(DbfAppendRecordStmt), nameof(DbfAppendRecordStmt))]
public sealed record DbfAppendRecordStmt(Expr Source, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"APPEND RECORD {Source}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfAppendRecordStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfAppendRecordStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfAppendRecordStmt((Expr)Source.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `APPEND FROM file [FOR condizione]`.
/// </summary>
[JsonDerivedType(typeof(DbfAppendFromStmt), nameof(DbfAppendFromStmt))]
public sealed record DbfAppendFromStmt(Expr FileName, DbfForClause? Scope, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"APPEND FROM {FileName} {Scope}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfAppendFromStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfAppendFromStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfAppendFromStmt((Expr)FileName.DeepClone(), (DbfForClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `JOIN WITH alias TO file FOR condizione [FIELDS lista]`.
/// Questa versione è aggiornata per utilizzare un SymbolId per l'alias.
/// </summary>
[JsonDerivedType(typeof(DbfJoinWithStmt), nameof(DbfJoinWithStmt))]
public sealed record DbfJoinWithStmt(
    int AliasSymbolId,
    Token OriginalAliasToken,
    Expr FileName,
    DbfForClause Condition,
    DbfFieldsClause? Fields,
    Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo per il debug.
    /// </summary>
    public override string ToString()
    {
        var fieldsStr = Fields != null
            ? $" FIELDS {string.Join(", ", Fields.FieldExpressions)}"
            : "";

        // Sintassi Qualificata: DBF.JOINWITH("alias", "file", "condizione", ["campi"])
        if (Token.Lexeme.Equals("JOINWITH", StringComparison.OrdinalIgnoreCase))
        {
            var fieldsArg = Fields != null
                ? $", [{string.Join(", ", Fields.FieldExpressions.Select(f => $"\"{f}\""))}]"
                : "";
            return $"JOINWITH(\"{OriginalAliasToken.Lexeme}\", {FileName}, \"{Condition.Condition}\"{fieldsArg})";
        }

        // Sintassi Legacy: JOIN WITH alias TO file FOR condizione [FIELDS ...]
        return $"JOIN WITH {OriginalAliasToken.Lexeme} TO {FileName} FOR {Condition.Condition}{fieldsStr}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfJoinWithStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfJoinWithStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedFields = Fields?.DeepClone();
        return new DbfJoinWithStmt(
            AliasSymbolId,
            (Token)OriginalAliasToken.DeepClone(),
            (Expr)FileName.DeepClone(),
            (DbfForClause)Condition.DeepClone(),
            clonedFields,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta la query SELECT di una vista DBF virtuale (senza clausola INTO).
/// </summary>
[JsonDerivedType(typeof(DbfSelectQuery), nameof(DbfSelectQuery))]
public sealed record DbfSelectQuery(
    ImmutableArray<SelectField>? SelectFields,
    FromClause From,
    JoinClause? Join,
    Expr? Where,
    Token Token)
{
    public override string ToString()
    {
        var sb = new StringBuilder("SELECT ");
        if (SelectFields != null) sb.Append(string.Join(", ", SelectFields));
        sb.Append(' ').Append(From);
        if (Join != null) sb.Append($" {Join}");
        if (Where != null) sb.Append($" WHERE {Where}");
        return sb.ToString();
    }

    public DbfSelectQuery DeepClone()
    {
        ImmutableArray<SelectField>? clonedFields = null;
        if (SelectFields.HasValue)
        {
            clonedFields =
            [
                ..SelectFields.Value.Select(f =>
                    new SelectField((Expr)f.Expression.DeepClone(), (Token?)f.Alias?.DeepClone()))
            ];
        }

        return new DbfSelectQuery(
            clonedFields,
            new FromClause((Token)From.TableAlias.DeepClone(), (Token?)From.AsAlias?.DeepClone()),
            Join != null
                ? new JoinClause((Token)Join.TableAlias.DeepClone(), (Token?)Join.AsAlias?.DeepClone(),
                    (Expr)Join.Condition.DeepClone())
                : null,
            Where != null ? (Expr)Where.DeepClone() : null,
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `CREATE VIEW viewName AS SELECT ... FROM alias [JOIN ...]`.
/// Registra una work area DBF virtuale (non materializzata su disco).
/// </summary>
[JsonDerivedType(typeof(DbfCreateViewStmt), nameof(DbfCreateViewStmt))]
public sealed record DbfCreateViewStmt(
    int ViewSymbolId,
    Token ViewNameToken,
    DbfSelectQuery? SelectQuery,
    Expr? QueryExpression,
    Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        Token.Lexeme.Equals("CREATEVIEW", StringComparison.OrdinalIgnoreCase)
            ? $"CREATEVIEW(\"{ViewNameToken.Lexeme}\", {QueryExpression})"
            : $"CREATE VIEW {ViewNameToken.Lexeme} AS {SelectQuery?.ToString() ?? QueryExpression?.ToString()}";

    public override AstNode DeepClone()
    {
        return new DbfCreateViewStmt(
            ViewSymbolId,
            (Token)ViewNameToken.DeepClone(),
            (DbfSelectQuery?)SelectQuery?.DeepClone(),
            (Expr?)QueryExpression?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `COPY STRUCTURE TO file`.
/// </summary>
[JsonDerivedType(typeof(DbfCopyStructureStmt), nameof(DbfCopyStructureStmt))]
public sealed record DbfCopyStructureStmt(Expr FileName, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"COPY STRUCTURE TO {FileName}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfCopyStructureStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfCopyStructureStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfCopyStructureStmt((Expr)FileName.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET ORDER TO valore`.
/// </summary>
[JsonDerivedType(typeof(DbfSetOrderStmt), nameof(DbfSetOrderStmt))]
public sealed record DbfSetOrderStmt(Expr Value, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
        ? $"SET ORDER TO {Value}"
        : $"{Token.Lexeme}({Value})";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetOrderStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetOrderStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetOrderStmt((Expr)Value.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET RELATION TO espressione INTO alias`.
/// </summary>
[JsonDerivedType(typeof(DbfSetRelationStmt), nameof(DbfSetRelationStmt))]
public sealed record DbfSetRelationStmt(Expr? KeyExpression, Token? ChildAlias, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        if (!Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase))
            return KeyExpression == null
                ? $"{Token.Lexeme}()"
                : $"{Token.Lexeme}({KeyExpression}, \"{ChildAlias?.Lexeme ?? "n/a"}\")";
        return KeyExpression == null
            ? "SET RELATION TO"
            : $"SET RELATION TO {KeyExpression} INTO {ChildAlias?.Lexeme ?? "n/a"}";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetRelationStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetRelationStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetRelationStmt((Expr?)KeyExpression?.DeepClone(), (Token?)ChildAlias?.DeepClone(),
            (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `INDEX ON espressione TO nomeFile`.
/// </summary>
[JsonDerivedType(typeof(DbfIndexOnStmt), nameof(DbfIndexOnStmt))]
public sealed record DbfIndexOnStmt(Expr KeyExpression, Expr FileName, Token Token, Expr? TagName = null) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => TagName is null
        ? $"INDEX ON {KeyExpression} TO {FileName}"
        : $"INDEX ON {KeyExpression} TAG {TagName} OF {FileName}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfIndexOnStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfIndexOnStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfIndexOnStmt(
            (Expr)KeyExpression.DeepClone(),
            (Expr)FileName.DeepClone(),
            (Token)Token.DeepClone(),
            (Expr?)TagName?.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET EXACT ON|OFF`.
/// </summary>
[JsonDerivedType(typeof(DbfSetExactStmt), nameof(DbfSetExactStmt))]
public sealed record DbfSetExactStmt(Token State, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET EXACT {State.Lexeme}"
            : $"{Token.Lexeme}({State.Lexeme.ToUpper()})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetExactStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetExactStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetExactStmt((Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET DEFAULT TO path`.
/// </summary>
[JsonDerivedType(typeof(DbfSetDefaultStmt), nameof(DbfSetDefaultStmt))]
public sealed record DbfSetDefaultStmt(Expr Path, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET DEFAULT TO {Path}"
            : $"{Token.Lexeme}({Path})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetDefaultStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetDefaultStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetDefaultStmt((Expr)Path.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Istruzione `RDD.SETDEFAULT("ISAM")` — imposta il driver database predefinito.
/// </summary>
[JsonDerivedType(typeof(RddSetDefaultStmt), nameof(RddSetDefaultStmt))]
public sealed record RddSetDefaultStmt(Expr DriverName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SETDEFAULT({DriverName})";
    public override AstNode DeepClone() => new RddSetDefaultStmt((Expr)DriverName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SET DELETED ON|OFF` legacy.</summary>
[JsonDerivedType(typeof(RddSetDeletedStmt), nameof(RddSetDeletedStmt))]
public sealed record RddSetDeletedStmt(bool On, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"SET DELETED {(On ? "ON" : "OFF")}";
    public override AstNode DeepClone() => new RddSetDeletedStmt(On, (Token)Token.DeepClone());
}

/// <summary>
/// Istruzione `USE "file" VIA "driver" ALIAS x` — RDD unificato.
/// </summary>
[JsonDerivedType(typeof(RddUseStmt), nameof(RddUseStmt))]
public sealed record RddUseStmt(Expr FileName, Expr Driver, Token? AliasToken, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"USE {FileName} VIA {Driver}" + (AliasToken != null ? $" ALIAS {AliasToken.Lexeme}" : "");
    public override AstNode DeepClone() => new RddUseStmt((Expr)FileName.DeepClone(), (Expr)Driver.DeepClone(),
        (Token?)AliasToken?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Tipo di lock RDD.</summary>
public enum RddLockKind { RLock, FLock, Unlock }

/// <summary>Istruzione RDD.RLOCK / RDD.FLOCK / RDD.UNLOCK.</summary>
[JsonDerivedType(typeof(RddLockStmt), nameof(RddLockStmt))]
public sealed record RddLockStmt(RddLockKind Kind, Token? AreaToken, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.{Kind}";
    public override AstNode DeepClone() => new RddLockStmt(Kind, (Token?)AreaToken?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SET FIELD area, campo, valore`.</summary>
[JsonDerivedType(typeof(RddSetFieldStmt), nameof(RddSetFieldStmt))]
public sealed record RddSetFieldStmt(Expr Area, Expr Field, Expr Value, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SETFIELD({Area}, {Field}, {Value})";
    public override AstNode DeepClone() => new RddSetFieldStmt((Expr)Area.DeepClone(), (Expr)Field.DeepClone(), (Expr)Value.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SET REPROCESS TO n`.</summary>
[JsonDerivedType(typeof(RddSetReprocessStmt), nameof(RddSetReprocessStmt))]
public sealed record RddSetReprocessStmt(Expr Ms, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SETREPROCESS({Ms})";
    public override AstNode DeepClone() => new RddSetReprocessStmt((Expr)Ms.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SET ORDER TO area, ordine`.</summary>
[JsonDerivedType(typeof(RddSetOrderStmt), nameof(RddSetOrderStmt))]
public sealed record RddSetOrderStmt(Expr Area, Expr Order, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SETORDER({Area}, {Order})";
    public override AstNode DeepClone() => new RddSetOrderStmt((Expr)Area.DeepClone(), (Expr)Order.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SET LOCK TIMEOUT TO n`.</summary>
[JsonDerivedType(typeof(RddSetLockTimeoutStmt), nameof(RddSetLockTimeoutStmt))]
public sealed record RddSetLockTimeoutStmt(Expr Ms, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SETLOCKTIMEOUT({Ms})";
    public override AstNode DeepClone() => new RddSetLockTimeoutStmt((Expr)Ms.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD ON EVENT "evento" [, callback]`.</summary>
[JsonDerivedType(typeof(RddOnEventStmt), nameof(RddOnEventStmt))]
public sealed record RddOnEventStmt(Expr EventName, Expr? Callback, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.ONEVENT({EventName})";
    public override AstNode DeepClone() => new RddOnEventStmt((Expr)EventName.DeepClone(), (Expr?)Callback?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD OFF EVENT "evento"`.</summary>
[JsonDerivedType(typeof(RddOffEventStmt), nameof(RddOffEventStmt))]
public sealed record RddOffEventStmt(Expr EventName, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.OFFEVENT({EventName})";
    public override AstNode DeepClone() => new RddOffEventStmt((Expr)EventName.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD CREATE INDEX nome ON chiave`.</summary>
[JsonDerivedType(typeof(RddCreateIndexStmt), nameof(RddCreateIndexStmt))]
public sealed record RddCreateIndexStmt(Expr Area, Expr Name, Expr Key, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.CREATEINDEX({Area}, {Name}, {Key})";
    public override AstNode DeepClone() => new RddCreateIndexStmt((Expr)Area.DeepClone(), (Expr)Name.DeepClone(), (Expr)Key.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD CLOSE INDEX nome`.</summary>
[JsonDerivedType(typeof(RddCloseIndexStmt), nameof(RddCloseIndexStmt))]
public sealed record RddCloseIndexStmt(Expr Area, Expr Name, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.CLOSEINDEX({Area}, {Name})";
    public override AstNode DeepClone() => new RddCloseIndexStmt((Expr)Area.DeepClone(), (Expr)Name.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD COPY TO file [VIA driver]`.</summary>
[JsonDerivedType(typeof(RddCopyToStmt), nameof(RddCopyToStmt))]
public sealed record RddCopyToStmt(Expr Area, Expr FileName, Expr? OutDriver, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.COPYTO({Area}, {FileName})";
    public override AstNode DeepClone() => new RddCopyToStmt((Expr)Area.DeepClone(), (Expr)FileName.DeepClone(), (Expr?)OutDriver?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD SORT TO file [FIELDS ...]`.</summary>
[JsonDerivedType(typeof(RddSortToStmt), nameof(RddSortToStmt))]
public sealed record RddSortToStmt(Expr Area, Expr FileName, Expr? Fields, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.SORTTO({Area}, {FileName})";
    public override AstNode DeepClone() => new RddSortToStmt((Expr)Area.DeepClone(), (Expr)FileName.DeepClone(), (Expr?)Fields?.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>Istruzione `RDD TOTAL ON chiave`.</summary>
[JsonDerivedType(typeof(RddTotalOnStmt), nameof(RddTotalOnStmt))]
 public sealed record RddTotalOnStmt(Expr Area, Expr Key, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.TOTALON({Area}, {Key})";
    public override AstNode DeepClone() => new RddTotalOnStmt((Expr)Area.DeepClone(), (Expr)Key.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>RDD DELETE [scope] — marca i record come cancellati.</summary>
[JsonDerivedType(typeof(RddDeleteStmt), nameof(RddDeleteStmt))]
public sealed record RddDeleteStmt(DbfScopeClause? Scope, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.DELETE({Scope})";
    public override AstNode DeepClone() => new RddDeleteStmt(Scope?.DeepClone() as DbfScopeClause, (Token)Token.DeepClone());
}

/// <summary>RDD RECALL [scope] — ripristina i record cancellati.</summary>
[JsonDerivedType(typeof(RddRecallStmt), nameof(RddRecallStmt))]
public sealed record RddRecallStmt(DbfScopeClause? Scope, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.RECALL({Scope})";
    public override AstNode DeepClone() => new RddRecallStmt(Scope?.DeepClone() as DbfScopeClause, (Token)Token.DeepClone());
}

/// <summary>RDD REPLACE field WITH value [, ...] [scope] — sostituisce i valori dei campi.</summary>
[JsonDerivedType(typeof(RddReplaceStmt), nameof(RddReplaceStmt))]
public sealed record RddReplaceStmt(ImmutableArray<Assignment> Replacements, DbfScopeClause? Scope, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"RDD.REPLACE({string.Join(", ", Replacements)})";
    public override AstNode DeepClone()
    {
        var clonedReplacements = Replacements
            .Select(r => new Assignment((Target)r.Target.DeepClone(), (Expr)r.Value.DeepClone()))
            .ToImmutableArray();
        return new RddReplaceStmt(clonedReplacements, (DbfScopeClause?)Scope?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET PATH TO pathList`.
/// </summary>
[JsonDerivedType(typeof(DbfSetPathStmt), nameof(DbfSetPathStmt))]
public sealed record DbfSetPathStmt(Expr PathList, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET PATH TO {PathList}"
            : $"{Token.Lexeme}({PathList})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetPathStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetPathStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetPathStmt((Expr)PathList.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET EXCLUSIVE ON|OFF`.
/// </summary>
[JsonDerivedType(typeof(DbfSetExclusiveStmt), nameof(DbfSetExclusiveStmt))]
public sealed record DbfSetExclusiveStmt(Token State, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET EXCLUSIVE {State.Lexeme}"
            : $"{Token.Lexeme}({State.Lexeme.ToUpper()})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetExclusiveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetExclusiveStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetExclusiveStmt((Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `SET SCHEMA STRICT ON|OFF`.
/// </summary>
[JsonDerivedType(typeof(DbfSetSchemaStmt), nameof(DbfSetSchemaStmt))]
public sealed record DbfSetSchemaStmt(Token State, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString()
    {
        return Token.Lexeme.Equals("SET", StringComparison.OrdinalIgnoreCase)
            ? $"SET SCHEMA STRICT {State.Lexeme}"
            : $"{Token.Lexeme}({State.Lexeme.ToUpper()})";
    }

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfSetSchemaStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfSetSchemaStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfSetSchemaStmt((Token)State.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `APPEND BLANK`.
/// </summary>
[JsonDerivedType(typeof(DbfAppendBlankStmt), nameof(DbfAppendBlankStmt))]
public sealed record DbfAppendBlankStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "APPEND BLANK";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfAppendBlankStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfAppendBlankStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new DbfAppendBlankStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DBF VALIDATE [area]`.
/// </summary>
[JsonDerivedType(typeof(DbfValidateStmt), nameof(DbfValidateStmt))]
public sealed record DbfValidateStmt(Expr? Area, Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"DBF VALIDATE{(Area != null ? $" {Area}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DbfValidateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DbfValidateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DbfValidateStmt((Expr?)Area?.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Tipi di funzioni di aggregazione supportate dal comando AGGREGATE.
/// </summary>
public enum AggregateKind
{
    Count,
    Sum,
    Avg,
    Min,
    Max
}

/// <summary>
/// Definisce una singola operazione di aggregazione (es. AVG(Saldo) AS Media).
/// </summary>
[JsonDerivedType(typeof(AggregateDefinition), nameof(AggregateDefinition))]
public sealed record AggregateDefinition(
    AggregateKind Kind,
    Expr? Expression, // Null per COUNT(*)
    Token Alias
)
{
    public override string ToString() =>
        $"{(Kind == AggregateKind.Count && Expression == null ? "COUNT(*)" : $"{Kind.ToString().ToUpper()}({Expression})")} AS {Alias.Lexeme}";

    public AggregateDefinition DeepClone()
    {
        return new AggregateDefinition(Kind, (Expr?)Expression?.DeepClone(), (Token)Alias.DeepClone());
    }
}

/// <summary>
/// Rappresenta l'istruzione `DBF AGGREGATE ON key TO file [FOR scope] COMPUTE ...`
/// o la sua variante moderna `DBF.AGGREGATE(...)`.
/// Unifica entrambe le sintassi memorizzando le definizioni di aggregazione come un'espressione.
/// </summary>
[JsonDerivedType(typeof(DbfAggregateStmt), nameof(DbfAggregateStmt))]
public sealed record DbfAggregateStmt(
    Expr KeyExpression,
    Expr FileName,
    Expr AggregationsExpr,
    DbfForClause? Scope,
    Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() =>
        $"AGGREGATE ON {KeyExpression} TO {FileName} COMPUTE {AggregationsExpr}";

    public override AstNode DeepClone()
    {
        return new DbfAggregateStmt(
            (Expr)KeyExpression.DeepClone(),
            (Expr)FileName.DeepClone(),
            (Expr)AggregationsExpr.DeepClone(),
            (DbfForClause?)Scope?.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta l'istruzione `DBF CREATE "fileName" FROM TYPE TypeName` o la sua variante moderna.
/// Viene utilizzata per creare un nuovo file DBF vuoto basato sulla struttura di un UDT.
/// </summary>
/// <param name="FileName">L'espressione che restituisce il percorso del nuovo file DBF da creare.</param>
/// <param name="TypeNameToken">Il token con il nome del tipo (UDT) da cui derivare la struttura.</param>
/// <param name="Token">Il token 'CREATE' per riferimento alla posizione.</param>
[JsonDerivedType(typeof(DbfCreateStmt), nameof(DbfCreateStmt))]
public sealed record DbfCreateStmt(Expr FileName, Token TypeNameToken, Token Token)
    : Stmt(Token.Position)
{
    public override string ToString() => $"CREATE {FileName} FROM TYPE {TypeNameToken.Lexeme}";

    public override AstNode DeepClone()
    {
        return new DbfCreateStmt(
            (Expr)FileName.DeepClone(),
            (Token)TypeNameToken.DeepClone(),
            (Token)Token.DeepClone()
        );
    }
}


/// <summary>
/// Rappresenta l'istruzione `DBF REPORT FORM` per la generazione di report dichiarativi.
/// </summary>
/// <param name="FormFile">L'espressione che restituisce il percorso del file di definizione del report (.frx).</param>
/// <param name="Scope">La clausola di scope opzionale (FOR, WHILE, etc.) per filtrare i record.</param>
/// <param name="Destination">L'espressione opzionale del file di destinazione per l'output (`TO FILE`).</param>
/// <param name="ToPrint">Flag che indica se l'output deve essere inviato alla stampante (`TO PRINT`).</param>
/// <param name="Token">Il token 'REPORT' per riferimento alla posizione.</param>
[JsonDerivedType(typeof(DbfReportStmt), nameof(DbfReportStmt))]
public sealed record DbfReportStmt(
    Expr FormFile,
    DbfScopeClause? Scope,
    Expr? Destination,
    bool ToPrint,
    Token Token)
    : Stmt(Token.Position)
{
    public override string ToString()
    {
        var sb = new StringBuilder($"REPORT FORM {FormFile}");
        if (Scope != null) sb.Append($" {Scope}");
        if (ToPrint) sb.Append(" TO PRINT");
        else if (Destination != null) sb.Append($" TO FILE {Destination}");
        return sb.ToString();
    }

    public override AstNode DeepClone()
    {
        return new DbfReportStmt(
            (Expr)FormFile.DeepClone(),
            (DbfScopeClause?)Scope?.DeepClone(),
            (Expr?)Destination?.DeepClone(),
            ToPrint,
            (Token)Token.DeepClone()
        );
    }
}
