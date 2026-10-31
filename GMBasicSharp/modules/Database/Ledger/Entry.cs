// FILE: Interpreter.Ledger.LedgerEntry.cs

// ReSharper disable once CheckNamespace
// ReSharper disable UnusedAutoPropertyAccessor.Global
namespace Basic.Ledger;

/// <summary>
/// Rappresenta una singola riga ("posting" o "gamba") all'interno di una transazione Ledger.
/// Questa è la struttura dati fondamentale utilizzata per le operazioni in memoria.
/// </summary>
public sealed record LedgerEntry
{
    /// <summary>
    /// L'ID univoco della transazione a cui questa voce appartiene.
    /// </summary>
    public long TransactionId { get; init; }

    /// <summary>
    /// La data della transazione.
    /// </summary>
    public DateOnly Date { get; init; }

    /// <summary>
    /// Lo stato della transazione (es. ' ' per normale, '*' per saldata, '!' per pendente).
    /// </summary>
    public char Status { get; init; }
    
    /// <summary>
    /// La descrizione generale della transazione.
    /// </summary>
    public required string Description { get; init; }

    /// <summary>
    /// Il nome completo del conto per questa specifica riga (es. "Spese:Ufficio").
    /// </summary>
    public required string Account { get; init; }

    /// <summary>
    /// L'importo per questa specifica riga. Può essere positivo o negativo.
    /// </summary>
    public decimal Amount { get; init; }
    
    /// <summary>
    /// L'eventuale commento specifico per questa riga, che segue l'importo.
    /// </summary>
    public string? Comment { get; init; }
    
    /// <summary>
    /// Un flag interno per il parser che indica se l'importo era presente esplicitamente nel file
    /// o se è stato calcolato per bilanciare la transazione.
    /// </summary>
    public bool AmountIsExplicit { get; init; }

    /// <summary>
    /// Codice valuta ISO opzionale (es. USD). Null = valuta base del libro mastro.
    /// </summary>
    public string? Currency { get; init; }

    /// <summary>
    /// Tasso di cambio verso la valuta base (es. 0.95 per 100 USD → 95 base). Default 1.
    /// </summary>
    public decimal ExchangeRate { get; init; } = 1m;

    /// <summary>Importo normalizzato in valuta base per bilanciamento e report.</summary>
    public decimal BaseAmount => Amount * ExchangeRate;
}
