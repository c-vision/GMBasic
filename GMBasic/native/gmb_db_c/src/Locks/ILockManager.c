/* FILE: native/gmb_db_c/src/Locks/ILockManager.c */
/* Port 1:1 di modules/Database/Locks/ILockManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.ILockManager.cs
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//namespace Basic.Databases;
//
///// <summary>
///// Definisce il contratto per un gestore di lock, astraendo la strategia
///// di locking (intra-processo o multi-processo). Questo permette di scambiare
///// le implementazioni in base al caso d'uso.
///// </summary>
//public interface ILockManager : IDisposable
//{
//    /// <summary>
//    /// Tenta di acquisire un lock di lettura (condiviso) su una risorsa identificata dalla sua chiave.
//    /// </summary>
//    /// <param name="resourceKey">La chiave stringa univoca che identifica la risorsa (es. "R_123" per un record).</param>
//    /// <param name="timeoutMilliseconds">Il numero di millisecondi di attesa per il lock prima di lanciare un'eccezione.</param>
//    /// <param name="resourceDescription">Una descrizione leggibile della risorsa, usata per i messaggi di errore.</param>
//    /// <exception cref="LockTimeoutException">Lanciata se il lock non può essere acquisito entro il timeout specificato.</exception>
//    void AcquireReadLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
//
//    /// <summary>
//    /// Tenta di acquisire un lock di scrittura (esclusivo) su una risorsa identificata dalla sua chiave.
//    /// </summary>
//    /// <param name="resourceKey">La chiave stringa univoca che identifica la risorsa.</param>
//    /// <param name="timeoutMilliseconds">Il numero di millisecondi di attesa per il lock prima di lanciare un'eccezione.</param>
//    /// <param name="resourceDescription">Una descrizione leggibile della risorsa, usata per i messaggi di errore.</param>
//    /// <exception cref="LockTimeoutException">Lanciata se il lock non può essere acquisito entro il timeout specificato.</exception>
//    void AcquireWriteLock(string resourceKey, int timeoutMilliseconds, string resourceDescription);
//
//    /// <summary>
//    /// Rilascia un lock di lettura precedentemente acquisito su una risorsa.
//    /// Se nessun lock di lettura è detenuto, l'operazione non ha effetto.
//    /// </summary>
//    /// <param name="resourceKey">La chiave stringa univoca che identifica la risorsa.</param>
//    void ReleaseReadLock(string resourceKey);
//
//    /// <summary>
//    /// Rilascia un lock di scrittura precedentemente acquisito su una risorsa.
//    /// Se nessun lock di scrittura è detenuto, l'operazione non ha effetto.
//    /// </summary>
//    /// <param name="resourceKey">La chiave stringa univoca che identifica la risorsa.</param>
//    void ReleaseWriteLock(string resourceKey);
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* nessun membro estraibile — TU presente */
static int gmb_port_tu __attribute__((unused));
