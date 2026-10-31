/* FILE: native/gmb_db_c/src/Databases/DeadlockException.c */
/* Port 1:1 di common/Databases/DeadlockException.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.DeadlockException
//
//// ReSharper disable CheckNamespace
//// ReSharper disable InconsistentNaming
//
//namespace Basic.Databases;
//
///// <summary>
///// Eccezione lanciata quando il rilevatore di deadlock interrompe una transazione
///// per sbloccare un ciclo di attesa tra thread (vittima = transazione più giovane).
///// </summary>
//public sealed class DeadlockException : IOException
//{
//    /// <summary>
//    /// Inizializza una nuova istanza con messaggio descrittivo.
//    /// </summary>
//    /// <param name="message">Descrizione del deadlock rilevato.</param>
//    public DeadlockException(string message) : base(message)
//    {
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

