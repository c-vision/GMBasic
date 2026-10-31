// FILE: Interpreter.Ast.Ledger.cs



// ReSharper disable NotAccessedPositionalProperty.Global
// ReSharper disable InconsistentNaming

namespace Basic;

// --- Record AST per i Comandi del Sottosistema Ledger ---

/// <summary>
/// Rappresenta il comando `LEDGER OPEN file$ AS handle`.
/// </summary>
public sealed record LedgerOpenStmt(Expr FilePath, Target Handle) : Stmt(FilePath.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER OPEN {FilePath} AS {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerOpenStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerOpenStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerOpenStmt((Expr)FilePath.DeepClone(), (Target)Handle.DeepClone());
    }
}

/// <summary>
/// Rappresenta una singola riga di posting all'interno di un blocco `LEDGER ADD`.
/// </summary>
/// <param name="Account">L'espressione per il nome del conto.</param>
/// <param name="Amount">L'espressione (opzionale) per l'importo.</param>
public sealed record LedgerPosting(Expr Account, Expr? Amount)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"{Account}{(Amount != null ? " " + Amount : "")}";

    /// <summary>
    /// Crea una copia profonda di questo oggetto LedgerPosting.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerPosting con copie delle sue espressioni.</returns>
    public LedgerPosting DeepClone()
    {
        return new LedgerPosting((Expr)Account.DeepClone(), (Expr?)Amount?.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER ADD [DATA] date [DESC] desc ... END ADD`.
/// </summary>
public sealed record LedgerAddStmt(
    Expr Handle,
    Expr Date,
    Expr Description,
    ImmutableArray<LedgerPosting> Postings,
    Target NewIdVariable
) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER ADD {Handle} DATE {Date} DESC {Description}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerAddStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerAddStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedPostings = Postings.Select(p => p.DeepClone()).ToImmutableArray();
        return new LedgerAddStmt(
            (Expr)Handle.DeepClone(), // MODIFICA: Cast a Expr
            (Expr)Date.DeepClone(),
            (Expr)Description.DeepClone(),
            clonedPostings,
            (Target)NewIdVariable.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER REGISTER handle INTO array [WHERE query$]`.
/// </summary>
public sealed record LedgerRegisterStmt(
    Expr Handle,
    Target DestinationArray,
    Expr? Query
) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"LEDGER REGISTER {Handle} INTO {DestinationArray}{(Query != null ? " WHERE " + Query : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerRegisterStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerRegisterStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerRegisterStmt(
            (Expr)Handle.DeepClone(),
            (Target)DestinationArray.DeepClone(),
            (Expr?)Query?.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER BALANCE handle INTO array [WHERE query$]`.
/// </summary>
public sealed record LedgerBalanceStmt(
    Expr Handle,
    Target DestinationArray,
    Expr? Query
) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        $"LEDGER BALANCE {Handle} INTO {DestinationArray}{(Query != null ? " WHERE " + Query : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerBalanceStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerBalanceStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerBalanceStmt(
            (Expr)Handle.DeepClone(),
            (Target)DestinationArray.DeepClone(),
            (Expr?)Query?.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER EDIT handle ID id DESC desc ... END EDIT`.
/// </summary>
public sealed record LedgerEditStmt(
    Expr Handle,
    Expr TransactionId,
    Expr Date,
    Expr Description,
    ImmutableArray<LedgerPosting> Postings,
    Target NewIdVariable
) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER EDIT {Handle} ID {TransactionId}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerEditStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerEditStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedPostings = Postings.Select(p => p.DeepClone()).ToImmutableArray();
        return new LedgerEditStmt(
            (Expr)Handle.DeepClone(), // MODIFICA: Cast a Expr
            (Expr)TransactionId.DeepClone(),
            (Expr)Date.DeepClone(),
            (Expr)Description.DeepClone(),
            clonedPostings,
            (Target)NewIdVariable.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER DELETE handle ID id`.
/// </summary>
public sealed record LedgerDeleteStmt(Expr Handle, Expr TransactionId) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER DELETE {Handle} ID {TransactionId}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerDeleteStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerDeleteStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerDeleteStmt((Expr)Handle.DeepClone(), (Expr)TransactionId.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER SAVE handle`.
/// </summary>
public sealed record LedgerSaveStmt(Expr Handle) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER SAVE {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerSaveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerSaveStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerSaveStmt((Expr)Handle.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER CLOSE handle`.
/// </summary>
public sealed record LedgerCloseStmt(Expr Handle) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER CLOSE {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerCloseStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerCloseStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerCloseStmt((Expr)Handle.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER PACK handle`.
/// </summary>
public sealed record LedgerPackStmt(Expr Handle) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER PACK {Handle}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerPackStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerPackStmt con una copia della sua espressione figlia.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerPackStmt((Expr)Handle.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER ACCOUNTS handle INTO array`.
/// </summary>
public sealed record LedgerAccountsStmt(Expr Handle, Expr? RootAccount, Target DestinationArray)
    : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER ACCOUNTS {Handle} INTO {DestinationArray}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerAccountsStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerAccountsStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerAccountsStmt((Expr)Handle.DeepClone(), (Expr?)RootAccount?.DeepClone(),
            (Target)DestinationArray.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER VALIDATE handle INTO var`.
/// </summary>
public sealed record LedgerValidateStmt(Expr Handle, Target DestinationVariable) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER VALIDATE {Handle} INTO {DestinationVariable}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerValidateStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerValidateStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerValidateStmt((Expr)Handle.DeepClone(), (Target)DestinationVariable.DeepClone());
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER GET handle [ID id] INTO array`.
/// Senza la clausola `ID` legge la transazione corrente del cursore.
/// </summary>
public sealed record LedgerGetStmt(
    Expr Handle,
    Expr? TransactionId,
    Target DestinationArray
) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => TransactionId is null
        ? $"LEDGER GET {Handle} INTO {DestinationArray}"
        : $"LEDGER GET {Handle} ID {TransactionId} INTO {DestinationArray}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerGetStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerGetStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new LedgerGetStmt(
            (Expr)Handle.DeepClone(),
            (Expr?)TransactionId?.DeepClone(),
            (Target)DestinationArray.DeepClone()
        );
    }
}

/// <summary>
/// Rappresenta il comando `LEDGER GO handle TOP|BOTTOM|<transactionId>` — posiziona il cursore.
/// </summary>
public sealed record LedgerGoStmt(Expr Handle, Expr Destination) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER GO {Handle} {Destination}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerGoStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerGoStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone() =>
        new LedgerGoStmt((Expr)Handle.DeepClone(), (Expr)Destination.DeepClone());
}

/// <summary>
/// Rappresenta il comando `LEDGER SKIP handle [count]` — muove il cursore in avanti/indietro.
/// </summary>
public sealed record LedgerSkipStmt(Expr Handle, Expr? Count) : Stmt(Handle.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LEDGER SKIP {Handle}{(Count is null ? "" : " " + Count)}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LedgerSkipStmt.
    /// </summary>
    /// <returns>Una nuova istanza di LedgerSkipStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone() =>
        new LedgerSkipStmt((Expr)Handle.DeepClone(), (Expr?)Count?.DeepClone());
}

/// <summary>Rappresenta `LEDGER BUDGET handle, account$, amount, period$`.</summary>
public sealed record LedgerBudgetStmt(
    Expr Handle,
    Expr Account,
    Expr Amount,
    Expr Period
) : Stmt(Handle.Position)
{
    public override string ToString() => $"LEDGER BUDGET {Handle} {Account} {Amount} {Period}";

    public override AstNode DeepClone() =>
        new LedgerBudgetStmt(
            (Expr)Handle.DeepClone(),
            (Expr)Account.DeepClone(),
            (Expr)Amount.DeepClone(),
            (Expr)Period.DeepClone());
}

/// <summary>Rappresenta `LEDGER VARIANCE handle INTO var` o funzione con account/period.</summary>
public sealed record LedgerVarianceStmt(
    Expr Handle,
    Expr Account,
    Expr Period,
    Target DestinationVariable
) : Stmt(Handle.Position)
{
    public override string ToString() => $"LEDGER VARIANCE {Handle} {Account} {Period} INTO {DestinationVariable}";

    public override AstNode DeepClone() =>
        new LedgerVarianceStmt(
            (Expr)Handle.DeepClone(),
            (Expr)Account.DeepClone(),
            (Expr)Period.DeepClone(),
            (Target)DestinationVariable.DeepClone());
}

/// <summary>Rappresenta `LEDGER PLOT handle, query$, chartType$` — restituisce handle grafico.</summary>
public sealed record LedgerPlotStmt(
    Expr Handle,
    Expr Query,
    Expr ChartType,
    Target DestinationVariable
) : Stmt(Handle.Position)
{
    public override string ToString() => $"LEDGER PLOT {Handle} {Query} {ChartType} INTO {DestinationVariable}";

    public override AstNode DeepClone() =>
        new LedgerPlotStmt(
            (Expr)Handle.DeepClone(),
            (Expr)Query.DeepClone(),
            (Expr)ChartType.DeepClone(),
            (Target)DestinationVariable.DeepClone());
}

/// <summary>Rappresenta `LEDGER IMPORT handle, csvPath$, mapping$`.</summary>
public sealed record LedgerImportStmt(
    Expr Handle,
    Expr CsvPath,
    Expr MappingRules,
    Target? DestinationVariable
) : Stmt(Handle.Position)
{
    public override string ToString() => $"LEDGER IMPORT {Handle} {CsvPath} {MappingRules}";

    public override AstNode DeepClone() =>
        new LedgerImportStmt(
            (Expr)Handle.DeepClone(),
            (Expr)CsvPath.DeepClone(),
            (Expr)MappingRules.DeepClone(),
            (Target?)DestinationVariable?.DeepClone());
}