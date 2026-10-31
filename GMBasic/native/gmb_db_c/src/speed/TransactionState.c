/* FILE: native/gmb_db_c/src/SPEED/TransactionState.c */
/* Port 1:1 di modules/Database/SPEED/TransactionState.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.TransactionState.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Collections.Concurrent;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Mantiene lo stato di una transazione attiva per un singolo thread.
///// Incapsula un buffer di operazioni (una Memtable temporanea per ogni tabella modificata)
///// che vengono accumulate prima di essere applicate atomicamente al commit.
///// </summary>
//internal class TransactionState
//{
//    /// <summary>
//    /// Un dizionario che mappa l'ID di una tabella a una Memtable temporanea.
//    /// Questa struttura funge da buffer in memoria per le operazioni (Set/Delete)
//    /// eseguite all'interno della transazione, separate per ogni tabella.
//    /// Le modifiche non sono visibili ad altre transazioni fino al commit.
//    /// </summary>
//    public ConcurrentDictionary<int, Memtable> OperationBuffers { get; }
//
//    /// <summary>
//    /// Un set di chiavi che sono state lette durante la transazione.
//    /// Questo è il primo passo per implementare futuri livelli di isolamento
//    /// come Repeatable Read o Serializable. Per ora, non è utilizzato attivamente.
//    /// </summary>
//    public HashSet<byte[]> ReadSet { get; }
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="TransactionState"/>.
//    /// </summary>
//    public TransactionState()
//    {
//        OperationBuffers = new ConcurrentDictionary<int, Memtable>();
//        ReadSet = new HashSet<byte[]>(new ByteArrayEqualityComparer());
//    }
//
//    /// <summary>
//    /// Recupera la Memtable-buffer per una data tabella, creandola se non esiste.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella per cui ottenere il buffer.</param>
//    /// <returns>L'istanza di <see cref="Memtable"/> per il buffer della transazione.</returns>
//    public Memtable GetBufferForTable(int tableId)
//    {
//        return OperationBuffers.GetOrAdd(tableId, _ => new Memtable());
//    }
//}

/* --- C ---
 * Lo stato transazione vive in Database (inTx + txBuf). Begin/Commit/Rollback sono in Database.c.
 * I nomi fill_1to1 `int Foo(void)` non sono in gmb_db_port.h. */
