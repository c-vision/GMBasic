// FILE: Basic.Databases.Speed.Database.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Collections.Concurrent;
using System.Collections.Immutable;
using Basic.SPEED;

namespace Basic.Databases.Speed;

/// <summary>
/// Definisce la firma per un callback in grado di valutare un'espressione in formato stringa
/// all'interno di un contesto di riga specifico. Questo disaccoppia il motore
/// di query dall'implementazione specifica dell'Executor.
/// </summary>
/// <param name="expressionString">La stringa dell'espressione da valutare.</param>
/// <param name="rowContext">Il dizionario di dati che rappresenta la riga corrente, con chiavi qualificate da alias.</param>
/// <returns>Il risultato della valutazione.</returns>
public delegate object? ExpressionEvaluator(string expressionString, Dictionary<string, object?> rowContext);

/// <summary>
/// Mantiene lo stato del cursore di navigazione per una singola tabella, inclusa la posizione
/// corrente, lo stato BOF/EOF e l'indice attivo per l'ordinamento.
/// </summary>
internal class TableCursor
{
    /// <summary>
    /// L'ID della tabella a cui questo cursore si riferisce.
    /// </summary>
    public int TableId { get; }

    /// <summary>
    /// La chiave dell'indice attivo su cui è attualmente posizionato il cursore.
    /// Se l'ordine è sulla chiave primaria, questo valore coincide con <see cref="CurrentPrimaryKey"/>.
    /// </summary>
    public byte[]? CurrentIndexKey { get; private set; }

    /// <summary>
    /// La chiave primaria del record a cui punta il cursore. Questo valore identifica
    /// univocamente il record nella tabella principale, indipendentemente dall'ordine attivo.
    /// </summary>
    public byte[]? CurrentPrimaryKey { get; private set; }

    /// <summary>
    /// Il nome dell'indice attualmente utilizzato per la navigazione (l'ordine attivo).
    /// </summary>
    public string ActiveIndexName { get; set; }

    /// <summary>
    /// Flag che indica se il cursore è posizionato prima del primo record logico.
    /// </summary>
    public bool Bof { get; set; }

    /// <summary>
    /// Flag che indica se il cursore è posizionato dopo l'ultimo record logico.
    /// </summary>
    public bool Eof { get; set; }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="TableCursor"/>.
    /// </summary>
    /// <param name="tableId">L'ID della tabella associata a questo cursore.</param>
    /// <param name="primaryKeyName">Il nome della chiave primaria, usata come ordine di default.</param>
    public TableCursor(int tableId, string primaryKeyName)
    {
        TableId = tableId;
        ActiveIndexName = primaryKeyName;
        ClearPosition();
    }

    /// <summary>
    /// Imposta la posizione corrente del cursore, aggiornando sia la chiave dell'indice attivo
    /// sia la chiave primaria del record corrispondente. Resetta i flag BOF/EOF.
    /// </summary>
    /// <param name="currentIndexKey">La chiave dall'indice attivo.</param>
    /// <param name="primaryKey">La chiave primaria del record a cui puntare.</param>
    public void SetPosition(byte[] currentIndexKey, byte[] primaryKey)
    {
        CurrentIndexKey = currentIndexKey;
        CurrentPrimaryKey = primaryKey;
        Bof = false;
        Eof = false;
    }

    /// <summary>
    /// Invalida la posizione del cursore, impostando le chiavi a null.
    /// </summary>
    public void ClearPosition()
    {
        CurrentIndexKey = null;
        CurrentPrimaryKey = null;
    }
}

/// <summary>
/// Classe principale che orchestra il motore di database LSM-Tree "Speed".
/// Gestisce le operazioni di scrittura (Set/Delete), lettura (Get), le transazioni, l'esecuzione di query complesse,
/// e i processi in background come il flush delle Memtable e la compattazione.
/// </summary>
public partial class Database : IDisposable
{
    #region Constants and Fields

    /// <summary>
    /// La soglia (numero di file) che, una volta raggiunta nel Livello 0, innesca
    /// un'operazione di compattazione in background.
    /// </summary>
    public const int CompactLevel0Threshold = 4;

    /// <summary>
    /// Il percorso completo della directory radice del database Speed.
    /// Reso 'internal' per essere accessibile da altri componenti del motore come il Compactor.
    /// </summary>
    internal string DatabasePath { get; }

    /// <summary>
    /// L'istanza del gestore del file Manifest, che tiene traccia dello schema e della struttura dei file del database.
    /// Reso 'internal' per essere accessibile da altri componenti del motore.
    /// </summary>
    internal Manifest Manifest { get; }

    /// <summary>
    /// Dizionario delle Memtable attive, una per ogni tabella attualmente in uso. La chiave è l'ID della tabella.
    /// </summary>
    private readonly Dictionary<int, Memtable> _activeMemtables;

    /// <summary>
    /// Dizionario dei file Write-Ahead Log attivi, uno per ogni tabella con scritture in corso. La chiave è l'ID della tabella.
    /// </summary>
    private readonly Dictionary<int, WriteAheadLog> _activeWals;

    /// <summary>
    /// Cache thread-safe per le istanze di SstReader, per evitare di riaprire e rileggere l'indice dei file SSTable.
    /// La chiave è il percorso completo del file SSTable.
    /// </summary>
    private readonly ConcurrentDictionary<string, SstReader> _sstReaderCache;

    /// <summary>
    /// Dizionario thread-safe per tracciare le operazioni di compattazione in background.
    /// La chiave è l'ID della tabella. Assicura che al massimo una compattazione per tabella sia in esecuzione.
    /// </summary>
    private readonly ConcurrentDictionary<int, Task> _compactionTasks;

    /// <summary>
    /// Dizionario per gestire lock specifici per ogni tabella.
    /// Questo previene race condition durante le operazioni di flush e compattazione.
    /// </summary>
    private readonly ConcurrentDictionary<int, object> _tableLocks = new();

    /// <summary>
    /// Traccia lo stato delle transazioni attive per ogni thread.
    /// La chiave è l'ID del thread gestito.
    /// </summary>
    private readonly ConcurrentDictionary<int, TransactionState> _transactionStates = new();

    /// <summary>
    /// Optional per-table expression evaluators for complex secondary index keys (set by GPM on USE).
    /// ponytail: absent evaluators fall back to field-name lookup in <see cref="ComputeIndexKeyBytes"/>.
    /// </summary>
    private readonly ConcurrentDictionary<int, ExpressionEvaluator> _indexKeyEvaluators = new();

    /// <summary>
    /// La dimensione massima stimata in byte che una Memtable può raggiungere prima
    /// che venga attivato un flush automatico su disco.
    /// </summary>
    private readonly long _memtableThreshold = 4 * 1024 * 1024;

    /// <summary>
    /// Mantiene lo stato del cursore per ogni tabella aperta.
    /// La chiave è l'ID della tabella, il valore è un oggetto che memorizza la posizione corrente.
    /// </summary>
    private readonly ConcurrentDictionary<int, TableCursor> _cursors = new();

    /// <summary>
    /// Lock globale utilizzato per proteggere l'accesso alle strutture dati interne
    /// come `_activeMemtables` e `_activeWals` durante le operazioni di creazione/sostituzione.
    /// Garantisce che le operazioni che modificano la lista delle memtable o dei WAL attivi
    /// siano thread-safe.
    /// </summary>
    private readonly Lock _lock = new();

    private int _groupCommitDepth;

    /// <summary>
    /// ponytail: SetMany batch — evita fsync per-entry su WAL indici secondari; flush unico a fine batch.
    /// </summary>
    private int _batchWalSuppressDepth;

    /// <summary>
    /// Controlla fsync per auto-commit e <see cref="FlushDurability"/>.
    /// </summary>
    public SpeedDurabilityMode DurabilityMode { get; set; } = SpeedDurabilityMode.Full;

    /// <summary>
    /// True durante group-commit — fsync differito fino a <see cref="EndGroupCommit"/>.
    /// </summary>
    public bool IsGroupCommitActive => _groupCommitDepth > 0;

    /// <summary>
    /// Sorgente del token di cancellazione utilizzato per segnalare a tutti i task in background
    /// (come il Garbage Collector e i task di compattazione) di terminare in modo pulito
    /// quando il database viene chiuso tramite <see cref="Dispose"/>.
    /// </summary>
    private readonly CancellationTokenSource _cts = new();

    /// <summary>
    /// Riferimento al task a lunga esecuzione del Garbage Collector.
    /// Viene mantenuto per poterne attendere il completamento durante la fase di <see cref="Dispose"/>.
    /// </summary>
    private readonly Task _garbageCollectorTask;

    /// <summary>
    /// True when this instance participated in the cross-process exclusive lock (ref-counted per path).
    /// </summary>
    private readonly bool _crossProcessLockHeld;

    #endregion

    /// <summary>
    /// Inizializza una nuova istanza del motore di database Speed.
    /// Crea le directory necessarie se non esistono, carica il manifest, ed esegue
    /// il recupero da eventuali file Write-Ahead Log residui da una sessione precedente interrotta.
    /// </summary>
    /// <param name="path">Il percorso della directory radice del database.</param>
    /// <param name="crossProcessLock">
    /// When true (default), acquires an exclusive cross-process lock on the database directory.
    /// ponytail: offline validate/info requires the runtime not to hold the same DB open elsewhere.
    /// </param>
    public Database(string path, bool crossProcessLock = true)
    {
        DatabasePath = path;
        Directory.CreateDirectory(DatabasePath);
        Directory.CreateDirectory(Path.Combine(DatabasePath, Config.WalDirectoryName));
        Directory.CreateDirectory(Path.Combine(DatabasePath, Config.SstDirectoryName));

        if (crossProcessLock)
        {
            SpeedDatabaseLock.Acquire(DatabasePath);
            _crossProcessLockHeld = true;
        }

        Manifest = Manifest.Load(DatabasePath);
        _activeMemtables = new Dictionary<int, Memtable>();
        _activeWals = new Dictionary<int, WriteAheadLog>();
        _sstReaderCache = new ConcurrentDictionary<string, SstReader>();
        _compactionTasks = new ConcurrentDictionary<int, Task>();
        WriteAheadLog.Recover(DatabasePath, _activeMemtables);
        MaybeRebuildUntrustedRowCounts();

        // Avvia il task del Garbage Collector in background.
        _garbageCollectorTask = RunGarbageCollectorAsync(_cts.Token);
    }

    #region Query Execution Engine

    /// <summary>
    /// Esegue un piano di query complesso, orchestrando le fasi di join, filtro, raggruppamento,
    /// aggregazione e ordinamento, e restituisce il set di risultati finale.
    /// </summary>
    /// <param name="plan">L'oggetto QueryPlan che descrive la query da eseguire.</param>
    /// <param name="evaluator">Il callback all'Executor per la valutazione delle espressioni.</param>
    /// <returns>Una lista di dizionari, dove ogni dizionario rappresenta una riga del risultato finale.</returns>
    public List<Dictionary<string, object?>> ExecuteQuery(QueryPlan plan, ExpressionEvaluator evaluator)
    {
        var (sourceRows, whereAppliedByIndex) = ExecuteJoinPhase(plan, evaluator);
        var filteredRows = whereAppliedByIndex
            ? sourceRows
            : ExecuteWherePhase(sourceRows, plan, evaluator);
        var groupedRows = ExecuteGroupByAndHavingPhase(filteredRows.ToList(), plan, evaluator);
        var orderedRows = ExecuteOrderByPhase(groupedRows, plan, evaluator);
        return ExecuteProjectionPhase(orderedRows, plan, evaluator);
    }

    /// <summary>
    /// Esegue la fase di acquisizione dei dati sorgente, gestendo sia la scansione di una singola tabella
    /// sia un'operazione di JOIN tra due tabelle. Implementa un "Nested Loop Join" indicizzato,
    /// che itera sulla tabella esterna ("sinistra") e, per ogni riga, esegue una ricerca per chiave (SEEK)
    /// sulla tabella interna ("destra"), garantendo performance efficienti.
    /// </summary>
    /// <param name="plan">Il piano di query contenente le definizioni delle sorgenti e del join.</param>
    /// <param name="evaluator">Il delegato fornito dall'Executor per valutare la condizione di join.</param>
    /// <returns>
    /// Un <see cref="IEnumerable{T}"/> di dizionari. Ogni dizionario rappresenta una riga combinata
    /// risultante dal join (o una riga singola se non c'è join), con i nomi dei campi già qualificati
    /// con i rispettivi alias di tabella (es. "Clienti.ID", "Ordini.Importo").
    /// </returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se la tabella o l'indice specificato nella clausola JOIN non vengono trovati nel manifest.
    /// </exception>
    private (IEnumerable<Dictionary<string, object?>> Rows, bool WhereAppliedByIndex) ExecuteJoinPhase(
        QueryPlan plan,
        ExpressionEvaluator evaluator)
    {
        IEnumerable<Dictionary<string, object?>> leftSource;
        var whereAppliedByIndex = false;

        if (plan.JoinClause is null)
        {
            var indexed = TryExecuteIndexedEqualitySource(plan);
            if (indexed is not null)
            {
                leftSource = indexed;
                whereAppliedByIndex = true;
            }
            else
            {
                leftSource = ExecuteSourcePhase(plan.FromSource.TableName);
            }
        }
        else
        {
            leftSource = TryExecuteIndexedEqualitySource(plan) ?? ExecuteSourcePhase(plan.FromSource.TableName);
        }

        if (plan.JoinClause == null)
        {
            return (QualifyRows(leftSource, plan.FromSource.Alias), whereAppliedByIndex);
        }

        if (!Manifest.TryGetTable(plan.JoinClause.RightSource.TableName, out var rightTableMeta) ||
            rightTableMeta is null)
            throw new InvalidOperationException(
                $"Tabella per il JOIN '{plan.JoinClause.RightSource.TableName}' non trovata.");

        Basic.Databases.Speed.Config.SpeedIndexDefinition? indexToUse = null;
        try { indexToUse = QueryOptimizer.ResolveJoinIndex(rightTableMeta, Manifest, plan.JoinClause.RightIndexName); }
        catch (InvalidOperationException) { indexToUse = null; }

        var joinedRows = new List<Dictionary<string, object?>>();
        if (indexToUse is not null)
        {
            var (scanSourceTableId, isIndex) = GetScanSource(rightTableMeta.Id, indexToUse.Name);
            foreach (var leftRow in leftSource)
            {
                var qualifiedLeftRow = QualifyRow(leftRow, plan.FromSource.Alias);
                var rightKeyObject = evaluator(plan.JoinClause.LeftKeyExpressionString, qualifiedLeftRow);
                var rightKeyToSeek = SpeedMarshaller.ToBytes(rightKeyObject, indexToUse.DataType, rightTableMeta.Schema);
                var foundValueBytes = Get(scanSourceTableId, rightKeyToSeek);
                if (foundValueBytes != null)
                {
                    var primaryKeyBytes = isIndex ? foundValueBytes : rightKeyToSeek;
                    var rightRowBytes = Get(rightTableMeta.Id, primaryKeyBytes);
                    if (rightRowBytes != null)
                    {
                        // Usa l'overload corretto di FromBytes che accetta solo lo schema.
                        if (SpeedMarshaller.FromBytes(rightRowBytes, rightTableMeta.Schema) is
                            Dictionary<string, object?> udtDict)
                        {
                            var rightRow = QualifyRow(udtDict, plan.JoinClause.RightSource.Alias);
                            joinedRows.Add(qualifiedLeftRow.Concat(rightRow).ToDictionary(kvp => kvp.Key, kvp => kvp.Value,
                                StringComparer.OrdinalIgnoreCase));
                        }
                    }
                }
            }
        }
        else
        {
            // Nessun INDICE sul campo di join (es. `JOIN ... ON A.id = B.id_cliente` dove B è
            // indicizzata su un altro campo): nested-loop con SCAN completo della tabella
            // destra. Prima ResolveJoinIndex lanciava → la query ritornava 0 righe (bug
            // speed/select: PCode/VM-C fanno lo scan, IL no).
            var rightRows = new List<Dictionary<string, object?>>();
            foreach (var rr in ExecuteSourcePhase(plan.JoinClause.RightSource.TableName))
                rightRows.Add(QualifyRow(rr, plan.JoinClause.RightSource.Alias));
            var rightFieldExpr = plan.JoinClause.RightSource.Alias + "." + plan.JoinClause.RightIndexName;
            foreach (var leftRow in leftSource)
            {
                var qualifiedLeftRow = QualifyRow(leftRow, plan.FromSource.Alias);
                var leftKey = evaluator(plan.JoinClause.LeftKeyExpressionString, qualifiedLeftRow);
                foreach (var rightRow in rightRows)
                {
                    var rightKey = evaluator(rightFieldExpr, rightRow);
                    if (!JoinKeysEqual(leftKey, rightKey)) continue;
                    joinedRows.Add(qualifiedLeftRow.Concat(rightRow).ToDictionary(kvp => kvp.Key, kvp => kvp.Value,
                        StringComparer.OrdinalIgnoreCase));
                }
            }
        }

        return (joinedRows, false);
    }

    /// <summary>Confronto chiave di JOIN: numerici per valore (long/int/double), altrimenti
    /// Equals (stringhe). Null non combacia mai.</summary>
    private static bool JoinKeysEqual(object? a, object? b)
    {
        if (a is null || b is null) return false;
        if (a is IConvertible && b is IConvertible && IsNumeric(a) && IsNumeric(b))
            return Convert.ToDouble(a, System.Globalization.CultureInfo.InvariantCulture)
                .Equals(Convert.ToDouble(b, System.Globalization.CultureInfo.InvariantCulture));
        return a.Equals(b);
    }
    private static bool IsNumeric(object o) =>
        o is sbyte or byte or short or ushort or int or uint or long or ulong or float or double or decimal;
    private IEnumerable<Dictionary<string, object?>> QualifyRows(
        IEnumerable<Dictionary<string, object?>> rows,
        string alias)
    {
        foreach (var row in rows)
            yield return QualifyRow(row, alias);
    }

