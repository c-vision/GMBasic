/* FILE: native/gmb_db_c/src/SPEED/Database.c */
/* Port 1:1 di modules/Database/SPEED/Database.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Database.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Collections.Concurrent;
//using System.Collections.Immutable;
//using Basic.SPEED;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Definisce la firma per un callback in grado di valutare un'espressione in formato stringa
///// all'interno di un contesto di riga specifico. Questo disaccoppia il motore
///// di query dall'implementazione specifica dell'Executor.
///// </summary>
///// <param name="expressionString">La stringa dell'espressione da valutare.</param>
///// <param name="rowContext">Il dizionario di dati che rappresenta la riga corrente, con chiavi qualificate da alias.</param>
///// <returns>Il risultato della valutazione.</returns>
//public delegate object? ExpressionEvaluator(string expressionString, Dictionary<string, object?> rowContext);
//
///// <summary>
///// Mantiene lo stato del cursore di navigazione per una singola tabella, inclusa la posizione
///// corrente, lo stato BOF/EOF e l'indice attivo per l'ordinamento.
///// </summary>
//internal class TableCursor
//{
//    /// <summary>
//    /// L'ID della tabella a cui questo cursore si riferisce.
//    /// </summary>
//    public int TableId { get; }
//
//    /// <summary>
//    /// La chiave dell'indice attivo su cui è attualmente posizionato il cursore.
//    /// Se l'ordine è sulla chiave primaria, questo valore coincide con <see cref="CurrentPrimaryKey"/>.
//    /// </summary>
//    public byte[]? CurrentIndexKey { get; private set; }
//
//    /// <summary>
//    /// La chiave primaria del record a cui punta il cursore. Questo valore identifica
//    /// univocamente il record nella tabella principale, indipendentemente dall'ordine attivo.
//    /// </summary>
//    public byte[]? CurrentPrimaryKey { get; private set; }
//
//    /// <summary>
//    /// Il nome dell'indice attualmente utilizzato per la navigazione (l'ordine attivo).
//    /// </summary>
//    public string ActiveIndexName { get; set; }
//
//    /// <summary>
//    /// Flag che indica se il cursore è posizionato prima del primo record logico.
//    /// </summary>
//    public bool Bof { get; set; }
//
//    /// <summary>
//    /// Flag che indica se il cursore è posizionato dopo l'ultimo record logico.
//    /// </summary>
//    public bool Eof { get; set; }
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="TableCursor"/>.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella associata a questo cursore.</param>
//    /// <param name="primaryKeyName">Il nome della chiave primaria, usata come ordine di default.</param>
//    public TableCursor(int tableId, string primaryKeyName)
//    {
//        TableId = tableId;
//        ActiveIndexName = primaryKeyName;
//        ClearPosition();
//    }
//
//    /// <summary>
//    /// Imposta la posizione corrente del cursore, aggiornando sia la chiave dell'indice attivo
//    /// sia la chiave primaria del record corrispondente. Resetta i flag BOF/EOF.
//    /// </summary>
//    /// <param name="currentIndexKey">La chiave dall'indice attivo.</param>
//    /// <param name="primaryKey">La chiave primaria del record a cui puntare.</param>
//    public void SetPosition(byte[] currentIndexKey, byte[] primaryKey)
//    {
//        CurrentIndexKey = currentIndexKey;
//        CurrentPrimaryKey = primaryKey;
//        Bof = false;
//        Eof = false;
//    }
//
//    /// <summary>
//    /// Invalida la posizione del cursore, impostando le chiavi a null.
//    /// </summary>
//    public void ClearPosition()
//    {
//        CurrentIndexKey = null;
//        CurrentPrimaryKey = null;
//    }
//}
//
///// <summary>
///// Classe principale che orchestra il motore di database LSM-Tree "Speed".
///// Gestisce le operazioni di scrittura (Set/Delete), lettura (Get), le transazioni, l'esecuzione di query complesse,
///// e i processi in background come il flush delle Memtable e la compattazione.
///// </summary>
//public partial class Database : IDisposable
//{
//    #region Constants and Fields
//
//    /// <summary>
//    /// La soglia (numero di file) che, una volta raggiunta nel Livello 0, innesca
//    /// un'operazione di compattazione in background.
//    /// </summary>
//    public const int CompactLevel0Threshold = 4;
//
//    /// <summary>
//    /// Il percorso completo della directory radice del database Speed.
//    /// Reso 'internal' per essere accessibile da altri componenti del motore come il Compactor.
//    /// </summary>
//    internal string DatabasePath { get; }
//
//    /// <summary>
//    /// L'istanza del gestore del file Manifest, che tiene traccia dello schema e della struttura dei file del database.
//    /// Reso 'internal' per essere accessibile da altri componenti del motore.
//    /// </summary>
//    internal Manifest Manifest { get; }
//
//    /// <summary>
//    /// Dizionario delle Memtable attive, una per ogni tabella attualmente in uso. La chiave è l'ID della tabella.
//    /// </summary>
//    private readonly Dictionary<int, Memtable> _activeMemtables;
//
//    /// <summary>
//    /// Dizionario dei file Write-Ahead Log attivi, uno per ogni tabella con scritture in corso. La chiave è l'ID della tabella.
//    /// </summary>
//    private readonly Dictionary<int, WriteAheadLog> _activeWals;
//
//    /// <summary>
//    /// Cache thread-safe per le istanze di SstReader, per evitare di riaprire e rileggere l'indice dei file SSTable.
//    /// La chiave è il percorso completo del file SSTable.
//    /// </summary>
//    private readonly ConcurrentDictionary<string, SstReader> _sstReaderCache;
//
//    /// <summary>
//    /// Dizionario thread-safe per tracciare le operazioni di compattazione in background.
//    /// La chiave è l'ID della tabella. Assicura che al massimo una compattazione per tabella sia in esecuzione.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, Task> _compactionTasks;
//
//    /// <summary>
//    /// Dizionario per gestire lock specifici per ogni tabella.
//    /// Questo previene race condition durante le operazioni di flush e compattazione.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, object> _tableLocks = new();
//
//    /// <summary>
//    /// Traccia lo stato delle transazioni attive per ogni thread.
//    /// La chiave è l'ID del thread gestito.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, TransactionState> _transactionStates = new();
//
//    /// <summary>
//    /// Optional per-table expression evaluators for complex secondary index keys (set by GPM on USE).
//    /// ponytail: absent evaluators fall back to field-name lookup in <see cref="ComputeIndexKeyBytes"/>.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, ExpressionEvaluator> _indexKeyEvaluators = new();
//
//    /// <summary>
//    /// La dimensione massima stimata in byte che una Memtable può raggiungere prima
//    /// che venga attivato un flush automatico su disco.
//    /// </summary>
//    private readonly long _memtableThreshold = 4 * 1024 * 1024;
//
//    /// <summary>
//    /// Mantiene lo stato del cursore per ogni tabella aperta.
//    /// La chiave è l'ID della tabella, il valore è un oggetto che memorizza la posizione corrente.
//    /// </summary>
//    private readonly ConcurrentDictionary<int, TableCursor> _cursors = new();
//
//    /// <summary>
//    /// Lock globale utilizzato per proteggere l'accesso alle strutture dati interne
//    /// come `_activeMemtables` e `_activeWals` durante le operazioni di creazione/sostituzione.
//    /// Garantisce che le operazioni che modificano la lista delle memtable o dei WAL attivi
//    /// siano thread-safe.
//    /// </summary>
//    private readonly Lock _lock = new();
//
//    private int _groupCommitDepth;
//
//    /// <summary>
//    /// ponytail: SetMany batch — evita fsync per-entry su WAL indici secondari; flush unico a fine batch.
//    /// </summary>
//    private int _batchWalSuppressDepth;
//
//    /// <summary>
//    /// Controlla fsync per auto-commit e <see cref="FlushDurability"/>.
//    /// </summary>
//    public SpeedDurabilityMode DurabilityMode { get; set; } = SpeedDurabilityMode.Full;
//
//    /// <summary>
//    /// True durante group-commit — fsync differito fino a <see cref="EndGroupCommit"/>.
//    /// </summary>
//    public bool IsGroupCommitActive => _groupCommitDepth > 0;
//
//    /// <summary>
//    /// Sorgente del token di cancellazione utilizzato per segnalare a tutti i task in background
//    /// (come il Garbage Collector e i task di compattazione) di terminare in modo pulito
//    /// quando il database viene chiuso tramite <see cref="Dispose"/>.
//    /// </summary>
//    private readonly CancellationTokenSource _cts = new();
//
//    /// <summary>
//    /// Riferimento al task a lunga esecuzione del Garbage Collector.
//    /// Viene mantenuto per poterne attendere il completamento durante la fase di <see cref="Dispose"/>.
//    /// </summary>
//    private readonly Task _garbageCollectorTask;
//
//    /// <summary>
//    /// True when this instance participated in the cross-process exclusive lock (ref-counted per path).
//    /// </summary>
//    private readonly bool _crossProcessLockHeld;
//
//    #endregion
//
//    /// <summary>
//    /// Inizializza una nuova istanza del motore di database Speed.
//    /// Crea le directory necessarie se non esistono, carica il manifest, ed esegue
//    /// il recupero da eventuali file Write-Ahead Log residui da una sessione precedente interrotta.
//    /// </summary>
//    /// <param name="path">Il percorso della directory radice del database.</param>
//    /// <param name="crossProcessLock">
//    /// When true (default), acquires an exclusive cross-process lock on the database directory.
//    /// ponytail: offline validate/info requires the runtime not to hold the same DB open elsewhere.
//    /// </param>
//    public Database(string path, bool crossProcessLock = true)
//    {
//        DatabasePath = path;
//        Directory.CreateDirectory(DatabasePath);
//        Directory.CreateDirectory(Path.Combine(DatabasePath, Config.WalDirectoryName));
//        Directory.CreateDirectory(Path.Combine(DatabasePath, Config.SstDirectoryName));
//
//        if (crossProcessLock)
//        {
//            SpeedDatabaseLock.Acquire(DatabasePath);
//            _crossProcessLockHeld = true;
//        }
//
//        Manifest = Manifest.Load(DatabasePath);
//        _activeMemtables = new Dictionary<int, Memtable>();
//        _activeWals = new Dictionary<int, WriteAheadLog>();
//        _sstReaderCache = new ConcurrentDictionary<string, SstReader>();
//        _compactionTasks = new ConcurrentDictionary<int, Task>();
//        WriteAheadLog.Recover(DatabasePath, _activeMemtables);
//        MaybeRebuildUntrustedRowCounts();
//
//        // Avvia il task del Garbage Collector in background.
//        _garbageCollectorTask = RunGarbageCollectorAsync(_cts.Token);
//    }
//
//    #region Query Execution Engine
//
//    /// <summary>
//    /// Esegue un piano di query complesso, orchestrando le fasi di join, filtro, raggruppamento,
//    /// aggregazione e ordinamento, e restituisce il set di risultati finale.
//    /// </summary>
//    /// <param name="plan">L'oggetto QueryPlan che descrive la query da eseguire.</param>
//    /// <param name="evaluator">Il callback all'Executor per la valutazione delle espressioni.</param>
//    /// <returns>Una lista di dizionari, dove ogni dizionario rappresenta una riga del risultato finale.</returns>
//    public List<Dictionary<string, object?>> ExecuteQuery(QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        var (sourceRows, whereAppliedByIndex) = ExecuteJoinPhase(plan, evaluator);
//        var filteredRows = whereAppliedByIndex
//            ? sourceRows
//            : ExecuteWherePhase(sourceRows, plan, evaluator);
//        var groupedRows = ExecuteGroupByAndHavingPhase(filteredRows.ToList(), plan, evaluator);
//        var orderedRows = ExecuteOrderByPhase(groupedRows, plan, evaluator);
//        return ExecuteProjectionPhase(orderedRows, plan, evaluator);
//    }
//
//    /// <summary>
//    /// Esegue la fase di acquisizione dei dati sorgente, gestendo sia la scansione di una singola tabella
//    /// sia un'operazione di JOIN tra due tabelle. Implementa un "Nested Loop Join" indicizzato,
//    /// che itera sulla tabella esterna ("sinistra") e, per ogni riga, esegue una ricerca per chiave (SEEK)
//    /// sulla tabella interna ("destra"), garantendo performance efficienti.
//    /// </summary>
//    /// <param name="plan">Il piano di query contenente le definizioni delle sorgenti e del join.</param>
//    /// <param name="evaluator">Il delegato fornito dall'Executor per valutare la condizione di join.</param>
//    /// <returns>
//    /// Un <see cref="IEnumerable{T}"/> di dizionari. Ogni dizionario rappresenta una riga combinata
//    /// risultante dal join (o una riga singola se non c'è join), con i nomi dei campi già qualificati
//    /// con i rispettivi alias di tabella (es. "Clienti.ID", "Ordini.Importo").
//    /// </returns>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se la tabella o l'indice specificato nella clausola JOIN non vengono trovati nel manifest.
//    /// </exception>
//    private (IEnumerable<Dictionary<string, object?>> Rows, bool WhereAppliedByIndex) ExecuteJoinPhase(
//        QueryPlan plan,
//        ExpressionEvaluator evaluator)
//    {
//        IEnumerable<Dictionary<string, object?>> leftSource;
//        var whereAppliedByIndex = false;
//
//        if (plan.JoinClause is null)
//        {
//            var indexed = TryExecuteIndexedEqualitySource(plan);
//            if (indexed is not null)
//            {
//                leftSource = indexed;
//                whereAppliedByIndex = true;
//            }
//            else
//            {
//                leftSource = ExecuteSourcePhase(plan.FromSource.TableName);
//            }
//        }
//        else
//        {
//            leftSource = TryExecuteIndexedEqualitySource(plan) ?? ExecuteSourcePhase(plan.FromSource.TableName);
//        }
//
//        if (plan.JoinClause == null)
//        {
//            return (QualifyRows(leftSource, plan.FromSource.Alias), whereAppliedByIndex);
//        }
//
//        if (!Manifest.TryGetTable(plan.JoinClause.RightSource.TableName, out var rightTableMeta) ||
//            rightTableMeta is null)
//            throw new InvalidOperationException(
//                $"Tabella per il JOIN '{plan.JoinClause.RightSource.TableName}' non trovata.");
//
//        var indexToUse = QueryOptimizer.ResolveJoinIndex(
//            rightTableMeta, Manifest, plan.JoinClause.RightIndexName);
//
//        var (scanSourceTableId, isIndex) = GetScanSource(rightTableMeta.Id, indexToUse.Name);
//
//        var joinedRows = new List<Dictionary<string, object?>>();
//        foreach (var leftRow in leftSource)
//        {
//            var qualifiedLeftRow = QualifyRow(leftRow, plan.FromSource.Alias);
//            var rightKeyObject = evaluator(plan.JoinClause.LeftKeyExpressionString, qualifiedLeftRow);
//            var rightKeyToSeek = SpeedMarshaller.ToBytes(rightKeyObject, indexToUse.DataType, rightTableMeta.Schema);
//            var foundValueBytes = Get(scanSourceTableId, rightKeyToSeek);
//
//            if (foundValueBytes != null)
//            {
//                var primaryKeyBytes = isIndex ? foundValueBytes : rightKeyToSeek;
//                var rightRowBytes = Get(rightTableMeta.Id, primaryKeyBytes);
//                if (rightRowBytes != null)
//                {
//                    // Usa l'overload corretto di FromBytes che accetta solo lo schema.
//                    if (SpeedMarshaller.FromBytes(rightRowBytes, rightTableMeta.Schema) is
//                        Dictionary<string, object?> udtDict)
//                    {
//                        var rightRow = QualifyRow(udtDict, plan.JoinClause.RightSource.Alias);
//                        joinedRows.Add(qualifiedLeftRow.Concat(rightRow).ToDictionary(kvp => kvp.Key, kvp => kvp.Value,
//                            StringComparer.OrdinalIgnoreCase));
//                    }
//                }
//            }
//        }
//
//        return (joinedRows, false);
//    }
//
//    private IEnumerable<Dictionary<string, object?>> QualifyRows(
//        IEnumerable<Dictionary<string, object?>> rows,
//        string alias)
//    {
//        foreach (var row in rows)
//            yield return QualifyRow(row, alias);
//    }
//
//    /// <summary>
//    /// Esegue la scansione di una singola tabella sorgente, fungendo da produttore di righe
//    /// per le fasi successive della pipeline di query. Recupera i metadati della tabella,
//    /// itera su tutti i suoi record (usando il metodo <see cref="Scan"/>) e deserializza
//    /// ogni record dal suo formato binario a un dizionario chiave-valore.
//    /// </summary>
//    /// <param name="tableName">Il nome della tabella da scansionare, come specificato nella clausola FROM.</param>
//    /// <returns>
//    /// Un <see cref="IEnumerable{T}"/> di dizionari, dove ogni dizionario rappresenta
//    /// un record completo della tabella. Le chiavi del dizionario sono i nomi dei campi
//    /// e i valori sono i dati deserializzati.
//    /// </returns>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se la tabella con il nome specificato non viene trovata nel manifest del database.
//    /// </exception>
//    private IEnumerable<Dictionary<string, object?>> ExecuteSourcePhase(string tableName)
//    {
//        if (!Manifest.TryGetTable(tableName, out var tableMeta) || tableMeta is null)
//            throw new InvalidOperationException($"Tabella sorgente '{tableName}' non trovata.");
//
//        foreach (var kvp in Scan(tableMeta.Id))
//        {
//            if (kvp.Value != null)
//            {
//                // Usa l'overload corretto di FromBytes che accetta solo lo schema.
//                if (SpeedMarshaller.FromBytes(kvp.Value, tableMeta.Schema) is Dictionary<string, object?> udtDict)
//                    yield return udtDict;
//            }
//        }
//    }
//
//    /// <summary>
//    /// When WHERE is top-level AND of equalities on indexed fields, seek via the optimizer-chosen index
//    /// instead of scanning the main table. ponytail: AND-only; OR/range/functions fall back to scan.
//    /// </summary>
//    private IEnumerable<Dictionary<string, object?>>? TryExecuteIndexedEqualitySource(QueryPlan plan)
//    {
//        if (plan.JoinClause is not null) return null;
//
//        var equalities = QueryOptimizer.TryParseCompoundAndEqualities(plan.WhereClauseString, plan.FromSource.Alias);
//        if (equalities is null || equalities.Count == 0) return null;
//
//        if (!Manifest.TryGetTable(plan.FromSource.TableName, out var tableMeta) || tableMeta is null)
//            return null;
//
//        var predicates = equalities
//            .Select(e => new QueryOptimizer.EqualityPredicate(e.FieldName, e.TableAlias))
//            .ToList();
//
//        var accessPlan = QueryOptimizer.SelectBestIndex(
//            tableMeta,
//            Manifest,
//            predicates,
//            predicates[0].FieldName);
//        if (accessPlan is null) return null;
//
//        var indexDef = QueryOptimizer.GetIndexDefinition(tableMeta.Schema, accessPlan.IndexName);
//        if (indexDef is null) return null;
//
//        var seekPredicate = equalities.First(e =>
//            QueryOptimizer.PredicateMatchesIndex(e.FieldName, indexDef));
//
//        var literalValue = QueryOptimizer.CoerceLiteral(seekPredicate.RawValue, indexDef.DataType);
//        if (literalValue is null) return null;
//
//        var seekKey = SpeedMarshaller.ToBytes(literalValue, indexDef.DataType, tableMeta.Schema);
//        var (scanSourceTableId, isIndex) = GetScanSource(tableMeta.Id, accessPlan.IndexName);
//        var foundValue = Get(scanSourceTableId, seekKey);
//        if (foundValue is null) return [];
//
//        var primaryKeyBytes = isIndex ? foundValue : seekKey;
//        var rowBytes = Get(tableMeta.Id, primaryKeyBytes);
//        if (rowBytes is null) return [];
//
//        if (SpeedMarshaller.FromBytes(rowBytes, tableMeta.Schema) is not Dictionary<string, object?> row)
//            return [];
//
//        if (!QueryOptimizer.RowMatchesEqualities(row, tableMeta.Schema, equalities))
//            return [];
//
//        return [row];
//    }
//
//    /// <summary>
//    /// Applica la clausola WHERE a un set di righe.
//    /// </summary>
//    private IEnumerable<Dictionary<string, object?>> ExecuteWherePhase(IEnumerable<Dictionary<string, object?>> rows,
//        QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        // Controlla se la stringa 'WhereClauseString' è nulla o vuota.
//        if (string.IsNullOrEmpty(plan.WhereClauseString))
//        {
//            return rows;
//        }
//
//        // Passa la stringa 'WhereClauseString' direttamente all'evaluator.
//        return rows.Where(row => IsTrue(evaluator(plan.WhereClauseString, row)));
//    }
//
//    /// <summary>
//    /// Esegue le fasi di raggruppamento (`GROUP BY`), aggregazione (es. `SUM`, `COUNT`) e
//    /// filtro post-aggregazione (`HAVING`) su un set di dati pre-filtrato.
//    /// Se non è presente una clausola `GROUP BY`, restituisce semplicemente le righe in input.
//    /// </summary>
//    /// <param name="rows">La lista delle righe (già filtrate dalla clausola `WHERE`) da processare.
//    /// Questa lista viene materializzata in memoria per permettere il raggruppamento.</param>
//    /// <param name="plan">Il piano di query contenente le definizioni per `GROUP BY`, `HAVING` e le proiezioni.</param>
//    /// <param name="evaluator">Il delegato per valutare le espressioni di raggruppamento e le condizioni di `HAVING`.</param>
//    /// <returns>
//    /// Una nuova lista di dizionari, dove ogni dizionario rappresenta una singola riga aggregata.
//    /// Le chiavi del dizionario sono gli alias definiti nella clausola `SELECT` (sia per i campi di
//    /// raggruppamento sia per i risultati delle funzioni di aggregazione).
//    /// </returns>
//    private List<Dictionary<string, object?>> ExecuteGroupByAndHavingPhase(List<Dictionary<string, object?>> rows,
//        QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        if (plan.GroupByClauseStrings == null || !plan.GroupByClauseStrings.Value.Any())
//        {
//            var hasAggregates = plan.Projections.Any(p => p.ExpressionString.Contains('('));
//            if (!hasAggregates) return rows;
//            var singleGroupRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//            foreach (var proj in plan.Projections)
//            {
//                var values = rows.Select(row => evaluator(proj.ExpressionString, row)).ToList();
//                singleGroupRow[proj.Alias] = Aggregate(values, proj.ExpressionString);
//            }
//
//            return [singleGroupRow];
//        }
//
//        var groups =
//            new Dictionary<ImmutableArray<object?>, List<Dictionary<string, object?>>>(
//                ImmutableArrayEqualityComparer<object?>.Instance);
//        foreach (var row in rows)
//        {
//            var key = plan.GroupByClauseStrings.Value.Select(expr => evaluator(expr, row)).ToImmutableArray();
//            if (!groups.TryGetValue(key, out var groupRows))
//            {
//                groupRows = [];
//                groups[key] = groupRows;
//            }
//
//            groupRows.Add(row);
//        }
//
//        var aggregatedRows = new List<Dictionary<string, object?>>();
//        foreach (var group in groups)
//        {
//            var aggregatedRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//            foreach (var proj in plan.Projections)
//            {
//                if (proj.ExpressionString.Contains('('))
//                {
//                    var values = group.Value.Select(row => evaluator(proj.ExpressionString, row)).ToList();
//                    aggregatedRow[proj.Alias] = Aggregate(values, proj.ExpressionString);
//                }
//                else
//                {
//                    aggregatedRow[proj.Alias] = evaluator(proj.ExpressionString, group.Value.First());
//                }
//            }
//
//            if (!string.IsNullOrEmpty(plan.HavingClauseString) &&
//                !IsTrue(evaluator(plan.HavingClauseString, aggregatedRow))) continue;
//            aggregatedRows.Add(aggregatedRow);
//        }
//
//        return aggregatedRows;
//    }
//
//    /// <summary>
//    /// Ordina un set di righe in memoria in base alle colonne e alle direzioni specificate
//    /// nella clausola `ORDER BY` del piano di query. Se non è specificato alcun ordinamento,
//    /// restituisce la lista di righe originale.
//    /// </summary>
//    /// <param name="rows">La lista di righe da ordinare (tipicamente il risultato della fase di raggruppamento o di filtro).</param>
//    /// <param name="plan">Il piano di query da cui ottenere le definizioni di ordinamento.</param>
//    /// <param name="evaluator">Il delegato per valutare le espressioni di ordinamento su ogni riga,
//    /// trasformandole in valori comparabili.</param>
//    /// <returns>
//    /// Una nuova lista di dizionari contenente le righe ordinate secondo le specifiche.
//    /// L'ordinamento è stabile e supporta più colonne con direzioni miste (ASC/DESC).
//    /// </returns>
//    private List<Dictionary<string, object?>> ExecuteOrderByPhase(List<Dictionary<string, object?>> rows,
//        QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        if (plan.OrderByClauses == null || !plan.OrderByClauses.Value.Any()) return rows;
//
//        IOrderedEnumerable<Dictionary<string, object?>>? orderedRows = null;
//        for (var i = 0; i < plan.OrderByClauses.Value.Length; i++)
//        {
//            var orderBy = plan.OrderByClauses.Value[i];
//            var keySelector = (Dictionary<string, object?> row) =>
//                evaluator(orderBy.ExpressionString, row) as IComparable;
//            if (i == 0)
//            {
//                orderedRows = orderBy.Descending ? rows.OrderByDescending(keySelector) : rows.OrderBy(keySelector);
//            }
//            else
//            {
//                orderedRows = orderBy.Descending
//                    ? orderedRows!.ThenByDescending(keySelector)
//                    : orderedRows!.ThenBy(keySelector);
//            }
//        }
//
//        return orderedRows?.ToList() ?? rows;
//    }
//
//    /// <summary>
//    /// Esegue la fase finale della pipeline di query, trasformando il set di dati intermedi
//    /// nel formato di output finale definito dalla clausola `SELECT`. Per ogni riga in input,
//    /// calcola il valore di ogni espressione nella lista di proiezione e lo assegna al
//    /// campo con l'alias corrispondente nella riga di risultato.
//    /// </summary>
//    /// <param name="rows">L'enumerabile di righe processate (filtrate, raggruppate, ordinate) da proiettare.</param>
//    /// <param name="plan">Il piano di query da cui ottenere la lista delle proiezioni (campi e alias).</param>
//    /// <param name="evaluator">Il delegato per calcolare il valore di ogni espressione di proiezione.</param>
//    /// <returns>
//    /// La lista finale di dizionari, dove ogni dizionario rappresenta una riga completa
//    /// del risultato della query, pronta per essere restituita all'Executor. Se la query
//    /// includeva un `GROUP BY`, questo metodo si occupa principalmente di assicurare che
//    /// le colonne abbiano gli alias corretti, dato che i valori sono già stati calcolati.
//    /// </returns>
//    private List<Dictionary<string, object?>> ExecuteProjectionPhase(IEnumerable<Dictionary<string, object?>> rows,
//        QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        if (plan.GroupByClauseStrings != null && plan.GroupByClauseStrings.Value.Any()) return rows.ToList();
//
//        var finalResults = new List<Dictionary<string, object?>>();
//        foreach (var row in rows)
//        {
//            var projectedRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
//            foreach (var projection in plan.Projections)
//            {
//                projectedRow[projection.Alias] = evaluator(projection.ExpressionString, row);
//            }
//
//            finalResults.Add(projectedRow);
//        }
//
//        return finalResults;
//    }
//
//    #endregion
//
//    #region Public API (CRUD, Transactions, etc.)
//
//    /// <summary>
//    /// Rilascia tutte le risorse gestite dal motore di database, garantendo una chiusura pulita.
//    /// </summary>
//    public void Dispose()
//    {
//        // Segnala al Garbage Collector e ad altri task di terminare.
//        _cts.Cancel();
//        // Attende brevemente che il GC completi il suo ciclo corrente.
//        _garbageCollectorTask.Wait(1000);
//
//        Task.WhenAll(_compactionTasks.Values).Wait(5000);
//        lock (_lock)
//        {
//            FlushAllActiveWals();
//        }
//
//        foreach (var wal in _activeWals.Values) wal.Dispose();
//        foreach (var reader in _sstReaderCache.Values) reader.Dispose();
//
//        if (_crossProcessLockHeld)
//        {
//            SpeedDatabaseLock.Release(DatabasePath);
//        }
//    }
//
//    /// <summary>
//    /// Forza fsync su tutti i WAL attivi.
//    /// </summary>
//    public void FlushDurability()
//    {
//        lock (_lock)
//        {
//            FlushAllActiveWals();
//        }
//    }
//
//    /// <summary>
//    /// ponytail: differisce fsync come bulk-load ma senza transazione esplicita.
//    /// Chiamare <see cref="EndGroupCommit"/> per un fsync unico in Full mode.
//    /// </summary>
//    public void BeginGroupCommit() => _groupCommitDepth++;
//
//    /// <summary>
//    /// Termina sessione group-commit e sincronizza su disco se <see cref="SpeedDurabilityMode.Full"/>.
//    /// </summary>
//    public void EndGroupCommit()
//    {
//        if (_groupCommitDepth == 0)
//        {
//            throw new InvalidOperationException("Nessuna sessione group-commit attiva.");
//        }
//
//        _groupCommitDepth--;
//        if (_groupCommitDepth == 0 && DurabilityMode == SpeedDurabilityMode.Full)
//        {
//            FlushDurability();
//        }
//    }
//
//    private bool ShouldFsyncWal() =>
//        DurabilityMode == SpeedDurabilityMode.Full
//        && _groupCommitDepth == 0
//        && _batchWalSuppressDepth == 0;
//
//    private void MaybeFlushWal(WriteAheadLog wal)
//    {
//        if (ShouldFsyncWal())
//        {
//            wal.Flush();
//        }
//    }
//
//    private void MaybeFlushAllActiveWals()
//    {
//        if (ShouldFsyncWal())
//        {
//            FlushAllActiveWals();
//        }
//    }
//
//    private void FlushAllActiveWals()
//    {
//        foreach (var wal in _activeWals.Values)
//        {
//            wal.Flush();
//        }
//    }
//
//    /// <summary>
//    /// Avvia una nuova transazione per il thread corrente. Se una transazione è già in corso
//    /// per questo thread, l'operazione non ha effetto. Le operazioni di scrittura successive
//    /// (Set, Delete) verranno bufferizzate in memoria fino a un Commit o Rollback.
//    /// </summary>
//    public void BeginTransaction()
//    {
//        _transactionStates.TryAdd(Environment.CurrentManagedThreadId, new TransactionState());
//    }
//
//    /// <summary>
//    /// Rende permanenti le modifiche di una transazione in corso per il thread corrente.
//    /// L'operazione è atomica: itera su ogni tabella modificata all'interno della transazione,
//    /// scrive tutte le operazioni bufferizzate nel rispettivo Write-Ahead Log, e solo dopo
//    /// applica le modifiche alle Memtable principali. Se la transazione è vuota, l'operazione termina
//    /// senza eseguire I/O.
//    /// </summary>
//    public void Commit()
//    {
//        if (_transactionStates.TryRemove(Environment.CurrentManagedThreadId, out var txState))
//        {
//            if (txState.OperationBuffers.IsEmpty) return;
//            lock (_lock)
//            {
//                foreach (var (tableId, operationBuffer) in txState.OperationBuffers)
//                {
//                    if (operationBuffer.EstimatedSize == 0) continue;
//                    var wal = GetOrCreateWal(tableId);
//                    var memtable = GetOrCreateMemtable(tableId);
//                    foreach (var op in operationBuffer)
//                    {
//                        byte[]? oldValue = null;
//                        if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
//                        {
//                            oldValue = TryGetOldValueForIndex(tableId, op.Key);
//                        }
//
//                        if (op.Value != null)
//                        {
//                            AdjustRowCountOnSet(tableId, op.Key);
//                            wal.LogSet(tableId, op.Key, op.Value);
//                            memtable.Set(op.Key, op.Value);
//                            if (IsMainDataTable(tableId))
//                                MaintainSecondaryIndexesOnPut(tableId, op.Key, oldValue, op.Value);
//                        }
//                        else
//                        {
//                            AdjustRowCountOnDelete(tableId, op.Key);
//                            wal.LogDelete(tableId, op.Key);
//                            memtable.Delete(op.Key);
//                            if (IsMainDataTable(tableId))
//                                MaintainSecondaryIndexesOnDelete(tableId, op.Key, oldValue);
//                        }
//                    }
//
//                    MaybeFlushWal(wal);
//                    if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
//                }
//            }
//        }
//    }
//
//    /// <summary>
//    /// Inserisce o aggiorna molte coppie chiave-valore con un solo lock e batch WAL.
//    /// ponytail: un fsync a fine batch (se Full e non in group-commit); indici secondari senza flush per entry.
//    /// </summary>
//    public void SetMany(int tableId, IReadOnlyList<KeyValuePair<byte[], byte[]>> entries)
//    {
//        ArgumentNullException.ThrowIfNull(entries);
//        if (entries.Count == 0)
//        {
//            return;
//        }
//
//        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
//        {
//            var buffer = txState.GetBufferForTable(tableId);
//            foreach (var entry in entries)
//            {
//                buffer.Set(entry.Key, entry.Value);
//            }
//
//            return;
//        }
//
//        Memtable memtable;
//        lock (_lock)
//        {
//            memtable = GetOrCreateMemtable(tableId);
//            var wal = GetOrCreateWal(tableId);
//            var trackIndexes = IsMainDataTable(tableId) && HasSecondaryIndexes(tableId);
//
//            _batchWalSuppressDepth++;
//            try
//            {
//                foreach (var entry in entries)
//                {
//                    byte[]? oldValue = null;
//                    if (trackIndexes)
//                    {
//                        oldValue = TryGetOldValueForIndex(tableId, entry.Key);
//                    }
//
//                    AdjustRowCountOnSet(tableId, entry.Key);
//                    wal.LogSet(tableId, entry.Key, entry.Value);
//                    memtable.Set(entry.Key, entry.Value);
//                    if (trackIndexes)
//                    {
//                        MaintainSecondaryIndexesOnPut(tableId, entry.Key, oldValue, entry.Value);
//                    }
//                }
//            }
//            finally
//            {
//                _batchWalSuppressDepth--;
//                if (_batchWalSuppressDepth == 0)
//                {
//                    MaybeFlushAllActiveWals();
//                }
//            }
//        }
//
//        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
//    }
//
//    /// <summary>
//    /// Annulla una transazione in corso per il thread corrente, scartando tutte le modifiche
//    /// bufferizzate in memoria. Le operazioni registrate non verranno mai applicate
//    /// alla Memtable principale o scritte su disco.
//    /// </summary>
//    public void Rollback()
//    {
//        _transactionStates.TryRemove(Environment.CurrentManagedThreadId, out _);
//    }
//
//    /// <summary>
//    /// Inserisce o aggiorna una coppia chiave-valore per una data tabella.
//    /// Se il thread corrente si trova all'interno di una transazione attiva (iniziata con <see cref="BeginTransaction"/>),
//    /// l'operazione viene registrata in un buffer temporaneo in memoria. Altrimenti, l'operazione
//    /// viene scritta immediatamente nel Write-Ahead Log e nella Memtable attiva, garantendo l'atomicità
//    /// della singola operazione.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella in cui scrivere.</param>
//    /// <param name="key">La chiave del dato, rappresentata come array di byte.</param>
//    /// <param name="value">Il valore del dato, rappresentato come array di byte.</param>
//    public void Set(int tableId, byte[] key, byte[] value)
//    {
//        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
//        {
//            txState.GetBufferForTable(tableId).Set(key, value);
//            return;
//        }
//
//        Memtable memtable;
//        lock (_lock)
//        {
//            byte[]? oldValue = null;
//            if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
//            {
//                oldValue = TryGetOldValueForIndex(tableId, key);
//            }
//
//            AdjustRowCountOnSet(tableId, key);
//            var wal = GetOrCreateWal(tableId);
//            wal.LogSet(tableId, key, value);
//            memtable = GetOrCreateMemtable(tableId);
//            memtable.Set(key, value);
//            MaybeFlushWal(wal);
//            if (IsMainDataTable(tableId))
//                MaintainSecondaryIndexesOnPut(tableId, key, oldValue, value);
//        }
//
//        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
//    }
//
//    /// <summary>
//    /// Cancella una chiave da una data tabella. L'operazione è un "soft delete": se eseguita
//    /// al di fuori di una transazione, scrive una "tombstone" (valore nullo) nel Write-Ahead Log
//    /// e nella Memtable. Se all'interno di una transazione, la tombstone viene bufferizzata.
//    /// La rimozione fisica dei dati avverrà durante un processo di compattazione futuro.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella in cui eseguire la cancellazione.</param>
//    /// <param name="key">La chiave da cancellare, rappresentata come array di byte.</param>
//    public void Delete(int tableId, byte[] key)
//    {
//        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
//        {
//            txState.GetBufferForTable(tableId).Delete(key);
//            return;
//        }
//
//        Memtable memtable;
//        lock (_lock)
//        {
//            byte[]? oldValue = null;
//            if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
//            {
//                oldValue = TryGetOldValueForIndex(tableId, key);
//            }
//
//            AdjustRowCountOnDelete(tableId, key);
//            var wal = GetOrCreateWal(tableId);
//            wal.LogDelete(tableId, key);
//            memtable = GetOrCreateMemtable(tableId);
//            memtable.Delete(key);
//            MaybeFlushWal(wal);
//            if (IsMainDataTable(tableId))
//                MaintainSecondaryIndexesOnDelete(tableId, key, oldValue);
//        }
//
//        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
//    }
//
//    /// <summary>
//    /// Recupera il valore più recente associato a una chiave da una data tabella.
//    /// La ricerca avviene a cascata per garantire che venga restituita la versione più aggiornata
//    /// e visibile del dato, rispettando la semantica "read-your-own-writes" per le transazioni.
//    /// L'ordine di ricerca è:
//    /// 1. Buffer della transazione corrente (se attiva).
//    /// 2. Memtable attiva in memoria.
//    /// 3. File SSTable su disco, dal più recente al più vecchio, attraverso tutti i livelli.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella da cui leggere.</param>
//    /// <param name="key">La chiave da cercare, rappresentata come array di byte.</param>
//    /// <returns>Il valore come array di byte, o <c>null</c> se la chiave non esiste o è stata cancellata (tombstone).</returns>
//    public byte[]? Get(int tableId, byte[] key)
//    {
//        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
//        {
//            if (txState.OperationBuffers.TryGetValue(tableId, out var tableBuffer) &&
//                tableBuffer.Get(key, out var txValue))
//                return txValue;
//        }
//
//        Memtable currentMemtable;
//        lock (_lock)
//        {
//            currentMemtable = GetOrCreateMemtable(tableId);
//        }
//
//        if (currentMemtable.Get(key, out var memValue)) return memValue;
//
//        if (Manifest.TryGetTable(tableId, out var tableMetadata) && tableMetadata != null)
//        {
//            foreach (var sstFileName in tableMetadata.SstFilesByLevel.Keys.OrderBy(k => k)
//                         .SelectMany(level => tableMetadata.GetFilesForLevel(level).Reverse()))
//            {
//                var reader = _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName),
//                    path => new SstReader(path));
//                if (reader.TryFind(key, out var sstValue)) return sstValue;
//            }
//        }
//
//        return null;
//    }
//
//    /// <summary>
//    /// Esegue una scansione di un intervallo di chiavi per una data tabella e restituisce
//    /// un enumeratore per iterare sui risultati in ordine. L'operazione lavora su uno snapshot
//    /// consistente dello stato del database (Memtable e lista di SSTable) al momento della chiamata,
//    /// garantendo che l'iterazione non sia influenzata da modifiche concorrenti.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella su cui eseguire la scansione.</param>
//    /// <param name="startKey">La chiave di inizio (inclusiva) della scansione. Se null, la scansione parte dall'inizio.</param>
//    /// <param name="endKey">La chiave di fine (esclusiva) della scansione. Se null, la scansione arriva fino alla fine.</param>
//    /// <returns>Un <see cref="IEnumerable{T}"/> che produce le coppie chiave-valore nell'intervallo specificato, unendo i dati da tutte le fonti.</returns>
//    public IEnumerable<KeyValuePair<byte[], byte[]?>> Scan(int tableId, byte[]? startKey = null, byte[]? endKey = null)
//    {
//        Memtable memtableSnapshot;
//        List<string> sstFilesSnapshot;
//        lock (_lock)
//        {
//            memtableSnapshot = GetOrCreateMemtable(tableId);
//            sstFilesSnapshot = Manifest.TryGetTable(tableId, out var meta) && meta != null
//                ? meta.SstFilesByLevel.SelectMany(kvp => kvp.Value).ToList()
//                : [];
//        }
//
//        var readers = sstFilesSnapshot.Select(f =>
//            _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, f),
//                path => new SstReader(path))).ToList();
//        return new DatabaseIterator(memtableSnapshot, readers, startKey, endKey);
//    }
//
//    /// <summary>
//    /// Crea una nuova tabella nel database basata su uno schema fornito, se non esiste già.
//    /// Se la tabella con lo stesso nome esiste già, valida che lo schema fornito sia identico
//    /// a quello memorizzato per prevenire corruzione dei dati.
//    /// L'operazione è thread-safe.
//    /// </summary>
//    /// <param name="schema">Lo schema completo della tabella da creare.</param>
//    /// <returns>I metadati della tabella, sia essa nuova o preesistente e validata.</returns>
//    /// <exception cref="InvalidOperationException">
//    /// Lanciata se la tabella esiste ma lo schema fornito non corrisponde a quello esistente.
//    /// </exception>
//    public TableMetadata CreateTable(Config.SpeedSchema schema)
//    {
//        var meta = Manifest.CreateTable(schema);
//        _tableLocks.TryAdd(meta.Id, new object());
//        if (!schema.TypeName.StartsWith("_idx_", StringComparison.Ordinal))
//            EnsureSecondaryIndexTables(meta);
//        return meta;
//    }
//
//    /// <summary>
//    /// Registers an expression evaluator for secondary index key extraction on a main table.
//    /// </summary>
//    public void SetIndexKeyEvaluator(int mainTableId, ExpressionEvaluator evaluator)
//    {
//        ArgumentNullException.ThrowIfNull(evaluator);
//        _indexKeyEvaluators[mainTableId] = evaluator;
//    }
//
//    #endregion
//
//    #region Schema Modification (Index & View Management)
//
//    /// <summary>
//    /// Rimuove un indice secondario da una tabella. L'operazione rimuove la definizione
//    /// dell'indice dallo schema della tabella principale e la tabella-indice associata,
//    /// schedulando i suoi file per la cancellazione in background dal Garbage Collector.
//    /// L'intera operazione è thread-safe.
//    /// </summary>
//    /// <param name="mainTableId">L'ID della tabella principale da cui rimuovere l'indice.</param>
//    /// <param name="indexName">Il nome dell'indice secondario da eliminare.</param>
//    /// <exception cref="InvalidOperationException">Lanciata se la tabella principale non viene trovata.</exception>
//    public void DropIndex(int mainTableId, string indexName)
//    {
//        // Recupera i metadati della tabella principale per costruirne il nome.
//        if (!Manifest.TryGetTable(mainTableId, out var mainTableMeta) || mainTableMeta is null)
//            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");
//
//        // Costruisce il nome della tabella-indice da eliminare.
//        var indexTableName = $"_idx_{mainTableMeta.Name}_{indexName}";
//
//        // La manipolazione del manifest è già thread-safe al suo interno.
//        var (tableId, filesToDelete) = Manifest.PrepareTableForDeletion(indexTableName);
//
//        if (tableId != -1) // Se la tabella-indice è stata trovata e preparata per la cancellazione
//        {
//            // Aggiunge i file alla lista di pulizia.
//            Manifest.AddFilesToGarbageCollection(filesToDelete);
//
//            // Rimuove la definizione dell'indice secondario dallo schema della tabella principale.
//            Manifest.RemoveSecondaryIndex(mainTableMeta.Name, indexName);
//
//            // Salva tutte le modifiche al manifest.
//            Manifest.Save();
//
//            // Acquisisce un lock globale per modificare in sicurezza le strutture dati in memoria.
//            lock (_lock)
//            {
//                // Rimuovi eventuali risorse in memoria associate alla tabella-indice cancellata.
//                _activeMemtables.Remove(tableId);
//                _activeWals.Remove(tableId);
//            }
//
//            _cursors.TryRemove(tableId, out _);
//            _tableLocks.TryRemove(tableId, out _);
//        }
//    }
//
//    /// <summary>
//    /// Rimuove una vista materializzata dal database. L'operazione rimuove la tabella
//    /// che contiene i dati della vista e schedula i suoi file per la cancellazione in background.
//    /// </summary>
//    /// <param name="viewName">Il nome della vista da eliminare.</param>
//    public void DropView(string viewName)
//    {
//        DropTableInternal(viewName);
//    }
//
//    /// <summary>
//    /// Svuota una tabella (usato per le viste) e la ripopola eseguendo una query.
//    /// L'operazione è atomica dal punto di vista del manifest: la vecchia tabella viene
//    /// rimossa e una nuova viene creata e popolata.
//    /// </summary>
//    /// <param name="viewName">Il nome della vista/tabella da aggiornare.</param>
//    /// <param name="plan">Il piano di query per generare i nuovi dati.</param>
//    /// <param name="evaluator">Il delegato per la valutazione delle espressioni della query.</param>
//    /// <exception cref="InvalidOperationException">Lanciata se la vista specificata non viene trovata nel manifest.</exception>
//    public void RefreshView(string viewName, QueryPlan plan, ExpressionEvaluator evaluator)
//    {
//        if (!Manifest.TryGetTable(viewName, out var viewMeta) || viewMeta is null)
//            throw new InvalidOperationException($"Vista '{viewName}' non trovata.");
//
//        // Salva lo schema originale della vista.
//        var originalSchema = viewMeta.Schema;
//
//        // 1. Svuota la tabella della vista rimuovendola e schedulando i suoi file per la cancellazione.
//        DropTableInternal(viewName);
//
//        // 2. Ricrea la tabella con lo stesso schema. Questo le assegnerà un nuovo ID interno
//        //    e la lascerà vuota, pronta per essere popolata.
//        var newMeta = CreateTable(originalSchema);
//
//        // 3. Esegui la query di definizione della vista per ottenere il nuovo set di dati.
//        var newRows = ExecuteQuery(plan, evaluator);
//
//        // 4. Inserisci i nuovi dati nella tabella della vista appena ricreata.
//        //    Questa operazione può essere lunga e viene eseguita in un'unica transazione implicita
//        //    dal punto di vista del motore (anche se non è una transazione ACID formale a livello utente).
//        BeginTransaction();
//        try
//        {
//            foreach (var row in newRows)
//            {
//                var pkField = newMeta.Schema.PrimaryKey;
//
//                // Estrae il valore della chiave primaria dalla riga di risultato.
//                // Il nome del campo nella riga deve corrispondere al nome del campo PK nello schema della vista.
//                if (!row.TryGetValue(pkField.Name, out var pkValue))
//                    throw new InvalidOperationException(
//                        $"La query per la vista '{viewName}' non produce un valore per la sua chiave primaria '{pkField.Name}'.");
//
//                // Serializza la chiave e l'intero record (il valore).
//                var keyBytes = SpeedMarshaller.ToBytes(pkValue, pkField.DataType, newMeta.Schema);
//                var valueBytes = SpeedMarshaller.ToBytes(row, newMeta.Schema);
//
//                // Inserisce il nuovo record.
//                Set(newMeta.Id, keyBytes, valueBytes);
//            }
//
//            Commit();
//        }
//        catch
//        {
//            Rollback();
//            throw; // Rilancia l'eccezione originale dopo aver annullato la transazione.
//        }
//    }
//
//    /// <summary>
//    /// Logica interna condivisa per rimuovere una tabella (che sia una tabella dati,
//    /// un indice o una vista) dal manifest e schedulare i suoi file per il garbage collector.
//    /// L'operazione è thread-safe.
//    /// </summary>
//    /// <param name="tableName">Il nome della tabella da rimuovere.</param>
//    private void DropTableInternal(string tableName)
//    {
//        // La manipolazione del manifest è già thread-safe al suo interno.
//        var (tableId, filesToDelete) = Manifest.PrepareTableForDeletion(tableName);
//
//        if (tableId != -1) // Se la tabella è stata trovata e preparata per la cancellazione
//        {
//            // Aggiunge i file alla lista di pulizia e salva il manifest.
//            Manifest.AddFilesToGarbageCollection(filesToDelete);
//            Manifest.Save();
//
//            // Acquisisce un lock globale per modificare in sicurezza le strutture dati in memoria.
//            lock (_lock)
//            {
//                // Rimuovi eventuali risorse in memoria associate alla tabella cancellata.
//                _activeMemtables.Remove(tableId);
//                _activeWals.Remove(tableId);
//            }
//
//            // Queste strutture sono ConcurrentDictionary, quindi la loro rimozione è thread-safe
//            // e non necessita del lock globale.
//            _cursors.TryRemove(tableId, out _);
//            _tableLocks.TryRemove(tableId, out _);
//        }
//    }
//
//    #endregion
//
//    #region Navigazione e Cursori
//
//    /// <summary>
//    /// Imposta l'indice attivo per la navigazione su una tabella.
//    /// </summary>
//    public bool SetOrder(int tableId, string indexName)
//    {
//        if (!Manifest.TryGetTable(tableId, out var tableMeta) || tableMeta is null) return false;
//        var cursor = GetCursor(tableId);
//        if (string.IsNullOrEmpty(indexName) ||
//            indexName.Equals(tableMeta.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
//        {
//            cursor.ActiveIndexName = tableMeta.Schema.PrimaryKey.Name;
//            return true;
//        }
//
//        if (tableMeta.Schema.SecondaryKeys.Any(sk => sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase)))
//        {
//            cursor.ActiveIndexName = indexName;
//            return true;
//        }
//
//        return false;
//    }
//
//    /// <summary>
//    /// Posiziona il cursore sul primo record di una tabella, secondo l'ordine attivo.
//    /// </summary>
//    public bool GoTop(int tableId)
//    {
//        var cursor = GetCursor(tableId);
//        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
//        var firstRecord = Scan(scanSourceTableId).FirstOrDefault();
//        if (firstRecord.Key != null)
//        {
//            var primaryKey = isIndex ? firstRecord.Value : firstRecord.Key;
//            if (primaryKey == null) return MoveToEmpty(cursor);
//            cursor.SetPosition(firstRecord.Key, primaryKey);
//            return true;
//        }
//
//        return MoveToEmpty(cursor);
//    }
//
//    /// <summary>
//    /// Posiziona il cursore sull'ultimo record di una tabella, secondo l'ordine attivo.
//    /// </summary>
//    public bool GoBottom(int tableId)
//    {
//        var cursor = GetCursor(tableId);
//        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
//        var lastRecord = Scan(scanSourceTableId).LastOrDefault();
//        if (lastRecord.Key != null)
//        {
//            var primaryKey = isIndex ? lastRecord.Value : lastRecord.Key;
//            if (primaryKey == null) return MoveToEmpty(cursor);
//            cursor.SetPosition(lastRecord.Key, primaryKey);
//            return true;
//        }
//
//        return MoveToEmpty(cursor);
//    }
//
//    /// <summary>
//    /// Muove il cursore di un numero specificato di posizioni. Questa versione è stata corretta per
//    /// gestire la navigazione all'indietro (`recordCount mipre di 0`) in modo efficiente in termini di memoria,
//    /// evitando di caricare l'intero set di dati su disco.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella su cui operare.</param>
//    /// <param name="recordCount">Il numero di record da saltare. Positivo per andare avanti, negativo per tornare indietro. Default è 1.</param>
//    /// <returns>True se il cursore è stato posizionato su un record valido, altrimenti False (se viene raggiunto BOF o EOF).</returns>
//    public bool Skip(int tableId, int recordCount = 1)
//    {
//        var cursor = GetCursor(tableId);
//        if (cursor.CurrentIndexKey == null || recordCount == 0) return false;
//        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
//        var direction = Math.Sign(recordCount);
//        var toSkip = Math.Abs(recordCount);
//
//        KeyValuePair<byte[], byte[]?> finalRecord = default;
//
//        if (direction > 0)
//        {
//            // La navigazione in avanti è efficiente e può rimanere invariata.
//            var scan = Scan(scanSourceTableId, cursor.CurrentIndexKey).Skip(1).Take(toSkip);
//            finalRecord = scan.LastOrDefault();
//        }
//        else // Navigazione all'indietro
//        {
//            // Sostituisce .Reverse() con un approccio a memoria limitata.
//            // Mantiene un buffer degli ultimi 'toSkip' record incontrati prima della posizione corrente.
//            var buffer = new List<KeyValuePair<byte[], byte[]?>>(toSkip);
//            foreach (var kvp in Scan(scanSourceTableId, null, cursor.CurrentIndexKey))
//            {
//                buffer.Add(kvp);
//                if (buffer.Count > toSkip)
//                {
//                    buffer.RemoveAt(0); // Mantiene il buffer alla dimensione massima di 'toSkip'.
//                }
//            }
//
//            // Se abbiamo trovato esattamente il numero di record richiesto, il primo elemento
//            // del buffer è la nostra destinazione.
//            if (buffer.Count == toSkip)
//            {
//                finalRecord = buffer[0];
//            }
//            // Altrimenti (buffer.Count < toSkip), non ci sono abbastanza record.
//            // 'finalRecord' rimane default, e il codice successivo gestirà correttamente il caso BOF.
//        }
//
//        if (finalRecord.Key != null)
//        {
//            var pk = isIndex ? finalRecord.Value : finalRecord.Key;
//            if (pk == null) return MoveToBoundary(cursor, direction); // Trovato un record cancellato
//            cursor.SetPosition(finalRecord.Key, pk);
//            return true;
//        }
//
//        // Se finalRecord.Key è null, significa che abbiamo superato i limiti della tabella.
//        return MoveToBoundary(cursor, direction);
//    }
//
//    /// <summary>
//    /// Recupera il record alla posizione corrente del cursore.
//    /// </summary>
//    public byte[]? GetCurrent(int tableId)
//    {
//        if (_cursors.TryGetValue(tableId, out var cursor) && cursor.CurrentPrimaryKey != null)
//        {
//            return Get(tableId, cursor.CurrentPrimaryKey);
//        }
//
//        return null;
//    }
//
//    #endregion
//
//    #region Metodi Helper per Query
//
//    /// <summary>
//    /// Determina se un valore rappresenta la verità secondo la logica del linguaggio BASIC (booleano true o numero diverso da zero).
//    /// </summary>
//    /// <param name="value">Il valore da controllare.</param>
//    /// <returns>True se il valore è "truthy", altrimenti False.</returns>
//    private bool IsTrue(object? value) => value switch
//    {
//        bool b => b,
//        int i => i != 0,
//        long l => l != 0,
//        double d => d != 0.0,
//        decimal m => m != 0.0m,
//        _ => false
//    };
//
//    /// <summary>
//    /// Prepara una riga di dati per la valutazione qualificando ogni nome di campo con l'alias della sua tabella.
//    /// </summary>
//    /// <param name="row">Il dizionario che rappresenta la riga.</param>
//    /// <param name="alias">L'alias della tabella da anteporre ai nomi dei campi.</param>
//    /// <returns>Un nuovo dizionario con le chiavi qualificate (es. "ID" -> "Clienti.ID").</returns>
//    private Dictionary<string, object?> QualifyRow(Dictionary<string, object?> row, string alias)
//    {
//        return row.ToDictionary(kvp => $"{alias}.{kvp.Key}", kvp => kvp.Value, StringComparer.OrdinalIgnoreCase);
//    }
//
//    /// <summary>
//    /// Calcola una funzione di aggregazione su una lista di valori.
//    /// </summary>
//    /// <param name="values">La lista di valori su cui calcolare l'aggregazione.</param>
//    /// <param name="expression">La stringa dell'espressione di aggregazione (es. "SUM(Importo)").</param>
//    /// <returns>Il risultato dell'aggregazione.</returns>
//    private object? Aggregate(List<object?> values, string expression)
//    {
//        var funcName = expression[..expression.IndexOf('(')].Trim().ToUpperInvariant();
//        var numericValues = values.Where(v => v != null).Select(v =>
//        {
//            try
//            {
//                return Convert.ToDecimal(v);
//            }
//            catch
//            {
//                return (decimal?)null;
//            }
//        }).Where(v => v.HasValue).ToList();
//
//        return funcName switch
//        {
//            "COUNT" => (long)values.Count,
//            "SUM" => numericValues.Sum(v => v.GetValueOrDefault()),
//            "AVG" => numericValues.Count != 0 ? numericValues.Average(v => v.GetValueOrDefault()) : 0m,
//            "MIN" => values.Min(),
//            "MAX" => values.Max(),
//            _ => null
//        };
//    }
//
//    #endregion
//
//    #region Metodi Helper
//
//    /// <summary>
//    /// Recupera il cursore per una data tabella, creandolo se non esiste.
//    /// Questo metodo centralizza la logica di inizializzazione del cursore, garantendo
//    /// che ogni tabella abbia un solo cursore associato.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella per cui ottenere il cursore.</param>
//    /// <returns>L'istanza di <see cref="TableCursor"/> associata alla tabella.</returns>
//    /// <exception cref="InvalidOperationException">Se i metadati per la tabella non vengono trovati,
//    /// rendendo impossibile determinare la chiave primaria di default per il cursore.</exception>
//    private TableCursor GetCursor(int tableId)
//    {
//        if (_cursors.TryGetValue(tableId, out var cursor)) return cursor;
//        if (!Manifest.TryGetTable(tableId, out var tableMeta) || tableMeta is null)
//            throw new InvalidOperationException($"Tabella con ID {tableId} non trovata.");
//        return _cursors.GetOrAdd(tableId, id => new TableCursor(id, tableMeta.Schema.PrimaryKey.Name));
//    }
//
//    /// <summary>
//    /// Determina quale tabella scansionare (la tabella dati principale o una tabella-indice)
//    /// in base all'indice attivo specificato.
//    /// </summary>
//    /// <param name="mainTableId">L'ID della tabella dati principale su cui si sta operando.</param>
//    /// <param name="indexName">Il nome dell'indice attivo, usato per decidere la sorgente della scansione.</param>
//    /// <returns>
//    /// Una tupla contenente:
//    /// - <c>tableId</c>: L'ID della tabella fisica da scansionare (o la tabella principale o la tabella-indice).
//    /// - <c>isIndex</c>: Un booleano che è `true` se la scansione avviene su una tabella-indice.
//    /// </returns>
//    /// <exception cref="InvalidOperationException">Se la tabella principale o l'indice specificato non esistono.</exception>
//    private (int tableId, bool isIndex) GetScanSource(int mainTableId, string indexName)
//    {
//        if (!Manifest.TryGetTable(mainTableId, out var mainTableMeta) || mainTableMeta is null)
//            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");
//        if (indexName.Equals(mainTableMeta.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
//            return (mainTableId, false);
//
//        var indexTableName = QueryOptimizer.GetIndexTableName(mainTableMeta.Name, indexName);
//        if (Manifest.TryGetTable(indexTableName, out var indexTableMeta) && indexTableMeta is not null)
//            return (indexTableMeta.Id, true);
//
//        throw new InvalidOperationException($"Indice '{indexName}' non trovato per la tabella '{mainTableMeta.Name}'.");
//    }
//
//    /// <summary>
//    /// Imposta un cursore allo stato "vuoto", invalidando la sua posizione e impostando
//    /// sia il flag BOF che EOF a true. Usato quando una navigazione (es. GoTop) non trova record.
//    /// </summary>
//    /// <param name="cursor">Il cursore da modificare.</param>
//    /// <returns>Restituisce sempre False, per coerenza con i metodi di navigazione che indicano un fallimento.</returns>
//    private bool MoveToEmpty(TableCursor cursor)
//    {
//        cursor.ClearPosition();
//        cursor.Bof = true;
//        cursor.Eof = true;
//        return false;
//    }
//
//    /// <summary>
//    /// Imposta un cursore a uno dei limiti (BOF o EOF) dopo un'operazione di `Skip` fallita.
//    /// </summary>
//    /// <param name="cursor">Il cursore da modificare.</param>
//    /// <param name="direction">La direzione dello `Skip` (+1 per impostare EOF, -1 per impostare BOF).</param>
//    /// <returns>Restituisce sempre False, indicando che lo `Skip` non ha trovato un record valido.</returns>
//    private bool MoveToBoundary(TableCursor cursor, int direction)
//    {
//        cursor.ClearPosition();
//        if (direction > 0)
//        {
//            cursor.Eof = true;
//            cursor.Bof = false;
//        }
//        else
//        {
//            cursor.Bof = true;
//            cursor.Eof = false;
//        }
//
//        return false;
//    }
//
//    /// <summary>
//    /// Esegue un task in background a lunga esecuzione che si occupa di eliminare
//    /// periodicamente i file SSTable non più referenziati (orfani).
//    /// </summary>
//    /// <param name="token">Il token di cancellazione per fermare il task in modo pulito.</param>
//    private async Task RunGarbageCollectorAsync(CancellationToken token)
//    {
//        // Esegui in un loop finché non viene richiesta la cancellazione.
//        while (!token.IsCancellationRequested)
//        {
//            try
//            {
//                // Attendi per un intervallo di tempo prima del prossimo ciclo di pulizia.
//                await Task.Delay(TimeSpan.FromMinutes(5), token);
//
//                var filesToDelete = Manifest.GetFilesForGarbageCollection();
//                if (filesToDelete.Count == 0) continue;
//
//                var manifestChanged = false;
//                foreach (var fileName in filesToDelete)
//                {
//                    var filePath = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
//                    if (File.Exists(filePath))
//                    {
//                        try
//                        {
//                            // Tenta di eliminare il file.
//                            File.Delete(filePath);
//                            Manifest.RemoveFileFromGarbageCollection(fileName);
//                            manifestChanged = true;
//                        }
//                        catch (IOException)
//                        {
//                            // Il file potrebbe essere ancora in uso da un reader o un altro processo.
//                            // Verrà riprovato al prossimo ciclo.
//                        }
//                    }
//                    else
//                    {
//                        // Se il file non esiste già, puliscilo dalla lista.
//                        Manifest.RemoveFileFromGarbageCollection(fileName);
//                        manifestChanged = true;
//                    }
//                }
//
//                // Salva il manifest solo se la lista di pulizia è stata modificata.
//                if (manifestChanged)
//                {
//                    Manifest.Save();
//                }
//            }
//            catch (TaskCanceledException)
//            {
//                // Uscita pulita richiesta da Dispose().
//                break;
//            }
//            catch (Exception ex)
//            {
//                // Logga l'errore e continua. Il GC non deve mai crashare.
//                // TODO: Sostituire con un sistema di logging vero.
//                Console.WriteLine($"ERRORE NEL GARBAGE COLLECTOR DEL MOTORE SPEED: {ex.Message}");
//            }
//        }
//    }
//
//    /// <summary>
//    /// Attiva il processo di flush di una Memtable su disco quando questa supera la soglia di dimensione.
//    /// L'operazione è protetta da un lock globale per sostituire in modo sicuro la Memtable attiva
//    /// con una nuova e avviare il task di scrittura in background.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella il cui Memtable deve essere "flushato".</param>
//    /// <param name="memtableToFlush">L'istanza specifica della Memtable che ha superato la soglia.</param>
//    private void TriggerFlush(int tableId, Memtable memtableToFlush)
//    {
//        lock (_lock)
//        {
//            if (_activeMemtables.GetValueOrDefault(tableId) != memtableToFlush) return;
//            _activeMemtables[tableId] = new Memtable();
//            var oldWal = _activeWals[tableId];
//            _activeWals.Remove(tableId);
//            Task.Run(() => FlushAndCompactAsync(tableId, memtableToFlush, oldWal));
//        }
//    }
//
//    /// <summary>
//    /// Avvia una catena di compattazioni a cascata per una data tabella.
//    /// Controlla il Livello 0, e se viene compattato, controlla il Livello 1, e così via.
//    /// L'operazione viene eseguita in un task "fire-and-forget" per non bloccare il chiamante.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella da controllare.</param>
//    private void TriggerCompactionChain(int tableId)
//    {
//        _ = _compactionTasks.GetOrAdd(tableId, id =>
//        {
//            return Task.Run(async () =>
//            {
//                if (Manifest.TryGetTable(id, out var table) && table != null)
//                {
//                    // Passa 'this' (l'istanza di Database) invece dei singoli parametri.
//                    if (table.GetFilesForLevel(0).Count() >= CompactLevel0Threshold)
//                    {
//                        await Compactor.CompactLevel0Async(this, id, _sstReaderCache);
//                    }
//
//                    // Ricarica i metadati della tabella perché la compattazione L0 potrebbe averli cambiati.
//                    if (Manifest.TryGetTable(id, out table) && table != null)
//                    {
//                        var levels = table.SstFilesByLevel.Keys.OrderBy(k => k).ToList();
//                        foreach (var level in levels)
//                        {
//                            // La condizione di compattazione per i livelli superiori deve essere controllata qui.
//                            if (level > 0 && table.GetFilesForLevel(level).Count() >= 10) // Usa la soglia corretta
//                            {
//                                await Compactor.CompactLevelAsync(this, id, level, _sstReaderCache);
//                            }
//                        }
//                    }
//                }
//            }).ContinueWith(t =>
//            {
//                if (t.IsFaulted)
//                {
//                    Console.WriteLine(
//                        $"ERRORE CRITICO: La catena di compattazione per la tabella {tableId} è fallita: {t.Exception?.InnerException?.Message}");
//                }
//
//                _compactionTasks.TryRemove(tableId, out _);
//            });
//        });
//    }
//
//    /// <summary>
//    /// Cerca un record per chiave utilizzando un indice specificato e, se trovato, posiziona
//    /// il cursore su di esso.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella principale su cui eseguire la ricerca.</param>
//    /// <param name="keyToSeek">La rappresentazione in byte della chiave da cercare.</param>
//    /// <param name="indexName">Il nome dell'indice da utilizzare. Se nullo o vuoto, viene usata la chiave primaria.</param>
//    /// <returns>True se un record viene trovato e il cursore viene posizionato, altrimenti False.</returns>
//    public bool Seek(int tableId, byte[] keyToSeek, string? indexName)
//    {
//        var cursor = GetCursor(tableId);
//        var (scanSourceTableId, isIndex) = GetScanSource(tableId, indexName ?? cursor.ActiveIndexName);
//
//        var foundValue = Get(scanSourceTableId, keyToSeek);
//
//        if (foundValue != null)
//        {
//            byte[] primaryKey;
//            byte[] indexKey;
//
//            if (isIndex)
//            {
//                // La chiave trovata è quella dell'indice, il valore è la chiave primaria.
//                indexKey = keyToSeek;
//                primaryKey = foundValue;
//
//                // Verifica che il record a cui punta l'indice esista ancora.
//                if (Get(tableId, primaryKey) == null)
//                {
//                    cursor.ClearPosition();
//                    return false; // L'indice punta a un record cancellato/inesistente.
//                }
//            }
//            else
//            {
//                // La chiave trovata è la chiave primaria.
//                indexKey = keyToSeek;
//                primaryKey = keyToSeek;
//            }
//
//            cursor.SetPosition(indexKey, primaryKey);
//            return true;
//        }
//
//        // Se non viene trovato nulla, invalida la posizione del cursore.
//        cursor.ClearPosition();
//        return false;
//    }
//
//    /// <summary>
//    /// Esegue in background l'operazione di flush di una Memtable su un nuovo file SSTable e,
//    /// se necessario, avvia un'operazione di compattazione a cascata.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella interessata.</param>
//    /// <param name="memtableToFlush">La Memtable "congelata" da scrivere su disco.</param>
//    /// <param name="walToClose">Il file WAL associato alla Memtable da chiudere e cancellare.</param>
//    private void FlushAndCompactAsync(int tableId, Memtable memtableToFlush, WriteAheadLog walToClose)
//    {
//        var tableLock = _tableLocks.GetOrAdd(tableId, _ => new object());
//        lock (tableLock)
//        {
//            var fileNumber = Manifest.GetNextFileNumber();
//            var sstFileName = $"{fileNumber:D6}_{tableId}.sst";
//            var sstPath = Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName);
//            SstWriter.WriteFromMemtable(sstPath, memtableToFlush);
//            Manifest.AddSstFile(tableId, sstPath);
//            Manifest.Save();
//            walToClose.Dispose();
//            var walPath = walToClose.GetWalPath();
//            if (File.Exists(walPath)) File.Delete(walPath);
//            TriggerCompactionChain(tableId);
//        }
//    }
//
//    /// <summary>
//    /// Recupera la Memtable attiva per una data tabella, creandola se non esiste.
//    /// L'operazione è thread-safe.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella.</param>
//    /// <returns>L'istanza di Memtable attiva.</returns>
//    private Memtable GetOrCreateMemtable(int tableId)
//    {
//        if (!_activeMemtables.TryGetValue(tableId, out var memtable))
//        {
//            memtable = new Memtable();
//            _activeMemtables[tableId] = memtable;
//        }
//
//        return memtable;
//    }
//
//    /// <summary>
//    /// Recupera il Write-Ahead Log attivo per una data tabella, creandolo se non esiste.
//    /// L'operazione è thread-safe e aggiorna il manifest con il nuovo numero di file.
//    /// </summary>
//    /// <param name="tableId">L'ID della tabella.</param>
//    /// <returns>L'istanza di WriteAheadLog attiva.</returns>
//    private WriteAheadLog GetOrCreateWal(int tableId)
//    {
//        if (!_activeWals.TryGetValue(tableId, out var wal))
//        {
//            wal = new WriteAheadLog(DatabasePath, Manifest.GetNextFileNumber());
//            _activeWals[tableId] = wal;
//            Manifest.Save();
//        }
//
//        return wal;
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#define SPEED_MAX_TABLES 32
#define SPEED_MAX_SST_CACHE 64
#define SPEED_SST_SEARCH_MAX 64

typedef struct {
    int tableId;
    Memtable* mem;
} SpeedMemSlot;

typedef struct {
    int tableId;
    SpeedWriteAheadLog* wal;
} SpeedWalSlot;

typedef struct {
    char path[1024];
    SstReader* reader;
} SpeedSstCacheSlot;

typedef struct SpeedTableCursor {
    int used;
    int tableId;
    uint8_t* indexKey;
    int indexKeyLen;
    uint8_t* pk;
    int pkLen;
    char activeIndex[64];
    int bof;
    int eof;
} SpeedTableCursor;

struct Database {
    char DatabasePath[1024];
    Manifest* manifest;
    SpeedMemSlot mems[SPEED_MAX_TABLES];
    int memN;
    SpeedWalSlot wals[SPEED_MAX_TABLES];
    int walN;
    SpeedSstCacheSlot sstCache[SPEED_MAX_SST_CACHE];
    int sstCacheN;
    int durabilityFull;
    int inTx;
    SpeedMemSlot txBuf[SPEED_MAX_TABLES];
    int txN;
    int groupCommitDepth;
    int batchWalSuppressDepth;
    int lockHeld;
    SpeedTableCursor cursors[SPEED_MAX_TABLES];
    struct {
        int tableId;
        SpeedExpressionEvaluator ev;
        void* user;
    } indexEvals[SPEED_MAX_TABLES];
    int indexEvalN;
};

static int db_dup_bytes(const uint8_t* src, int len, uint8_t** out, int* outLen)
{
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (len < 0) len = 0;
    if (len == 0) return 1;
    if (!src) return 0;
    *out = (uint8_t*)malloc((size_t)len);
    if (!*out) return 0;
    memcpy(*out, src, (size_t)len);
    if (outLen) *outLen = len;
    return 1;
}

static void db_cursor_clear_pos(SpeedTableCursor* c)
{
    if (!c) return;
    free(c->indexKey);
    free(c->pk);
    c->indexKey = NULL;
    c->pk = NULL;
    c->indexKeyLen = 0;
    c->pkLen = 0;
}

static SpeedTableCursor* db_cursor_find(const Database* self, int tableId)
{
    int i;
    if (!self) return NULL;
    for (i = 0; i < SPEED_MAX_TABLES; i++)
        if (self->cursors[i].used && self->cursors[i].tableId == tableId)
            return (SpeedTableCursor*)&self->cursors[i];
    return NULL;
}

static SpeedTableCursor* db_get_cursor(Database* self, int tableId)
{
    SpeedTableCursor* c;
    char name[128];
    int i;
    if (!self || tableId <= 0) return NULL;
    c = db_cursor_find(self, tableId);
    if (c) return c;
    if (!Manifest_TryGetTable(self->manifest, tableId, name, (int)sizeof(name))) return NULL;
    for (i = 0; i < SPEED_MAX_TABLES; i++) {
        if (self->cursors[i].used) continue;
        memset(&self->cursors[i], 0, sizeof(self->cursors[i]));
        self->cursors[i].used = 1;
        self->cursors[i].tableId = tableId;
        snprintf(self->cursors[i].activeIndex, sizeof(self->cursors[i].activeIndex), "Id");
        return &self->cursors[i];
    }
    return NULL;
}

static int db_move_empty(SpeedTableCursor* c)
{
    db_cursor_clear_pos(c);
    c->bof = 1;
    c->eof = 1;
    return 0;
}

static int db_move_boundary(SpeedTableCursor* c, int direction)
{
    db_cursor_clear_pos(c);
    if (direction > 0) { c->eof = 1; c->bof = 0; }
    else { c->bof = 1; c->eof = 0; }
    return 0;
}

static int db_cursor_set(SpeedTableCursor* c, const uint8_t* ik, int ikl, const uint8_t* pk, int pkl)
{
    uint8_t *nik = NULL, *npk = NULL;
    int nikl = 0, npkl = 0;
    if (!db_dup_bytes(ik, ikl, &nik, &nikl) || !db_dup_bytes(pk, pkl, &npk, &npkl)) {
        free(nik);
        free(npk);
        return 0;
    }
    db_cursor_clear_pos(c);
    c->indexKey = nik;
    c->indexKeyLen = nikl;
    c->pk = npk;
    c->pkLen = npkl;
    c->bof = 0;
    c->eof = 0;
    return 1;
}

static int db_get_scan_source(Database* self, int mainTableId, const char* indexName,
                              int* outId, int* isIndex)
{
    char mainName[128], idxTable[256];
    int idxId = 0;
    if (outId) *outId = mainTableId;
    if (isIndex) *isIndex = 0;
    if (!self || !indexName || !indexName[0] || gmb_strcasecmp(indexName, "Id") == 0)
        return 1;
    if (!Manifest_TryGetTable(self->manifest, mainTableId, mainName, (int)sizeof(mainName)))
        return 0;
    Manifest_IndexTableName(mainName, indexName, idxTable, (int)sizeof(idxTable));
    if (Manifest_TryGetTableByName(self->manifest, idxTable, &idxId) && idxId > 0) {
        if (outId) *outId = idxId;
        if (isIndex) *isIndex = 1;
        return 1;
    }
    return 0;
}

static Memtable* db_get_or_create_memtable(Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->memN; i++)
        if (self->mems[i].tableId == tableId) return self->mems[i].mem;
    if (self->memN >= SPEED_MAX_TABLES) return NULL;
    self->mems[self->memN].tableId = tableId;
    self->mems[self->memN].mem = Memtable_New();
    return self->mems[self->memN++].mem;
}

static SpeedWriteAheadLog* db_get_or_create_wal(Database* self, int tableId)
{
    int i;
    uint64_t n;
    for (i = 0; i < self->walN; i++)
        if (self->wals[i].tableId == tableId) return self->wals[i].wal;
    if (self->walN >= SPEED_MAX_TABLES) return NULL;
    n = Manifest_GetNextFileNumber(self->manifest);
    self->wals[self->walN].tableId = tableId;
    self->wals[self->walN].wal = SpeedWriteAheadLog_New(self->DatabasePath, n);
    self->walN++;
    Manifest_Save(self->manifest);
    return self->wals[self->walN - 1].wal;
}

static SpeedWriteAheadLog* db_take_wal(Database* self, int tableId)
{
    int i;
    SpeedWriteAheadLog* wal;
    for (i = 0; i < self->walN; i++) {
        if (self->wals[i].tableId != tableId) continue;
        wal = self->wals[i].wal;
        memmove(&self->wals[i], &self->wals[i + 1], (size_t)(self->walN - i - 1) * sizeof(self->wals[0]));
        self->walN--;
        return wal;
    }
    return NULL;
}

/* ponytail: Recover replays *.wal into memtables but does not close those files (C# same).
 * First SST flush must consume them or reopen resurrects flushed keys. Ceiling: all tables share the wal dir. */
