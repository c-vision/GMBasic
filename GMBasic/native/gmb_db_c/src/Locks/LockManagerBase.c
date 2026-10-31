/* FILE: native/gmb_db_c/src/Locks/LockManagerBase.c */
/* Port 1:1 di modules/Database/Locks/LockManagerBase.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.LockManagerBase.cs
//
//using System.Text;
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//namespace Basic.Databases;
//
///// <summary>
///// Classe base astratta per i gestori di lock. Implementa la logica comune dei
///// metodi "wrapper" per il locking a livello di record e di chiave, delegando
///// l'implementazione specifica della strategia di lock (intra o cross-process)
///// alle classi derivate tramite metodi abstract.
///// </summary>
//internal abstract class LockManagerBase : ILockManager
//{
//    #region Metodi Abstract da Implementare nelle Classi Derivate
//
//    /// <inheritdoc/>
//    public abstract void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
//    /// <inheritdoc/>
//    public abstract void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
//    /// <inheritdoc/>
//    public abstract void ReleaseReadLock(string resourceKey);
//    /// <inheritdoc/>
//    public abstract void ReleaseWriteLock(string resourceKey);
//    /// <inheritdoc/>
//    public abstract void Dispose();
//
//    #endregion
//
//    #region Helper Pubblici per la Creazione Chiavi (Condivisi)
//
//    /// <summary>
//    /// Genera la chiave stringa standardizzata per un lock a livello di record.
//    /// </summary>
//    /// <param name="recordIdentifier">L'identificatore numerico del record (offset per ISAM, recno per DBF).</param>
//    /// <returns>La chiave univoca della risorsa per il lock del record.</returns>
//    public static string GetRecordLockKey(long recordIdentifier) => $"R_{recordIdentifier}";
//
//    /// <summary>
//    /// Genera la chiave stringa standardizzata per un lock su una chiave di indice (Next-Key Lock).
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
//    /// <param name="key">La rappresentazione in byte della chiave.</param>
//    /// <returns>La chiave univoca della risorsa per il lock della chiave/intervallo.</returns>
//    public static string GetKeyLockKey(string indexName, byte[] key) => $"K_{indexName}_{Convert.ToHexString(key)}";
//
//    #endregion
//
//    #region Wrapper per Locking a Livello Record
//
//    /// <summary>
//    /// Tenta di acquisire un lock di lettura (condiviso) su un record specifico.
//    /// </summary>
//    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
//    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
//    public void AcquireReadLock(long recordIdentifier, int timeoutMilliseconds)
//    {
//        if (recordIdentifier < 0)
//            throw new ArgumentException("L'identificatore del record non può essere negativo.", nameof(recordIdentifier));
//
//        var lockKey = GetRecordLockKey(recordIdentifier);
//        AcquireReadLock(lockKey, timeoutMilliseconds, $"record all'identificatore {recordIdentifier}");
//    }
//
//    /// <summary>
//    /// Tenta di acquisire un lock di scrittura (esclusivo) su un record specifico.
//    /// </summary>
//    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
//    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
//    public void AcquireWriteLock(long recordIdentifier, int timeoutMilliseconds)
//    {
//        if (recordIdentifier < 0)
//            throw new ArgumentException("L'identificatore del record non può essere negativo.", nameof(recordIdentifier));
//
//        var lockKey = GetRecordLockKey(recordIdentifier);
//        AcquireWriteLock(lockKey, timeoutMilliseconds, $"record all'identificatore {recordIdentifier}");
//    }
//
//    /// <summary>
//    /// Rilascia un lock di lettura precedentemente acquisito su un record specifico.
//    /// </summary>
//    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
//    public void ReleaseReadLock(long recordIdentifier)
//    {
//        if (recordIdentifier < 0) return;
//        var lockKey = GetRecordLockKey(recordIdentifier);
//        ReleaseReadLock(lockKey);
//    }
//
//    /// <summary>
//    /// Rilascia un lock di scrittura precedentemente acquisito su un record specifico.
//    /// </summary>
//    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
//    public void ReleaseWriteLock(long recordIdentifier)
//    {
//        if (recordIdentifier < 0) return;
//        var lockKey = GetRecordLockKey(recordIdentifier);
//        ReleaseWriteLock(lockKey);
//    }
//
//    #endregion
//
//    #region Wrapper per Locking a Livello Chiave/Intervallo
//
//    /// <summary>
//    /// Tenta di acquisire un lock di lettura (condiviso) su una chiave di indice e sull'intervallo successivo.
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
//    /// <param name="key">La rappresentazione in byte della chiave.</param>
//    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
//    public void AcquireKeyReadLock(string indexName, byte[] key, int timeoutMilliseconds)
//    {
//        var lockKey = GetKeyLockKey(indexName, key);
//        AcquireReadLock(lockKey, timeoutMilliseconds, $"chiave di indice '{Encoding.UTF8.GetString(key)}'");
//    }
//
//    /// <summary>
//    /// Tenta di acquisire un lock di scrittura (esclusivo) su una chiave di indice e sull'intervallo successivo.
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
//    /// <param name="key">La rappresentazione in byte della chiave.</param>
//    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
//    public void AcquireKeyWriteLock(string indexName, byte[] key, int timeoutMilliseconds)
//    {
//        var lockKey = GetKeyLockKey(indexName, key);
//        AcquireWriteLock(lockKey, timeoutMilliseconds, $"chiave di indice '{Encoding.UTF8.GetString(key)}'");
//    }
//
//    /// <summary>
//    /// Rilascia un lock di lettura precedentemente acquisito su una chiave di indice.
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
//    /// <param name="key">La rappresentazione in byte della chiave.</param>
//    public void ReleaseKeyReadLock(string indexName, byte[] key)
//    {
//        var lockKey = GetKeyLockKey(indexName, key);
//        ReleaseReadLock(lockKey);
//    }
//
//    /// <summary>
//    /// Rilascia un lock di scrittura precedentemente acquisito su una chiave di indice.
//    /// </summary>
//    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
//    /// <param name="key">La rappresentazione in byte della chiave.</param>
//    public void ReleaseKeyWriteLock(string indexName, byte[] key)
//    {
//        var lockKey = GetKeyLockKey(indexName, key);
//        ReleaseWriteLock(lockKey);
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal abstract class LockManagerBase */

