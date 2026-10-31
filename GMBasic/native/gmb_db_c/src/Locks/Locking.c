/* FILE: native/gmb_db_c/src/Locks/Locking.c */
/* Port 1:1 di modules/Database/Locks/Locking.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Locks.Locking.cs
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//namespace Basic.Databases;
//
///// <summary>
///// Specifica la strategia di locking da utilizzare per la gestione della concorrenza.
///// </summary>
//public enum LockingStrategy
//{
//    /// <summary>
//    /// Utilizza un meccanismo di locking ottimizzato per l'accesso multi-threaded
//    /// all'interno di un singolo processo. Offre le massime performance ma non è
//    /// sicuro per l'accesso da processi multipli (es. file sharing).
//    /// </summary>
//    IntraProcess,
//    
//    /// <summary>
//    /// Utilizza un meccanismo di locking basato su handle del sistema operativo
//    /// (es. Mutex, Semaphore) per garantire la sicurezza in scenari multi-processo.
//    /// È più robusto per il file sharing, ma ha un overhead di performance maggiore.
//    /// </summary>
//    CrossProcess
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public enum LockingStrategy — tipo in gmb_db_port.h (condiviso tra TU). */
/* IntraProcess = 0, CrossProcess = 1 */