static void db_unlink_all_wals(const char* databasePath)
{
    char walDirectory[1100];
    DIR* dir;
    struct dirent* ent;
    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    dir = opendir(walDirectory);
    if (!dir) return;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".wal") == 0) {
            char path[1100];
            snprintf(path, sizeof(path), "%s/%s", walDirectory, ent->d_name);
            unlink(path);
        }
    }
    closedir(dir);
}

static SstReader* db_sst_get_or_add(Database* self, const char* path)
{
    int i;
    for (i = 0; i < self->sstCacheN; i++)
        if (strcmp(self->sstCache[i].path, path) == 0) return self->sstCache[i].reader;
    if (self->sstCacheN >= SPEED_MAX_SST_CACHE) return NULL;
    self->sstCache[self->sstCacheN].reader = SstReader_Open(path);
    if (!self->sstCache[self->sstCacheN].reader) return NULL;
    snprintf(self->sstCache[self->sstCacheN].path, sizeof(self->sstCache[self->sstCacheN].path), "%s", path);
    return self->sstCache[self->sstCacheN++].reader;
}

static int db_should_fsync(const Database* self)
{
    return self->durabilityFull && self->groupCommitDepth == 0 && self->batchWalSuppressDepth == 0;
}

static void db_maybe_flush_wal(Database* self, SpeedWriteAheadLog* wal)
{
    if (wal && db_should_fsync(self)) SpeedWriteAheadLog_Flush(wal);
}

