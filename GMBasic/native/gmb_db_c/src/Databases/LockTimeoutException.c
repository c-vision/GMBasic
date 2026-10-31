/* FILE: native/gmb_db_c/src/Databases/LockTimeoutException.c */
/* Port 1:1 di common/Databases/LockTimeoutException.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.LockTimeoutException.cs
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//namespace Basic.Databases;
//
///// <summary>
///// Eccezione lanciata quando un'operazione non riesce ad acquisire un lock
///// su una risorsa (come un record o un file) entro il tempo limite specificato.
///// Questo evento può indicare una forte contesa sulla risorsa o un potenziale deadlock.
///// </summary>
//public class LockTimeoutException : IOException
//{
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="LockTimeoutException"/> con un messaggio di errore specifico.
//    /// </summary>
//    /// <param name="message">Il messaggio che descrive l'errore.</param>
//    public LockTimeoutException(string message) : base(message) { }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

