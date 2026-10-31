// FILE: Basic.Databases.Speed.QueryPlan.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections.Immutable;

namespace Basic.Databases.Speed;

// Nota di Progettazione: Queste classi 'record' rappresentano un piano di esecuzione
// pulito e disaccoppiato dall'AST del parser. L'Executor sarà responsabile di
// mappare i nodi dell'AST in queste strutture prima di passare il piano al motore Speed.
// L'uso di 'string' per le espressioni garantisce l'indipendenza dall'Executor.

/// <summary>
/// Rappresenta un singolo campo o espressione da proiettare nel risultato di una query (clausola SELECT).
/// </summary>
/// <param name="ExpressionString">La stringa dell'espressione da valutare per questo campo (es. "UPPER(Nome)" o "Prezzo * Quantita").</param>
/// <param name="Alias">Il nome del campo nella tabella/array di destinazione (es. "NomeMaiuscolo" o "TotaleRiga").</param>
public sealed record QueryProjection(string ExpressionString, string Alias);

/// <summary>
/// Rappresenta una sorgente dati per la query (una tabella, vista o sottoquery).
/// </summary>
/// <param name="TableName">Il nome della tabella originale.</param>
/// <param name="Alias">L'alias usato per la tabella all'interno della query (es. "FROM Clienti AS c").</param>
public sealed record QuerySource(string TableName, string Alias);

/// <summary>
/// Rappresenta una clausola di join tra due sorgenti dati, strutturata per un'esecuzione efficiente
/// tramite ricerca indicizzata.
/// </summary>
/// <param name="RightSource">La tabella "destra" (interna) del join.</param>
/// <param name="LeftKeyExpressionString">L'espressione da valutare sulla tabella sinistra per ottenere il valore da cercare.</param>
/// <param name="RightIndexName">Il nome dell'indice sulla tabella destra da usare per la ricerca (SEEK).</param>
public sealed record QueryJoin(QuerySource RightSource, string LeftKeyExpressionString, string RightIndexName);

/// <summary>
/// Rappresenta una colonna di ordinamento nella clausola ORDER BY.
/// </summary>
/// <param name="ExpressionString">La stringa dell'espressione da usare per l'ordinamento.</param>
/// <param name="Descending">True se l'ordinamento è discendente (DESC), altrimenti False (ASC).</param>
public sealed record QueryOrderBy(string ExpressionString, bool Descending);

/// <summary>
/// Rappresenta il piano di esecuzione completo per una query `SPEED SELECT`.
/// Questo oggetto viene costruito dall'Executor e passato al motore `Database` per l'esecuzione.
/// Contiene solo tipi di dati semplici per garantire il disaccoppiamento.
/// </summary>
/// <param name="Projections">La lista dei campi da includere nel risultato (clausola SELECT).</param>
/// <param name="DestinationVariableName">Il nome della variabile array UDT di destinazione per i risultati (`INTO` clause).</param>
/// <param name="DestinationUdtName">Il nome del `TYPE` di destinazione per i risultati (dalla definizione dell'array).</param>
/// <param name="FromSource">La tabella di partenza (clausola FROM).</param>
/// <param name="JoinClause">La clausola di join opzionale.</param>
/// <param name="WhereClauseString">La stringa dell'espressione di filtro opzionale (clausola WHERE).</param>
/// <param name="GroupByClauseStrings">La lista di stringhe di espressioni di raggruppamento opzionali (clausola GROUP BY).</param>
/// <param name="HavingClauseString">La stringa dell'espressione di filtro post-aggregazione opzionale (clausola HAVING).</param>
/// <param name="OrderByClauses">La lista di colonne di ordinamento opzionali (clausola ORDER BY).</param>
public sealed record QueryPlan(
    ImmutableArray<QueryProjection> Projections,
    string DestinationVariableName,
    string DestinationUdtName,
    QuerySource FromSource,
    QueryJoin? JoinClause,
    string? WhereClauseString,
    ImmutableArray<string>? GroupByClauseStrings,
    string? HavingClauseString,
    ImmutableArray<QueryOrderBy>? OrderByClauses
);