static void db_maybe_flush_and_compact(Database* self, int tableId);

static void db_flush_all_wals(Database* self)
{
    int i;
    for (i = 0; i < self->walN; i++)
        SpeedWriteAheadLog_Flush(self->wals[i].wal);
}

const char* Database_GetPath(const Database* self)
{
    return self ? self->DatabasePath : "";
}

Manifest* Database_GetManifest(Database* self)
{
    return self ? self->manifest : NULL;
}

SstReader* Database_SstGetOrAdd(Database* self, const char* path)
{
    return self ? db_sst_get_or_add(self, path) : NULL;
}

void Database_SstCacheRemove(Database* self, const char* path)
{
    int i;
    if (!self || !path) return;
    for (i = 0; i < self->sstCacheN; i++) {
        if (strcmp(self->sstCache[i].path, path) != 0) continue;
        SstReader_Dispose(self->sstCache[i].reader);
        memmove(&self->sstCache[i], &self->sstCache[i + 1],
                (size_t)(self->sstCacheN - i - 1) * sizeof(self->sstCache[0]));
        self->sstCacheN--;
        return;
    }
}

static Memtable* db_tx_buffer(Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->txN; i++)
        if (self->txBuf[i].tableId == tableId) return self->txBuf[i].mem;
    if (self->txN >= SPEED_MAX_TABLES) return NULL;
    self->txBuf[self->txN].tableId = tableId;
    self->txBuf[self->txN].mem = Memtable_New();
    return self->txBuf[self->txN++].mem;
}

