/* FILE: native/gmb_db_c/src/Databases/Commons.c */
/* Port 1:1 di common/Databases/Commons.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases;
//
///// <summary>
///// Definisce il contratto per un callback che un motore di database può usare per
///// delegare la valutazione di un'espressione di chiave all'Executor del linguaggio.
///// </summary>
///// <param name="expressionString">La stringa dell'espressione da valutare (campo, UPPER(c), JSON_VALUE, UDF se IsUnsafe).</param>
///// <param name="recordContext">Un dizionario contenente i valori dei campi del record corrente,
///// che funge da scope per la valutazione.</param>
///// <param name="targetType">Il tipo di dato di destinazione a cui il risultato deve essere convertito.</param>
///// <returns>Il risultato della valutazione dell'espressione, convertito al tipo corretto.</returns>
///// <remarks>
///// ISAM valuta prima le espressioni built-in engine-side (UPPER, JSON_VALUE, COMPOSITE via IsamKeyExpressionEvaluator).
///// Per UDF o espressioni BASIC complesse impostare
///// <c>IndexDefinition.IsUnsafe=true</c> e fornire un callback GPM dall'Executor.
///// </remarks>
//public delegate object? KeyExpressionEvaluatorCallback(string expressionString,
//    IReadOnlyDictionary<string, object?> recordContext, DataType targetType);

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

