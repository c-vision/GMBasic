// FILE: Basic.Databases.DeadlockException

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming

namespace Basic.Databases;

/// <summary>
/// Eccezione lanciata quando il rilevatore di deadlock interrompe una transazione
/// per sbloccare un ciclo di attesa tra thread (vittima = transazione più giovane).
/// </summary>
public sealed class DeadlockException : IOException
{
    /// <summary>
    /// Inizializza una nuova istanza con messaggio descrittivo.
    /// </summary>
    /// <param name="message">Descrizione del deadlock rilevato.</param>
    public DeadlockException(string message) : base(message)
    {
    }
}