static Memtable* db_tx_buffer_get(const Database* self, int tableId)
{
    int i;
    for (i = 0; i < self->txN; i++)
        if (self->txBuf[i].tableId == tableId) return self->txBuf[i].mem;
    return NULL;
}

static void db_tx_clear(Database* self)
{
    int i;
    for (i = 0; i < self->txN; i++)
        Memtable_Dispose(self->txBuf[i].mem);
    self->txN = 0;
    self->inTx = 0;
}

static int db_copy_from_memtable(Memtable* mem, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen)
{
    uint8_t* mv = NULL;
    int mvlen = 0, tomb = 0;
    if (!Memtable_Get(mem, key, keyLen, &mv, &mvlen, &tomb)) return -1;
    if (tomb || !mv) return 0;
    if (value) {
        *value = (uint8_t*)malloc((size_t)mvlen);
        if (mvlen > 0) memcpy(*value, mv, (size_t)mvlen);
    }
    if (valueLen) *valueLen = mvlen;
    return 1;
}

Database* Database_Open(const char* path)
{
    Database* self;
    Memtable** recovered = NULL;
    int* ids = NULL;
    int n = 0, i;
    if (!path) return NULL;
    self = (Database*)calloc(1, sizeof(*self));
    snprintf(self->DatabasePath, sizeof(self->DatabasePath), "%s", path);
    self->durabilityFull = 1;
    mkdir(path, 0777);
    {
        char sub[1100];
        snprintf(sub, sizeof(sub), "%s/%s", path, Config_WalDirectoryName);
        mkdir(sub, 0777);
        snprintf(sub, sizeof(sub), "%s/%s", path, Config_SstDirectoryName);
        mkdir(sub, 0777);
    }
    if (SpeedDatabaseLock_Acquire(path) != GMB_DB_OK) {
        free(self);
        return NULL;
    }
    self->lockHeld = 1;
    self->manifest = Manifest_Load(path);
    if (!self->manifest) {
        SpeedDatabaseLock_Release(path);
        free(self);
        return NULL;
    }
    SpeedWriteAheadLog_Recover(path, &recovered, &ids, &n);
    for (i = 0; i < n && self->memN < SPEED_MAX_TABLES; i++) {
        self->mems[self->memN].tableId = ids[i];
        self->mems[self->memN].mem = recovered[i];
        self->memN++;
    }
    free(recovered);
    free(ids);
    Database_MaybeRebuildUntrustedRowCounts(self);
    return self;
}

