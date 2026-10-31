// FILE: Basic.Databases.Locks.LockManagerBase.cs

using System.Text;

// ReSharper disable CheckNamespace
// ReSharper disable InconsistentNaming
namespace Basic.Databases;

/// <summary>
/// Classe base astratta per i gestori di lock. Implementa la logica comune dei
/// metodi "wrapper" per il locking a livello di record e di chiave, delegando
/// l'implementazione specifica della strategia di lock (intra o cross-process)
/// alle classi derivate tramite metodi abstract.
/// </summary>
internal abstract class LockManagerBase : ILockManager
{
    #region Metodi Abstract da Implementare nelle Classi Derivate

    /// <inheritdoc/>
    public abstract void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
    /// <inheritdoc/>
    public abstract void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
    /// <inheritdoc/>
    public abstract void ReleaseReadLock(string resourceKey);
    /// <inheritdoc/>
    public abstract void ReleaseWriteLock(string resourceKey);
    /// <inheritdoc/>
    public abstract void Dispose();

    #endregion

    #region Helper Pubblici per la Creazione Chiavi (Condivisi)

    /// <summary>
    /// Genera la chiave stringa standardizzata per un lock a livello di record.
    /// </summary>
    /// <param name="recordIdentifier">L'identificatore numerico del record (offset per ISAM, recno per DBF).</param>
    /// <returns>La chiave univoca della risorsa per il lock del record.</returns>
    public static string GetRecordLockKey(long recordIdentifier) => $"R_{recordIdentifier}";

    /// <summary>
    /// Genera la chiave stringa standardizzata per un lock su una chiave di indice (Next-Key Lock).
    /// </summary>
    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
    /// <param name="key">La rappresentazione in byte della chiave.</param>
    /// <returns>La chiave univoca della risorsa per il lock della chiave/intervallo.</returns>
    public static string GetKeyLockKey(string indexName, byte[] key) => $"K_{indexName}_{Convert.ToHexString(key)}";

    #endregion

    #region Wrapper per Locking a Livello Record

    /// <summary>
    /// Tenta di acquisire un lock di lettura (condiviso) su un record specifico.
    /// </summary>
    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
    public void AcquireReadLock(long recordIdentifier, int timeoutMilliseconds)
    {
        if (recordIdentifier < 0)
            throw new ArgumentException("L'identificatore del record non può essere negativo.", nameof(recordIdentifier));

        var lockKey = GetRecordLockKey(recordIdentifier);
        AcquireReadLock(lockKey, timeoutMilliseconds, $"record all'identificatore {recordIdentifier}");
    }

    /// <summary>
    /// Tenta di acquisire un lock di scrittura (esclusivo) su un record specifico.
    /// </summary>
    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
    public void AcquireWriteLock(long recordIdentifier, int timeoutMilliseconds)
    {
        if (recordIdentifier < 0)
            throw new ArgumentException("L'identificatore del record non può essere negativo.", nameof(recordIdentifier));

        var lockKey = GetRecordLockKey(recordIdentifier);
        AcquireWriteLock(lockKey, timeoutMilliseconds, $"record all'identificatore {recordIdentifier}");
    }

    /// <summary>
    /// Rilascia un lock di lettura precedentemente acquisito su un record specifico.
    /// </summary>
    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
    public void ReleaseReadLock(long recordIdentifier)
    {
        if (recordIdentifier < 0) return;
        var lockKey = GetRecordLockKey(recordIdentifier);
        ReleaseReadLock(lockKey);
    }

    /// <summary>
    /// Rilascia un lock di scrittura precedentemente acquisito su un record specifico.
    /// </summary>
    /// <param name="recordIdentifier">L'identificatore numerico del record.</param>
    public void ReleaseWriteLock(long recordIdentifier)
    {
        if (recordIdentifier < 0) return;
        var lockKey = GetRecordLockKey(recordIdentifier);
        ReleaseWriteLock(lockKey);
    }

    #endregion

    #region Wrapper per Locking a Livello Chiave/Intervallo

    /// <summary>
    /// Tenta di acquisire un lock di lettura (condiviso) su una chiave di indice e sull'intervallo successivo.
    /// </summary>
    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
    /// <param name="key">La rappresentazione in byte della chiave.</param>
    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
    public void AcquireKeyReadLock(string indexName, byte[] key, int timeoutMilliseconds)
    {
        var lockKey = GetKeyLockKey(indexName, key);
        AcquireReadLock(lockKey, timeoutMilliseconds, $"chiave di indice '{Encoding.UTF8.GetString(key)}'");
    }

    /// <summary>
    /// Tenta di acquisire un lock di scrittura (esclusivo) su una chiave di indice e sull'intervallo successivo.
    /// </summary>
    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
    /// <param name="key">La rappresentazione in byte della chiave.</param>
    /// <param name="timeoutMilliseconds">Il timeout per l'attesa.</param>
    public void AcquireKeyWriteLock(string indexName, byte[] key, int timeoutMilliseconds)
    {
        var lockKey = GetKeyLockKey(indexName, key);
        AcquireWriteLock(lockKey, timeoutMilliseconds, $"chiave di indice '{Encoding.UTF8.GetString(key)}'");
    }

    /// <summary>
    /// Rilascia un lock di lettura precedentemente acquisito su una chiave di indice.
    /// </summary>
    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
    /// <param name="key">La rappresentazione in byte della chiave.</param>
    public void ReleaseKeyReadLock(string indexName, byte[] key)
    {
        var lockKey = GetKeyLockKey(indexName, key);
        ReleaseReadLock(lockKey);
    }

    /// <summary>
    /// Rilascia un lock di scrittura precedentemente acquisito su una chiave di indice.
    /// </summary>
    /// <param name="indexName">Il nome dell'indice a cui appartiene la chiave.</param>
    /// <param name="key">La rappresentazione in byte della chiave.</param>
    public void ReleaseKeyWriteLock(string indexName, byte[] key)
    {
        var lockKey = GetKeyLockKey(indexName, key);
        ReleaseWriteLock(lockKey);
    }

    #endregion
}