/* Genera la chiave stringa standardizzata per un lock a livello di record. */
void LockManagerBase_GetRecordLockKey(int64_t recordIdentifier, char* out, int cap)
{
    snprintf(out, (size_t)cap, "R_%lld", (long long)recordIdentifier);
}

/* Genera la chiave stringa standardizzata per un lock su una chiave di indice (Next-Key Lock). */
void LockManagerBase_GetKeyLockKey(const char* indexName, const unsigned char* key, int keyLen,
                                   char* out, int cap)
{
    int i, n = 0;
    n += snprintf(out, (size_t)cap, "K_%s_", indexName ? indexName : "");
    for (i = 0; i < keyLen && n + 2 < cap; i++)
        n += snprintf(out + n, (size_t)(cap - n), "%02X", key[i]);
}

int LockManagerBase_AcquireReadLockRecord(void* self, int64_t recordIdentifier, int timeoutMilliseconds,
    int (*acquire)(void*, const char*, int, const char*))
{
    char lockKey[64];
    char desc[96];
    if (recordIdentifier < 0) return GMB_DB_ARG;
    LockManagerBase_GetRecordLockKey(recordIdentifier, lockKey, (int)sizeof(lockKey));
    snprintf(desc, sizeof(desc), "record all'identificatore %lld", (long long)recordIdentifier);
    return acquire(self, lockKey, timeoutMilliseconds, desc);
}