void Database_Dispose(Database* self)
{
    int i;
    if (!self) return;
    Database_CollectGarbage(self);
    for (i = 0; i < self->walN; i++) {
        SpeedWriteAheadLog_Flush(self->wals[i].wal);
        SpeedWriteAheadLog_Dispose(self->wals[i].wal);
    }
    for (i = 0; i < self->sstCacheN; i++)
        SstReader_Dispose(self->sstCache[i].reader);
    for (i = 0; i < self->memN; i++)
        Memtable_Dispose(self->mems[i].mem);
    for (i = 0; i < SPEED_MAX_TABLES; i++) {
        if (!self->cursors[i].used) continue;
        db_cursor_clear_pos(&self->cursors[i]);
        self->cursors[i].used = 0;
    }
    db_tx_clear(self);
    Manifest_Dispose(self->manifest);
    if (self->lockHeld)
        SpeedDatabaseLock_Release(self->DatabasePath);
    free(self);
}

int Database_CollectGarbage(Database* self)
{
    char names[64][256];
    int n, i, deleted = 0, changed = 0;
    if (!self || !self->manifest) return 0;
    n = Manifest_GetFilesForGarbageCollection(self->manifest, names, 64);
    for (i = 0; i < n; i++) {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        if (unlink(path) == 0 || access(path, F_OK) != 0) {
            Manifest_RemoveFileFromGarbageCollection(self->manifest, names[i]);
            deleted++;
            changed = 1;
        }
    }
    if (changed)
        Manifest_Save(self->manifest);
    return deleted;
}

int Database_CreateTable(Database* self, const char* typeName)
{
    if (!self) return -1;
    return Manifest_CreateTable(self->manifest, typeName);
}

int Database_CreateTableWithFields(Database* self, const char* typeName,
                                   const char* const* names, const char* const* types,
                                   const int* lengths, int fieldCount)
{
    int id, i;
    if (!self || !typeName || fieldCount < 0) return -1;
    id = Database_CreateTable(self, typeName);
    if (id <= 0) return -1;
    for (i = 0; i < fieldCount; i++) {
        const char* n = names && names[i] ? names[i] : "";
        const char* t = types && types[i] ? types[i] : "String";
        int len = lengths ? lengths[i] : 0;
        if (Manifest_AddField(self->manifest, id, n, t, len) != GMB_DB_OK) return -1;
    }
    return id;
}

static void db_evict_table(Database* self, int tableId)
{
    int i;
    SpeedWriteAheadLog* wal;
    wal = db_take_wal(self, tableId);
    if (wal) {
        SpeedWriteAheadLog_Flush(wal);
        SpeedWriteAheadLog_Dispose(wal);
    }
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        Memtable_Dispose(self->mems[i].mem);
        memmove(&self->mems[i], &self->mems[i + 1],
                (size_t)(self->memN - i - 1) * sizeof(self->mems[0]));
        self->memN--;
        break;
    }
}

int Database_DropTableInternal(Database* self, const char* tableName)
{
    char files[64][256];
    const char* ptrs[64];
    int n, i, tableId = -1;
    if (!self || !tableName) return GMB_DB_ARG;
    n = Manifest_PrepareTableForDeletion(self->manifest, tableName, &tableId, files, 64);
    if (tableId < 0) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        char path[1100];
        ptrs[i] = files[i];
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, files[i]);
        Database_SstCacheRemove(self, path);
    }
    if (n > 0)
        Manifest_AddFilesToGarbageCollection(self->manifest, ptrs, n);
    Manifest_Save(self->manifest);
    db_evict_table(self, tableId);
    {
        SpeedTableCursor* c = db_cursor_find(self, tableId);
        if (c) {
            db_cursor_clear_pos(c);
            c->used = 0;
        }
    }
    return GMB_DB_OK;
}

int Database_Set(Database* self, int tableId, const uint8_t* key, int keyLen, const uint8_t* value, int valueLen)
{
    Memtable* mem;
    SpeedWriteAheadLog* wal;
    uint8_t* old = NULL;
    int oldLen = 0;
    int maintain = 0;
    if (!self || !key || !value) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        return mem ? Memtable_Set(mem, key, keyLen, value, valueLen) : GMB_DB_ERR;
    }
    Database_AdjustRowCountOnSet(self, tableId, key, keyLen);
    if (Database_IsMainDataTable(self, tableId)
        && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0) {
        Database_Get(self, tableId, key, keyLen, &old, &oldLen);
        maintain = 1;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) { free(old); return GMB_DB_ERR; }
    SpeedWriteAheadLog_LogSet(wal, tableId, key, keyLen, value, valueLen);
    if (Memtable_Set(mem, key, keyLen, value, valueLen) != GMB_DB_OK) { free(old); return GMB_DB_ERR; }
    db_maybe_flush_wal(self, wal);
    if (maintain)
        Database_MaintainSecondaryIndexesOnPut(self, tableId, key, keyLen, old, oldLen, value, valueLen);
    free(old);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_Delete(Database* self, int tableId, const uint8_t* key, int keyLen)
{
    Memtable* mem;
    SpeedWriteAheadLog* wal;
    uint8_t* old = NULL;
    int oldLen = 0;
    int maintain = 0;
    if (!self || !key) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        return mem ? Memtable_Delete(mem, key, keyLen) : GMB_DB_ERR;
    }
    Database_AdjustRowCountOnDelete(self, tableId, key, keyLen);
    if (Database_IsMainDataTable(self, tableId)
        && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0) {
        Database_Get(self, tableId, key, keyLen, &old, &oldLen);
        maintain = 1;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) { free(old); return GMB_DB_ERR; }
    SpeedWriteAheadLog_LogDelete(wal, tableId, key, keyLen);
    if (Memtable_Delete(mem, key, keyLen) != GMB_DB_OK) { free(old); return GMB_DB_ERR; }
    db_maybe_flush_wal(self, wal);
    if (maintain)
        Database_MaintainSecondaryIndexesOnDelete(self, tableId, key, keyLen, old, oldLen);
    free(old);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_Get(Database* self, int tableId, const uint8_t* key, int keyLen, uint8_t** value, int* valueLen)
{
    Memtable* mem;
    char names[SPEED_SST_SEARCH_MAX][256];
    int n, i;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!self || !key) return 0;
    if (self->inTx) {
        Memtable* buf = db_tx_buffer_get(self, tableId);
        if (buf) {
            int rc = db_copy_from_memtable(buf, key, keyLen, value, valueLen);
            if (rc >= 0) return rc;
        }
    }
    mem = db_get_or_create_memtable(self, tableId);
    {
        int rc = mem ? db_copy_from_memtable(mem, key, keyLen, value, valueLen) : -1;
        if (rc >= 0) return rc;
    }
    n = Manifest_CollectSstNewestFirst(self->manifest, tableId, names, SPEED_SST_SEARCH_MAX);
    for (i = 0; i < n; i++) {
        char path[1100];
        SstReader* r;
        uint8_t* sv = NULL;
        int slen = 0;
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        r = db_sst_get_or_add(self, path);
        if (!r) continue;
        if (SstReader_TryFind(r, key, keyLen, &sv, &slen)) {
            if (!sv) return 0;
            if (value) *value = sv;
            else free(sv);
            if (valueLen) *valueLen = slen;
            return 1;
        }
    }
    return 0;
}

void Database_ForceFlushTableSync(Database* self, int tableId)
{
    Memtable* toFlush = NULL;
    SpeedWriteAheadLog* oldWal;
    uint64_t fileNumber;
    char sstPath[1100];
    int i;
    if (!self) return;
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        if (Memtable_get_KeyCount(self->mems[i].mem) == 0) return;
        toFlush = self->mems[i].mem;
        self->mems[i].mem = Memtable_New();
        break;
    }
    if (!toFlush) return;
    oldWal = db_take_wal(self, tableId);
    fileNumber = Manifest_GetNextFileNumber(self->manifest);
    snprintf(sstPath, sizeof(sstPath), "%s/%s/%06llu_%d.sst",
             self->DatabasePath, Config_SstDirectoryName,
             (unsigned long long)fileNumber, tableId);
    SstWriter_WriteFromMemtable(sstPath, toFlush);
    Manifest_AddSstFile(self->manifest, tableId, sstPath);
    Manifest_Save(self->manifest);
    if (oldWal) {
        char walCopy[1024];
        const char* walPath = SpeedWriteAheadLog_GetWalPath(oldWal);
        snprintf(walCopy, sizeof(walCopy), "%s", walPath ? walPath : "");
        SpeedWriteAheadLog_Dispose(oldWal);
        if (walCopy[0]) unlink(walCopy);
    }
    db_unlink_all_wals(self->DatabasePath);
    Memtable_Dispose(toFlush);
}

void Database_TriggerCompactionChain(Database* self, int tableId)
{
    int level;
    if (!self) return;
    Compactor_CompactLevel0(self, tableId);
    for (level = 1; level < 8; level++) {
        char names[64][256];
        int n = Manifest_GetFilesForLevel(self->manifest, tableId, level, names, 64);
        if (n >= Compactor_CompactLevelThreshold)
            Compactor_CompactLevel(self, tableId, level);
    }
}

/* ponytail: C# fire-and-forget Task.Run; C runs inline so Dispose cannot race the SST rewrite. */
static void db_maybe_flush_and_compact(Database* self, int tableId)
{
    int i;
    if (!self || self->inTx) return;
    for (i = 0; i < self->memN; i++) {
        if (self->mems[i].tableId != tableId) continue;
        if (Memtable_get_EstimatedSize(self->mems[i].mem) > Database_MemtableThreshold) {
            Database_ForceFlushTableSync(self, tableId);
            Database_TriggerCompactionChain(self, tableId);
        }
        return;
    }
}

void Database_BeginTransaction(Database* self)
{
    if (!self || self->inTx) return;
    self->inTx = 1;
    self->txN = 0;
}

void Database_Rollback(Database* self)
{
    if (self) db_tx_clear(self);
}

int Database_Commit(Database* self)
{
    int t, i;
    if (!self || !self->inTx) return GMB_DB_OK;
    if (self->txN == 0) { self->inTx = 0; return GMB_DB_OK; }
    for (t = 0; t < self->txN; t++) {
        Memtable* buf = self->txBuf[t].mem;
        int tableId = self->txBuf[t].tableId;
        int n = Memtable_get_KeyCount(buf);
        SpeedWriteAheadLog* wal;
        Memtable* mem;
        if (n == 0) continue;
        wal = db_get_or_create_wal(self, tableId);
        mem = db_get_or_create_memtable(self, tableId);
        if (!wal || !mem) { db_tx_clear(self); return GMB_DB_ERR; }
        for (i = 0; i < n; i++) {
            uint8_t *key, *val, *old = NULL;
            int keyLen, valLen, tomb, oldLen = 0;
            Memtable_Nth(buf, i, &key, &keyLen, &val, &valLen, &tomb);
            if (Database_IsMainDataTable(self, tableId)
                && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0)
                Database_Get(self, tableId, key, keyLen, &old, &oldLen);
            if (tomb) {
                SpeedWriteAheadLog_LogDelete(wal, tableId, key, keyLen);
                if (Memtable_Delete(mem, key, keyLen) != GMB_DB_OK) { free(old); db_tx_clear(self); return GMB_DB_ERR; }
                Database_MaintainSecondaryIndexesOnDelete(self, tableId, key, keyLen, old, oldLen);
            } else {
                SpeedWriteAheadLog_LogSet(wal, tableId, key, keyLen, val, valLen);
                if (Memtable_Set(mem, key, keyLen, val, valLen) != GMB_DB_OK) { free(old); db_tx_clear(self); return GMB_DB_ERR; }
                Database_MaintainSecondaryIndexesOnPut(self, tableId, key, keyLen, old, oldLen, val, valLen);
            }
            free(old);
            old = NULL;
            oldLen = 0;
        }
        db_maybe_flush_wal(self, wal);
    }
    db_tx_clear(self);
    return GMB_DB_OK;
}

typedef struct {
    uint8_t* key;
    int keyLen;
    uint8_t* val;
    int valLen;
    int tomb;
    int source;
} ScanEnt;

struct DatabaseScan {
    ScanEnt* live;
    int n;
    int i;
};

static int db_scan_cmp(const void* a, const void* b)
{
    const ScanEnt* x = (const ScanEnt*)a;
    const ScanEnt* y = (const ScanEnt*)b;
    int c = ByteArrayComparer_Compare(x->key, x->keyLen, y->key, y->keyLen);
    if (c != 0) return c;
    return x->source - y->source;
}

static int db_scan_push(ScanEnt** ents, int* n, int* cap, const uint8_t* key, int keyLen,
                        const uint8_t* val, int valLen, int tomb, int source)
{
    ScanEnt* e;
    if (*n >= *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *ents = (ScanEnt*)realloc(*ents, (size_t)(*cap) * sizeof(ScanEnt));
    }
    e = &(*ents)[(*n)++];
    e->keyLen = keyLen;
    e->valLen = valLen;
    e->tomb = tomb;
    e->source = source;
    e->key = (uint8_t*)malloc((size_t)keyLen);
    if (keyLen > 0) memcpy(e->key, key, (size_t)keyLen);
    e->val = NULL;
    if (!tomb && valLen > 0) {
        e->val = (uint8_t*)malloc((size_t)valLen);
        memcpy(e->val, val, (size_t)valLen);
    }
    return 0;
}

DatabaseScan* Database_ScanRange(Database* self, int tableId,
                                 const uint8_t* startKey, int startKeyLen,
                                 const uint8_t* endKey, int endKeyLen)
{
    DatabaseScan* scan;
    ScanEnt* ents = NULL;
    int n = 0, cap = 0, i, src = 0;
    Memtable* mem;
    char names[SPEED_SST_SEARCH_MAX][256];
    int nfiles;
    if (!self) return NULL;
    mem = db_get_or_create_memtable(self, tableId);
    if (mem) {
        int kc = Memtable_get_KeyCount(mem);
        for (i = 0; i < kc; i++) {
            uint8_t *k, *v;
            int kl, vl, tomb;
            Memtable_Nth(mem, i, &k, &kl, &v, &vl, &tomb);
            db_scan_push(&ents, &n, &cap, k, kl, v, vl, tomb, src);
        }
    }
    src = 1;
    nfiles = Manifest_CollectSstOldestFirst(self->manifest, tableId, names, SPEED_SST_SEARCH_MAX);
    for (i = 0; i < nfiles; i++, src++) {
        char path[1100];
        SstReader* r;
        snprintf(path, sizeof(path), "%s/%s/%s", self->DatabasePath, Config_SstDirectoryName, names[i]);
        r = db_sst_get_or_add(self, path);
        if (!r) continue;
        SstReader_Rewind(r);
        for (;;) {
            uint8_t *k = NULL, *v = NULL;
            int kl = 0, vl = 0;
            if (!SstReader_Next(r, &k, &kl, &v, &vl)) break;
            db_scan_push(&ents, &n, &cap, k, kl, v, vl, v == NULL, src);
            free(k);
            free(v);
        }
    }
    if (n > 1) qsort(ents, (size_t)n, sizeof(ScanEnt), db_scan_cmp);
    scan = (DatabaseScan*)calloc(1, sizeof(*scan));
    for (i = 0; i < n; ) {
        int j = i + 1;
        int inRange = 1;
        while (j < n && ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, ents[j].key, ents[j].keyLen) == 0) {
            free(ents[j].key);
            free(ents[j].val);
            j++;
        }
        if (startKey && startKeyLen > 0 &&
            ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, startKey, startKeyLen) < 0)
            inRange = 0;
        if (endKey && endKeyLen > 0 &&
            ByteArrayComparer_Compare(ents[i].key, ents[i].keyLen, endKey, endKeyLen) >= 0)
            inRange = 0;
        if (!ents[i].tomb && inRange) {
            scan->live = (ScanEnt*)realloc(scan->live, (size_t)(scan->n + 1) * sizeof(ScanEnt));
            scan->live[scan->n++] = ents[i];
        } else {
            free(ents[i].key);
            free(ents[i].val);
        }
        i = j;
    }
    free(ents);
    return scan;
}

DatabaseScan* Database_Scan(Database* self, int tableId)
{
    return Database_ScanRange(self, tableId, NULL, 0, NULL, 0);
}

int Database_ScanNext(DatabaseScan* scan, uint8_t** key, int* keyLen, uint8_t** value, int* valueLen)
{
    ScanEnt* e;
    if (key) *key = NULL;
    if (keyLen) *keyLen = 0;
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!scan || scan->i >= scan->n) return 0;
    e = &scan->live[scan->i++];
    if (key) { *key = e->key; e->key = NULL; }
    else free(e->key);
    if (keyLen) *keyLen = e->keyLen;
    if (value) { *value = e->val; e->val = NULL; }
    else free(e->val);
    if (valueLen) *valueLen = e->valLen;
    return 1;
}

void Database_ScanDispose(DatabaseScan* scan)
{
    int i;
    if (!scan) return;
    for (i = 0; i < scan->n; i++) {
        free(scan->live[i].key);
        free(scan->live[i].val);
    }
    free(scan->live);
    free(scan);
}

int Database_SetOrder(Database* self, int tableId, const char* indexName)
{
    SpeedTableCursor* c;
    char mainName[128];
    int i, n;
    if (!self || !Manifest_TryGetTable(self->manifest, tableId, mainName, (int)sizeof(mainName)))
        return 0;
    c = db_get_cursor(self, tableId);
    if (!c) return 0;
    if (!indexName || !indexName[0] || gmb_strcasecmp(indexName, "Id") == 0) {
        snprintf(c->activeIndex, sizeof(c->activeIndex), "Id");
        return 1;
    }
    n = Manifest_SecondaryKeyCount(self->manifest, tableId);
    for (i = 0; i < n; i++) {
        char iname[64];
        if (!Manifest_GetSecondaryKey(self->manifest, tableId, i, iname, (int)sizeof(iname), NULL, 0, NULL))
            continue;
        if (gmb_strcasecmp(iname, indexName) == 0) {
            snprintf(c->activeIndex, sizeof(c->activeIndex), "%s", iname);
            return 1;
        }
    }
    return 0;
}

int Database_GoTop(Database* self, int tableId)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    DatabaseScan* scan;
    uint8_t *k = NULL, *v = NULL;
    int kl = 0, vl = 0, ok;
    if (!c) return 0;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_empty(c);
    scan = Database_Scan(self, srcId);
    if (!scan || !Database_ScanNext(scan, &k, &kl, &v, &vl) || !k) {
        free(k); free(v);
        Database_ScanDispose(scan);
        return db_move_empty(c);
    }
    ok = db_cursor_set(c, k, kl, isIndex ? v : k, isIndex ? vl : kl);
    free(k); free(v);
    Database_ScanDispose(scan);
    return ok ? 1 : db_move_empty(c);
}

int Database_GoBottom(Database* self, int tableId)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    DatabaseScan* scan;
    uint8_t *lastK = NULL, *lastV = NULL;
    int lastKl = 0, lastVl = 0, ok;
    if (!c) return 0;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_empty(c);
    scan = Database_Scan(self, srcId);
    if (!scan) return db_move_empty(c);
    {
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            free(lastK); free(lastV);
            lastK = k; lastKl = kl;
            lastV = v; lastVl = vl;
            k = NULL; v = NULL;
        }
    }
    Database_ScanDispose(scan);
    if (!lastK) return db_move_empty(c);
    ok = db_cursor_set(c, lastK, lastKl, isIndex ? lastV : lastK, isIndex ? lastVl : lastKl);
    free(lastK); free(lastV);
    return ok ? 1 : db_move_empty(c);
}

int Database_Skip(Database* self, int tableId, int recordCount)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0, direction, toSkip, ok = 0;
    DatabaseScan* scan;
    uint8_t *destK = NULL, *destV = NULL;
    int destKl = 0, destVl = 0;
    if (!c || !c->indexKey || recordCount == 0) return 0;
    direction = recordCount > 0 ? 1 : -1;
    toSkip = recordCount > 0 ? recordCount : -recordCount;
    if (!db_get_scan_source(self, tableId, c->activeIndex, &srcId, &isIndex))
        return db_move_boundary(c, direction);
    scan = Database_Scan(self, srcId);
    if (!scan) return db_move_boundary(c, direction);
    if (direction > 0) {
        int seen = 0, remain = toSkip;
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            if (!seen) {
                if (ByteArrayComparer_Compare(k, kl, c->indexKey, c->indexKeyLen) == 0)
                    seen = 1;
                free(k); free(v);
                continue;
            }
            remain--;
            free(destK); free(destV);
            destK = k; destKl = kl;
            destV = v; destVl = vl;
            k = NULL; v = NULL;
            if (remain == 0) break;
        }
        free(k); free(v);
        if (remain > 0) {
            free(destK); free(destV);
            destK = NULL;
        }
    } else {
        typedef struct { uint8_t *k, *v; int kl, vl; } SkipBuf;
        SkipBuf* buf = (SkipBuf*)calloc((size_t)toSkip, sizeof(SkipBuf));
        int n = 0, i;
        uint8_t *k = NULL, *v = NULL;
        int kl = 0, vl = 0;
        if (!buf) { Database_ScanDispose(scan); return 0; }
        while (Database_ScanNext(scan, &k, &kl, &v, &vl)) {
            if (ByteArrayComparer_Compare(k, kl, c->indexKey, c->indexKeyLen) == 0) {
                free(k); free(v);
                break;
            }
            if (n == toSkip) {
                free(buf[0].k); free(buf[0].v);
                memmove(&buf[0], &buf[1], (size_t)(toSkip - 1) * sizeof(buf[0]));
                n--;
            }
            buf[n].k = k; buf[n].kl = kl; buf[n].v = v; buf[n].vl = vl;
            n++;
            k = NULL; v = NULL;
        }
        if (n == toSkip) {
            destK = buf[0].k; destKl = buf[0].kl;
            destV = buf[0].v; destVl = buf[0].vl;
            buf[0].k = NULL; buf[0].v = NULL;
        }
        for (i = 0; i < n; i++) { free(buf[i].k); free(buf[i].v); }
        free(buf);
    }
    Database_ScanDispose(scan);
    if (!destK) return db_move_boundary(c, direction);
    ok = db_cursor_set(c, destK, destKl, isIndex ? destV : destK, isIndex ? destVl : destKl);
    free(destK); free(destV);
    return ok ? 1 : db_move_boundary(c, direction);
}

int Database_Seek(Database* self, int tableId, const uint8_t* key, int keyLen, const char* indexName)
{
    SpeedTableCursor* c = db_get_cursor(self, tableId);
    int srcId = tableId, isIndex = 0;
    uint8_t* found = NULL;
    int foundLen = 0;
    const char* order;
    if (!c || !key) return 0;
    order = (indexName && indexName[0]) ? indexName : c->activeIndex;
    if (!db_get_scan_source(self, tableId, order, &srcId, &isIndex)) {
        db_cursor_clear_pos(c);
        return 0;
    }
    if (!Database_Get(self, srcId, key, keyLen, &found, &foundLen) || !found) {
        db_cursor_clear_pos(c);
        return 0;
    }
    if (isIndex) {
        uint8_t* main = NULL;
        int mainLen = 0;
        if (!Database_Get(self, tableId, found, foundLen, &main, &mainLen)) {
            free(found);
            db_cursor_clear_pos(c);
            return 0;
        }
        free(main);
        if (!db_cursor_set(c, key, keyLen, found, foundLen)) {
            free(found);
            db_cursor_clear_pos(c);
            return 0;
        }
        free(found);
        return 1;
    }
    if (!db_cursor_set(c, key, keyLen, key, keyLen)) {
        free(found);
        db_cursor_clear_pos(c);
        return 0;
    }
    free(found);
    return 1;
}

int Database_GetCurrent(Database* self, int tableId, uint8_t** value, int* valueLen)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    if (value) *value = NULL;
    if (valueLen) *valueLen = 0;
    if (!c || !c->pk) return 0;
    return Database_Get(self, tableId, c->pk, c->pkLen, value, valueLen);
}

int Database_GetCurrentKey(Database* self, int tableId, const uint8_t** key, int* keyLen)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    if (key) *key = NULL;
    if (keyLen) *keyLen = 0;
    if (!c || !c->pk) return 0;
    if (key) *key = c->pk;
    if (keyLen) *keyLen = c->pkLen;
    return 1;
}

int Database_Eof(const Database* self, int tableId)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    return c && c->eof;
}

int Database_Bof(const Database* self, int tableId)
{
    SpeedTableCursor* c = db_cursor_find(self, tableId);
    return c && c->bof;
}

int Database_DropView(Database* self, const char* viewName)
{
    return Database_DropTableInternal(self, viewName);
}

int Database_DropIndex(Database* self, int mainTableId, const char* indexName)
{
    char mainName[128], idxName[256];
    int idxId = 0;
    if (!self || !indexName || !indexName[0]) return GMB_DB_ARG;
    if (!Manifest_TryGetTable(self->manifest, mainTableId, mainName, (int)sizeof(mainName)))
        return GMB_DB_ERR;
    Manifest_IndexTableName(mainName, indexName, idxName, (int)sizeof(idxName));
    if (!Manifest_TryGetTableByName(self->manifest, idxName, &idxId) || idxId <= 0)
        return GMB_DB_ERR;
    if (Database_DropTableInternal(self, idxName) != GMB_DB_OK) return GMB_DB_ERR;
    Manifest_RemoveSecondaryIndex(self->manifest, mainName, indexName);
    Manifest_Save(self->manifest);
    return GMB_DB_OK;
}

void Database_SetIndexKeyEvaluator(Database* self, int mainTableId,
                                   SpeedExpressionEvaluator evaluator, void* evalUser)
{
    int i;
    if (!self || !evaluator) return;
    for (i = 0; i < self->indexEvalN; i++) {
        if (self->indexEvals[i].tableId != mainTableId) continue;
        self->indexEvals[i].ev = evaluator;
        self->indexEvals[i].user = evalUser;
        return;
    }
    if (self->indexEvalN >= SPEED_MAX_TABLES) return;
    self->indexEvals[self->indexEvalN].tableId = mainTableId;
    self->indexEvals[self->indexEvalN].ev = evaluator;
    self->indexEvals[self->indexEvalN].user = evalUser;
    self->indexEvalN++;
}

int Database_EvalIndexKey(Database* self, int tableId, const char* expression,
                          const uint8_t* udt, int udtLen, uint8_t** out, int* outLen)
{
    int i, n, c, slot = -1;
    QueryResultRow row;
    SpeedEvalValue val;
    if (out) *out = NULL;
    if (outLen) *outLen = 0;
    if (!self || !expression) return 0;
    for (i = 0; i < self->indexEvalN; i++) {
        if (self->indexEvals[i].tableId == tableId) { slot = i; break; }
    }
    if (slot < 0 || !self->indexEvals[slot].ev) return 0;
    memset(&row, 0, sizeof(row));
    memset(&val, 0, sizeof(val));
    n = Manifest_FieldCount(self->manifest, tableId);
    for (i = 0; i < n && row.CellN < QUERYPLAN_MAX_PROJ; i++) {
        char fname[64], tname[32];
        uint8_t* extracted = NULL;
        int elen = 0, flen = 0;
        if (!Manifest_GetField(self->manifest, tableId, i, fname, (int)sizeof(fname),
                               tname, (int)sizeof(tname), &flen))
            continue;
        (void)tname;
        if (udt && udtLen > 0)
            SpeedMarshaller_ExtractField(self->manifest, tableId, udt, udtLen, fname, &extracted, &elen);
        snprintf(row.Cells[row.CellN].Alias, sizeof(row.Cells[0].Alias), "%s", fname);
        row.Cells[row.CellN].Bytes = extracted;
        row.Cells[row.CellN].Len = elen;
        row.CellN++;
    }
    self->indexEvals[slot].ev(expression, &row, self->indexEvals[slot].user, &val);
    for (c = 0; c < row.CellN; c++) free(row.Cells[c].Bytes);
    if (val.isNull || !val.bytes || val.len <= 0) {
        SpeedEvalValue_Clear(&val);
        return 0;
    }
    if (!db_dup_bytes(val.bytes, val.len, out, outLen)) {
        SpeedEvalValue_Clear(&val);
        return 0;
    }
    SpeedEvalValue_Clear(&val);
    return 1;
}

void Database_MaybeRebuildUntrustedRowCounts(Database* self)
{
    int i, id = 0, n;
    if (!self || !self->manifest) return;
    n = Manifest_TableCount(self->manifest);
    for (i = 0; i < n; i++) {
        if (!Manifest_GetTableIdAt(self->manifest, i, &id)) continue;
        if (!Manifest_IsRowCountTrusted(self->manifest, id)) {
            Database_RebuildManifestRowCounts(self);
            return;
        }
    }
}

int Database_ForceCompactTable(Database* self, const char* tableName)
{
    int tableId = 0;
    if (!self || !tableName || !tableName[0]) return 0;
    if (!Manifest_TryGetTableByName(self->manifest, tableName, &tableId)) return -1;
    Database_ForceFlushTableSync(self, tableId);
    return Compactor_CompactLevels01Forced(self, tableId);
}

int Database_CompactAll(Database* self)
{
    int i, n, id, reduced = 0, t, seen;
    if (!self || !self->manifest) return 0;
    n = Manifest_TableCount(self->manifest);
    for (i = 0; i < n; i++) {
        if (!Manifest_GetTableIdAt(self->manifest, i, &id) || id <= 0) continue;
        Database_ForceFlushTableSync(self, id);
        reduced += Compactor_CompactLevels01Forced(self, id);
    }
    for (i = 0; i < self->memN; i++) {
        id = self->mems[i].tableId;
        seen = 0;
        for (t = 0; t < n; t++) {
            int mid = 0;
            if (Manifest_GetTableIdAt(self->manifest, t, &mid) && mid == id) {
                seen = 1;
                break;
            }
        }
        if (seen) continue;
        Database_ForceFlushTableSync(self, id);
        reduced += Compactor_CompactLevels01Forced(self, id);
    }
    return reduced;
}

void Database_FlushDurability(Database* self)
{
    if (self) db_flush_all_wals(self);
}

/* Imposta la durability del motore SPEED (parità C# DurabilityMode): full=1 → fsync per set,
 * full=0 → differito (group-commit/bulk). `durabilityFull` è la field LETTA dall'engine
 * (Database_FlushDurability / NeedsFsync): `gmb_db_speed_set_durability` scriveva un campo
 * MAI letto → durabilityFull restava 1 → fsync per ogni set (bug
 * SPEED-Memtable-On-e-Durability-Non-Cablata, causa #2). */
void Database_SetDurability(Database* self, int full)
{
    if (self) self->durabilityFull = full ? 1 : 0;
}

int Database_GetDurabilityFull(const Database* self) { return self ? self->durabilityFull : 0; }
int Database_GetGroupCommitDepth(const Database* self) { return self ? self->groupCommitDepth : 0; }

void Database_BeginGroupCommit(Database* self)
{
    if (self) self->groupCommitDepth++;
}

int Database_EndGroupCommit(Database* self)
{
    if (!self || self->groupCommitDepth == 0) return GMB_DB_ERR;
    self->groupCommitDepth--;
    if (self->groupCommitDepth == 0 && self->durabilityFull)
        db_flush_all_wals(self);
    return GMB_DB_OK;
}

int Database_SetMany(Database* self, int tableId, const DatabaseKv* entries, int count)
{
    SpeedWriteAheadLog* wal;
    Memtable* mem;
    int i;
    if (!self) return GMB_DB_ARG;
    if (count <= 0) return GMB_DB_OK;
    if (!entries) return GMB_DB_ARG;
    if (self->inTx) {
        mem = db_tx_buffer(self, tableId);
        if (!mem) return GMB_DB_ERR;
        for (i = 0; i < count; i++) {
            if (Memtable_Set(mem, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen) != GMB_DB_OK)
                return GMB_DB_ERR;
        }
        return GMB_DB_OK;
    }
    wal = db_get_or_create_wal(self, tableId);
    mem = db_get_or_create_memtable(self, tableId);
    if (!wal || !mem) return GMB_DB_ERR;
    self->batchWalSuppressDepth++;
    for (i = 0; i < count; i++) {
        uint8_t* old = NULL;
        int oldLen = 0;
        if (Database_IsMainDataTable(self, tableId)
            && Manifest_SecondaryKeyCount(self->manifest, tableId) > 0)
            Database_Get(self, tableId, entries[i].key, entries[i].keyLen, &old, &oldLen);
        SpeedWriteAheadLog_LogSet(wal, tableId, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen);
        if (Memtable_Set(mem, entries[i].key, entries[i].keyLen, entries[i].value, entries[i].valueLen) != GMB_DB_OK) {
            self->batchWalSuppressDepth--;
            free(old);
            return GMB_DB_ERR;
        }
        Database_MaintainSecondaryIndexesOnPut(self, tableId, entries[i].key, entries[i].keyLen,
                                               old, oldLen, entries[i].value, entries[i].valueLen);
        free(old);
    }
    self->batchWalSuppressDepth--;
    if (self->batchWalSuppressDepth == 0 && db_should_fsync(self))
        db_flush_all_wals(self);
    db_maybe_flush_and_compact(self, tableId);
    return GMB_DB_OK;
}

int Database_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/speedsc_XXXXXX";
    Database* db;
    int tableId, vlen, found;
    uint8_t *val, key[8], value[8];

    if (!mkdtemp(dir)) return -1;
    db = Database_Open(dir);
    if (!db) return -2;
    tableId = Database_CreateTable(db, "Person");
    if (tableId <= 0) { Database_Dispose(db); return -3; }

    snprintf((char*)key, sizeof(key), "%s", "k001");
    snprintf((char*)value, sizeof(value), "%s", "v001");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -4; }
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -5; }
    free(val);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -6;
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -7; }
    free(val);

    Database_ForceFlushTableSync(db, tableId);
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -8; }
    free(val);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -9;
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v001", 4) != 0) { free(val); Database_Dispose(db); return -10; }
    free(val);

    if (Database_Delete(db, tableId, key, 4) != GMB_DB_OK) { Database_Dispose(db); return -11; }
    if (Database_Get(db, tableId, key, 4, &val, &vlen)) { free(val); Database_Dispose(db); return -12; }

    snprintf((char*)key, sizeof(key), "%s", "k002");
    snprintf((char*)value, sizeof(value), "%s", "v002");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -13; }
    Database_ForceFlushTableSync(db, tableId);
    Database_Dispose(db);

    db = Database_Open(dir);
    if (!db) return -14;
    found = Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen);
    if (found) { free(val); Database_Dispose(db); return -15; }
    found = Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen);
    if (!found || vlen != 4 || memcmp(val, "v002", 4) != 0) { free(val); Database_Dispose(db); return -16; }
    free(val);

    Database_BeginTransaction(db);
    snprintf((char*)key, sizeof(key), "%s", "k003");
    snprintf((char*)value, sizeof(value), "%s", "v003");
    if (Database_Set(db, tableId, key, 4, value, 4) != GMB_DB_OK) { Database_Dispose(db); return -17; }
    found = Database_Get(db, tableId, key, 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -18; }
    free(val);
    Database_Rollback(db);
    if (Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -19; }

    Database_BeginTransaction(db);
    if (Database_Set(db, tableId, (const uint8_t*)"k003", 4, (const uint8_t*)"v003", 4) != GMB_DB_OK) {
        Database_Dispose(db); return -20;
    }
    if (Database_Delete(db, tableId, (const uint8_t*)"k002", 4) != GMB_DB_OK) { Database_Dispose(db); return -21; }
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -22; }
    if (Database_Commit(db) != GMB_DB_OK) { Database_Dispose(db); return -23; }
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -24; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -25; }
    free(val);

    {
        DatabaseScan* scan = Database_Scan(db, tableId);
        uint8_t *sk = NULL, *sv = NULL;
        int skl = 0, svl = 0, seen = 0;
        char got[8][8];
        int gn = 0;
        if (!scan) { Database_Dispose(db); return -26; }
        while (Database_ScanNext(scan, &sk, &skl, &sv, &svl)) {
            if (gn < 8 && skl < 8) {
                memcpy(got[gn], sk, (size_t)skl);
                got[gn][skl] = 0;
                gn++;
            }
            seen++;
            free(sk);
            free(sv);
        }
        Database_ScanDispose(scan);
        if (seen != 2 || gn != 2 || strcmp(got[0], "k001") != 0 || strcmp(got[1], "k003") != 0) {
            Database_Dispose(db); return -27;
        }
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -28;
    if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -29; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -30; }
    free(val);

    {
        int reduced, n0, n1;
        char names[8][256];
        DatabaseScan* scan;
        uint8_t *sk = NULL, *sv = NULL;
        int skl = 0, svl = 0, seen = 0;
        char got[8];
        reduced = Database_ForceCompactTable(db, "Person");
        if (reduced < 1) { Database_Dispose(db); return -31; }
        if (Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -32; }
        if (Database_Get(db, tableId, (const uint8_t*)"k002", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -33; }
        found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
        if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -34; }
        free(val);
        n0 = Manifest_GetFilesForLevel(db->manifest, tableId, 0, names, 8);
        n1 = Manifest_GetFilesForLevel(db->manifest, tableId, 1, names, 8);
        if (n0 != 0 || n1 != 1) { Database_Dispose(db); return -35; }
        scan = Database_Scan(db, tableId);
        if (!scan) { Database_Dispose(db); return -36; }
        got[0] = 0;
        while (Database_ScanNext(scan, &sk, &skl, &sv, &svl)) {
            seen++;
            if (skl < 8) { memcpy(got, sk, (size_t)skl); got[skl] = 0; }
            free(sk);
            free(sv);
        }
        Database_ScanDispose(scan);
        if (seen != 1 || strcmp(got, "k003") != 0) { Database_Dispose(db); return -37; }
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -38;
    if (Database_Get(db, tableId, (const uint8_t*)"k001", 4, &val, &vlen)) { free(val); Database_Dispose(db); return -39; }
    found = Database_Get(db, tableId, (const uint8_t*)"k003", 4, &val, &vlen);
    if (!found || memcmp(val, "v003", 4) != 0) { free(val); Database_Dispose(db); return -40; }
    free(val);

    {
        DatabaseKv batch[3];
        uint8_t k0[4] = { 'k', '0', '1', '0' }, v0[4] = { 'v', '0', '1', '0' };
        uint8_t k1[4] = { 'k', '0', '1', '1' }, v1[4] = { 'v', '0', '1', '1' };
        uint8_t k2[4] = { 'k', '0', '1', '2' }, v2[4] = { 'v', '0', '1', '2' };
        batch[0].key = k0; batch[0].keyLen = 4; batch[0].value = v0; batch[0].valueLen = 4;
        batch[1].key = k1; batch[1].keyLen = 4; batch[1].value = v1; batch[1].valueLen = 4;
        batch[2].key = k2; batch[2].keyLen = 4; batch[2].value = v2; batch[2].valueLen = 4;
        if (Database_SetMany(db, tableId, batch, 3) != GMB_DB_OK) { Database_Dispose(db); return -41; }
        found = Database_Get(db, tableId, k0, 4, &val, &vlen);
        if (!found || memcmp(val, v0, 4) != 0) { free(val); Database_Dispose(db); return -42; }
        free(val);
        found = Database_Get(db, tableId, k2, 4, &val, &vlen);
        if (!found || memcmp(val, v2, 4) != 0) { free(val); Database_Dispose(db); return -43; }
        free(val);
        if (Database_SetMany(db, tableId, batch, 0) != GMB_DB_OK) { Database_Dispose(db); return -44; }
        if (Database_EndGroupCommit(db) != GMB_DB_ERR) { Database_Dispose(db); return -45; }
        Database_BeginGroupCommit(db);
        Database_BeginGroupCommit(db);
        if (Database_Set(db, tableId, (const uint8_t*)"k020", 4, (const uint8_t*)"v020", 4) != GMB_DB_OK) {
            Database_Dispose(db); return -46;
        }
        if (Database_EndGroupCommit(db) != GMB_DB_OK) { Database_Dispose(db); return -47; }
        if (Database_EndGroupCommit(db) != GMB_DB_OK) { Database_Dispose(db); return -48; }
        found = Database_Get(db, tableId, (const uint8_t*)"k020", 4, &val, &vlen);
        if (!found || memcmp(val, "v020", 4) != 0) { free(val); Database_Dispose(db); return -49; }
        free(val);
    }

    Database_Dispose(db);
    db = Database_Open(dir);
    if (!db) return -50;
    found = Database_Get(db, tableId, (const uint8_t*)"k010", 4, &val, &vlen);
    if (!found || memcmp(val, "v010", 4) != 0) { free(val); Database_Dispose(db); return -51; }
    free(val);
    found = Database_Get(db, tableId, (const uint8_t*)"k020", 4, &val, &vlen);
    if (!found || memcmp(val, "v020", 4) != 0) { free(val); Database_Dispose(db); return -52; }
    free(val);

    {
        int nav = Database_CreateTable(db, "Nav");
        uint8_t *cur = NULL;
        int clen = 0;
        if (nav <= 0) { Database_Dispose(db); return -53; }
        if (Database_Set(db, nav, (const uint8_t*)"a", 1, (const uint8_t*)"va", 2) != GMB_DB_OK ||
            Database_Set(db, nav, (const uint8_t*)"b", 1, (const uint8_t*)"vb", 2) != GMB_DB_OK ||
            Database_Set(db, nav, (const uint8_t*)"c", 1, (const uint8_t*)"vc", 2) != GMB_DB_OK) {
            Database_Dispose(db); return -54;
        }
        if (!Database_GoTop(db, nav) || Database_Bof(db, nav) || Database_Eof(db, nav)) {
            Database_Dispose(db); return -55;
        }
        if (!Database_GetCurrent(db, nav, &cur, &clen) || clen != 2 || memcmp(cur, "va", 2) != 0) {
            free(cur); Database_Dispose(db); return -56;
        }
        free(cur);
        if (!Database_Skip(db, nav, 1) || !Database_GetCurrent(db, nav, &cur, &clen) ||
            memcmp(cur, "vb", 2) != 0) {
            free(cur); Database_Dispose(db); return -57;
        }
        free(cur);
        if (!Database_Skip(db, nav, 1) || !Database_GetCurrent(db, nav, &cur, &clen) ||
            memcmp(cur, "vc", 2) != 0) {
            free(cur); Database_Dispose(db); return -58;
        }
        free(cur);
        if (Database_Skip(db, nav, 1) || !Database_Eof(db, nav) || Database_Bof(db, nav)) {
            Database_Dispose(db); return -59;
        }
        if (!Database_GoBottom(db, nav) || !Database_GetCurrent(db, nav, &cur, &clen) ||
            memcmp(cur, "vc", 2) != 0) {
            free(cur); Database_Dispose(db); return -60;
        }
        free(cur);
        if (!Database_Skip(db, nav, -1) || !Database_GetCurrent(db, nav, &cur, &clen) ||
            memcmp(cur, "vb", 2) != 0) {
            free(cur); Database_Dispose(db); return -61;
        }
        free(cur);
        if (!Database_Seek(db, nav, (const uint8_t*)"a", 1, NULL) ||
            !Database_GetCurrent(db, nav, &cur, &clen) || memcmp(cur, "va", 2) != 0) {
            free(cur); Database_Dispose(db); return -62;
        }
        free(cur);
        if (Database_Seek(db, nav, (const uint8_t*)"z", 1, NULL) || Database_GetCurrent(db, nav, &cur, &clen)) {
            free(cur); Database_Dispose(db); return -63;
        }
        if (!Database_SetOrder(db, nav, "Id") || Database_SetOrder(db, nav, "NoSuch")) {
            Database_Dispose(db); return -64;
        }
        {
            DatabaseScan* rs;
            uint8_t *k = NULL, *v = NULL;
            int kl = 0, vl = 0, nscan = 0;
            const char* fnames[] = { "Id", "Name" };
            const char* ftypes[] = { "Integer", "String" };
            int flens[] = { 0, 20 };
            int tid;
            rs = Database_ScanRange(db, nav, (const uint8_t*)"b", 1, (const uint8_t*)"d", 1);
            if (!rs) { Database_Dispose(db); return -67; }
            while (Database_ScanNext(rs, &k, &kl, &v, &vl)) {
                nscan++;
                free(k);
                free(v);
            }
            Database_ScanDispose(rs);
            if (nscan != 2) { Database_Dispose(db); return -68; }
            rs = Database_ScanRange(db, nav, (const uint8_t*)"b", 1, (const uint8_t*)"c", 1);
            if (!rs) { Database_Dispose(db); return -69; }
            nscan = 0;
            while (Database_ScanNext(rs, &k, &kl, &v, &vl)) {
                nscan++;
                free(k);
                free(v);
            }
            Database_ScanDispose(rs);
            if (nscan != 1) { Database_Dispose(db); return -70; }
            tid = Database_CreateTableWithFields(db, "PersonEx", fnames, ftypes, flens, 2);
            if (tid <= 0) { Database_Dispose(db); return -71; }
        }
        if (Database_DropView(db, "Nav") != GMB_DB_OK) { Database_Dispose(db); return -65; }
        if (Database_GoTop(db, nav)) { Database_Dispose(db); return -66; }
    }

    Database_Dispose(db);
    return GMB_DB_OK;
}