    /// <summary>
    /// Esegue la scansione di una singola tabella sorgente, fungendo da produttore di righe
    /// per le fasi successive della pipeline di query. Recupera i metadati della tabella,
    /// itera su tutti i suoi record (usando il metodo <see cref="Scan"/>) e deserializza
    /// ogni record dal suo formato binario a un dizionario chiave-valore.
    /// </summary>
    /// <param name="tableName">Il nome della tabella da scansionare, come specificato nella clausola FROM.</param>
    /// <returns>
    /// Un <see cref="IEnumerable{T}"/> di dizionari, dove ogni dizionario rappresenta
    /// un record completo della tabella. Le chiavi del dizionario sono i nomi dei campi
    /// e i valori sono i dati deserializzati.
    /// </returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se la tabella con il nome specificato non viene trovata nel manifest del database.
    /// </exception>
    private IEnumerable<Dictionary<string, object?>> ExecuteSourcePhase(string tableName)
    {
        if (!Manifest.TryGetTable(tableName, out var tableMeta) || tableMeta is null)
            throw new InvalidOperationException($"Tabella sorgente '{tableName}' non trovata.");

        foreach (var kvp in Scan(tableMeta.Id))
        {
            if (kvp.Value != null)
            {
                // Usa l'overload corretto di FromBytes che accetta solo lo schema.
                if (SpeedMarshaller.FromBytes(kvp.Value, tableMeta.Schema) is Dictionary<string, object?> udtDict)
                    yield return udtDict;
            }
        }
    }

    /// <summary>
    /// When WHERE is top-level AND of equalities on indexed fields, seek via the optimizer-chosen index
    /// instead of scanning the main table. ponytail: AND-only; OR/range/functions fall back to scan.
    /// </summary>
    private IEnumerable<Dictionary<string, object?>>? TryExecuteIndexedEqualitySource(QueryPlan plan)
    {
        if (plan.JoinClause is not null) return null;

        var equalities = QueryOptimizer.TryParseCompoundAndEqualities(plan.WhereClauseString, plan.FromSource.Alias);
        if (equalities is null || equalities.Count == 0) return null;

        if (!Manifest.TryGetTable(plan.FromSource.TableName, out var tableMeta) || tableMeta is null)
            return null;

        var predicates = equalities
            .Select(e => new QueryOptimizer.EqualityPredicate(e.FieldName, e.TableAlias))
            .ToList();

        var accessPlan = QueryOptimizer.SelectBestIndex(
            tableMeta,
            Manifest,
            predicates,
            predicates[0].FieldName);
        if (accessPlan is null) return null;

        var indexDef = QueryOptimizer.GetIndexDefinition(tableMeta.Schema, accessPlan.IndexName);
        if (indexDef is null) return null;

        var seekPredicate = equalities.First(e =>
            QueryOptimizer.PredicateMatchesIndex(e.FieldName, indexDef));

        var literalValue = QueryOptimizer.CoerceLiteral(seekPredicate.RawValue, indexDef.DataType);
        if (literalValue is null) return null;

        var seekKey = SpeedMarshaller.ToBytes(literalValue, indexDef.DataType, tableMeta.Schema);
        var (scanSourceTableId, isIndex) = GetScanSource(tableMeta.Id, accessPlan.IndexName);
        var foundValue = Get(scanSourceTableId, seekKey);
        if (foundValue is null) return [];

        var primaryKeyBytes = isIndex ? foundValue : seekKey;
        var rowBytes = Get(tableMeta.Id, primaryKeyBytes);
        if (rowBytes is null) return [];

        if (SpeedMarshaller.FromBytes(rowBytes, tableMeta.Schema) is not Dictionary<string, object?> row)
            return [];

        if (!QueryOptimizer.RowMatchesEqualities(row, tableMeta.Schema, equalities))
            return [];

        return [row];
    }

    /// <summary>
    /// Applica la clausola WHERE a un set di righe.
    /// </summary>
    private IEnumerable<Dictionary<string, object?>> ExecuteWherePhase(IEnumerable<Dictionary<string, object?>> rows,
        QueryPlan plan, ExpressionEvaluator evaluator)
    {
        // Controlla se la stringa 'WhereClauseString' è nulla o vuota.
        if (string.IsNullOrEmpty(plan.WhereClauseString))
        {
            return rows;
        }

        // Passa la stringa 'WhereClauseString' direttamente all'evaluator.
        return rows.Where(row => IsTrue(evaluator(plan.WhereClauseString, row)));
    }

    /// <summary>
    /// Esegue le fasi di raggruppamento (`GROUP BY`), aggregazione (es. `SUM`, `COUNT`) e
    /// filtro post-aggregazione (`HAVING`) su un set di dati pre-filtrato.
    /// Se non è presente una clausola `GROUP BY`, restituisce semplicemente le righe in input.
    /// </summary>
    /// <param name="rows">La lista delle righe (già filtrate dalla clausola `WHERE`) da processare.
    /// Questa lista viene materializzata in memoria per permettere il raggruppamento.</param>
    /// <param name="plan">Il piano di query contenente le definizioni per `GROUP BY`, `HAVING` e le proiezioni.</param>
    /// <param name="evaluator">Il delegato per valutare le espressioni di raggruppamento e le condizioni di `HAVING`.</param>
    /// <returns>
    /// Una nuova lista di dizionari, dove ogni dizionario rappresenta una singola riga aggregata.
    /// Le chiavi del dizionario sono gli alias definiti nella clausola `SELECT` (sia per i campi di
    /// raggruppamento sia per i risultati delle funzioni di aggregazione).
    /// </returns>
    private List<Dictionary<string, object?>> ExecuteGroupByAndHavingPhase(List<Dictionary<string, object?>> rows,
        QueryPlan plan, ExpressionEvaluator evaluator)
    {
        if (plan.GroupByClauseStrings == null || !plan.GroupByClauseStrings.Value.Any())
        {
            var hasAggregates = plan.Projections.Any(p => p.ExpressionString.Contains('('));
            if (!hasAggregates) return rows;
            var singleGroupRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            foreach (var proj in plan.Projections)
            {
                var values = rows.Select(row => evaluator(proj.ExpressionString, row)).ToList();
                singleGroupRow[proj.Alias] = Aggregate(values, proj.ExpressionString);
            }

            return [singleGroupRow];
        }

        var groups =
            new Dictionary<ImmutableArray<object?>, List<Dictionary<string, object?>>>(
                ImmutableArrayEqualityComparer<object?>.Instance);
        foreach (var row in rows)
        {
            var key = plan.GroupByClauseStrings.Value.Select(expr => evaluator(expr, row)).ToImmutableArray();
            if (!groups.TryGetValue(key, out var groupRows))
            {
                groupRows = [];
                groups[key] = groupRows;
            }

            groupRows.Add(row);
        }

        var aggregatedRows = new List<Dictionary<string, object?>>();
        foreach (var group in groups)
        {
            var aggregatedRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            foreach (var proj in plan.Projections)
            {
                if (proj.ExpressionString.Contains('('))
                {
                    var values = group.Value.Select(row => evaluator(proj.ExpressionString, row)).ToList();
                    aggregatedRow[proj.Alias] = Aggregate(values, proj.ExpressionString);
                }
                else
                {
                    aggregatedRow[proj.Alias] = evaluator(proj.ExpressionString, group.Value.First());
                }
            }

            if (!string.IsNullOrEmpty(plan.HavingClauseString) &&
                !IsTrue(evaluator(plan.HavingClauseString, aggregatedRow))) continue;
            aggregatedRows.Add(aggregatedRow);
        }

        return aggregatedRows;
    }

    /// <summary>
    /// Ordina un set di righe in memoria in base alle colonne e alle direzioni specificate
    /// nella clausola `ORDER BY` del piano di query. Se non è specificato alcun ordinamento,
    /// restituisce la lista di righe originale.
    /// </summary>
    /// <param name="rows">La lista di righe da ordinare (tipicamente il risultato della fase di raggruppamento o di filtro).</param>
    /// <param name="plan">Il piano di query da cui ottenere le definizioni di ordinamento.</param>
    /// <param name="evaluator">Il delegato per valutare le espressioni di ordinamento su ogni riga,
    /// trasformandole in valori comparabili.</param>
    /// <returns>
    /// Una nuova lista di dizionari contenente le righe ordinate secondo le specifiche.
    /// L'ordinamento è stabile e supporta più colonne con direzioni miste (ASC/DESC).
    /// </returns>
    private List<Dictionary<string, object?>> ExecuteOrderByPhase(List<Dictionary<string, object?>> rows,
        QueryPlan plan, ExpressionEvaluator evaluator)
    {
        if (plan.OrderByClauses == null || !plan.OrderByClauses.Value.Any()) return rows;

        IOrderedEnumerable<Dictionary<string, object?>>? orderedRows = null;
        for (var i = 0; i < plan.OrderByClauses.Value.Length; i++)
        {
            var orderBy = plan.OrderByClauses.Value[i];
            var keySelector = (Dictionary<string, object?> row) =>
                evaluator(orderBy.ExpressionString, row) as IComparable;
            if (i == 0)
            {
                orderedRows = orderBy.Descending ? rows.OrderByDescending(keySelector) : rows.OrderBy(keySelector);
            }
            else
            {
                orderedRows = orderBy.Descending
                    ? orderedRows!.ThenByDescending(keySelector)
                    : orderedRows!.ThenBy(keySelector);
            }
        }

        return orderedRows?.ToList() ?? rows;
    }

    /// <summary>
    /// Esegue la fase finale della pipeline di query, trasformando il set di dati intermedi
    /// nel formato di output finale definito dalla clausola `SELECT`. Per ogni riga in input,
    /// calcola il valore di ogni espressione nella lista di proiezione e lo assegna al
    /// campo con l'alias corrispondente nella riga di risultato.
    /// </summary>
    /// <param name="rows">L'enumerabile di righe processate (filtrate, raggruppate, ordinate) da proiettare.</param>
    /// <param name="plan">Il piano di query da cui ottenere la lista delle proiezioni (campi e alias).</param>
    /// <param name="evaluator">Il delegato per calcolare il valore di ogni espressione di proiezione.</param>
    /// <returns>
    /// La lista finale di dizionari, dove ogni dizionario rappresenta una riga completa
    /// del risultato della query, pronta per essere restituita all'Executor. Se la query
    /// includeva un `GROUP BY`, questo metodo si occupa principalmente di assicurare che
    /// le colonne abbiano gli alias corretti, dato che i valori sono già stati calcolati.
    /// </returns>
    private List<Dictionary<string, object?>> ExecuteProjectionPhase(IEnumerable<Dictionary<string, object?>> rows,
        QueryPlan plan, ExpressionEvaluator evaluator)
    {
        if (plan.GroupByClauseStrings != null && plan.GroupByClauseStrings.Value.Any()) return rows.ToList();

        var finalResults = new List<Dictionary<string, object?>>();
        foreach (var row in rows)
        {
            var projectedRow = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
            foreach (var projection in plan.Projections)
            {
                projectedRow[projection.Alias] = evaluator(projection.ExpressionString, row);
            }

            finalResults.Add(projectedRow);
        }

        return finalResults;
    }

    #endregion

    #region Public API (CRUD, Transactions, etc.)

    /// <summary>
    /// Rilascia tutte le risorse gestite dal motore di database, garantendo una chiusura pulita.
    /// </summary>
    public void Dispose()
    {
        // Segnala al Garbage Collector e ad altri task di terminare.
        _cts.Cancel();
        // Attende brevemente che il GC completi il suo ciclo corrente.
        _garbageCollectorTask.Wait(1000);

        Task.WhenAll(_compactionTasks.Values).Wait(5000);
        lock (_lock)
        {
            FlushAllActiveWals();
        }

        foreach (var wal in _activeWals.Values) wal.Dispose();
        foreach (var reader in _sstReaderCache.Values) reader.Dispose();

        if (_crossProcessLockHeld)
        {
            SpeedDatabaseLock.Release(DatabasePath);
        }
    }

    /// <summary>
    /// Forza fsync su tutti i WAL attivi.
    /// </summary>
    public void FlushDurability()
    {
        lock (_lock)
        {
            FlushAllActiveWals();
        }
    }

    /// <summary>
    /// ponytail: differisce fsync come bulk-load ma senza transazione esplicita.
    /// Chiamare <see cref="EndGroupCommit"/> per un fsync unico in Full mode.
    /// </summary>
    public void BeginGroupCommit() => _groupCommitDepth++;

    /// <summary>
    /// Termina sessione group-commit e sincronizza su disco se <see cref="SpeedDurabilityMode.Full"/>.
    /// </summary>
    public void EndGroupCommit()
    {
        if (_groupCommitDepth == 0)
        {
            throw new InvalidOperationException("Nessuna sessione group-commit attiva.");
        }

        _groupCommitDepth--;
        if (_groupCommitDepth == 0 && DurabilityMode == SpeedDurabilityMode.Full)
        {
            FlushDurability();
        }
    }

    private bool ShouldFsyncWal() =>
        DurabilityMode == SpeedDurabilityMode.Full
        && _groupCommitDepth == 0
        && _batchWalSuppressDepth == 0;

    private void MaybeFlushWal(WriteAheadLog wal)
    {
        if (ShouldFsyncWal())
        {
            wal.Flush();
        }
    }

    private void MaybeFlushAllActiveWals()
    {
        if (ShouldFsyncWal())
        {
            FlushAllActiveWals();
        }
    }

    private void FlushAllActiveWals()
    {
        foreach (var wal in _activeWals.Values)
        {
            wal.Flush();
        }
    }

    /// <summary>
    /// Avvia una nuova transazione per il thread corrente. Se una transazione è già in corso
    /// per questo thread, l'operazione non ha effetto. Le operazioni di scrittura successive
    /// (Set, Delete) verranno bufferizzate in memoria fino a un Commit o Rollback.
    /// </summary>
    public void BeginTransaction()
    {
        _transactionStates.TryAdd(Environment.CurrentManagedThreadId, new TransactionState());
    }

    /// <summary>
    /// Rende permanenti le modifiche di una transazione in corso per il thread corrente.
    /// L'operazione è atomica: itera su ogni tabella modificata all'interno della transazione,
    /// scrive tutte le operazioni bufferizzate nel rispettivo Write-Ahead Log, e solo dopo
    /// applica le modifiche alle Memtable principali. Se la transazione è vuota, l'operazione termina
    /// senza eseguire I/O.
    /// </summary>
    public void Commit()
    {
        if (_transactionStates.TryRemove(Environment.CurrentManagedThreadId, out var txState))
        {
            if (txState.OperationBuffers.IsEmpty) return;
            lock (_lock)
            {
                foreach (var (tableId, operationBuffer) in txState.OperationBuffers)
                {
                    if (operationBuffer.EstimatedSize == 0) continue;
                    var wal = GetOrCreateWal(tableId);
                    var memtable = GetOrCreateMemtable(tableId);
                    foreach (var op in operationBuffer)
                    {
                        byte[]? oldValue = null;
                        if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
                        {
                            oldValue = TryGetOldValueForIndex(tableId, op.Key);
                        }

                        if (op.Value != null)
                        {
                            AdjustRowCountOnSet(tableId, op.Key);
                            wal.LogSet(tableId, op.Key, op.Value);
                            memtable.Set(op.Key, op.Value);
                            if (IsMainDataTable(tableId))
                                MaintainSecondaryIndexesOnPut(tableId, op.Key, oldValue, op.Value);
                        }
                        else
                        {
                            AdjustRowCountOnDelete(tableId, op.Key);
                            wal.LogDelete(tableId, op.Key);
                            memtable.Delete(op.Key);
                            if (IsMainDataTable(tableId))
                                MaintainSecondaryIndexesOnDelete(tableId, op.Key, oldValue);
                        }
                    }

                    MaybeFlushWal(wal);
                    if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
                }
            }
        }
    }

    /// <summary>
    /// Inserisce o aggiorna molte coppie chiave-valore con un solo lock e batch WAL.
    /// ponytail: un fsync a fine batch (se Full e non in group-commit); indici secondari senza flush per entry.
    /// </summary>
    public void SetMany(int tableId, IReadOnlyList<KeyValuePair<byte[], byte[]>> entries)
    {
        ArgumentNullException.ThrowIfNull(entries);
        if (entries.Count == 0)
        {
            return;
        }

        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
        {
            var buffer = txState.GetBufferForTable(tableId);
            foreach (var entry in entries)
            {
                buffer.Set(entry.Key, entry.Value);
            }

            return;
        }

        Memtable memtable;
        lock (_lock)
        {
            memtable = GetOrCreateMemtable(tableId);
            var wal = GetOrCreateWal(tableId);
            var trackIndexes = IsMainDataTable(tableId) && HasSecondaryIndexes(tableId);

            _batchWalSuppressDepth++;
            try
            {
                foreach (var entry in entries)
                {
                    byte[]? oldValue = null;
                    if (trackIndexes)
                    {
                        oldValue = TryGetOldValueForIndex(tableId, entry.Key);
                    }

                    AdjustRowCountOnSet(tableId, entry.Key);
                    wal.LogSet(tableId, entry.Key, entry.Value);
                    memtable.Set(entry.Key, entry.Value);
                    if (trackIndexes)
                    {
                        MaintainSecondaryIndexesOnPut(tableId, entry.Key, oldValue, entry.Value);
                    }
                }
            }
            finally
            {
                _batchWalSuppressDepth--;
                if (_batchWalSuppressDepth == 0)
                {
                    MaybeFlushAllActiveWals();
                }
            }
        }

        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
    }

    /// <summary>
    /// Annulla una transazione in corso per il thread corrente, scartando tutte le modifiche
    /// bufferizzate in memoria. Le operazioni registrate non verranno mai applicate
    /// alla Memtable principale o scritte su disco.
    /// </summary>
    public void Rollback()
    {
        _transactionStates.TryRemove(Environment.CurrentManagedThreadId, out _);
    }

    /// <summary>
    /// Inserisce o aggiorna una coppia chiave-valore per una data tabella.
    /// Se il thread corrente si trova all'interno di una transazione attiva (iniziata con <see cref="BeginTransaction"/>),
    /// l'operazione viene registrata in un buffer temporaneo in memoria. Altrimenti, l'operazione
    /// viene scritta immediatamente nel Write-Ahead Log e nella Memtable attiva, garantendo l'atomicità
    /// della singola operazione.
    /// </summary>
    /// <param name="tableId">L'ID della tabella in cui scrivere.</param>
    /// <param name="key">La chiave del dato, rappresentata come array di byte.</param>
    /// <param name="value">Il valore del dato, rappresentato come array di byte.</param>
    public void Set(int tableId, byte[] key, byte[] value)
    {
        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
        {
            txState.GetBufferForTable(tableId).Set(key, value);
            return;
        }

        Memtable memtable;
        lock (_lock)
        {
            byte[]? oldValue = null;
            if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
            {
                oldValue = TryGetOldValueForIndex(tableId, key);
            }

            AdjustRowCountOnSet(tableId, key);
            var wal = GetOrCreateWal(tableId);
            wal.LogSet(tableId, key, value);
            memtable = GetOrCreateMemtable(tableId);
            memtable.Set(key, value);
            MaybeFlushWal(wal);
            if (IsMainDataTable(tableId))
                MaintainSecondaryIndexesOnPut(tableId, key, oldValue, value);
        }

        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
    }

    /// <summary>
    /// Cancella una chiave da una data tabella. L'operazione è un "soft delete": se eseguita
    /// al di fuori di una transazione, scrive una "tombstone" (valore nullo) nel Write-Ahead Log
    /// e nella Memtable. Se all'interno di una transazione, la tombstone viene bufferizzata.
    /// La rimozione fisica dei dati avverrà durante un processo di compattazione futuro.
    /// </summary>
    /// <param name="tableId">L'ID della tabella in cui eseguire la cancellazione.</param>
    /// <param name="key">La chiave da cancellare, rappresentata come array di byte.</param>
    public void Delete(int tableId, byte[] key)
    {
        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
        {
            txState.GetBufferForTable(tableId).Delete(key);
            return;
        }

        Memtable memtable;
        lock (_lock)
        {
            byte[]? oldValue = null;
            if (IsMainDataTable(tableId) && HasSecondaryIndexes(tableId))
            {
                oldValue = TryGetOldValueForIndex(tableId, key);
            }

            AdjustRowCountOnDelete(tableId, key);
            var wal = GetOrCreateWal(tableId);
            wal.LogDelete(tableId, key);
            memtable = GetOrCreateMemtable(tableId);
            memtable.Delete(key);
            MaybeFlushWal(wal);
            if (IsMainDataTable(tableId))
                MaintainSecondaryIndexesOnDelete(tableId, key, oldValue);
        }

        if (memtable.EstimatedSize > _memtableThreshold) TriggerFlush(tableId, memtable);
    }

    /// <summary>
    /// Recupera il valore più recente associato a una chiave da una data tabella.
    /// La ricerca avviene a cascata per garantire che venga restituita la versione più aggiornata
    /// e visibile del dato, rispettando la semantica "read-your-own-writes" per le transazioni.
    /// L'ordine di ricerca è:
    /// 1. Buffer della transazione corrente (se attiva).
    /// 2. Memtable attiva in memoria.
    /// 3. File SSTable su disco, dal più recente al più vecchio, attraverso tutti i livelli.
    /// </summary>
    /// <param name="tableId">L'ID della tabella da cui leggere.</param>
    /// <param name="key">La chiave da cercare, rappresentata come array di byte.</param>
    /// <returns>Il valore come array di byte, o <c>null</c> se la chiave non esiste o è stata cancellata (tombstone).</returns>
    public byte[]? Get(int tableId, byte[] key)
    {
        if (_transactionStates.TryGetValue(Environment.CurrentManagedThreadId, out var txState))
        {
            if (txState.OperationBuffers.TryGetValue(tableId, out var tableBuffer) &&
                tableBuffer.Get(key, out var txValue))
                return txValue;
        }

        Memtable currentMemtable;
        lock (_lock)
        {
            currentMemtable = GetOrCreateMemtable(tableId);
        }

        if (currentMemtable.Get(key, out var memValue)) return memValue;

        if (Manifest.TryGetTable(tableId, out var tableMetadata) && tableMetadata != null)
        {
            foreach (var sstFileName in tableMetadata.SstFilesByLevel.Keys.OrderBy(k => k)
                         .SelectMany(level => tableMetadata.GetFilesForLevel(level).Reverse()))
            {
                var reader = _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName),
                    path => new SstReader(path));
                if (reader.TryFind(key, out var sstValue)) return sstValue;
            }
        }

        return null;
    }

    /// <summary>
    /// Esegue una scansione di un intervallo di chiavi per una data tabella e restituisce
    /// un enumeratore per iterare sui risultati in ordine. L'operazione lavora su uno snapshot
    /// consistente dello stato del database (Memtable e lista di SSTable) al momento della chiamata,
    /// garantendo che l'iterazione non sia influenzata da modifiche concorrenti.
    /// </summary>
    /// <param name="tableId">L'ID della tabella su cui eseguire la scansione.</param>
    /// <param name="startKey">La chiave di inizio (inclusiva) della scansione. Se null, la scansione parte dall'inizio.</param>
    /// <param name="endKey">La chiave di fine (esclusiva) della scansione. Se null, la scansione arriva fino alla fine.</param>
    /// <returns>Un <see cref="IEnumerable{T}"/> che produce le coppie chiave-valore nell'intervallo specificato, unendo i dati da tutte le fonti.</returns>
    public IEnumerable<KeyValuePair<byte[], byte[]?>> Scan(int tableId, byte[]? startKey = null, byte[]? endKey = null)
    {
        Memtable memtableSnapshot;
        List<string> sstFilesSnapshot;
        lock (_lock)
        {
            memtableSnapshot = GetOrCreateMemtable(tableId);
            sstFilesSnapshot = Manifest.TryGetTable(tableId, out var meta) && meta != null
                ? meta.SstFilesByLevel.SelectMany(kvp => kvp.Value).ToList()
                : [];
        }

        var readers = sstFilesSnapshot.Select(f =>
            _sstReaderCache.GetOrAdd(Path.Combine(DatabasePath, Config.SstDirectoryName, f),
                path => new SstReader(path))).ToList();
        return new DatabaseIterator(memtableSnapshot, readers, startKey, endKey);
    }

    /// <summary>
    /// Crea una nuova tabella nel database basata su uno schema fornito, se non esiste già.
    /// Se la tabella con lo stesso nome esiste già, valida che lo schema fornito sia identico
    /// a quello memorizzato per prevenire corruzione dei dati.
    /// L'operazione è thread-safe.
    /// </summary>
    /// <param name="schema">Lo schema completo della tabella da creare.</param>
    /// <returns>I metadati della tabella, sia essa nuova o preesistente e validata.</returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se la tabella esiste ma lo schema fornito non corrisponde a quello esistente.
    /// </exception>
    public TableMetadata CreateTable(Config.SpeedSchema schema)
    {
        var meta = Manifest.CreateTable(schema);
        _tableLocks.TryAdd(meta.Id, new object());
        if (!schema.TypeName.StartsWith("_idx_", StringComparison.Ordinal))
            EnsureSecondaryIndexTables(meta);
        return meta;
    }

    /// <summary>
    /// Registers an expression evaluator for secondary index key extraction on a main table.
    /// </summary>
    public void SetIndexKeyEvaluator(int mainTableId, ExpressionEvaluator evaluator)
    {
        ArgumentNullException.ThrowIfNull(evaluator);
        _indexKeyEvaluators[mainTableId] = evaluator;
    }

    #endregion

    #region Schema Modification (Index & View Management)

    /// <summary>
    /// Rimuove un indice secondario da una tabella. L'operazione rimuove la definizione
    /// dell'indice dallo schema della tabella principale e la tabella-indice associata,
    /// schedulando i suoi file per la cancellazione in background dal Garbage Collector.
    /// L'intera operazione è thread-safe.
    /// </summary>
    /// <param name="mainTableId">L'ID della tabella principale da cui rimuovere l'indice.</param>
    /// <param name="indexName">Il nome dell'indice secondario da eliminare.</param>
    /// <exception cref="InvalidOperationException">Lanciata se la tabella principale non viene trovata.</exception>
    public void DropIndex(int mainTableId, string indexName)
    {
        // Recupera i metadati della tabella principale per costruirne il nome.
        if (!Manifest.TryGetTable(mainTableId, out var mainTableMeta) || mainTableMeta is null)
            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");

        // Costruisce il nome della tabella-indice da eliminare.
        var indexTableName = $"_idx_{mainTableMeta.Name}_{indexName}";

        // La manipolazione del manifest è già thread-safe al suo interno.
        var (tableId, filesToDelete) = Manifest.PrepareTableForDeletion(indexTableName);

        if (tableId != -1) // Se la tabella-indice è stata trovata e preparata per la cancellazione
        {
            // Aggiunge i file alla lista di pulizia.
            Manifest.AddFilesToGarbageCollection(filesToDelete);

            // Rimuove la definizione dell'indice secondario dallo schema della tabella principale.
            Manifest.RemoveSecondaryIndex(mainTableMeta.Name, indexName);

            // Salva tutte le modifiche al manifest.
            Manifest.Save();

            // Acquisisce un lock globale per modificare in sicurezza le strutture dati in memoria.
            lock (_lock)
            {
                // Rimuovi eventuali risorse in memoria associate alla tabella-indice cancellata.
                _activeMemtables.Remove(tableId);
                _activeWals.Remove(tableId);
            }

            _cursors.TryRemove(tableId, out _);
            _tableLocks.TryRemove(tableId, out _);
        }
    }

    /// <summary>
    /// Rimuove una vista materializzata dal database. L'operazione rimuove la tabella
    /// che contiene i dati della vista e schedula i suoi file per la cancellazione in background.
    /// </summary>
    /// <param name="viewName">Il nome della vista da eliminare.</param>
    public void DropView(string viewName)
    {
        DropTableInternal(viewName);
    }

    /// <summary>
    /// Svuota una tabella (usato per le viste) e la ripopola eseguendo una query.
    /// L'operazione è atomica dal punto di vista del manifest: la vecchia tabella viene
    /// rimossa e una nuova viene creata e popolata.
    /// </summary>
    /// <param name="viewName">Il nome della vista/tabella da aggiornare.</param>
    /// <param name="plan">Il piano di query per generare i nuovi dati.</param>
    /// <param name="evaluator">Il delegato per la valutazione delle espressioni della query.</param>
    /// <exception cref="InvalidOperationException">Lanciata se la vista specificata non viene trovata nel manifest.</exception>
    public void RefreshView(string viewName, QueryPlan plan, ExpressionEvaluator evaluator)
    {
        if (!Manifest.TryGetTable(viewName, out var viewMeta) || viewMeta is null)
            throw new InvalidOperationException($"Vista '{viewName}' non trovata.");

        // Salva lo schema originale della vista.
        var originalSchema = viewMeta.Schema;

        // 1. Svuota la tabella della vista rimuovendola e schedulando i suoi file per la cancellazione.
        DropTableInternal(viewName);

        // 2. Ricrea la tabella con lo stesso schema. Questo le assegnerà un nuovo ID interno
        //    e la lascerà vuota, pronta per essere popolata.
        var newMeta = CreateTable(originalSchema);

        // 3. Esegui la query di definizione della vista per ottenere il nuovo set di dati.
        var newRows = ExecuteQuery(plan, evaluator);

        // 4. Inserisci i nuovi dati nella tabella della vista appena ricreata.
        //    Questa operazione può essere lunga e viene eseguita in un'unica transazione implicita
        //    dal punto di vista del motore (anche se non è una transazione ACID formale a livello utente).
        BeginTransaction();
        try
        {
            foreach (var row in newRows)
            {
                var pkField = newMeta.Schema.PrimaryKey;

                // Estrae il valore della chiave primaria dalla riga di risultato.
                // Il nome del campo nella riga deve corrispondere al nome del campo PK nello schema della vista.
                if (!row.TryGetValue(pkField.Name, out var pkValue))
                    throw new InvalidOperationException(
                        $"La query per la vista '{viewName}' non produce un valore per la sua chiave primaria '{pkField.Name}'.");

                // Serializza la chiave e l'intero record (il valore).
                var keyBytes = SpeedMarshaller.ToBytes(pkValue, pkField.DataType, newMeta.Schema);
                var valueBytes = SpeedMarshaller.ToBytes(row, newMeta.Schema);

                // Inserisce il nuovo record.
                Set(newMeta.Id, keyBytes, valueBytes);
            }

            Commit();
        }
        catch
        {
            Rollback();
            throw; // Rilancia l'eccezione originale dopo aver annullato la transazione.
        }
    }

    /// <summary>
    /// Logica interna condivisa per rimuovere una tabella (che sia una tabella dati,
    /// un indice o una vista) dal manifest e schedulare i suoi file per il garbage collector.
    /// L'operazione è thread-safe.
    /// </summary>
    /// <param name="tableName">Il nome della tabella da rimuovere.</param>
    private void DropTableInternal(string tableName)
    {
        // La manipolazione del manifest è già thread-safe al suo interno.
        var (tableId, filesToDelete) = Manifest.PrepareTableForDeletion(tableName);

        if (tableId != -1) // Se la tabella è stata trovata e preparata per la cancellazione
        {
            // Aggiunge i file alla lista di pulizia e salva il manifest.
            Manifest.AddFilesToGarbageCollection(filesToDelete);
            Manifest.Save();

            // Acquisisce un lock globale per modificare in sicurezza le strutture dati in memoria.
            lock (_lock)
            {
                // Rimuovi eventuali risorse in memoria associate alla tabella cancellata.
                _activeMemtables.Remove(tableId);
                _activeWals.Remove(tableId);
            }

            // Queste strutture sono ConcurrentDictionary, quindi la loro rimozione è thread-safe
            // e non necessita del lock globale.
            _cursors.TryRemove(tableId, out _);
            _tableLocks.TryRemove(tableId, out _);
        }
    }

    #endregion

    #region Navigazione e Cursori

    /// <summary>
    /// Imposta l'indice attivo per la navigazione su una tabella.
    /// </summary>
    public bool SetOrder(int tableId, string indexName)
    {
        if (!Manifest.TryGetTable(tableId, out var tableMeta) || tableMeta is null) return false;
        var cursor = GetCursor(tableId);
        if (string.IsNullOrEmpty(indexName) ||
            indexName.Equals(tableMeta.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
        {
            cursor.ActiveIndexName = tableMeta.Schema.PrimaryKey.Name;
            return true;
        }

        if (tableMeta.Schema.SecondaryKeys.Any(sk => sk.Name.Equals(indexName, StringComparison.OrdinalIgnoreCase)))
        {
            cursor.ActiveIndexName = indexName;
            return true;
        }

        return false;
    }

    /// <summary>
    /// Posiziona il cursore sul primo record di una tabella, secondo l'ordine attivo.
    /// </summary>
    public bool GoTop(int tableId)
    {
        var cursor = GetCursor(tableId);
        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
        var firstRecord = Scan(scanSourceTableId).FirstOrDefault();
        if (firstRecord.Key != null)
        {
            var primaryKey = isIndex ? firstRecord.Value : firstRecord.Key;
            if (primaryKey == null) return MoveToEmpty(cursor);
            cursor.SetPosition(firstRecord.Key, primaryKey);
            return true;
        }

        return MoveToEmpty(cursor);
    }

    /// <summary>
    /// Posiziona il cursore sull'ultimo record di una tabella, secondo l'ordine attivo.
    /// </summary>
    public bool GoBottom(int tableId)
    {
        var cursor = GetCursor(tableId);
        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
        var lastRecord = Scan(scanSourceTableId).LastOrDefault();
        if (lastRecord.Key != null)
        {
            var primaryKey = isIndex ? lastRecord.Value : lastRecord.Key;
            if (primaryKey == null) return MoveToEmpty(cursor);
            cursor.SetPosition(lastRecord.Key, primaryKey);
            return true;
        }

        return MoveToEmpty(cursor);
    }

    /// <summary>
    /// Muove il cursore di un numero specificato di posizioni. Questa versione è stata corretta per
    /// gestire la navigazione all'indietro (`recordCount mipre di 0`) in modo efficiente in termini di memoria,
    /// evitando di caricare l'intero set di dati su disco.
    /// </summary>
    /// <param name="tableId">L'ID della tabella su cui operare.</param>
    /// <param name="recordCount">Il numero di record da saltare. Positivo per andare avanti, negativo per tornare indietro. Default è 1.</param>
    /// <returns>True se il cursore è stato posizionato su un record valido, altrimenti False (se viene raggiunto BOF o EOF).</returns>
    public bool Skip(int tableId, int recordCount = 1)
    {
        var cursor = GetCursor(tableId);
        if (cursor.CurrentIndexKey == null || recordCount == 0) return false;
        var (scanSourceTableId, isIndex) = GetScanSource(tableId, cursor.ActiveIndexName);
        var direction = Math.Sign(recordCount);
        var toSkip = Math.Abs(recordCount);

        KeyValuePair<byte[], byte[]?> finalRecord = default;

        if (direction > 0)
        {
            // La navigazione in avanti è efficiente e può rimanere invariata.
            var scan = Scan(scanSourceTableId, cursor.CurrentIndexKey).Skip(1).Take(toSkip);
            finalRecord = scan.LastOrDefault();
        }
        else // Navigazione all'indietro
        {
            // Sostituisce .Reverse() con un approccio a memoria limitata.
            // Mantiene un buffer degli ultimi 'toSkip' record incontrati prima della posizione corrente.
            var buffer = new List<KeyValuePair<byte[], byte[]?>>(toSkip);
            foreach (var kvp in Scan(scanSourceTableId, null, cursor.CurrentIndexKey))
            {
                buffer.Add(kvp);
                if (buffer.Count > toSkip)
                {
                    buffer.RemoveAt(0); // Mantiene il buffer alla dimensione massima di 'toSkip'.
                }
            }

            // Se abbiamo trovato esattamente il numero di record richiesto, il primo elemento
            // del buffer è la nostra destinazione.
            if (buffer.Count == toSkip)
            {
                finalRecord = buffer[0];
            }
            // Altrimenti (buffer.Count < toSkip), non ci sono abbastanza record.
            // 'finalRecord' rimane default, e il codice successivo gestirà correttamente il caso BOF.
        }

        if (finalRecord.Key != null)
        {
            var pk = isIndex ? finalRecord.Value : finalRecord.Key;
            if (pk == null) return MoveToBoundary(cursor, direction); // Trovato un record cancellato
            cursor.SetPosition(finalRecord.Key, pk);
            return true;
        }

        // Se finalRecord.Key è null, significa che abbiamo superato i limiti della tabella.
        return MoveToBoundary(cursor, direction);
    }

    /// <summary>
    /// Recupera il record alla posizione corrente del cursore.
    /// </summary>
    public byte[]? GetCurrent(int tableId)
    {
        if (_cursors.TryGetValue(tableId, out var cursor) && cursor.CurrentPrimaryKey != null)
        {
            return Get(tableId, cursor.CurrentPrimaryKey);
        }

        return null;
    }

    #endregion

    #region Metodi Helper per Query

    /// <summary>
    /// Determina se un valore rappresenta la verità secondo la logica del linguaggio BASIC (booleano true o numero diverso da zero).
    /// </summary>
    /// <param name="value">Il valore da controllare.</param>
    /// <returns>True se il valore è "truthy", altrimenti False.</returns>
    private bool IsTrue(object? value) => value switch
    {
        bool b => b,
        int i => i != 0,
        long l => l != 0,
        double d => d != 0.0,
        decimal m => m != 0.0m,
        _ => false
    };

    /// <summary>
    /// Prepara una riga di dati per la valutazione qualificando ogni nome di campo con l'alias della sua tabella.
    /// </summary>
    /// <param name="row">Il dizionario che rappresenta la riga.</param>
    /// <param name="alias">L'alias della tabella da anteporre ai nomi dei campi.</param>
    /// <returns>Un nuovo dizionario con le chiavi qualificate (es. "ID" -> "Clienti.ID").</returns>
    private Dictionary<string, object?> QualifyRow(Dictionary<string, object?> row, string alias)
    {
        return row.ToDictionary(kvp => $"{alias}.{kvp.Key}", kvp => kvp.Value, StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Calcola una funzione di aggregazione su una lista di valori.
    /// </summary>
    /// <param name="values">La lista di valori su cui calcolare l'aggregazione.</param>
    /// <param name="expression">La stringa dell'espressione di aggregazione (es. "SUM(Importo)").</param>
    /// <returns>Il risultato dell'aggregazione.</returns>
    private object? Aggregate(List<object?> values, string expression)
    {
        var funcName = expression[..expression.IndexOf('(')].Trim().ToUpperInvariant();
        var numericValues = values.Where(v => v != null).Select(v =>
        {
            try
            {
                return Convert.ToDecimal(v);
            }
            catch
            {
                return (decimal?)null;
            }
        }).Where(v => v.HasValue).ToList();

        return funcName switch
        {
            "COUNT" => (long)values.Count,
            "SUM" => numericValues.Sum(v => v.GetValueOrDefault()),
            "AVG" => numericValues.Count != 0 ? numericValues.Average(v => v.GetValueOrDefault()) : 0m,
            "MIN" => values.Min(),
            "MAX" => values.Max(),
            _ => null
        };
    }

    #endregion

    #region Metodi Helper

    /// <summary>
    /// Recupera il cursore per una data tabella, creandolo se non esiste.
    /// Questo metodo centralizza la logica di inizializzazione del cursore, garantendo
    /// che ogni tabella abbia un solo cursore associato.
    /// </summary>
    /// <param name="tableId">L'ID della tabella per cui ottenere il cursore.</param>
    /// <returns>L'istanza di <see cref="TableCursor"/> associata alla tabella.</returns>
    /// <exception cref="InvalidOperationException">Se i metadati per la tabella non vengono trovati,
    /// rendendo impossibile determinare la chiave primaria di default per il cursore.</exception>
    internal TableCursor GetCursor(int tableId)
    {
        if (_cursors.TryGetValue(tableId, out var cursor)) return cursor;
        if (!Manifest.TryGetTable(tableId, out var tableMeta) || tableMeta is null)
            throw new InvalidOperationException($"Tabella con ID {tableId} non trovata.");
        return _cursors.GetOrAdd(tableId, id => new TableCursor(id, tableMeta.Schema.PrimaryKey.Name));
    }

    /// <summary>
    /// Determina quale tabella scansionare (la tabella dati principale o una tabella-indice)
    /// in base all'indice attivo specificato.
    /// </summary>
    /// <param name="mainTableId">L'ID della tabella dati principale su cui si sta operando.</param>
    /// <param name="indexName">Il nome dell'indice attivo, usato per decidere la sorgente della scansione.</param>
    /// <returns>
    /// Una tupla contenente:
    /// - <c>tableId</c>: L'ID della tabella fisica da scansionare (o la tabella principale o la tabella-indice).
    /// - <c>isIndex</c>: Un booleano che è `true` se la scansione avviene su una tabella-indice.
    /// </returns>
    /// <exception cref="InvalidOperationException">Se la tabella principale o l'indice specificato non esistono.</exception>
    private (int tableId, bool isIndex) GetScanSource(int mainTableId, string indexName)
    {
        if (!Manifest.TryGetTable(mainTableId, out var mainTableMeta) || mainTableMeta is null)
            throw new InvalidOperationException($"Tabella con ID {mainTableId} non trovata.");
        if (indexName.Equals(mainTableMeta.Schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
            return (mainTableId, false);

        var indexTableName = QueryOptimizer.GetIndexTableName(mainTableMeta.Name, indexName);
        if (Manifest.TryGetTable(indexTableName, out var indexTableMeta) && indexTableMeta is not null)
            return (indexTableMeta.Id, true);

        throw new InvalidOperationException($"Indice '{indexName}' non trovato per la tabella '{mainTableMeta.Name}'.");
    }

    /// <summary>
    /// Imposta un cursore allo stato "vuoto", invalidando la sua posizione e impostando
    /// sia il flag BOF che EOF a true. Usato quando una navigazione (es. GoTop) non trova record.
    /// </summary>
    /// <param name="cursor">Il cursore da modificare.</param>
    /// <returns>Restituisce sempre False, per coerenza con i metodi di navigazione che indicano un fallimento.</returns>
    private bool MoveToEmpty(TableCursor cursor)
    {
        cursor.ClearPosition();
        cursor.Bof = true;
        cursor.Eof = true;
        return false;
    }

    /// <summary>
    /// Imposta un cursore a uno dei limiti (BOF o EOF) dopo un'operazione di `Skip` fallita.
    /// </summary>
    /// <param name="cursor">Il cursore da modificare.</param>
    /// <param name="direction">La direzione dello `Skip` (+1 per impostare EOF, -1 per impostare BOF).</param>
    /// <returns>Restituisce sempre False, indicando che lo `Skip` non ha trovato un record valido.</returns>
    private bool MoveToBoundary(TableCursor cursor, int direction)
    {
        cursor.ClearPosition();
        if (direction > 0)
        {
            cursor.Eof = true;
            cursor.Bof = false;
        }
        else
        {
            cursor.Bof = true;
            cursor.Eof = false;
        }

        return false;
    }

    /// <summary>
    /// Esegue un task in background a lunga esecuzione che si occupa di eliminare
    /// periodicamente i file SSTable non più referenziati (orfani).
    /// </summary>
    /// <param name="token">Il token di cancellazione per fermare il task in modo pulito.</param>
    private async Task RunGarbageCollectorAsync(CancellationToken token)
    {
        // Esegui in un loop finché non viene richiesta la cancellazione.
        while (!token.IsCancellationRequested)
        {
            try
            {
                // Attendi per un intervallo di tempo prima del prossimo ciclo di pulizia.
                await Task.Delay(TimeSpan.FromMinutes(5), token);

                var filesToDelete = Manifest.GetFilesForGarbageCollection();
                if (filesToDelete.Count == 0) continue;

                var manifestChanged = false;
                foreach (var fileName in filesToDelete)
                {
                    var filePath = Path.Combine(DatabasePath, Config.SstDirectoryName, fileName);
                    if (File.Exists(filePath))
                    {
                        try
                        {
                            // Tenta di eliminare il file.
                            File.Delete(filePath);
                            Manifest.RemoveFileFromGarbageCollection(fileName);
                            manifestChanged = true;
                        }
                        catch (IOException)
                        {
                            // Il file potrebbe essere ancora in uso da un reader o un altro processo.
                            // Verrà riprovato al prossimo ciclo.
                        }
                    }
                    else
                    {
                        // Se il file non esiste già, puliscilo dalla lista.
                        Manifest.RemoveFileFromGarbageCollection(fileName);
                        manifestChanged = true;
                    }
                }

                // Salva il manifest solo se la lista di pulizia è stata modificata.
                if (manifestChanged)
                {
                    Manifest.Save();
                }
            }
            catch (TaskCanceledException)
            {
                // Uscita pulita richiesta da Dispose().
                break;
            }
            catch (Exception ex)
            {
                // Logga l'errore e continua. Il GC non deve mai crashare.
                // TODO: Sostituire con un sistema di logging vero.
                Console.WriteLine($"ERRORE NEL GARBAGE COLLECTOR DEL MOTORE SPEED: {ex.Message}");
            }
        }
    }

    /// <summary>
    /// Attiva il processo di flush di una Memtable su disco quando questa supera la soglia di dimensione.
    /// L'operazione è protetta da un lock globale per sostituire in modo sicuro la Memtable attiva
    /// con una nuova e avviare il task di scrittura in background.
    /// </summary>
    /// <param name="tableId">L'ID della tabella il cui Memtable deve essere "flushato".</param>
    /// <param name="memtableToFlush">L'istanza specifica della Memtable che ha superato la soglia.</param>
    private void TriggerFlush(int tableId, Memtable memtableToFlush)
    {
        lock (_lock)
        {
            if (_activeMemtables.GetValueOrDefault(tableId) != memtableToFlush) return;
            _activeMemtables[tableId] = new Memtable();
            var oldWal = _activeWals[tableId];
            _activeWals.Remove(tableId);
            Task.Run(() => FlushAndCompactAsync(tableId, memtableToFlush, oldWal));
        }
    }

    /// <summary>
    /// Avvia una catena di compattazioni a cascata per una data tabella.
    /// Controlla il Livello 0, e se viene compattato, controlla il Livello 1, e così via.
    /// L'operazione viene eseguita in un task "fire-and-forget" per non bloccare il chiamante.
    /// </summary>
    /// <param name="tableId">L'ID della tabella da controllare.</param>
    private void TriggerCompactionChain(int tableId)
    {
        _ = _compactionTasks.GetOrAdd(tableId, id =>
        {
            return Task.Run(async () =>
            {
                if (Manifest.TryGetTable(id, out var table) && table != null)
                {
                    // Passa 'this' (l'istanza di Database) invece dei singoli parametri.
                    if (table.GetFilesForLevel(0).Count() >= CompactLevel0Threshold)
                    {
                        await Compactor.CompactLevel0Async(this, id, _sstReaderCache);
                    }

                    // Ricarica i metadati della tabella perché la compattazione L0 potrebbe averli cambiati.
                    if (Manifest.TryGetTable(id, out table) && table != null)
                    {
                        var levels = table.SstFilesByLevel.Keys.OrderBy(k => k).ToList();
                        foreach (var level in levels)
                        {
                            // La condizione di compattazione per i livelli superiori deve essere controllata qui.
                            if (level > 0 && table.GetFilesForLevel(level).Count() >= 10) // Usa la soglia corretta
                            {
                                await Compactor.CompactLevelAsync(this, id, level, _sstReaderCache);
                            }
                        }
                    }
                }
            }).ContinueWith(t =>
            {
                if (t.IsFaulted)
                {
                    Console.WriteLine(
                        $"ERRORE CRITICO: La catena di compattazione per la tabella {tableId} è fallita: {t.Exception?.InnerException?.Message}");
                }

                _compactionTasks.TryRemove(tableId, out _);
            });
        });
    }

    /// <summary>
    /// Cerca un record per chiave utilizzando un indice specificato e, se trovato, posiziona
    /// il cursore su di esso.
    /// </summary>
    /// <param name="tableId">L'ID della tabella principale su cui eseguire la ricerca.</param>
    /// <param name="keyToSeek">La rappresentazione in byte della chiave da cercare.</param>
    /// <param name="indexName">Il nome dell'indice da utilizzare. Se nullo o vuoto, viene usata la chiave primaria.</param>
    /// <returns>True se un record viene trovato e il cursore viene posizionato, altrimenti False.</returns>
    public bool Seek(int tableId, byte[] keyToSeek, string? indexName)
    {
        var cursor = GetCursor(tableId);
        var (scanSourceTableId, isIndex) = GetScanSource(tableId, indexName ?? cursor.ActiveIndexName);

        var foundValue = Get(scanSourceTableId, keyToSeek);

        if (foundValue != null)
        {
            byte[] primaryKey;
            byte[] indexKey;

            if (isIndex)
            {
                // La chiave trovata è quella dell'indice, il valore è la chiave primaria.
                indexKey = keyToSeek;
                primaryKey = foundValue;

                // Verifica che il record a cui punta l'indice esista ancora.
                if (Get(tableId, primaryKey) == null)
                {
                    cursor.ClearPosition();
                    return false; // L'indice punta a un record cancellato/inesistente.
                }
            }
            else
            {
                // La chiave trovata è la chiave primaria.
                indexKey = keyToSeek;
                primaryKey = keyToSeek;
            }

            cursor.SetPosition(indexKey, primaryKey);
            return true;
        }

        // Se non viene trovato nulla, invalida la posizione del cursore.
        cursor.ClearPosition();
        return false;
    }

    /// <summary>
    /// Esegue in background l'operazione di flush di una Memtable su un nuovo file SSTable e,
    /// se necessario, avvia un'operazione di compattazione a cascata.
    /// </summary>
    /// <param name="tableId">L'ID della tabella interessata.</param>
    /// <param name="memtableToFlush">La Memtable "congelata" da scrivere su disco.</param>
    /// <param name="walToClose">Il file WAL associato alla Memtable da chiudere e cancellare.</param>
    private void FlushAndCompactAsync(int tableId, Memtable memtableToFlush, WriteAheadLog walToClose)
    {
        var tableLock = _tableLocks.GetOrAdd(tableId, _ => new object());
        lock (tableLock)
        {
            var fileNumber = Manifest.GetNextFileNumber();
            var sstFileName = $"{fileNumber:D6}_{tableId}.sst";
            var sstPath = Path.Combine(DatabasePath, Config.SstDirectoryName, sstFileName);
            SstWriter.WriteFromMemtable(sstPath, memtableToFlush);
            Manifest.AddSstFile(tableId, sstPath);
            Manifest.Save();
            walToClose.Dispose();
            var walPath = walToClose.GetWalPath();
            if (File.Exists(walPath)) File.Delete(walPath);
            TriggerCompactionChain(tableId);
        }
    }

    /// <summary>
    /// Recupera la Memtable attiva per una data tabella, creandola se non esiste.
    /// L'operazione è thread-safe.
    /// </summary>
    /// <param name="tableId">L'ID della tabella.</param>
    /// <returns>L'istanza di Memtable attiva.</returns>
    private Memtable GetOrCreateMemtable(int tableId)
    {
        if (!_activeMemtables.TryGetValue(tableId, out var memtable))
        {
            memtable = new Memtable();
            _activeMemtables[tableId] = memtable;
        }

        return memtable;
    }

    /// <summary>
    /// Recupera il Write-Ahead Log attivo per una data tabella, creandolo se non esiste.
    /// L'operazione è thread-safe e aggiorna il manifest con il nuovo numero di file.
    /// </summary>
    /// <param name="tableId">L'ID della tabella.</param>
    /// <returns>L'istanza di WriteAheadLog attiva.</returns>
    private WriteAheadLog GetOrCreateWal(int tableId)
    {
        if (!_activeWals.TryGetValue(tableId, out var wal))
        {
            wal = new WriteAheadLog(DatabasePath, Manifest.GetNextFileNumber());
            _activeWals[tableId] = wal;
            Manifest.Save();
        }

        return wal;
    }

    #endregion
}