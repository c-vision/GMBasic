/* FILE: native/gmb_db_c/src/Databases/TransactionContext.c */
/* Port 1:1 di common/Databases/TransactionContext.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.TransactionContext.cs
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//
//
//namespace Basic.Databases;
//
///// <summary>
///// Definisce i livelli di isolamento delle transazioni supportati dai motori di database,
///// che determinano il grado di protezione di una transazione dalle interferenze
///// causate da altre transazioni in esecuzione concorrente.
///// </summary>
//public enum IsolationLevel
//{
//    /// <summary>
//    /// Livello 1: Previene le "Letture Sporche" (Dirty Reads).
//    /// Garantisce che una transazione possa leggere solo dati che sono stati commessi in modo permanente.
//    /// È il livello di default.
//    /// </summary>
//    ReadCommitted,
//
//    /// <summary>
//    /// Livello 2: Previene le "Letture Non Ripetibili" (Non-Repeatable Reads).
//    /// Garantisce che, se una transazione rilegge lo stesso record più volte, otterrà sempre gli stessi dati.
//    /// Acquisisce lock di lettura sui record letti e li mantiene fino alla fine della transazione.
//    /// </summary>
//    RepeatableRead,
//
//    /// <summary>
//    /// Livello 3: Previene le "Letture Fantasma" (Phantom Reads).
//    /// È il livello di isolamento più stringente. Garantisce che le query eseguite più volte all'interno
//    /// della stessa transazione restituiscano sempre lo stesso insieme di righe.
//    /// </summary>
//    Serializable
//}
//
///// <summary>
///// Definisce il contratto per un handle di file che supporta operazioni transazionali.
///// </summary>
//public interface ITransactionalFileHandle
//{
//    long BeginTransaction();
//    void CommitTransaction();
//    void RollbackTransaction();
//}
//
///// <summary>
///// Mantiene lo stato di una singola transazione attiva per qualsiasi motore di database,
///// includendo il suo livello di isolamento e tutti i lock acquisiti.
///// </summary>
//public class TransactionContext
//{
//    /// <summary>
//    /// L'ID univoco della transazione.
//    /// </summary>
//    public long TransactionId { get; }
//
//    /// <summary>
//    /// Il livello di isolamento per questa transazione.
//    /// </summary>
//    public IsolationLevel IsolationLevel { get; }
//
//    // --- Lock per ISAM ---
//    /// <summary>
//    /// Traccia gli offset dei record su cui è stato acquisito un lock di lettura (per ISAM).
//    /// </summary>
//    public HashSet<long> ReadLockedOffsets { get; } = new();
//
//    /// <summary>
//    /// Traccia gli offset dei record su cui è stato acquisito un lock di scrittura (per ISAM).
//    /// </summary>
//    public HashSet<long> WriteLockedOffsets { get; } = new();
//
//    // --- Lock per DBF ---
//    /// <summary>
//    /// Traccia i numeri di record (1-based) su cui è stato acquisito un lock di lettura (per DBF).
//    /// </summary>
//    public HashSet<long> ReadLockedRecNos { get; } = new();
//
//    /// <summary>
//    /// Traccia i numeri di record (1-based) su cui è stato acquisito un lock di scrittura (per DBF).
//    /// </summary>
//    public HashSet<long> WriteLockedRecNos { get; } = new();
//
//    // --- Lock comuni a entrambi i motori ---
//    /// <summary>
//    /// Traccia le chiavi di risorsa per i lock di lettura su chiavi/intervalli di indice (per `Serializable`).
//    /// </summary>
//    public HashSet<string> ReadLockedKeys { get; } = new();
//
//    /// <summary>
//    /// Traccia le chiavi di risorsa per i lock di scrittura su chiavi/intervalli di indice (per `Serializable`).
//    /// </summary>
//    public HashSet<string> WriteLockedKeys { get; } = new();
//
//    // --- Gestione Undo (Comune) ---
//    /// <summary>
//    /// Traccia gli identificatori dei record di "undo" creati durante la transazione.
//    /// Per ISAM, contiene gli offset. Per DBF, contiene i numeri di record.
//    /// </summary>
//    public HashSet<long> UndoLogIdentifiers { get; } = new();
//
//    /// <summary>
//    /// Record ISAM inseriti durante la transazione i cui indici B+Tree verranno aggiornati solo al commit.
//    /// </summary>
//    public List<(UdtInstance Udt, long Offset)> PendingIndexInserts { get; } = new();
//
//    /// <summary>
//    /// Righe del log di replica DBF bufferizzate fino al commit della transazione esterna.
//    /// </summary>
//    public List<string> PendingDbfReplicationOps { get; } = new();
//
//    /// <summary>
//    /// Righe del log di replica ISAM bufferizzate fino al commit della transazione esterna.
//    /// </summary>
//    public List<string> PendingIsamReplicationOps { get; } = new();
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="TransactionContext"/>.
//    /// </summary>
//    /// <param name="transactionId">L'ID della transazione.</param>
//    /// <param name="isolationLevel">Il livello di isolamento.</param>
//    public TransactionContext(long transactionId, IsolationLevel isolationLevel)
//    {
//        TransactionId = transactionId;
//        IsolationLevel = isolationLevel;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */












