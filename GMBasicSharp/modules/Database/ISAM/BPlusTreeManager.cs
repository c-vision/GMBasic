// FILE: Basic.ISAM.BPlusTreeManager.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Buffers;
using System.Text;
using Basic.ISAM;
using Basic.Databases;

namespace Basic.ISAM;

/// <summary>
/// Gestisce tutte le operazioni per un singolo indice B+Tree, inclusa la navigazione,
/// la modifica, e l'interazione con il disco per la persistenza dei nodi.
/// </summary>
internal class BPlusTreeManager
{
    private readonly IsamFileHandle _fileHandle;
    private readonly Config.IsamSchema _schema;
    private long _rootNodeOffset;
    private readonly Config.IndexDefinition _indexDefinition;

    internal long RootNodeOffset => _rootNodeOffset;

    private const int PageSize = 4096;

    /// <summary>
    /// Numero massimo di chiavi per nodo in base alla lunghezza chiave e alla dimensione pagina.
    /// </summary>
    internal int GetMaxKeysPerNode()
    {
        const int leafHeaderSize = 1 + sizeof(long) + sizeof(long) + sizeof(int);
        var perLeafKey = sizeof(int) + _indexDefinition.KeyLength + sizeof(int) + sizeof(long);
        return Math.Max(2, (PageSize - leafHeaderSize) / perLeafKey);
    }

    /// <summary>
    /// Capacità massima chiavi per nodi interni (include puntatori ai figli).
    /// </summary>
    internal int GetMaxKeysForInternalNode()
    {
        const int internalHeaderSize = 1 + sizeof(int);
        const int childListOverhead = sizeof(int);
        var perKey = sizeof(int) + _indexDefinition.KeyLength;
        const int perChild = sizeof(long);
        var bytesPerKeySlot = perKey + perChild;
        var fixedOverhead = internalHeaderSize + childListOverhead + perChild;
        return Math.Max(2, (PageSize - fixedOverhead) / bytesPerKeySlot);
    }

    // ponytail: LRU con pin su radice durante bulk insert; upgrade path: ref-count pcache SQLite
    private readonly IsamLruCache<long, BPlusNode> _nodeCache = new();

    /// <summary>Read-ahead sequenziale su catena foglie (ISAM-C2): 2 pagine in buffer.</summary>
    private const int ReadAheadLeafSlots = 2;

    private readonly long[] _readAheadLeafOffsets = [-1, -1];

    private readonly BPlusLeafNode?[] _readAheadLeaves = [null, null];

    /// <summary>
    /// Oggetto di sincronizzazione per garantire la thread-safety delle operazioni
    /// che modificano la struttura dell'albero B+Tree (es. Insert, Delete, Validate).
    /// </summary>
    private readonly Lock _syncRoot = new();

    /// <summary>
    /// Flag che determina se la cache dei nodi B+Tree in memoria (LRU) è abilitata.
    /// Viene impostato su <c>false</c> quando la strategia di locking è multi-processo
    /// per prevenire problemi di cache incoerente (stale cache).
    /// </summary>
    private readonly bool _useNodeCache;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="BPlusTreeManager"/>.
    /// </summary>
    /// <param name="fileHandle">L'handle del file ISAM su cui operare.</param>
    /// <param name="schema">Lo schema completo del database.</param>
    /// <param name="indexDefinition">La definizione specifica dell'indice che questo manager gestirà.</param>
    /// <param name="rootNodeOffset">L'offset fisico della pagina radice di questo indice, letto dall'header del file.</param>
    public BPlusTreeManager(IsamFileHandle fileHandle, Config.IsamSchema schema, Config.IndexDefinition indexDefinition,
        long rootNodeOffset)
    {
        _fileHandle = fileHandle;
        _schema = schema;
        _indexDefinition = indexDefinition;
        _rootNodeOffset = rootNodeOffset;

        // Disabilita la cache se siamo in modalità multi-processo
        // per prevenire corruzione da cache incoerente (stale cache).
        _useNodeCache = fileHandle.LockingStrategy == LockingStrategy.IntraProcess;
    }

    #region Public API

    /// <summary>
    /// Avvia una ricerca per una chiave specifica all'interno dell'indice B+Tree,
    /// applicando il locking a livello di chiave per il livello di isolamento `Serializable`.
    /// </summary>
    /// <param name="key">La rappresentazione binaria della chiave da cercare.</param>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se fornito e il livello è `Serializable`,
    /// verrà acquisito un lock di lettura sulla chiave (e sull'intervallo successivo)
    /// per prevenire letture fantasma.
    /// </param>
    /// <returns>
    /// Una lista di <c>long</c> contenente tutti gli offset dei record che corrispondono esattamente alla chiave.
    /// Se la chiave non viene trovata, restituisce una lista vuota.
    /// </returns>
    /// <exception cref="Databases.LockTimeoutException">
    /// Lanciata se, in una transazione `Serializable`, non è possibile acquisire un lock di lettura
    /// sulla chiave, indicando che un'altra transazione la sta modificando.
    /// </exception>
    public List<long> Search(byte[] key, TransactionContext? context)
    {
        using var _readScope = _fileHandle.EnterFileStructureReadScope();
        if (!_useNodeCache)
        {
            _fileHandle.SyncStructuralStateFromDisk();
            RefreshRootOffsetFromHeader();
        }

        // Se in una transazione Serializable, acquisisci un lock di lettura sull'intervallo di chiave.
        if (context is { IsolationLevel: IsolationLevel.Serializable })
        {
            var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
            var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
            _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
            context.ReadLockedKeys.Add(lockKey);
        }

        if (_rootNodeOffset == -1) return new List<long>();
        var rootNode = LoadNode(_rootNodeOffset);
        return rootNode.Search(key, this);
    }

    /// <summary>Primo record con chiave &gt;= <paramref name="key"/> (seek prefisso filtro).</summary>
    public List<long> SearchLowerBound(byte[] key, TransactionContext? context)
    {
        using var _readScope = _fileHandle.EnterFileStructureReadScope();
        if (!_useNodeCache)
        {
            _fileHandle.SyncStructuralStateFromDisk();
            RefreshRootOffsetFromHeader();
        }

        if (context is { IsolationLevel: IsolationLevel.Serializable })
        {
            var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
            var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
            _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
            context.ReadLockedKeys.Add(lockKey);
        }

        if (_rootNodeOffset == -1)
        {
            return [];
        }

        var rootNode = LoadNode(_rootNodeOffset);
        return rootNode.SearchLowerBound(key, this);
    }

    /// <summary>
    /// Avvia l'operazione di inserimento di una nuova coppia chiave/offset nell'indice B+Tree,
    /// gestendo il locking a livello di chiave per supportare il livello di isolamento `Serializable`.
    /// </summary>
    /// <param name="key">La rappresentazione binaria della chiave da inserire.</param>
    /// <param name="recordOffset">L'offset fisico del record dati associato alla chiave.</param>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se `null`, l'operazione viene eseguita in modalità auto-commit.
    /// Se fornito, i lock acquisiti verranno mantenuti fino al termine della transazione.
    /// </param>
    /// <exception cref="LockTimeoutException">
    /// Lanciata se non è possibile acquisire un lock di scrittura sulla chiave, indicando che un'altra
    /// transazione `Serializable` sta proteggendo l'intervallo con un lock di lettura.
    /// </exception>
    /// <remarks>
    /// Prima di qualsiasi modifica all'albero, questo metodo acquisisce un lock di scrittura sulla chiave.
    /// Questo è il meccanismo fondamentale che previene le letture fantasma: un inserimento non può
    /// procedere se la sua chiave ricade in un intervallo protetto da un lock di lettura di un'altra transazione.
    /// Se l'inserimento è parte di una transazione, il lock viene registrato nel contesto e mantenuto;
    /// altrimenti, viene rilasciato immediatamente dopo l'operazione (auto-commit).
    /// </remarks>
    public void Insert(byte[] key, long recordOffset, TransactionContext? context)
    {
        if (!IsValidRecordOffset(recordOffset))
        {
            throw new InvalidOperationException(
                $"Offset record non valido per l'indice '{_indexDefinition.Name}': {recordOffset}.");
        }

        lock (_syncRoot)
        {
            InsertUnderTreeLock(key, recordOffset, context);
        }
    }

    private void InsertUnderTreeLock(byte[] key, long recordOffset, TransactionContext? context)
    {
        if (!_useNodeCache)
        {
            _fileHandle.SyncStructuralStateFromDisk();
            RefreshRootOffsetFromHeader();
        }

        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
        var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
        var keyAlreadyLocked = context?.WriteLockedKeys.Contains(lockKey) == true;
        var skipKeyLock = context == null && _fileHandle.LockingStrategy == LockingStrategy.IntraProcess;
        using (_fileHandle.EnterStructuralLock())
        {
            if (!_useNodeCache)
            {
                RefreshRootOffsetFromHeader();
            }

            if (!skipKeyLock && !keyAlreadyLocked)
            {
                _fileHandle.RecordLockManager.AcquireWriteLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
            }

            try
            {
                if (!skipKeyLock)
                {
                    context?.WriteLockedKeys.Add(lockKey);
                }

                if (_rootNodeOffset == -1)
                {
                    var root = new BPlusLeafNode();
                    root.Insert(key, recordOffset, this, context);
                    _rootNodeOffset = SaveNode(root);
                    UpdateRootOffsetInHeader();
                    return;
                }

                var rootNode = LoadNode(_rootNodeOffset);
                var result = rootNode.Insert(key, recordOffset, this, context);

                if (result.NewNode != null)
                {
                    using var _ = IsamSlowOpLog.Measure("bplus-split", $"index={_indexDefinition.Name} root");
                    if (result.PromotedKey is null)
                        throw new InvalidOperationException("Lo split della radice deve produrre una chiave promosta.");

                    var newSiblingOffset = result.NewNodeOffset;
                    if (newSiblingOffset < 0)
                    {
                        newSiblingOffset = SaveNode(result.NewNode);
                    }

                    var newRoot = new BPlusInternalNode();
                    newRoot.Keys.Add(result.PromotedKey);
                    newRoot.ChildrenOffsets.Add(_rootNodeOffset);
                    newRoot.ChildrenOffsets.Add(newSiblingOffset);
                    _rootNodeOffset = SaveNode(newRoot);
                    UpdateRootOffsetInHeader();
                }
                else if (result.NodeUpdated)
                {
                    SaveNode(rootNode);
                }
            }
            finally
            {
                if (context == null && !keyAlreadyLocked && !skipKeyLock)
                {
                    _fileHandle.RecordLockManager.ReleaseWriteLock(lockKey);
                }
            }
        }
    }

    /// <summary>
    /// Inserimento bulk ordinato: riempie foglie in RAM, un SaveNode per pagina piena (split) + flush finale.
    /// ponytail: richiede chiavi strettamente crescenti e ≥ ultima chiave esistente; no key-lock per batch IntraProcess.
    /// </summary>
    internal void InsertManyAppendOrdered(ReadOnlySpan<(byte[] Key, long RecordOffset)> entries, TransactionContext? context)
    {
        if (entries.Length == 0)
        {
            return;
        }

        for (var i = 1; i < entries.Length; i++)
        {
            if (new ByteArrayComparer().Compare(entries[i - 1].Key, entries[i].Key) >= 0)
            {
                throw new InvalidOperationException(
                    $"InsertManyAppendOrdered: chiavi non strettamente crescenti a indice {i} per indice '{_indexDefinition.Name}'.");
            }
        }

        lock (_syncRoot)
        {
            if (!_useNodeCache)
            {
                _fileHandle.SyncStructuralStateFromDisk();
                RefreshRootOffsetFromHeader();
            }

            using (_fileHandle.EnterStructuralLock())
            {
                if (!_useNodeCache)
                {
                    RefreshRootOffsetFromHeader();
                }

                var pinnedRoot = _rootNodeOffset;
                if (pinnedRoot > 0 && _useNodeCache)
                {
                    _nodeCache.Pin(pinnedRoot);
                }

                try
                {
                    BPlusLeafNode leaf;
                    var dirty = false;

                    if (_rootNodeOffset == -1)
                    {
                        leaf = new BPlusLeafNode();
                    }
                    else
                    {
                        leaf = FindLastLeaf();
                        if (leaf.KeyCount > 0
                            && new ByteArrayComparer().Compare(leaf.Keys[^1], entries[0].Key) >= 0)
                        {
                            throw new InvalidOperationException(
                                $"InsertManyAppendOrdered: prima chiave batch deve essere > ultima chiave indice '{_indexDefinition.Name}'.");
                        }
                    }

                    foreach (var (key, recordOffset) in entries)
                    {
                        if (!IsValidRecordOffset(recordOffset))
                        {
                            throw new InvalidOperationException(
                                $"Offset record non valido per l'indice '{_indexDefinition.Name}': {recordOffset}.");
                        }

                        if (!leaf.TryAppendMonotonic(key, recordOffset, this, out var split))
                        {
                            throw new InvalidOperationException(
                                $"InsertManyAppendOrdered: chiave non monotona in foglia per indice '{_indexDefinition.Name}'.");
                        }

                        if (split == null)
                        {
                            dirty = true;
                            continue;
                        }

                        if (_rootNodeOffset == -1)
                        {
                            PromoteLeafSplitAsRoot(leaf, split);
                        }
                        else
                        {
                            PromoteLeafSplit(leaf, split);
                        }

                        leaf = (BPlusLeafNode)split.NewNode!;
                        dirty = false;
                    }

                    if (_rootNodeOffset == -1)
                    {
                        _rootNodeOffset = SaveNode(leaf);
                        UpdateRootOffsetInHeader();
                    }
                    else if (dirty)
                    {
                        SaveNode(leaf);
                    }
                }
                finally
                {
                    if (pinnedRoot > 0 && _useNodeCache)
                    {
                        _nodeCache.Unpin(pinnedRoot);
                    }
                }
            }
        }
    }

    private void PromoteLeafSplitAsRoot(BPlusNode leftChild, BPlusInsertResult split)
    {
        var newRoot = new BPlusInternalNode();
        newRoot.Keys.Add(split.PromotedKey!);
        newRoot.ChildrenOffsets.Add(leftChild.NodeOffset);
        newRoot.ChildrenOffsets.Add(split.NewNodeOffset);
        _rootNodeOffset = SaveNode(newRoot);
        UpdateRootOffsetInHeader();
    }

    private void PromoteLeafSplit(BPlusLeafNode leftLeaf, BPlusInsertResult split)
    {
        if (_rootNodeOffset == leftLeaf.NodeOffset)
        {
            PromoteLeafSplitAsRoot(leftLeaf, split);
            return;
        }

        var parent = FindParentInternalNode(leftLeaf.NodeOffset)
                     ?? throw new InvalidOperationException(
                         $"Padre B+ non trovato per foglia a offset {leftLeaf.NodeOffset}.");
        InsertPromotedKeyIntoInternal(parent, leftLeaf.NodeOffset, split);
    }

    private void InsertPromotedKeyIntoInternal(
        BPlusInternalNode parent,
        long leftChildOffset,
        BPlusInsertResult childSplit)
    {
        while (true)
        {
            var childIndex = parent.ChildrenOffsets.IndexOf(leftChildOffset);
            if (childIndex < 0)
            {
                throw new InvalidOperationException("Figlio non trovato nel nodo interno padre B+.");
            }

            parent.Keys.Insert(childIndex, childSplit.PromotedKey!);
            parent.ChildrenOffsets.Insert(childIndex + 1, childSplit.NewNodeOffset);

            if (parent.KeyCount <= GetMaxKeysForInternalNode())
            {
                SaveNode(parent);
                return;
            }

            var sibling = new BPlusInternalNode();
            var splitIndex = BPlusFillFactor.ComputeInternalSplitIndex(parent.KeyCount);
            var promotedKey = parent.Keys[splitIndex];

            sibling.Keys.AddRange(parent.Keys.Skip(splitIndex + 1));
            sibling.ChildrenOffsets.AddRange(parent.ChildrenOffsets.Skip(splitIndex + 1));
            parent.Keys.RemoveRange(splitIndex, parent.KeyCount - splitIndex);
            parent.ChildrenOffsets.RemoveRange(splitIndex + 1, parent.ChildrenOffsets.Count - (splitIndex + 1));

            SaveNode(parent);
            var siblingOffset = SaveNode(sibling);

            if (parent.NodeOffset == _rootNodeOffset)
            {
                PromoteLeafSplitAsRoot(parent, new BPlusInsertResult
                {
                    NodeUpdated = true,
                    NewNode = sibling,
                    NewNodeOffset = siblingOffset,
                    PromotedKey = promotedKey,
                });
                return;
            }

            leftChildOffset = parent.NodeOffset;
            childSplit = new BPlusInsertResult
            {
                NodeUpdated = true,
                NewNode = sibling,
                NewNodeOffset = siblingOffset,
                PromotedKey = promotedKey,
            };

            parent = FindParentInternalNode(leftChildOffset)
                     ?? throw new InvalidOperationException(
                         "Padre nodo interno B+ non trovato durante propagazione split.");
        }
    }

    private BPlusInternalNode? FindParentInternalNode(long childOffset)
    {
        if (_rootNodeOffset == -1 || _rootNodeOffset == childOffset)
        {
            return null;
        }

        var pending = new Stack<long>();
        pending.Push(_rootNodeOffset);

        while (pending.Count > 0)
        {
            var nodeOffset = pending.Pop();
            var node = LoadNode(nodeOffset);
            if (node.IsLeaf)
            {
                continue;
            }

            var internalNode = (BPlusInternalNode)node;
            if (internalNode.ChildrenOffsets.Contains(childOffset))
            {
                return internalNode;
            }

            foreach (var childOff in internalNode.ChildrenOffsets)
            {
                pending.Push(childOff);
            }
        }

        return null;
    }

    /// <summary>
    /// Recupera l'offset fisico del primo record nella sequenza logica dell'indice,
    /// acquisendo un lock di lettura sulla sua chiave se in una transazione `Serializable`.
    /// </summary>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se `Serializable`, viene posto un lock di lettura
    /// sulla prima chiave dell'indice.
    /// </param>
    /// <returns>L'offset fisico del primo record attivo, o -1 se l'indice è vuoto.</returns>
    public long GetFirstRecordOffset(TransactionContext? context)
    {
        if (_rootNodeOffset == -1) return -1;

        using var _readScope = _fileHandle.EnterFileStructureReadScope();
        var leaf = FindFirstLeaf();
        while (true)
        {
            for (var keyIndex = 0; keyIndex < leaf.KeyCount; keyIndex++)
            {
                foreach (var offset in leaf.RecordOffsets[keyIndex])
                {
                    if (!IsActiveRecord(offset))
                    {
                        continue;
                    }

                    AcquireSerializableReadLockForKey(context, leaf.Keys[keyIndex]);
                    return offset;
                }
            }

            if (leaf.NextLeafOffset == -1)
            {
                return -1;
            }

            leaf = LoadLeafNode(leaf.NextLeafOffset);
        }
    }

    /// <summary>
    /// Recupera l'offset fisico dell'ultimo record nella sequenza logica dell'indice,
    /// acquisendo un lock di lettura sulla sua chiave se in una transazione `Serializable`.
    /// </summary>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se `Serializable`, viene posto un lock di lettura
    /// sull'ultima chiave dell'indice.
    /// </param>
    /// <returns>L'offset fisico dell'ultimo record attivo, o -1 se l'indice è vuoto.</returns>
    public long GetLastRecordOffset(TransactionContext? context)
    {
        if (_rootNodeOffset == -1) return -1;

        using var _readScope = _fileHandle.EnterFileStructureReadScope();
        var leaf = FindLastLeaf();
        while (true)
        {
            for (var keyIndex = leaf.KeyCount - 1; keyIndex >= 0; keyIndex--)
            {
                var offsets = leaf.RecordOffsets[keyIndex];
                for (var offsetIndex = offsets.Count - 1; offsetIndex >= 0; offsetIndex--)
                {
                    var offset = offsets[offsetIndex];
                    if (!IsActiveRecord(offset))
                    {
                        continue;
                    }

                    AcquireSerializableReadLockForKey(context, leaf.Keys[keyIndex]);
                    return offset;
                }
            }

            leaf = FindPreviousLeaf(leaf);
            if (leaf is null)
            {
                return -1;
            }
        }
    }

    /// <summary>
    /// Calcola l'offset di un record a una distanza specificata, seguendo l'ordine dell'indice
    /// e acquisendo lock di lettura sulle chiavi attraversate in una transazione `Serializable`.
    /// </summary>
    /// <param name="currentRecordOffset">L'offset del record di partenza.</param>
    /// <param name="skipCount">Il numero di record logici da saltare.</param>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se `Serializable`, vengono acquisiti lock di lettura
    /// su ogni chiave attraversata durante la navigazione.
    /// </param>
    /// <param name="evaluator">Il callback necessario per calcolare le chiavi dei record durante la navigazione inversa.</param>
    /// <returns>L'offset del record di destinazione, o -1 se si supera l'inizio o la fine dell'indice.</returns>
    public long FindNextRecordOffset(long currentRecordOffset, int skipCount, TransactionContext? context,
        KeyExpressionEvaluatorCallback evaluator)
    {
        using var _readScope = _fileHandle.EnterFileStructureReadScope();
        var (currentLeaf, keyIndex, offsetIndex) = FindRecordPosition(currentRecordOffset, evaluator);
        if (currentLeaf is null) return -1;

        for (var i = 0; i < Math.Abs(skipCount); i++)
        {
            var previousKeyIndex = keyIndex;

            if (skipCount > 0)
            {
                offsetIndex++;
                if (offsetIndex >= currentLeaf.RecordOffsets[keyIndex].Count)
                {
                    keyIndex++;
                    offsetIndex = 0;
                    if (keyIndex >= currentLeaf.KeyCount)
                    {
                        if (currentLeaf.NextLeafOffset == -1) return -1; // EOF
                        currentLeaf = LoadLeafNode(currentLeaf.NextLeafOffset);
                        keyIndex = 0;
                    }
                }
            }
            else // skipCount < 0
            {
                offsetIndex--;
                if (offsetIndex < 0)
                {
                    keyIndex--;
                    if (keyIndex < 0)
                    {
                        currentLeaf = FindPreviousLeaf(currentLeaf);
                        if (currentLeaf is null) return -1; // BOF
                        keyIndex = currentLeaf.KeyCount - 1;
                    }

                    offsetIndex = currentLeaf.RecordOffsets[keyIndex].Count - 1;
                }
            }

            if (keyIndex != previousKeyIndex && context is { IsolationLevel: IsolationLevel.Serializable })
            {
                var newKey = currentLeaf.Keys[keyIndex];
                var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, newKey);
                var description = $"chiave di indice '{Encoding.UTF8.GetString(newKey)}' durante la scansione";

                _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
                context.ReadLockedKeys.Add(lockKey);
            }
        }

        var candidate = currentLeaf.RecordOffsets[keyIndex][offsetIndex];
        var direction = Math.Sign(skipCount);
        var guard = 0;
        while (candidate != -1 && !IsActiveRecord(candidate) && guard++ < 1_000_000)
        {
            var advanced = FindNextRecordOffset(candidate, direction, context, evaluator);
            if (advanced == -1 || advanced == candidate)
            {
                return advanced;
            }

            candidate = advanced;
        }

        return candidate;
    }

    private bool IsActiveRecord(long recordOffset)
    {
        if (recordOffset < 0)
        {
            return false;
        }

        var metadata = _fileHandle.ReadRecordMetadata(recordOffset, _fileHandle.FileEncoding);
        return metadata.Status == Config.RecordStatusActive;
    }

    private void AcquireSerializableReadLockForKey(TransactionContext? context, byte[] key)
    {
        if (context is not { IsolationLevel: IsolationLevel.Serializable })
        {
            return;
        }

        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
        var description = $"chiave di indice '{_indexDefinition.Name}'";
        _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
        context.ReadLockedKeys.Add(lockKey);
    }

    private BPlusLeafNode FindLastLeaf()
    {
        var node = LoadNode(_rootNodeOffset);
        while (!node.IsLeaf)
        {
            var internalNode = (BPlusInternalNode)node;
            node = LoadNode(internalNode.ChildrenOffsets[^1]);
        }

        return (BPlusLeafNode)node;
    }

    /// <summary>
    /// Avvia la rimozione di una specifica coppia chiave/record offset dall'albero B+, gestendo il locking
    /// a livello di chiave per il livello di isolamento `Serializable` e il potenziale restringimento dell'albero.
    /// </summary>
    /// <param name="key">La rappresentazione binaria della chiave dell'indice da rimuovere.</param>
    /// <param name="recordOffset">L'offset fisico specifico del record da rimuovere.</param>
    /// <param name="context">
    /// Il contesto della transazione attiva. Se `null`, l'operazione è in modalità auto-commit.
    /// Se fornito, il lock di scrittura sulla chiave verrà mantenuto fino al termine della transazione.
    /// </param>
    /// <exception cref="Databases.LockTimeoutException">
    /// Lanciata se non è possibile acquisire un lock di scrittura sulla chiave, indicando che un'altra
    /// transazione sta leggendo il record o l'intervallo corrispondente.
    /// </exception>
    /// <remarks>
    /// Prima di modificare l'albero, questo metodo acquisisce un lock di scrittura sulla chiave da rimuovere.
    /// Questo previene conflitti con transazioni `Serializable` che potrebbero aver posto un lock di lettura
    /// sul record o sull'intervallo. Se la cancellazione fa sì che il nodo radice abbia un solo figlio,
    /// quest'ultimo viene promosso a nuova radice, riducendo l'altezza dell'albero.
    /// </remarks>
    public void Delete(byte[] key, long recordOffset, TransactionContext? context)
    {
        if (_rootNodeOffset == -1) return;

        // Acquisisce un lock di scrittura sulla chiave per proteggere l'operazione.
        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
        var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
        var keyAlreadyLocked = context?.WriteLockedKeys.Contains(lockKey) == true;
        using (_fileHandle.EnterStructuralLock())
        {
            if (!keyAlreadyLocked)
            {
                _fileHandle.RecordLockManager.AcquireWriteLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
            }

            try
            {
                if (context != null)
                {
                    context.WriteLockedKeys.Add(lockKey);
                }

                DeleteRecursive(_rootNodeOffset, null, -1, key, recordOffset);

                var rootPage = LoadNode(_rootNodeOffset);
                if (rootPage is { IsLeaf: false, KeyCount: 0 })
                {
                    var oldRootOffset = _rootNodeOffset;
                    _rootNodeOffset = ((BPlusInternalNode)rootPage).ChildrenOffsets.First();
                    UpdateRootOffsetInHeader();
                    _fileHandle.AddPageToFreeList(oldRootOffset);
                }
            }
            finally
            {
                if (context == null && !keyAlreadyLocked)
                {
                    _fileHandle.RecordLockManager.ReleaseWriteLock(lockKey);
                }
            }
        }
    }

    /// <summary>
    /// Restituisce la definizione completa (<see cref="Config.IndexDefinition"/>) dell'indice gestito da questo manager.
    /// </summary>
    /// <returns>La definizione dell'indice.</returns>
    public Config.IndexDefinition GetIndexDefinition()
    {
        return _indexDefinition;
    }

    /// <summary>
    /// Reimposta l'offset della radice del manager, tipicamente dopo uno svuotamento.
    /// </summary>
    public void ResetRoot(long newRootOffset)
    {
        _rootNodeOffset = newRootOffset;
        _nodeCache.Clear(); // Svuota la cache poiché i vecchi nodi non sono più validi.
        ClearReadAheadLeaves();
    }

    /// <summary>
    /// Restituisce l'offset corrente del nodo radice dell'albero.
    /// </summary>
    public long GetRootOffset()
    {
        return _rootNodeOffset;
    }

    #endregion

    #region Node I/O

    /// <summary>
    /// Retrieves a B+Tree node from a specified physical offset, utilizing an in-memory LRU cache to minimize disk I/O
    /// only if the locking strategy is Intra-Process.
    /// </summary>
    /// <param name="offset">The physical byte offset within the ISAM file where the node's data begins.</param>
    /// <returns>
    /// A fully deserialized <see cref="BPlusNode"/> instance (either a <see cref="BPlusLeafNode"/> or
    /// a <see cref="BPlusInternalNode"/>) ready for use.
    /// </returns>
    /// <remarks>
    /// This method is the central point for node retrieval. It implements a cache-aside pattern:
    /// 1. If caching is enabled, it first attempts to retrieve the node from the LRU cache.
    /// 2. On a cache miss or if caching is disabled, it performs a disk seek to the specified offset.
    /// 3. It reads a boolean flag to determine the node type (leaf or internal) and deserializes it.
    /// 4. If caching is enabled, the new node instance is added to the cache.
    /// </remarks>
    public BPlusNode LoadNode(long offset)
    {
        if (offset < 0)
        {
            throw new IOException(
                $"Offset nodo B+ non valido ({offset}). Probabile split radice interna senza salvataggio del fratello.");
        }

        if (_useNodeCache && _nodeCache.TryGetValue(offset, out var cachedNode))
        {
            ConsumeReadAheadSlot(offset);
            return cachedNode!;
        }

        if (TryConsumeReadAheadLeaf(offset, out var aheadLeaf))
        {
            if (_useNodeCache)
            {
                _nodeCache.AddOrUpdate(offset, aheadLeaf);
            }

            QueueLeafReadAhead(aheadLeaf.NextLeafOffset);
            return aheadLeaf;
        }

        var node = LoadNodeFromDisk(offset);
        if (_useNodeCache)
        {
            _nodeCache.AddOrUpdate(offset, node);
        }

        return node;
    }

    /// <summary>
    /// Carica un nodo foglia con read-ahead sulla pagina successiva della catena (SKIP/G0 forward).
    /// </summary>
    private BPlusLeafNode LoadLeafNode(long offset)
    {
        var node = LoadNode(offset);
        if (node is not BPlusLeafNode leaf)
        {
            throw new IOException($"Offset {offset} non è una foglia B+.");
        }

        QueueLeafReadAhead(leaf.NextLeafOffset);
        return leaf;
    }

    private BPlusNode LoadNodeFromDisk(long offset)
    {
        var pageBuffer = ReadFullPageBytes(offset, _fileHandle.GetStream().Length);
        if (!IsamPageChecksum.TryValidate(pageBuffer, out var storedCrc) && storedCrc != 0)
        {
            throw new IOException(
                $"Checksum pagina B+ non valido a offset {offset} (CRC 0x{storedCrc:X8}).");
        }

        var leafFlagValue = pageBuffer[0];
        BPlusNode node = leafFlagValue switch
        {
            1 => new BPlusLeafNode(),
            0 => new BPlusInternalNode(),
            _ => throw new IOException($"Nodo B+ corrotto a offset {offset}: flag iniziale 0x{leafFlagValue:X2}.")
        };
        node.NodeOffset = offset;

        using var nodeStream = new MemoryStream(pageBuffer, writable: false);
        using var reader = new BinaryReader(nodeStream, _fileHandle.FileEncoding, leaveOpen: true);
        node.Deserialize(reader);
        return node;
    }

    /// <summary>
    /// Precarica fino a 2 foglie della catena sotto <see cref="IsamFileHandle.RunUnderPhysicalIoLock"/>.
    /// </summary>
    private void QueueLeafReadAhead(long nextLeafOffset)
    {
        if (!_useNodeCache || nextLeafOffset < 0)
        {
            return;
        }

        _fileHandle.RunUnderPhysicalIoLock(() =>
        {
            var offset = nextLeafOffset;
            for (var slot = 0; slot < ReadAheadLeafSlots && offset >= 0; slot++)
            {
                if (_nodeCache.TryGetValue(offset, out var existing) && existing is BPlusLeafNode existingLeaf)
                {
                    offset = existingLeaf.NextLeafOffset;
                    continue;
                }

                if (IsReadAheadQueued(offset))
                {
                    offset = GetReadAheadLeaf(offset)?.NextLeafOffset ?? -1;
                    continue;
                }

                var node = LoadNodeFromDisk(offset);
                if (node is not BPlusLeafNode aheadLeaf)
                {
                    return;
                }

                _readAheadLeafOffsets[slot] = offset;
                _readAheadLeaves[slot] = aheadLeaf;
                _nodeCache.AddOrUpdate(offset, aheadLeaf);
                offset = aheadLeaf.NextLeafOffset;
            }
        });
    }

    private BPlusLeafNode? GetReadAheadLeaf(long offset)
    {
        for (var i = 0; i < ReadAheadLeafSlots; i++)
        {
            if (_readAheadLeafOffsets[i] == offset)
            {
                return _readAheadLeaves[i];
            }
        }

        return null;
    }

    private void ClearReadAheadLeaves()
    {
        for (var i = 0; i < ReadAheadLeafSlots; i++)
        {
            _readAheadLeafOffsets[i] = -1;
            _readAheadLeaves[i] = null;
        }
    }

    private void ConsumeReadAheadSlot(long offset)
    {
        for (var i = 0; i < ReadAheadLeafSlots; i++)
        {
            if (_readAheadLeafOffsets[i] != offset)
            {
                continue;
            }

            _readAheadLeafOffsets[i] = -1;
            _readAheadLeaves[i] = null;
        }
    }

    private bool TryConsumeReadAheadLeaf(long offset, out BPlusLeafNode leaf)
    {
        for (var i = 0; i < ReadAheadLeafSlots; i++)
        {
            if (_readAheadLeafOffsets[i] != offset || _readAheadLeaves[i] is not { } ahead)
            {
                continue;
            }

            _readAheadLeafOffsets[i] = -1;
            _readAheadLeaves[i] = null;
            leaf = ahead;
            return true;
        }

        leaf = null!;
        return false;
    }

    private bool IsReadAheadQueued(long offset)
    {
        for (var i = 0; i < ReadAheadLeafSlots; i++)
        {
            if (_readAheadLeafOffsets[i] == offset)
            {
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Allocates space for a new B+Tree node, implementing an efficient space management strategy.
    /// It prioritizes recycling pages from a managed free list before resorting to file extension.
    /// </summary>
    /// <returns>The physical byte offset of the newly allocated page within the ISAM file.</returns>
    /// <remarks>
    /// This method orchestrates the two primary space allocation mechanisms:
    /// 1.  **Free List Reuse:** It first checks the file header's <c>FreeListHeadOffset</c>. If this pointer is valid
    ///     (i.e., not -1), it indicates that there is at least one page available for reuse. The method then:
    ///     a. Retrieves the offset of the first page from the header.
    ///     b. Reads the first 8 bytes from this freed page, which now contains a pointer to the *next* page in the
    ///        singly-linked free list.
    ///     c. Updates the header to point to this next page, effectively "unlinking" the reclaimed page from the list.
    ///     d. Removes the reclaimed page's offset from the node cache to prevent stale reads.
    ///
    /// 2.  **File Extension:** If the free list is empty, the method allocates new space at the end of the file. It:
    ///     a. Reads the current high-water mark from the header's <c>NextAvailablePageOffset</c>.
    ///     b. Returns this offset for the new page's location.
    ///     c. Increments the header's <c>NextAvailablePageOffset</c> by a standard page size (4096 bytes) to
    ///        prepare for the next allocation.
    ///
    /// This strategy minimizes file growth and fragmentation by recycling space wherever possible.
    /// </remarks>
    private long AllocateNewPage()
    {
        using (_fileHandle.EnterStructuralLock())
        {
            var header = _fileHandle.Header;

            if (header.PageFreeListHeadOffset != -1)
            {
                var reclaimedPageOffset = header.PageFreeListHeadOffset;

                Span<byte> nextPtr = stackalloc byte[sizeof(long)];
                _fileHandle.ReadBytesAt(reclaimedPageOffset, nextPtr);
                var nextFreePageOffset = BitConverter.ToInt64(nextPtr);
                if (nextFreePageOffset <= 0)
                {
                    nextFreePageOffset = -1;
                }

                var newHeader = header;
                newHeader.PageFreeListHeadOffset = nextFreePageOffset;
                _fileHandle.UpdateHeader(newHeader);

                _nodeCache.Remove(reclaimedPageOffset);
                ConsumeReadAheadSlot(reclaimedPageOffset);

                ZeroPage(reclaimedPageOffset);
                return reclaimedPageOffset;
            }

            const int pageSize = 4096;
            return _fileHandle.ReserveHeapSpace(pageSize);
        }
    }

    private void ZeroPage(long pageOffset) => _fileHandle.ZeroIndexPage(pageOffset);

    /// <summary>
    /// Salva un nodo su disco. Se è un nuovo nodo, alloca nuovo spazio.
    /// Altrimenti, sovrascrive il nodo esistente.
    /// </summary>
    /// <returns>L'offset su disco dove il nodo è stato salvato.</returns>
    public long SaveNode(BPlusNode node)
    {
        var offset = node.NodeOffset;

        if (offset <= 0)
        {
            offset = AllocateNewPage();
            node.NodeOffset = offset;
        }

        // ponytail: ArrayPool per pagina; WAL/direct path copia ancora ToArray() in FileHandle
        var pageBuffer = ArrayPool<byte>.Shared.Rent(PageSize);
        try
        {
            pageBuffer.AsSpan(0, PageSize).Clear();
            using (var ms = new MemoryStream(pageBuffer, 0, PageSize, writable: true, publiclyVisible: true))
            using (var writer = new BinaryWriter(ms, _fileHandle.FileEncoding, leaveOpen: true))
            {
                ms.SetLength(0);
                node.Serialize(writer);
                var serializedLength = (int)ms.Position;
                if (serializedLength > PageSize)
                {
                    throw new InvalidOperationException(
                        $"Nodo B+ serializzato ({serializedLength} byte) supera la dimensione pagina ({PageSize}).");
                }
            }

            IsamPageChecksum.WriteChecksum(pageBuffer.AsSpan(0, PageSize));
            _fileHandle.WriteIndexPage(offset, pageBuffer.AsSpan(0, PageSize));
            _nodeCache.AddOrUpdate(offset, node);
            ConsumeReadAheadSlot(offset);
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(pageBuffer);
        }

        return offset;
    }

    /// <summary>
    /// Verifica i checksum di tutte le pagine dell'indice. Pagine legacy (CRC zero) sono accettate.
    /// </summary>
    internal bool ValidatePageChecksums(out string? errorMessage)
    {
        errorMessage = null;
        if (_rootNodeOffset == -1)
        {
            return true;
        }

        try
        {
            return ValidatePageChecksumsRecursive(_rootNodeOffset, new HashSet<long>());
        }
        catch (IOException ex)
        {
            errorMessage = ex.Message;
            return false;
        }
    }

    private bool ValidatePageChecksumsRecursive(long pageOffset, HashSet<long> visited)
    {
        if (!visited.Add(pageOffset))
        {
            return true;
        }

        var page = ReadFullPageBytes(pageOffset, _fileHandle.GetStream().Length);
        if (!IsamPageChecksum.TryValidate(page, out var storedCrc) && storedCrc != 0)
        {
            throw new IOException(
                $"Checksum pagina B+ non valido a offset {pageOffset} (CRC 0x{storedCrc:X8}).");
        }

        var flag = page[0];
        if (flag == 0)
        {
            using var ms = new MemoryStream(page, writable: false);
            using var reader = new BinaryReader(ms, _fileHandle.FileEncoding, leaveOpen: true);
            reader.ReadBoolean();
            var keyCount = reader.ReadInt32();
            for (var i = 0; i < keyCount; i++)
            {
                var keyLength = reader.ReadInt32();
                reader.ReadBytes(keyLength);
            }

            var childCount = reader.ReadInt32();
            for (var i = 0; i < childCount; i++)
            {
                var child = reader.ReadInt64();
                if (child > 0)
                {
                    ValidatePageChecksumsRecursive(child, visited);
                }
            }
        }

        return true;
    }

    private byte[] ReadFullPageBytes(long offset, long fileLength) =>
        _fileHandle.ReadPageBytes(offset, fileLength);

    /// <summary>Media fill factor foglie (debug / test ISAM-C6).</summary>
    internal double MeasureAverageLeafFillFactor()
    {
        if (_rootNodeOffset == -1)
        {
            return 0;
        }

        var maxKeys = GetMaxKeysPerNode();
        if (maxKeys <= 0)
        {
            return 0;
        }

        var firstLeaf = FindFirstLeaf();
        var totalKeys = 0;
        var leafCount = 0;
        var current = firstLeaf;
        while (true)
        {
            totalKeys += current.KeyCount;
            leafCount++;
            if (current.NextLeafOffset == -1)
            {
                break;
            }

            current = (BPlusLeafNode)LoadNode(current.NextLeafOffset);
        }

        return leafCount == 0 ? 0 : (double)totalKeys / (leafCount * maxKeys);
    }

    #endregion

    #region Maintenance

    /// <summary>
    /// Aggiorna i puntatori interni alle pagine dopo lo spostamento dell'area heap su disco.
    /// </summary>
    internal void RepairPointersAfterHeapMove(long heapStart, long heapEnd, long delta)
    {
        _nodeCache.Clear();
        ClearReadAheadLeaves();
        if (_rootNodeOffset == -1)
        {
            return;
        }

        var visited = new HashSet<long>();
        var queue = new Queue<long>();
        queue.Enqueue(_rootNodeOffset);

        while (queue.Count > 0)
        {
            var pageOffset = queue.Dequeue();
            if (!visited.Add(pageOffset))
            {
                continue;
            }

            var node = LoadNode(pageOffset);
            node.NodeOffset = pageOffset;

            switch (node)
            {
                case BPlusInternalNode internalNode:
                    for (var i = 0; i < internalNode.ChildrenOffsets.Count; i++)
                    {
                        var child = internalNode.ChildrenOffsets[i];
                        if (child >= heapStart && child < heapEnd)
                        {
                            internalNode.ChildrenOffsets[i] = child + delta;
                        }

                        if (internalNode.ChildrenOffsets[i] >= heapStart)
                        {
                            queue.Enqueue(internalNode.ChildrenOffsets[i]);
                        }
                    }

                    break;
                case BPlusLeafNode leaf:
                    if (leaf.NextLeafOffset >= heapStart && leaf.NextLeafOffset < heapEnd)
                    {
                        leaf.NextLeafOffset += delta;
                    }

                    if (leaf.PrevLeafOffset >= heapStart && leaf.PrevLeafOffset < heapEnd)
                    {
                        leaf.PrevLeafOffset += delta;
                    }

                    // Ogni voce di RecordOffsets punta al byte fisico del record dati a cui la chiave
                    // si riferisce: se il record è stato spostato dallo shift (perché anch'esso nella
                    // regione [heapStart, heapEnd)), il puntatore va corretto qui, altrimenti la foglia
                    // continua a puntare alla vecchia posizione (ora sovrascritta o disallineata),
                    // producendo record fantasma/corrotti in lettura (bug scoperto testando
                    // DbIsamFullTests.bas: CREATE+DROP INDEX seguiti da PACK/REINDEX su tabella popolata).
                    foreach (var offsets in leaf.RecordOffsets)
                    {
                        for (var i = 0; i < offsets.Count; i++)
                        {
                            if (offsets[i] >= heapStart && offsets[i] < heapEnd)
                            {
                                offsets[i] += delta;
                            }
                        }
                    }

                    if (leaf.NextLeafOffset >= heapStart)
                    {
                        queue.Enqueue(leaf.NextLeafOffset);
                    }

                    break;
            }

            SaveNode(node);
        }
    }

    /// <summary>
    /// Esegue una serie di controlli per verificare l'integrità strutturale dell'indice B+Tree.
    /// </summary>
    /// <returns>True se l'indice è strutturalmente valido, altrimenti False.</returns>
    /// <remarks>
    /// Questa operazione è non distruttiva e a sola lettura. Esegue una traversata completa
    /// dell'albero per validare gli invarianti, come il numero di chiavi per nodo, la coerenza
    /// dei puntatori e l'assenza di cicli.
    /// </remarks>
    public bool ValidateIndex()
    {
        lock (_syncRoot)
        {
            try
            {
                if (_rootNodeOffset == -1) return true;

                if (!ValidatePageRecursive(_rootNodeOffset, new HashSet<long>()))
                    return false;

                if (!CheckLeafListConsistency())
                    return false;

                return true;
            }
            catch
            {
                return false;
            }
        }
    }

    /// <summary>
    /// Metodo helper ricorsivo per validare una pagina e tutti i suoi discendenti.
    /// </summary>
    private bool ValidatePageRecursive(long pageOffset, HashSet<long> visitedPages)
    {
        // Rileva cicli: se abbiamo già visitato questa pagina, l'indice è corrotto.
        if (!visitedPages.Add(pageOffset)) return false;

        var page = LoadNode(pageOffset);

        // Controlla che il numero di chiavi sia entro i limiti validi.
        var maxKeys = page is BPlusInternalNode
            ? GetMaxKeysForInternalNode()
            : GetMaxKeysPerNode();
        if (page.KeyCount > maxKeys)
        {
            return false;
        }

        // La radice è l'unica pagina che può avere meno del numero minimo di chiavi.
        if (pageOffset != _rootNodeOffset)
        {
            var minKeys = page is BPlusInternalNode
                ? GetMinKeysForInternalNode()
                : GetMinKeysPerNode();
            if (page.KeyCount < minKeys)
            {
                return false;
            }
        }

        if (page is BPlusInternalNode internalNode)
        {
            // Se è un nodo interno, valida ricorsivamente tutti i suoi figli.
            foreach (var childOffset in internalNode.ChildrenOffsets)
            {
                if (childOffset <= 0 || !ValidatePageRecursive(childOffset, visitedPages))
                    return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Verifica la coerenza della lista doppiamente concatenata dei nodi foglia.
    /// </summary>
    private bool CheckLeafListConsistency()
    {
        if (_rootNodeOffset == -1) return true;

        var firstLeaf = FindFirstLeaf();
        var current = firstLeaf;
        long prevOffset = -1;

        // Scorre in avanti
        while (current.NextLeafOffset != -1)
        {
            if (current.PrevLeafOffset != prevOffset) return false; // Il puntatore all'indietro non corrisponde

            prevOffset = current.NodeOffset;
            var nextNode = LoadNode(current.NextLeafOffset);
            if (nextNode is not BPlusLeafNode node) return false; // Un nodo nella lista delle foglie non è una foglia

            current = node;
        }

        // Ora scorre all'indietro partendo dall'ultimo nodo
        while (current.PrevLeafOffset != -1)
        {
            var prevNode = LoadNode(current.PrevLeafOffset);
            if (prevNode is not BPlusLeafNode leafPrev) return false;
            if (leafPrev.NextLeafOffset != current.NodeOffset) return false; // Il puntatore in avanti non corrisponde

            current = leafPrev;
        }

        // Se il ciclo all'indietro ci ha riportato al primo nodo, la lista è consistente.
        return current.NodeOffset == firstLeaf.NodeOffset;
    }

    #endregion

    #region Private Helpers

    /// <summary>
    /// Calcola il numero minimo di chiavi che un nodo deve avere per essere considerato valido
    /// (tipicamente la metà della capacità massima).
    /// </summary>
    private int GetMinKeysPerNode() => BPlusFillFactor.ComputeMinKeys(GetMaxKeysPerNode());

    private int GetMinKeysForInternalNode() => BPlusFillFactor.ComputeMinKeys(GetMaxKeysForInternalNode());

    /// <summary>
    /// Metodo ricorsivo per la cancellazione che scende l'albero e gestisce il ribilanciamento
    /// (merge o ridistribuzione) durante la risalita.
    /// </summary>
    /// <param name="pageOffset">L'offset della pagina corrente da processare.</param>
    /// <param name="parent">Il nodo padre della pagina corrente (null per la radice).</param>
    /// <param name="parentIndex">L'indice del puntatore, nel padre, che punta alla pagina corrente.</param>
    /// <param name="key">La chiave da rimuovere.</param>
    /// <param name="recordOffset">Lo specifico offset del record da rimuovere.</param>
    private void DeleteRecursive(long pageOffset, BPlusInternalNode? parent, int parentIndex, byte[] key,
        long recordOffset)
    {
        var page = LoadNode(pageOffset);

        if (page.IsLeaf)
        {
            // --- CASO BASE: Siamo in una foglia ---
            var leaf = (BPlusLeafNode)page;
            if (leaf.Remove(key, recordOffset))
            {
                SaveNode(leaf);
            }

            return;
        }

        // --- CASO RICORSIVO: Siamo in un nodo interno ---
        var internalNode = (BPlusInternalNode)page;
        var childIndex = internalNode.FindChildIndexFor(key);
        var childOffset = internalNode.ChildrenOffsets[childIndex];

        // Scende ricorsivamente al figlio.
        DeleteRecursive(childOffset, internalNode, childIndex, key, recordOffset);

        // Dopo la chiamata, controlla se il figlio è in underflow.
        var childNode = LoadNode(childOffset);
        if (childNode.KeyCount < GetMinKeysPerNode())
        {
            // Se il figlio è in underflow, gestisci il ribilanciamento.
            HandleUnderflow(internalNode, childIndex);
        }
    }

    /// <summary>
    /// Gestisce una pagina figlio che è andata in "underflow" (troppo poche chiavi).
    /// </summary>
    /// <summary>
    /// Gestisce una pagina figlio che è andata in "underflow" (troppo poche chiavi) dopo una cancellazione,
    /// tentando prima una ridistribuzione (prestito) da un fratello e, se non possibile, un merge (unione).
    /// </summary>
    /// <param name="parent">La pagina padre del nodo in underflow.</param>
    /// <param name="childInUnderflowIndex">L'indice del puntatore, all'interno del padre, che punta al nodo in underflow.</param>
    private void HandleUnderflow(BPlusInternalNode parent, int childInUnderflowIndex)
    {
        // Tenta di ridistribuire dal fratello sinistro
        if (childInUnderflowIndex > 0)
        {
            var leftSibling = LoadNode(parent.ChildrenOffsets[childInUnderflowIndex - 1]);
            if (leftSibling.KeyCount > GetMinKeysPerNode())
            {
                Redistribute(parent, childInUnderflowIndex - 1, leftSibling,
                    LoadNode(parent.ChildrenOffsets[childInUnderflowIndex]));
                return;
            }
        }

        // Tenta di ridistribuire dal fratello destro
        if (childInUnderflowIndex < parent.KeyCount)
        {
            var rightSibling = LoadNode(parent.ChildrenOffsets[childInUnderflowIndex + 1]);
            if (rightSibling.KeyCount > GetMinKeysPerNode())
            {
                Redistribute(parent, childInUnderflowIndex, LoadNode(parent.ChildrenOffsets[childInUnderflowIndex]),
                    rightSibling);
                return;
            }
        }

        // Se non è possibile, esegue il merge
        if (childInUnderflowIndex > 0)
        {
            Merge(parent, childInUnderflowIndex - 1);
        }
        else
        {
            Merge(parent, childInUnderflowIndex);
        }
    }

    /// <summary>
    /// Ribalancia due nodi fratelli adiacenti quando uno è in underflow e l'altro ha chiavi da cedere.
    /// L'operazione sposta una chiave/puntatore dal fratello più pieno a quello in underflow, passando attraverso il padre.
    /// </summary>
    /// <param name="parent">La pagina padre comune.</param>
    /// <param name="leftNodeIndexInParent">L'indice del puntatore, nel padre, che punta al nodo di sinistra dei due fratelli.</param>
    /// <param name="leftChild">Il nodo fratello di sinistra.</param>
    /// <param name="rightChild">Il nodo fratello di destra.</param>
    internal void Redistribute(BPlusInternalNode parent, int leftNodeIndexInParent, BPlusNode leftChild,
        BPlusNode rightChild)
    {
        // Se il figlio sinistro ha bisogno di chiavi, le prende dal destro
        if (leftChild.KeyCount < GetMinKeysPerNode())
        {
            if (leftChild.IsLeaf)
            {
                var leftLeaf = (BPlusLeafNode)leftChild;
                var rightLeaf = (BPlusLeafNode)rightChild;

                // Prendi la prima entry dal fratello destro
                var borrowedKey = rightLeaf.Keys.First();
                var borrowedOffsets = rightLeaf.RecordOffsets.First();
                rightLeaf.Keys.RemoveAt(0);
                rightLeaf.RecordOffsets.RemoveAt(0);

                // Aggiungila in coda al fratello sinistro
                leftLeaf.Keys.Add(borrowedKey);
                leftLeaf.RecordOffsets.Add(borrowedOffsets);

                // Aggiorna la chiave separatrice nel padre con la nuova prima chiave del fratello destro
                parent.Keys[leftNodeIndexInParent] = rightLeaf.Keys.First();
            }
            else // Nodi Interni
            {
                var leftInternal = (BPlusInternalNode)leftChild;
                var rightInternal = (BPlusInternalNode)rightChild;

                // Tira giù la chiave separatrice dal padre
                leftInternal.Keys.Add(parent.Keys[leftNodeIndexInParent]);
                // Prendi il primo puntatore dal fratello destro
                leftInternal.ChildrenOffsets.Add(rightInternal.ChildrenOffsets.First());
                rightInternal.ChildrenOffsets.RemoveAt(0);

                // La prima chiave del fratello destro diventa la nuova chiave separatrice
                parent.Keys[leftNodeIndexInParent] = rightInternal.Keys.First();
                rightInternal.Keys.RemoveAt(0);
            }
        }
        else // Altrimenti, il figlio destro ha bisogno di chiavi e le prende dal sinistro
        {
            if (leftChild.IsLeaf)
            {
                var leftLeaf = (BPlusLeafNode)leftChild;
                var rightLeaf = (BPlusLeafNode)rightChild;

                // Prendi l'ultima entry dal fratello sinistro
                var borrowedKey = leftLeaf.Keys.Last();
                var borrowedOffsets = leftLeaf.RecordOffsets.Last();
                leftLeaf.Keys.RemoveAt(leftLeaf.KeyCount - 1);
                leftLeaf.RecordOffsets.RemoveAt(leftLeaf.RecordOffsets.Count - 1);

                // Aggiungila all'inizio del fratello destro
                rightLeaf.Keys.Insert(0, borrowedKey);
                rightLeaf.RecordOffsets.Insert(0, borrowedOffsets);

                // Aggiorna la chiave separatrice nel padre con la nuova prima chiave del fratello destro
                parent.Keys[leftNodeIndexInParent] = rightLeaf.Keys.First();
            }
            else // Nodi Interni
            {
                var leftInternal = (BPlusInternalNode)leftChild;
                var rightInternal = (BPlusInternalNode)rightChild;

                // Tira giù la chiave separatrice
                rightInternal.Keys.Insert(0, parent.Keys[leftNodeIndexInParent]);
                // Prendi l'ultimo puntatore dal fratello sinistro
                rightInternal.ChildrenOffsets.Insert(0, leftInternal.ChildrenOffsets.Last());
                leftInternal.ChildrenOffsets.RemoveAt(leftInternal.ChildrenOffsets.Count - 1);

                // L'ultima chiave del fratello sinistro diventa la nuova chiave separatrice
                parent.Keys[leftNodeIndexInParent] = leftInternal.Keys.Last();
                leftInternal.Keys.RemoveAt(leftInternal.KeyCount - 1);
            }
        }

        // Salva tutte e tre le pagine modificate su disco
        SaveNode(parent);
        SaveNode(leftChild);
        SaveNode(rightChild);
    }

    /// <summary>
    /// Unisce due nodi fratelli adiacenti (sinistro e destro) quando entrambi sono al minimo numero di chiavi o meno.
    /// L'operazione sposta tutto il contenuto del nodo destro nel nodo sinistro, tira giù la chiave separatrice
    /// dal nodo padre e rimuove il puntatore al nodo destro dal padre.
    /// </summary>
    /// <param name="parent">Il nodo padre che contiene i puntatori ai due figli da unire.</param>
    /// <param name="leftNodeIndexInParent">
    /// L'indice, nella lista dei puntatori del padre, che punta al nodo di sinistra dei due fratelli.
    /// Il nodo di destra si trova implicitamente all'indice `leftNodeIndexInParent + 1`.
    /// </param>
    /// <remarks>
    /// Questa operazione è l'inverso dello split e viene invocata per gestire l'underflow quando la ridistribuzione non è possibile.
    /// La pagina del nodo destro viene aggiunta alla free list per il riutilizzo.
    /// Se l'unione coinvolge nodi foglia, la lista doppiamente concatenata viene aggiornata di conseguenza.
    /// </remarks>
    internal void Merge(BPlusInternalNode parent, int leftNodeIndexInParent)
    {
        var leftChild = LoadNode(parent.ChildrenOffsets[leftNodeIndexInParent]);
        var rightChild = LoadNode(parent.ChildrenOffsets[leftNodeIndexInParent + 1]);

        // Tira giù la chiave separatrice dal padre
        var separatorKey = parent.Keys[leftNodeIndexInParent];

        if (leftChild.IsLeaf)
        {
            var leftLeaf = (BPlusLeafNode)leftChild;
            var rightLeaf = (BPlusLeafNode)rightChild;

            // Aggiunge tutte le entry del fratello destro a quello sinistro
            leftLeaf.Keys.AddRange(rightLeaf.Keys);
            leftLeaf.RecordOffsets.AddRange(rightLeaf.RecordOffsets);

            // Aggiorna la lista concatenata per "scavalcare" il nodo destro
            leftLeaf.NextLeafOffset = rightLeaf.NextLeafOffset;
            if (rightLeaf.NextLeafOffset != -1)
            {
                var nextNext = (BPlusLeafNode)LoadNode(rightLeaf.NextLeafOffset);
                // Aggiorna il puntatore all'indietro del nodo successivo perché punti al nodo sinistro
                nextNext.PrevLeafOffset = leftChild.NodeOffset;
                SaveNode(nextNext);
            }
        }
        else // Nodi interni
        {
            var leftInternal = (BPlusInternalNode)leftChild;
            var rightInternal = (BPlusInternalNode)rightChild;

            // Tira giù la chiave separatrice e la aggiunge al nodo sinistro
            leftInternal.Keys.Add(separatorKey);
            // Aggiunge tutte le chiavi e i puntatori del fratello destro
            leftInternal.Keys.AddRange(rightInternal.Keys);
            leftInternal.ChildrenOffsets.AddRange(rightInternal.ChildrenOffsets);
        }

        // Rimuove la chiave e il puntatore dal padre, che ora sono ridondanti
        parent.Keys.RemoveAt(leftNodeIndexInParent);
        parent.ChildrenOffsets.RemoveAt(leftNodeIndexInParent + 1);

        SaveNode(leftChild);
        SaveNode(parent);
        _fileHandle.AddPageToFreeList(rightChild.NodeOffset);
    }

    /// <summary>
    /// Naviga l'albero B+ a partire dalla radice per trovare il nodo foglia più a sinistra.
    /// Questo nodo è il primo nella sequenza logica dell'indice e contiene le chiavi con i valori più bassi.
    /// </summary>
    /// <returns>
    /// L'istanza di <see cref="BPlusLeafNode"/> che rappresenta il primo nodo foglia dell'albero.
    /// </returns>
    /// <remarks>
    /// L'operazione consiste nel seguire ricorsivamente il primo puntatore (indice 0) di ogni
    /// nodo interno fino a raggiungere un nodo foglia. Questo metodo è il punto di partenza
    /// per operazioni come GO TOP e per le scansioni sequenziali complete dell'indice.
    /// </remarks>
    internal BPlusLeafNode FindFirstLeaf()
    {
        if (_rootNodeOffset == -1)
        {
            return new BPlusLeafNode { NodeOffset = -1 };
        }

        var node = LoadNode(_rootNodeOffset);
        while (!node.IsLeaf)
        {
            node = LoadNode(((BPlusInternalNode)node).ChildrenOffsets[0]);
        }

        return (BPlusLeafNode)node;
    }

    /// <summary>
    /// Navigates backwards from a given leaf node to find its immediate predecessor in the doubly-linked list.
    /// </summary>
    /// <param name="currentLeaf">The leaf node from which to start the backward navigation.</param>
    /// <returns>The preceding <see cref="BPlusLeafNode"/>, or <c>null</c> if <paramref name="currentLeaf"/> is the
    /// first leaf in the sequence.</returns>
    /// <remarks>
    /// This operation is highly efficient (O(1)) as it directly uses the <c>PrevLeafOffset</c> pointer
    /// stored within the leaf node structure, avoiding a full scan of the index.
    /// </remarks>
    private BPlusLeafNode? FindPreviousLeaf(BPlusLeafNode currentLeaf)
    {
        // Utilizza il puntatore diretto 'PrevLeafOffset' per un'operazione O(1)
        // // invece di rieseguire la scansione dell'intera lista di foglie dall'inizio (O(N)).
        if (currentLeaf.PrevLeafOffset == -1)
        {
            // Questo è il primo nodo foglia, non ha predecessori.
            return null;
        }

        // Carica direttamente il nodo precedente usando l'offset memorizzato.
        return (BPlusLeafNode)LoadNode(currentLeaf.PrevLeafOffset);
    }

    /// <summary>
    /// Esegue una ricerca inversa per trovare la posizione precisa di una entry dell'indice di un record
    /// all'interno dell'albero B+. Dato l'offset fisico di un record, questo metodo determina quale
    /// nodo foglia contiene la sua chiave, l'indice di quella chiave all'interno del nodo e la sua
    /// posizione nella lista degli offset duplicati.
    /// </summary>
    /// <param name="recordOffset">L'offset fisico in byte del record dati da localizzare all'interno dell'indice.</param>
    /// <param name="evaluator">Il callback fornito dall'Engine per calcolare la chiave del record.</param>
    /// <returns>
    /// Una tupla contenente:
    /// - <c>leaf</c>: Il <see cref="BPlusLeafNode"/> dove risiede la entry dell'indice.
    /// - <c>keyIndex</c>: L'indice 0-based della chiave all'interno della lista di chiavi della foglia.
    /// - <c>offsetIndex</c>: L'indice 0-based di <paramref name="recordOffset"/> all'interno della lista di duplicati per quella chiave.
    /// Restituisce <c>(null, -1, -1)</c> se la chiave del record non viene trovata nell'indice o se viene rilevata un'inconsistenza.
    /// </returns>
    /// <remarks>
    /// Questo metodo è il pilastro della navigazione sequenziale (es. SKIP). Funziona leggendo prima l'intero
    /// record dal disco, usando l'evaluator per estrarne il valore della chiave, quindi attraversando l'albero B+
    /// per localizzare il nodo foglia e la entry specifici.
    /// </remarks>
    private (BPlusLeafNode? leaf, int keyIndex, int offsetIndex) FindRecordPosition(long recordOffset,
        KeyExpressionEvaluatorCallback evaluator)
    {
        // 1. Legge il record dal disco.
        var buffer = new byte[_fileHandle.Header.RecordSize];
        var stream = _fileHandle.GetStream();
        stream.Seek(recordOffset, SeekOrigin.Begin);
        stream.ReadExactly(buffer, 0, buffer.Length);

        var udtDef = _fileHandle.UdtDefinition;
        var (_, udtInstance) = RecordMarshaller.DeserializeRecord(_schema, buffer, udtDef, _fileHandle);

        // Poiché IndexDefinition usa ancora una stringa per il nome, si confronta con 'f.Name.Lexeme'.
        var udtField = (udtDef.Fields as IEnumerable<TypeField>).FirstOrDefault(f =>
            f.Name.Lexeme.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));

        var targetType = udtField?.DataType ?? new StringType(_indexDefinition.KeyLength);

        // CORREZIONE ARCHITETTURALE: Marshalling da ID-based a string-based.
        // Il delegato 'evaluator' si aspetta un dizionario con chiavi stringa.
        // Creiamo una mappa di conversione una-tantum dai SymbolId ai nomi dei campi.
        var idToNameMap = udtDef.Fields.ToDictionary(f => f.SymbolId, f => f.Name.Lexeme);

        // Convertiamo il dizionario Fields dell'istanza UDT nel formato richiesto.
        var stringKeyedFields = udtInstance.Fields.ToDictionary(
            kvp => idToNameMap[kvp.Key], // Converte SymbolId (int) in nome (string)
            kvp => kvp.Value
        );

        var keyValue = evaluator(_indexDefinition.KeyExpression, stringKeyedFields, targetType);
        var key = SchemaManager.SerializeKeyValue(keyValue, _indexDefinition);

        if (_rootNodeOffset == -1) return (null, -1, -1);

        // 2. Scende l'albero per trovare il nodo foglia corretto per questa chiave.
        var node = LoadNode(_rootNodeOffset);
        while (!node.IsLeaf)
        {
            var internalNode = (BPlusInternalNode)node;
            var index = internalNode.Keys.BinarySearch(key, new ByteArrayComparer());
            if (index < 0) index = ~index;
            else index += 1;

            node = LoadNode(internalNode.ChildrenOffsets[index]);
        }

        var leaf = (BPlusLeafNode)node;

        // 3. Cerca la chiave all'interno del nodo foglia.
        var keyIndex = leaf.Keys.BinarySearch(key, new ByteArrayComparer());
        if (keyIndex < 0)
        {
            return (null, -1, -1); // Inconsistenza tra dati e indice.
        }

        // 4. Cerca l'offset specifico del record all'interno della lista di duplicati.
        var offsetIndex = leaf.RecordOffsets[keyIndex].IndexOf(recordOffset);
        if (offsetIndex < 0)
        {
            return (null, -1, -1); // Altra inconsistenza.
        }

        return (leaf, keyIndex, offsetIndex);
    }

    /// <summary>
    /// Persists the current in-memory root node offset to the file's main header.
    /// This ensures that the database correctly points to the root of the B+Tree when it is next opened.
    /// </summary>
    /// <remarks>
    /// The method correctly identifies whether the update is for the primary key index or for one of the
    /// secondary indexes by comparing the manager's index field name against the schema definition.
    /// It then modifies the appropriate field in the header structure (<c>PrimaryKeyRootOffset</c> or an
    /// element of <c>SecondaryIndexRootOffsets</c>) and delegates the write operation to the file handle.
    /// This is a critical operation that must be called after any structural change to the tree root, such as
    /// a root split or a tree becoming empty.
    /// </remarks>
    private bool IsValidRecordOffset(long recordOffset)
    {
        var dataAreaStart = _fileHandle.DataAreaStartOffset;
        if (recordOffset < dataAreaStart)
        {
            return false;
        }

        var relative = recordOffset - dataAreaStart;
        return relative % _fileHandle.Header.RecordSize == 0;
    }

    private void RefreshRootOffsetFromHeader()
    {
        var header = _fileHandle.Header;
        if (_indexDefinition.Name.Equals(_schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
        {
            _rootNodeOffset = header.PrimaryKeyRootOffset;
            return;
        }

        var secondaryIndexPosition = _schema.SecondaryKeys.FindIndex(def =>
            def.Name.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));
        if (secondaryIndexPosition != -1)
        {
            _rootNodeOffset = header.SecondaryIndexRootOffsets[secondaryIndexPosition];
        }
    }

    private void UpdateRootOffsetInHeader()
    {
        using (_fileHandle.EnterStructuralLock())
        {
            var header = _fileHandle.Header;

            if (_indexDefinition.Name.Equals(_schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
            {
                header.PrimaryKeyRootOffset = _rootNodeOffset;
            }
            else
            {
                var secondaryIndexPosition = _schema.SecondaryKeys.FindIndex(def =>
                    def.Name.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));

                if (secondaryIndexPosition != -1)
                {
                    header.SecondaryIndexRootOffsets[secondaryIndexPosition] = _rootNodeOffset;
                }
                else
                {
                    throw new InvalidOperationException(
                        $"Indice secondario '{_indexDefinition.Name}' non trovato nello schema.");
                }
            }

            _fileHandle.UpdateHeader(header);
        }
    }

    private byte[] ReadNodeBytes(long offset, long fileLength)
    {
        const int pageSize = 4096;
        var bytesToRead = (int)Math.Min(pageSize, Math.Max(0, fileLength - offset));
        if (bytesToRead <= 0)
        {
            throw new EndOfStreamException($"Offset nodo B+ non valido: {offset}, lunghezza file: {fileLength}.");
        }

        var buffer = new byte[bytesToRead];
        _fileHandle.ReadBytesAt(offset, buffer);
        return buffer;
    }

    #endregion
}