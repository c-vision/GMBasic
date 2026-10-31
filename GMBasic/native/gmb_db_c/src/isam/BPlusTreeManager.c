/* FILE: native/gmb_db_c/src/ISAM/BPlusTreeManager.c */
/* Port 1:1 di modules/Database/ISAM/BPlusTreeManager.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.BPlusTreeManager.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Buffers;
//using System.Text;
//using Basic.ISAM;
//using Basic.Databases;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Gestisce tutte le operazioni per un singolo indice B+Tree, inclusa la navigazione,
///// la modifica, e l'interazione con il disco per la persistenza dei nodi.
///// </summary>
//internal class BPlusTreeManager
//{
//    private readonly IsamFileHandle _fileHandle;
//    private readonly Config.IsamSchema _schema;
//    private long _rootNodeOffset;
//    private readonly Config.IndexDefinition _indexDefinition;
//
//    internal long RootNodeOffset => _rootNodeOffset;
//
//    private const int PageSize = 4096;
//
//    /// <summary>
//    /// Numero massimo di chiavi per nodo in base alla lunghezza chiave e alla dimensione pagina.
//    /// </summary>
//    internal int GetMaxKeysPerNode()
//    {
//        const int leafHeaderSize = 1 + sizeof(long) + sizeof(long) + sizeof(int);
//        var perLeafKey = sizeof(int) + _indexDefinition.KeyLength + sizeof(int) + sizeof(long);
//        return Math.Max(2, (PageSize - leafHeaderSize) / perLeafKey);
//    }
//
//    /// <summary>
//    /// Capacità massima chiavi per nodi interni (include puntatori ai figli).
//    /// </summary>
//    internal int GetMaxKeysForInternalNode()
//    {
//        const int internalHeaderSize = 1 + sizeof(int);
//        const int childListOverhead = sizeof(int);
//        var perKey = sizeof(int) + _indexDefinition.KeyLength;
//        const int perChild = sizeof(long);
//        var bytesPerKeySlot = perKey + perChild;
//        var fixedOverhead = internalHeaderSize + childListOverhead + perChild;
//        return Math.Max(2, (PageSize - fixedOverhead) / bytesPerKeySlot);
//    }
//
//    // ponytail: LRU con pin su radice durante bulk insert; upgrade path: ref-count pcache SQLite
//    private readonly IsamLruCache<long, BPlusNode> _nodeCache = new();
//
//    /// <summary>Read-ahead sequenziale su catena foglie (ISAM-C2): 2 pagine in buffer.</summary>
//    private const int ReadAheadLeafSlots = 2;
//
//    private readonly long[] _readAheadLeafOffsets = [-1, -1];
//
//    private readonly BPlusLeafNode?[] _readAheadLeaves = [null, null];
//
//    /// <summary>
//    /// Oggetto di sincronizzazione per garantire la thread-safety delle operazioni
//    /// che modificano la struttura dell'albero B+Tree (es. Insert, Delete, Validate).
//    /// </summary>
//    private readonly Lock _syncRoot = new();
//
//    /// <summary>
//    /// Flag che determina se la cache dei nodi B+Tree in memoria (LRU) è abilitata.
//    /// Viene impostato su <c>false</c> quando la strategia di locking è multi-processo
//    /// per prevenire problemi di cache incoerente (stale cache).
//    /// </summary>
//    private readonly bool _useNodeCache;
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="BPlusTreeManager"/>.
//    /// </summary>
//    /// <param name="fileHandle">L'handle del file ISAM su cui operare.</param>
//    /// <param name="schema">Lo schema completo del database.</param>
//    /// <param name="indexDefinition">La definizione specifica dell'indice che questo manager gestirà.</param>
//    /// <param name="rootNodeOffset">L'offset fisico della pagina radice di questo indice, letto dall'header del file.</param>
//    public BPlusTreeManager(IsamFileHandle fileHandle, Config.IsamSchema schema, Config.IndexDefinition indexDefinition,
//        long rootNodeOffset)
//    {
//        _fileHandle = fileHandle;
//        _schema = schema;
//        _indexDefinition = indexDefinition;
//        _rootNodeOffset = rootNodeOffset;
//
//        // Disabilita la cache se siamo in modalità multi-processo
//        // per prevenire corruzione da cache incoerente (stale cache).
//        _useNodeCache = fileHandle.LockingStrategy == LockingStrategy.IntraProcess;
//    }
//
//    #region Public API
//
//    /// <summary>
//    /// Avvia una ricerca per una chiave specifica all'interno dell'indice B+Tree,
//    /// applicando il locking a livello di chiave per il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="key">La rappresentazione binaria della chiave da cercare.</param>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se fornito e il livello è `Serializable`,
//    /// verrà acquisito un lock di lettura sulla chiave (e sull'intervallo successivo)
//    /// per prevenire letture fantasma.
//    /// </param>
//    /// <returns>
//    /// Una lista di <c>long</c> contenente tutti gli offset dei record che corrispondono esattamente alla chiave.
//    /// Se la chiave non viene trovata, restituisce una lista vuota.
//    /// </returns>
//    /// <exception cref="Databases.LockTimeoutException">
//    /// Lanciata se, in una transazione `Serializable`, non è possibile acquisire un lock di lettura
//    /// sulla chiave, indicando che un'altra transazione la sta modificando.
//    /// </exception>
//    public List<long> Search(byte[] key, TransactionContext? context)
//    {
//        using var _readScope = _fileHandle.EnterFileStructureReadScope();
//        if (!_useNodeCache)
//        {
//            _fileHandle.SyncStructuralStateFromDisk();
//            RefreshRootOffsetFromHeader();
//        }
//
//        // Se in una transazione Serializable, acquisisci un lock di lettura sull'intervallo di chiave.
//        if (context is { IsolationLevel: IsolationLevel.Serializable })
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
//            var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
//            _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//            context.ReadLockedKeys.Add(lockKey);
//        }
//
//        if (_rootNodeOffset == -1) return new List<long>();
//        var rootNode = LoadNode(_rootNodeOffset);
//        return rootNode.Search(key, this);
//    }
//
//    /// <summary>Primo record con chiave &gt;= <paramref name="key"/> (seek prefisso filtro).</summary>
//    public List<long> SearchLowerBound(byte[] key, TransactionContext? context)
//    {
//        using var _readScope = _fileHandle.EnterFileStructureReadScope();
//        if (!_useNodeCache)
//        {
//            _fileHandle.SyncStructuralStateFromDisk();
//            RefreshRootOffsetFromHeader();
//        }
//
//        if (context is { IsolationLevel: IsolationLevel.Serializable })
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
//            var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
//            _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//            context.ReadLockedKeys.Add(lockKey);
//        }
//
//        if (_rootNodeOffset == -1)
//        {
//            return [];
//        }
//
//        var rootNode = LoadNode(_rootNodeOffset);
//        return rootNode.SearchLowerBound(key, this);
//    }
//
//    /// <summary>
//    /// Avvia l'operazione di inserimento di una nuova coppia chiave/offset nell'indice B+Tree,
//    /// gestendo il locking a livello di chiave per supportare il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="key">La rappresentazione binaria della chiave da inserire.</param>
//    /// <param name="recordOffset">L'offset fisico del record dati associato alla chiave.</param>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se `null`, l'operazione viene eseguita in modalità auto-commit.
//    /// Se fornito, i lock acquisiti verranno mantenuti fino al termine della transazione.
//    /// </param>
//    /// <exception cref="LockTimeoutException">
//    /// Lanciata se non è possibile acquisire un lock di scrittura sulla chiave, indicando che un'altra
//    /// transazione `Serializable` sta proteggendo l'intervallo con un lock di lettura.
//    /// </exception>
//    /// <remarks>
//    /// Prima di qualsiasi modifica all'albero, questo metodo acquisisce un lock di scrittura sulla chiave.
//    /// Questo è il meccanismo fondamentale che previene le letture fantasma: un inserimento non può
//    /// procedere se la sua chiave ricade in un intervallo protetto da un lock di lettura di un'altra transazione.
//    /// Se l'inserimento è parte di una transazione, il lock viene registrato nel contesto e mantenuto;
//    /// altrimenti, viene rilasciato immediatamente dopo l'operazione (auto-commit).
//    /// </remarks>
//    public void Insert(byte[] key, long recordOffset, TransactionContext? context)
//    {
//        if (!IsValidRecordOffset(recordOffset))
//        {
//            throw new InvalidOperationException(
//                $"Offset record non valido per l'indice '{_indexDefinition.Name}': {recordOffset}.");
//        }
//
//        lock (_syncRoot)
//        {
//            InsertUnderTreeLock(key, recordOffset, context);
//        }
//    }
//
//    private void InsertUnderTreeLock(byte[] key, long recordOffset, TransactionContext? context)
//    {
//        if (!_useNodeCache)
//        {
//            _fileHandle.SyncStructuralStateFromDisk();
//            RefreshRootOffsetFromHeader();
//        }
//
//        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
//        var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
//        var keyAlreadyLocked = context?.WriteLockedKeys.Contains(lockKey) == true;
//        var skipKeyLock = context == null && _fileHandle.LockingStrategy == LockingStrategy.IntraProcess;
//        using (_fileHandle.EnterStructuralLock())
//        {
//            if (!_useNodeCache)
//            {
//                RefreshRootOffsetFromHeader();
//            }
//
//            if (!skipKeyLock && !keyAlreadyLocked)
//            {
//                _fileHandle.RecordLockManager.AcquireWriteLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//            }
//
//            try
//            {
//                if (!skipKeyLock)
//                {
//                    context?.WriteLockedKeys.Add(lockKey);
//                }
//
//                if (_rootNodeOffset == -1)
//                {
//                    var root = new BPlusLeafNode();
//                    root.Insert(key, recordOffset, this, context);
//                    _rootNodeOffset = SaveNode(root);
//                    UpdateRootOffsetInHeader();
//                    return;
//                }
//
//                var rootNode = LoadNode(_rootNodeOffset);
//                var result = rootNode.Insert(key, recordOffset, this, context);
//
//                if (result.NewNode != null)
//                {
//                    using var _ = IsamSlowOpLog.Measure("bplus-split", $"index={_indexDefinition.Name} root");
//                    if (result.PromotedKey is null)
//                        throw new InvalidOperationException("Lo split della radice deve produrre una chiave promosta.");
//
//                    var newSiblingOffset = result.NewNodeOffset;
//                    if (newSiblingOffset < 0)
//                    {
//                        newSiblingOffset = SaveNode(result.NewNode);
//                    }
//
//                    var newRoot = new BPlusInternalNode();
//                    newRoot.Keys.Add(result.PromotedKey);
//                    newRoot.ChildrenOffsets.Add(_rootNodeOffset);
//                    newRoot.ChildrenOffsets.Add(newSiblingOffset);
//                    _rootNodeOffset = SaveNode(newRoot);
//                    UpdateRootOffsetInHeader();
//                }
//                else if (result.NodeUpdated)
//                {
//                    SaveNode(rootNode);
//                }
//            }
//            finally
//            {
//                if (context == null && !keyAlreadyLocked && !skipKeyLock)
//                {
//                    _fileHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//                }
//            }
//        }
//    }
//
//    /// <summary>
//    /// Inserimento bulk ordinato: riempie foglie in RAM, un SaveNode per pagina piena (split) + flush finale.
//    /// ponytail: richiede chiavi strettamente crescenti e ≥ ultima chiave esistente; no key-lock per batch IntraProcess.
//    /// </summary>
//    internal void InsertManyAppendOrdered(ReadOnlySpan<(byte[] Key, long RecordOffset)> entries, TransactionContext? context)
//    {
//        if (entries.Length == 0)
//        {
//            return;
//        }
//
//        for (var i = 1; i < entries.Length; i++)
//        {
//            if (new ByteArrayComparer().Compare(entries[i - 1].Key, entries[i].Key) >= 0)
//            {
//                throw new InvalidOperationException(
//                    $"InsertManyAppendOrdered: chiavi non strettamente crescenti a indice {i} per indice '{_indexDefinition.Name}'.");
//            }
//        }
//
//        lock (_syncRoot)
//        {
//            if (!_useNodeCache)
//            {
//                _fileHandle.SyncStructuralStateFromDisk();
//                RefreshRootOffsetFromHeader();
//            }
//
//            using (_fileHandle.EnterStructuralLock())
//            {
//                if (!_useNodeCache)
//                {
//                    RefreshRootOffsetFromHeader();
//                }
//
//                var pinnedRoot = _rootNodeOffset;
//                if (pinnedRoot > 0 && _useNodeCache)
//                {
//                    _nodeCache.Pin(pinnedRoot);
//                }
//
//                try
//                {
//                    BPlusLeafNode leaf;
//                    var dirty = false;
//
//                    if (_rootNodeOffset == -1)
//                    {
//                        leaf = new BPlusLeafNode();
//                    }
//                    else
//                    {
//                        leaf = FindLastLeaf();
//                        if (leaf.KeyCount > 0
//                            && new ByteArrayComparer().Compare(leaf.Keys[^1], entries[0].Key) >= 0)
//                        {
//                            throw new InvalidOperationException(
//                                $"InsertManyAppendOrdered: prima chiave batch deve essere > ultima chiave indice '{_indexDefinition.Name}'.");
//                        }
//                    }
//
//                    foreach (var (key, recordOffset) in entries)
//                    {
//                        if (!IsValidRecordOffset(recordOffset))
//                        {
//                            throw new InvalidOperationException(
//                                $"Offset record non valido per l'indice '{_indexDefinition.Name}': {recordOffset}.");
//                        }
//
//                        if (!leaf.TryAppendMonotonic(key, recordOffset, this, out var split))
//                        {
//                            throw new InvalidOperationException(
//                                $"InsertManyAppendOrdered: chiave non monotona in foglia per indice '{_indexDefinition.Name}'.");
//                        }
//
//                        if (split == null)
//                        {
//                            dirty = true;
//                            continue;
//                        }
//
//                        if (_rootNodeOffset == -1)
//                        {
//                            PromoteLeafSplitAsRoot(leaf, split);
//                        }
//                        else
//                        {
//                            PromoteLeafSplit(leaf, split);
//                        }
//
//                        leaf = (BPlusLeafNode)split.NewNode!;
//                        dirty = false;
//                    }
//
//                    if (_rootNodeOffset == -1)
//                    {
//                        _rootNodeOffset = SaveNode(leaf);
//                        UpdateRootOffsetInHeader();
//                    }
//                    else if (dirty)
//                    {
//                        SaveNode(leaf);
//                    }
//                }
//                finally
//                {
//                    if (pinnedRoot > 0 && _useNodeCache)
//                    {
//                        _nodeCache.Unpin(pinnedRoot);
//                    }
//                }
//            }
//        }
//    }
//
//    private void PromoteLeafSplitAsRoot(BPlusNode leftChild, BPlusInsertResult split)
//    {
//        var newRoot = new BPlusInternalNode();
//        newRoot.Keys.Add(split.PromotedKey!);
//        newRoot.ChildrenOffsets.Add(leftChild.NodeOffset);
//        newRoot.ChildrenOffsets.Add(split.NewNodeOffset);
//        _rootNodeOffset = SaveNode(newRoot);
//        UpdateRootOffsetInHeader();
//    }
//
//    private void PromoteLeafSplit(BPlusLeafNode leftLeaf, BPlusInsertResult split)
//    {
//        if (_rootNodeOffset == leftLeaf.NodeOffset)
//        {
//            PromoteLeafSplitAsRoot(leftLeaf, split);
//            return;
//        }
//
//        var parent = FindParentInternalNode(leftLeaf.NodeOffset)
//                     ?? throw new InvalidOperationException(
//                         $"Padre B+ non trovato per foglia a offset {leftLeaf.NodeOffset}.");
//        InsertPromotedKeyIntoInternal(parent, leftLeaf.NodeOffset, split);
//    }
//
//    private void InsertPromotedKeyIntoInternal(
//        BPlusInternalNode parent,
//        long leftChildOffset,
//        BPlusInsertResult childSplit)
//    {
//        while (true)
//        {
//            var childIndex = parent.ChildrenOffsets.IndexOf(leftChildOffset);
//            if (childIndex < 0)
//            {
//                throw new InvalidOperationException("Figlio non trovato nel nodo interno padre B+.");
//            }
//
//            parent.Keys.Insert(childIndex, childSplit.PromotedKey!);
//            parent.ChildrenOffsets.Insert(childIndex + 1, childSplit.NewNodeOffset);
//
//            if (parent.KeyCount <= GetMaxKeysForInternalNode())
//            {
//                SaveNode(parent);
//                return;
//            }
//
//            var sibling = new BPlusInternalNode();
//            var splitIndex = BPlusFillFactor.ComputeInternalSplitIndex(parent.KeyCount);
//            var promotedKey = parent.Keys[splitIndex];
//
//            sibling.Keys.AddRange(parent.Keys.Skip(splitIndex + 1));
//            sibling.ChildrenOffsets.AddRange(parent.ChildrenOffsets.Skip(splitIndex + 1));
//            parent.Keys.RemoveRange(splitIndex, parent.KeyCount - splitIndex);
//            parent.ChildrenOffsets.RemoveRange(splitIndex + 1, parent.ChildrenOffsets.Count - (splitIndex + 1));
//
//            SaveNode(parent);
//            var siblingOffset = SaveNode(sibling);
//
//            if (parent.NodeOffset == _rootNodeOffset)
//            {
//                PromoteLeafSplitAsRoot(parent, new BPlusInsertResult
//                {
//                    NodeUpdated = true,
//                    NewNode = sibling,
//                    NewNodeOffset = siblingOffset,
//                    PromotedKey = promotedKey,
//                });
//                return;
//            }
//
//            leftChildOffset = parent.NodeOffset;
//            childSplit = new BPlusInsertResult
//            {
//                NodeUpdated = true,
//                NewNode = sibling,
//                NewNodeOffset = siblingOffset,
//                PromotedKey = promotedKey,
//            };
//
//            parent = FindParentInternalNode(leftChildOffset)
//                     ?? throw new InvalidOperationException(
//                         "Padre nodo interno B+ non trovato durante propagazione split.");
//        }
//    }
//
//    private BPlusInternalNode? FindParentInternalNode(long childOffset)
//    {
//        if (_rootNodeOffset == -1 || _rootNodeOffset == childOffset)
//        {
//            return null;
//        }
//
//        var pending = new Stack<long>();
//        pending.Push(_rootNodeOffset);
//
//        while (pending.Count > 0)
//        {
//            var nodeOffset = pending.Pop();
//            var node = LoadNode(nodeOffset);
//            if (node.IsLeaf)
//            {
//                continue;
//            }
//
//            var internalNode = (BPlusInternalNode)node;
//            if (internalNode.ChildrenOffsets.Contains(childOffset))
//            {
//                return internalNode;
//            }
//
//            foreach (var childOff in internalNode.ChildrenOffsets)
//            {
//                pending.Push(childOff);
//            }
//        }
//
//        return null;
//    }
//
//    /// <summary>
//    /// Recupera l'offset fisico del primo record nella sequenza logica dell'indice,
//    /// acquisendo un lock di lettura sulla sua chiave se in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se `Serializable`, viene posto un lock di lettura
//    /// sulla prima chiave dell'indice.
//    /// </param>
//    /// <returns>L'offset fisico del primo record attivo, o -1 se l'indice è vuoto.</returns>
//    public long GetFirstRecordOffset(TransactionContext? context)
//    {
//        if (_rootNodeOffset == -1) return -1;
//
//        using var _readScope = _fileHandle.EnterFileStructureReadScope();
//        var leaf = FindFirstLeaf();
//        while (true)
//        {
//            for (var keyIndex = 0; keyIndex < leaf.KeyCount; keyIndex++)
//            {
//                foreach (var offset in leaf.RecordOffsets[keyIndex])
//                {
//                    if (!IsActiveRecord(offset))
//                    {
//                        continue;
//                    }
//
//                    AcquireSerializableReadLockForKey(context, leaf.Keys[keyIndex]);
//                    return offset;
//                }
//            }
//
//            if (leaf.NextLeafOffset == -1)
//            {
//                return -1;
//            }
//
//            leaf = LoadLeafNode(leaf.NextLeafOffset);
//        }
//    }
//
//    /// <summary>
//    /// Recupera l'offset fisico dell'ultimo record nella sequenza logica dell'indice,
//    /// acquisendo un lock di lettura sulla sua chiave se in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se `Serializable`, viene posto un lock di lettura
//    /// sull'ultima chiave dell'indice.
//    /// </param>
//    /// <returns>L'offset fisico dell'ultimo record attivo, o -1 se l'indice è vuoto.</returns>
//    public long GetLastRecordOffset(TransactionContext? context)
//    {
//        if (_rootNodeOffset == -1) return -1;
//
//        using var _readScope = _fileHandle.EnterFileStructureReadScope();
//        var leaf = FindLastLeaf();
//        while (true)
//        {
//            for (var keyIndex = leaf.KeyCount - 1; keyIndex >= 0; keyIndex--)
//            {
//                var offsets = leaf.RecordOffsets[keyIndex];
//                for (var offsetIndex = offsets.Count - 1; offsetIndex >= 0; offsetIndex--)
//                {
//                    var offset = offsets[offsetIndex];
//                    if (!IsActiveRecord(offset))
//                    {
//                        continue;
//                    }
//
//                    AcquireSerializableReadLockForKey(context, leaf.Keys[keyIndex]);
//                    return offset;
//                }
//            }
//
//            leaf = FindPreviousLeaf(leaf);
//            if (leaf is null)
//            {
//                return -1;
//            }
//        }
//    }
//
//    /// <summary>
//    /// Calcola l'offset di un record a una distanza specificata, seguendo l'ordine dell'indice
//    /// e acquisendo lock di lettura sulle chiavi attraversate in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="currentRecordOffset">L'offset del record di partenza.</param>
//    /// <param name="skipCount">Il numero di record logici da saltare.</param>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se `Serializable`, vengono acquisiti lock di lettura
//    /// su ogni chiave attraversata durante la navigazione.
//    /// </param>
//    /// <param name="evaluator">Il callback necessario per calcolare le chiavi dei record durante la navigazione inversa.</param>
//    /// <returns>L'offset del record di destinazione, o -1 se si supera l'inizio o la fine dell'indice.</returns>
//    public long FindNextRecordOffset(long currentRecordOffset, int skipCount, TransactionContext? context,
//        KeyExpressionEvaluatorCallback evaluator)
//    {
//        using var _readScope = _fileHandle.EnterFileStructureReadScope();
//        var (currentLeaf, keyIndex, offsetIndex) = FindRecordPosition(currentRecordOffset, evaluator);
//        if (currentLeaf is null) return -1;
//
//        for (var i = 0; i < Math.Abs(skipCount); i++)
//        {
//            var previousKeyIndex = keyIndex;
//
//            if (skipCount > 0)
//            {
//                offsetIndex++;
//                if (offsetIndex >= currentLeaf.RecordOffsets[keyIndex].Count)
//                {
//                    keyIndex++;
//                    offsetIndex = 0;
//                    if (keyIndex >= currentLeaf.KeyCount)
//                    {
//                        if (currentLeaf.NextLeafOffset == -1) return -1; // EOF
//                        currentLeaf = LoadLeafNode(currentLeaf.NextLeafOffset);
//                        keyIndex = 0;
//                    }
//                }
//            }
//            else // skipCount < 0
//            {
//                offsetIndex--;
//                if (offsetIndex < 0)
//                {
//                    keyIndex--;
//                    if (keyIndex < 0)
//                    {
//                        currentLeaf = FindPreviousLeaf(currentLeaf);
//                        if (currentLeaf is null) return -1; // BOF
//                        keyIndex = currentLeaf.KeyCount - 1;
//                    }
//
//                    offsetIndex = currentLeaf.RecordOffsets[keyIndex].Count - 1;
//                }
//            }
//
//            if (keyIndex != previousKeyIndex && context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                var newKey = currentLeaf.Keys[keyIndex];
//                var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, newKey);
//                var description = $"chiave di indice '{Encoding.UTF8.GetString(newKey)}' durante la scansione";
//
//                _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//                context.ReadLockedKeys.Add(lockKey);
//            }
//        }
//
//        var candidate = currentLeaf.RecordOffsets[keyIndex][offsetIndex];
//        var direction = Math.Sign(skipCount);
//        var guard = 0;
//        while (candidate != -1 && !IsActiveRecord(candidate) && guard++ < 1_000_000)
//        {
//            var advanced = FindNextRecordOffset(candidate, direction, context, evaluator);
//            if (advanced == -1 || advanced == candidate)
//            {
//                return advanced;
//            }
//
//            candidate = advanced;
//        }
//
//        return candidate;
//    }
//
//    private bool IsActiveRecord(long recordOffset)
//    {
//        if (recordOffset < 0)
//        {
//            return false;
//        }
//
//        var metadata = _fileHandle.ReadRecordMetadata(recordOffset, _fileHandle.FileEncoding);
//        return metadata.Status == Config.RecordStatusActive;
//    }
//
//    private void AcquireSerializableReadLockForKey(TransactionContext? context, byte[] key)
//    {
//        if (context is not { IsolationLevel: IsolationLevel.Serializable })
//        {
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
//        var description = $"chiave di indice '{_indexDefinition.Name}'";
//        _fileHandle.RecordLockManager.AcquireReadLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//        context.ReadLockedKeys.Add(lockKey);
//    }
//
//    private BPlusLeafNode FindLastLeaf()
//    {
//        var node = LoadNode(_rootNodeOffset);
//        while (!node.IsLeaf)
//        {
//            var internalNode = (BPlusInternalNode)node;
//            node = LoadNode(internalNode.ChildrenOffsets[^1]);
//        }
//
//        return (BPlusLeafNode)node;
//    }
//
//    /// <summary>
//    /// Avvia la rimozione di una specifica coppia chiave/record offset dall'albero B+, gestendo il locking
//    /// a livello di chiave per il livello di isolamento `Serializable` e il potenziale restringimento dell'albero.
//    /// </summary>
//    /// <param name="key">La rappresentazione binaria della chiave dell'indice da rimuovere.</param>
//    /// <param name="recordOffset">L'offset fisico specifico del record da rimuovere.</param>
//    /// <param name="context">
//    /// Il contesto della transazione attiva. Se `null`, l'operazione è in modalità auto-commit.
//    /// Se fornito, il lock di scrittura sulla chiave verrà mantenuto fino al termine della transazione.
//    /// </param>
//    /// <exception cref="Databases.LockTimeoutException">
//    /// Lanciata se non è possibile acquisire un lock di scrittura sulla chiave, indicando che un'altra
//    /// transazione sta leggendo il record o l'intervallo corrispondente.
//    /// </exception>
//    /// <remarks>
//    /// Prima di modificare l'albero, questo metodo acquisisce un lock di scrittura sulla chiave da rimuovere.
//    /// Questo previene conflitti con transazioni `Serializable` che potrebbero aver posto un lock di lettura
//    /// sul record o sull'intervallo. Se la cancellazione fa sì che il nodo radice abbia un solo figlio,
//    /// quest'ultimo viene promosso a nuova radice, riducendo l'altezza dell'albero.
//    /// </remarks>
//    public void Delete(byte[] key, long recordOffset, TransactionContext? context)
//    {
//        if (_rootNodeOffset == -1) return;
//
//        // Acquisisce un lock di scrittura sulla chiave per proteggere l'operazione.
//        var lockKey = LockManagerBase.GetKeyLockKey(_indexDefinition.Name, key);
//        var description = $"chiave di indice '{Encoding.UTF8.GetString(key)}'";
//        var keyAlreadyLocked = context?.WriteLockedKeys.Contains(lockKey) == true;
//        using (_fileHandle.EnterStructuralLock())
//        {
//            if (!keyAlreadyLocked)
//            {
//                _fileHandle.RecordLockManager.AcquireWriteLock(lockKey, _fileHandle.LockTimeoutMilliseconds, description);
//            }
//
//            try
//            {
//                if (context != null)
//                {
//                    context.WriteLockedKeys.Add(lockKey);
//                }
//
//                DeleteRecursive(_rootNodeOffset, null, -1, key, recordOffset);
//
//                var rootPage = LoadNode(_rootNodeOffset);
//                if (rootPage is { IsLeaf: false, KeyCount: 0 })
//                {
//                    var oldRootOffset = _rootNodeOffset;
//                    _rootNodeOffset = ((BPlusInternalNode)rootPage).ChildrenOffsets.First();
//                    UpdateRootOffsetInHeader();
//                    _fileHandle.AddPageToFreeList(oldRootOffset);
//                }
//            }
//            finally
//            {
//                if (context == null && !keyAlreadyLocked)
//                {
//                    _fileHandle.RecordLockManager.ReleaseWriteLock(lockKey);
//                }
//            }
//        }
//    }
//
//    /// <summary>
//    /// Restituisce la definizione completa (<see cref="Config.IndexDefinition"/>) dell'indice gestito da questo manager.
//    /// </summary>
//    /// <returns>La definizione dell'indice.</returns>
//    public Config.IndexDefinition GetIndexDefinition()
//    {
//        return _indexDefinition;
//    }
//
//    /// <summary>
//    /// Reimposta l'offset della radice del manager, tipicamente dopo uno svuotamento.
//    /// </summary>
//    public void ResetRoot(long newRootOffset)
//    {
//        _rootNodeOffset = newRootOffset;
//        _nodeCache.Clear(); // Svuota la cache poiché i vecchi nodi non sono più validi.
//        ClearReadAheadLeaves();
//    }
//
//    /// <summary>
//    /// Restituisce l'offset corrente del nodo radice dell'albero.
//    /// </summary>
//    public long GetRootOffset()
//    {
//        return _rootNodeOffset;
//    }
//
//    #endregion
//
//    #region Node I/O
//
//    /// <summary>
//    /// Retrieves a B+Tree node from a specified physical offset, utilizing an in-memory LRU cache to minimize disk I/O
//    /// only if the locking strategy is Intra-Process.
//    /// </summary>
//    /// <param name="offset">The physical byte offset within the ISAM file where the node's data begins.</param>
//    /// <returns>
//    /// A fully deserialized <see cref="BPlusNode"/> instance (either a <see cref="BPlusLeafNode"/> or
//    /// a <see cref="BPlusInternalNode"/>) ready for use.
//    /// </returns>
//    /// <remarks>
//    /// This method is the central point for node retrieval. It implements a cache-aside pattern:
//    /// 1. If caching is enabled, it first attempts to retrieve the node from the LRU cache.
//    /// 2. On a cache miss or if caching is disabled, it performs a disk seek to the specified offset.
//    /// 3. It reads a boolean flag to determine the node type (leaf or internal) and deserializes it.
//    /// 4. If caching is enabled, the new node instance is added to the cache.
//    /// </remarks>
//    public BPlusNode LoadNode(long offset)
//    {
//        if (offset < 0)
//        {
//            throw new IOException(
//                $"Offset nodo B+ non valido ({offset}). Probabile split radice interna senza salvataggio del fratello.");
//        }
//
//        if (_useNodeCache && _nodeCache.TryGetValue(offset, out var cachedNode))
//        {
//            ConsumeReadAheadSlot(offset);
//            return cachedNode!;
//        }
//
//        if (TryConsumeReadAheadLeaf(offset, out var aheadLeaf))
//        {
//            if (_useNodeCache)
//            {
//                _nodeCache.AddOrUpdate(offset, aheadLeaf);
//            }
//
//            QueueLeafReadAhead(aheadLeaf.NextLeafOffset);
//            return aheadLeaf;
//        }
//
//        var node = LoadNodeFromDisk(offset);
//        if (_useNodeCache)
//        {
//            _nodeCache.AddOrUpdate(offset, node);
//        }
//
//        return node;
//    }
//
//    /// <summary>
//    /// Carica un nodo foglia con read-ahead sulla pagina successiva della catena (SKIP/G0 forward).
//    /// </summary>
//    private BPlusLeafNode LoadLeafNode(long offset)
//    {
//        var node = LoadNode(offset);
//        if (node is not BPlusLeafNode leaf)
//        {
//            throw new IOException($"Offset {offset} non è una foglia B+.");
//        }
//
//        QueueLeafReadAhead(leaf.NextLeafOffset);
//        return leaf;
//    }
//
//    private BPlusNode LoadNodeFromDisk(long offset)
//    {
//        var pageBuffer = ReadFullPageBytes(offset, _fileHandle.GetStream().Length);
//        if (!IsamPageChecksum.TryValidate(pageBuffer, out var storedCrc) && storedCrc != 0)
//        {
//            throw new IOException(
//                $"Checksum pagina B+ non valido a offset {offset} (CRC 0x{storedCrc:X8}).");
//        }
//
//        var leafFlagValue = pageBuffer[0];
//        BPlusNode node = leafFlagValue switch
//        {
//            1 => new BPlusLeafNode(),
//            0 => new BPlusInternalNode(),
//            _ => throw new IOException($"Nodo B+ corrotto a offset {offset}: flag iniziale 0x{leafFlagValue:X2}.")
//        };
//        node.NodeOffset = offset;
//
//        using var nodeStream = new MemoryStream(pageBuffer, writable: false);
//        using var reader = new BinaryReader(nodeStream, _fileHandle.FileEncoding, leaveOpen: true);
//        node.Deserialize(reader);
//        return node;
//    }
//
//    /// <summary>
//    /// Precarica fino a 2 foglie della catena sotto <see cref="IsamFileHandle.RunUnderPhysicalIoLock"/>.
//    /// </summary>
//    private void QueueLeafReadAhead(long nextLeafOffset)
//    {
//        if (!_useNodeCache || nextLeafOffset < 0)
//        {
//            return;
//        }
//
//        _fileHandle.RunUnderPhysicalIoLock(() =>
//        {
//            var offset = nextLeafOffset;
//            for (var slot = 0; slot < ReadAheadLeafSlots && offset >= 0; slot++)
//            {
//                if (_nodeCache.TryGetValue(offset, out var existing) && existing is BPlusLeafNode existingLeaf)
//                {
//                    offset = existingLeaf.NextLeafOffset;
//                    continue;
//                }
//
//                if (IsReadAheadQueued(offset))
//                {
//                    offset = GetReadAheadLeaf(offset)?.NextLeafOffset ?? -1;
//                    continue;
//                }
//
//                var node = LoadNodeFromDisk(offset);
//                if (node is not BPlusLeafNode aheadLeaf)
//                {
//                    return;
//                }
//
//                _readAheadLeafOffsets[slot] = offset;
//                _readAheadLeaves[slot] = aheadLeaf;
//                _nodeCache.AddOrUpdate(offset, aheadLeaf);
//                offset = aheadLeaf.NextLeafOffset;
//            }
//        });
//    }
//
//    private BPlusLeafNode? GetReadAheadLeaf(long offset)
//    {
//        for (var i = 0; i < ReadAheadLeafSlots; i++)
//        {
//            if (_readAheadLeafOffsets[i] == offset)
//            {
//                return _readAheadLeaves[i];
//            }
//        }
//
//        return null;
//    }
//
//    private void ClearReadAheadLeaves()
//    {
//        for (var i = 0; i < ReadAheadLeafSlots; i++)
//        {
//            _readAheadLeafOffsets[i] = -1;
//            _readAheadLeaves[i] = null;
//        }
//    }
//
//    private void ConsumeReadAheadSlot(long offset)
//    {
//        for (var i = 0; i < ReadAheadLeafSlots; i++)
//        {
//            if (_readAheadLeafOffsets[i] != offset)
//            {
//                continue;
//            }
//
//            _readAheadLeafOffsets[i] = -1;
//            _readAheadLeaves[i] = null;
//        }
//    }
//
//    private bool TryConsumeReadAheadLeaf(long offset, out BPlusLeafNode leaf)
//    {
//        for (var i = 0; i < ReadAheadLeafSlots; i++)
//        {
//            if (_readAheadLeafOffsets[i] != offset || _readAheadLeaves[i] is not { } ahead)
//            {
//                continue;
//            }
//
//            _readAheadLeafOffsets[i] = -1;
//            _readAheadLeaves[i] = null;
//            leaf = ahead;
//            return true;
//        }
//
//        leaf = null!;
//        return false;
//    }
//
//    private bool IsReadAheadQueued(long offset)
//    {
//        for (var i = 0; i < ReadAheadLeafSlots; i++)
//        {
//            if (_readAheadLeafOffsets[i] == offset)
//            {
//                return true;
//            }
//        }
//
//        return false;
//    }
//
//    /// <summary>
//    /// Allocates space for a new B+Tree node, implementing an efficient space management strategy.
//    /// It prioritizes recycling pages from a managed free list before resorting to file extension.
//    /// </summary>
//    /// <returns>The physical byte offset of the newly allocated page within the ISAM file.</returns>
//    /// <remarks>
//    /// This method orchestrates the two primary space allocation mechanisms:
//    /// 1.  **Free List Reuse:** It first checks the file header's <c>FreeListHeadOffset</c>. If this pointer is valid
//    ///     (i.e., not -1), it indicates that there is at least one page available for reuse. The method then:
//    ///     a. Retrieves the offset of the first page from the header.
//    ///     b. Reads the first 8 bytes from this freed page, which now contains a pointer to the *next* page in the
//    ///        singly-linked free list.
//    ///     c. Updates the header to point to this next page, effectively "unlinking" the reclaimed page from the list.
//    ///     d. Removes the reclaimed page's offset from the node cache to prevent stale reads.
//    ///
//    /// 2.  **File Extension:** If the free list is empty, the method allocates new space at the end of the file. It:
//    ///     a. Reads the current high-water mark from the header's <c>NextAvailablePageOffset</c>.
//    ///     b. Returns this offset for the new page's location.
//    ///     c. Increments the header's <c>NextAvailablePageOffset</c> by a standard page size (4096 bytes) to
//    ///        prepare for the next allocation.
//    ///
//    /// This strategy minimizes file growth and fragmentation by recycling space wherever possible.
//    /// </remarks>
//    private long AllocateNewPage()
//    {
//        using (_fileHandle.EnterStructuralLock())
//        {
//            var header = _fileHandle.Header;
//
//            if (header.PageFreeListHeadOffset != -1)
//            {
//                var reclaimedPageOffset = header.PageFreeListHeadOffset;
//
//                Span<byte> nextPtr = stackalloc byte[sizeof(long)];
//                _fileHandle.ReadBytesAt(reclaimedPageOffset, nextPtr);
//                var nextFreePageOffset = BitConverter.ToInt64(nextPtr);
//                if (nextFreePageOffset <= 0)
//                {
//                    nextFreePageOffset = -1;
//                }
//
//                var newHeader = header;
//                newHeader.PageFreeListHeadOffset = nextFreePageOffset;
//                _fileHandle.UpdateHeader(newHeader);
//
//                _nodeCache.Remove(reclaimedPageOffset);
//                ConsumeReadAheadSlot(reclaimedPageOffset);
//
//                ZeroPage(reclaimedPageOffset);
//                return reclaimedPageOffset;
//            }
//
//            const int pageSize = 4096;
//            return _fileHandle.ReserveHeapSpace(pageSize);
//        }
//    }
//
//    private void ZeroPage(long pageOffset) => _fileHandle.ZeroIndexPage(pageOffset);
//
//    /// <summary>
//    /// Salva un nodo su disco. Se è un nuovo nodo, alloca nuovo spazio.
//    /// Altrimenti, sovrascrive il nodo esistente.
//    /// </summary>
//    /// <returns>L'offset su disco dove il nodo è stato salvato.</returns>
//    public long SaveNode(BPlusNode node)
//    {
//        var offset = node.NodeOffset;
//
//        if (offset <= 0)
//        {
//            offset = AllocateNewPage();
//            node.NodeOffset = offset;
//        }
//
//        // ponytail: ArrayPool per pagina; WAL/direct path copia ancora ToArray() in FileHandle
//        var pageBuffer = ArrayPool<byte>.Shared.Rent(PageSize);
//        try
//        {
//            pageBuffer.AsSpan(0, PageSize).Clear();
//            using (var ms = new MemoryStream(pageBuffer, 0, PageSize, writable: true, publiclyVisible: true))
//            using (var writer = new BinaryWriter(ms, _fileHandle.FileEncoding, leaveOpen: true))
//            {
//                ms.SetLength(0);
//                node.Serialize(writer);
//                var serializedLength = (int)ms.Position;
//                if (serializedLength > PageSize)
//                {
//                    throw new InvalidOperationException(
//                        $"Nodo B+ serializzato ({serializedLength} byte) supera la dimensione pagina ({PageSize}).");
//                }
//            }
//
//            IsamPageChecksum.WriteChecksum(pageBuffer.AsSpan(0, PageSize));
//            _fileHandle.WriteIndexPage(offset, pageBuffer.AsSpan(0, PageSize));
//            _nodeCache.AddOrUpdate(offset, node);
//            ConsumeReadAheadSlot(offset);
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(pageBuffer);
//        }
//
//        return offset;
//    }
//
//    /// <summary>
//    /// Verifica i checksum di tutte le pagine dell'indice. Pagine legacy (CRC zero) sono accettate.
//    /// </summary>
//    internal bool ValidatePageChecksums(out string? errorMessage)
//    {
//        errorMessage = null;
//        if (_rootNodeOffset == -1)
//        {
//            return true;
//        }
//
//        try
//        {
//            return ValidatePageChecksumsRecursive(_rootNodeOffset, new HashSet<long>());
//        }
//        catch (IOException ex)
//        {
//            errorMessage = ex.Message;
//            return false;
//        }
//    }
//
//    private bool ValidatePageChecksumsRecursive(long pageOffset, HashSet<long> visited)
//    {
//        if (!visited.Add(pageOffset))
//        {
//            return true;
//        }
//
//        var page = ReadFullPageBytes(pageOffset, _fileHandle.GetStream().Length);
//        if (!IsamPageChecksum.TryValidate(page, out var storedCrc) && storedCrc != 0)
//        {
//            throw new IOException(
//                $"Checksum pagina B+ non valido a offset {pageOffset} (CRC 0x{storedCrc:X8}).");
//        }
//
//        var flag = page[0];
//        if (flag == 0)
//        {
//            using var ms = new MemoryStream(page, writable: false);
//            using var reader = new BinaryReader(ms, _fileHandle.FileEncoding, leaveOpen: true);
//            reader.ReadBoolean();
//            var keyCount = reader.ReadInt32();
//            for (var i = 0; i < keyCount; i++)
//            {
//                var keyLength = reader.ReadInt32();
//                reader.ReadBytes(keyLength);
//            }
//
//            var childCount = reader.ReadInt32();
//            for (var i = 0; i < childCount; i++)
//            {
//                var child = reader.ReadInt64();
//                if (child > 0)
//                {
//                    ValidatePageChecksumsRecursive(child, visited);
//                }
//            }
//        }
//
//        return true;
//    }
//
//    private byte[] ReadFullPageBytes(long offset, long fileLength) =>
//        _fileHandle.ReadPageBytes(offset, fileLength);
//
//    /// <summary>Media fill factor foglie (debug / test ISAM-C6).</summary>
//    internal double MeasureAverageLeafFillFactor()
//    {
//        if (_rootNodeOffset == -1)
//        {
//            return 0;
//        }
//
//        var maxKeys = GetMaxKeysPerNode();
//        if (maxKeys <= 0)
//        {
//            return 0;
//        }
//
//        var firstLeaf = FindFirstLeaf();
//        var totalKeys = 0;
//        var leafCount = 0;
//        var current = firstLeaf;
//        while (true)
//        {
//            totalKeys += current.KeyCount;
//            leafCount++;
//            if (current.NextLeafOffset == -1)
//            {
//                break;
//            }
//
//            current = (BPlusLeafNode)LoadNode(current.NextLeafOffset);
//        }
//
//        return leafCount == 0 ? 0 : (double)totalKeys / (leafCount * maxKeys);
//    }
//
//    #endregion
//
//    #region Maintenance
//
//    /// <summary>
//    /// Aggiorna i puntatori interni alle pagine dopo lo spostamento dell'area heap su disco.
//    /// </summary>
//    internal void RepairPointersAfterHeapMove(long heapStart, long heapEnd, long delta)
//    {
//        _nodeCache.Clear();
//        ClearReadAheadLeaves();
//        if (_rootNodeOffset == -1)
//        {
//            return;
//        }
//
//        var visited = new HashSet<long>();
//        var queue = new Queue<long>();
//        queue.Enqueue(_rootNodeOffset);
//
//        while (queue.Count > 0)
//        {
//            var pageOffset = queue.Dequeue();
//            if (!visited.Add(pageOffset))
//            {
//                continue;
//            }
//
//            var node = LoadNode(pageOffset);
//            node.NodeOffset = pageOffset;
//
//            switch (node)
//            {
//                case BPlusInternalNode internalNode:
//                    for (var i = 0; i < internalNode.ChildrenOffsets.Count; i++)
//                    {
//                        var child = internalNode.ChildrenOffsets[i];
//                        if (child >= heapStart && child < heapEnd)
//                        {
//                            internalNode.ChildrenOffsets[i] = child + delta;
//                        }
//
//                        if (internalNode.ChildrenOffsets[i] >= heapStart)
//                        {
//                            queue.Enqueue(internalNode.ChildrenOffsets[i]);
//                        }
//                    }
//
//                    break;
//                case BPlusLeafNode leaf:
//                    if (leaf.NextLeafOffset >= heapStart && leaf.NextLeafOffset < heapEnd)
//                    {
//                        leaf.NextLeafOffset += delta;
//                    }
//
//                    if (leaf.PrevLeafOffset >= heapStart && leaf.PrevLeafOffset < heapEnd)
//                    {
//                        leaf.PrevLeafOffset += delta;
//                    }
//
//                    // Ogni voce di RecordOffsets punta al byte fisico del record dati a cui la chiave
//                    // si riferisce: se il record è stato spostato dallo shift (perché anch'esso nella
//                    // regione [heapStart, heapEnd)), il puntatore va corretto qui, altrimenti la foglia
//                    // continua a puntare alla vecchia posizione (ora sovrascritta o disallineata),
//                    // producendo record fantasma/corrotti in lettura (bug scoperto testando
//                    // DbIsamFullTests.bas: CREATE+DROP INDEX seguiti da PACK/REINDEX su tabella popolata).
//                    foreach (var offsets in leaf.RecordOffsets)
//                    {
//                        for (var i = 0; i < offsets.Count; i++)
//                        {
//                            if (offsets[i] >= heapStart && offsets[i] < heapEnd)
//                            {
//                                offsets[i] += delta;
//                            }
//                        }
//                    }
//
//                    if (leaf.NextLeafOffset >= heapStart)
//                    {
//                        queue.Enqueue(leaf.NextLeafOffset);
//                    }
//
//                    break;
//            }
//
//            SaveNode(node);
//        }
//    }
//
//    /// <summary>
//    /// Esegue una serie di controlli per verificare l'integrità strutturale dell'indice B+Tree.
//    /// </summary>
//    /// <returns>True se l'indice è strutturalmente valido, altrimenti False.</returns>
//    /// <remarks>
//    /// Questa operazione è non distruttiva e a sola lettura. Esegue una traversata completa
//    /// dell'albero per validare gli invarianti, come il numero di chiavi per nodo, la coerenza
//    /// dei puntatori e l'assenza di cicli.
//    /// </remarks>
//    public bool ValidateIndex()
//    {
//        lock (_syncRoot)
//        {
//            try
//            {
//                if (_rootNodeOffset == -1) return true;
//
//                if (!ValidatePageRecursive(_rootNodeOffset, new HashSet<long>()))
//                    return false;
//
//                if (!CheckLeafListConsistency())
//                    return false;
//
//                return true;
//            }
//            catch
//            {
//                return false;
//            }
//        }
//    }
//
//    /// <summary>
//    /// Metodo helper ricorsivo per validare una pagina e tutti i suoi discendenti.
//    /// </summary>
//    private bool ValidatePageRecursive(long pageOffset, HashSet<long> visitedPages)
//    {
//        // Rileva cicli: se abbiamo già visitato questa pagina, l'indice è corrotto.
//        if (!visitedPages.Add(pageOffset)) return false;
//
//        var page = LoadNode(pageOffset);
//
//        // Controlla che il numero di chiavi sia entro i limiti validi.
//        var maxKeys = page is BPlusInternalNode
//            ? GetMaxKeysForInternalNode()
//            : GetMaxKeysPerNode();
//        if (page.KeyCount > maxKeys)
//        {
//            return false;
//        }
//
//        // La radice è l'unica pagina che può avere meno del numero minimo di chiavi.
//        if (pageOffset != _rootNodeOffset)
//        {
//            var minKeys = page is BPlusInternalNode
//                ? GetMinKeysForInternalNode()
//                : GetMinKeysPerNode();
//            if (page.KeyCount < minKeys)
//            {
//                return false;
//            }
//        }
//
//        if (page is BPlusInternalNode internalNode)
//        {
//            // Se è un nodo interno, valida ricorsivamente tutti i suoi figli.
//            foreach (var childOffset in internalNode.ChildrenOffsets)
//            {
//                if (childOffset <= 0 || !ValidatePageRecursive(childOffset, visitedPages))
//                    return false;
//            }
//        }
//
//        return true;
//    }
//
//    /// <summary>
//    /// Verifica la coerenza della lista doppiamente concatenata dei nodi foglia.
//    /// </summary>
//    private bool CheckLeafListConsistency()
//    {
//        if (_rootNodeOffset == -1) return true;
//
//        var firstLeaf = FindFirstLeaf();
//        var current = firstLeaf;
//        long prevOffset = -1;
//
//        // Scorre in avanti
//        while (current.NextLeafOffset != -1)
//        {
//            if (current.PrevLeafOffset != prevOffset) return false; // Il puntatore all'indietro non corrisponde
//
//            prevOffset = current.NodeOffset;
//            var nextNode = LoadNode(current.NextLeafOffset);
//            if (nextNode is not BPlusLeafNode node) return false; // Un nodo nella lista delle foglie non è una foglia
//
//            current = node;
//        }
//
//        // Ora scorre all'indietro partendo dall'ultimo nodo
//        while (current.PrevLeafOffset != -1)
//        {
//            var prevNode = LoadNode(current.PrevLeafOffset);
//            if (prevNode is not BPlusLeafNode leafPrev) return false;
//            if (leafPrev.NextLeafOffset != current.NodeOffset) return false; // Il puntatore in avanti non corrisponde
//
//            current = leafPrev;
//        }
//
//        // Se il ciclo all'indietro ci ha riportato al primo nodo, la lista è consistente.
//        return current.NodeOffset == firstLeaf.NodeOffset;
//    }
//
//    #endregion
//
//    #region Private Helpers
//
//    /// <summary>
//    /// Calcola il numero minimo di chiavi che un nodo deve avere per essere considerato valido
//    /// (tipicamente la metà della capacità massima).
//    /// </summary>
//    private int GetMinKeysPerNode() => BPlusFillFactor.ComputeMinKeys(GetMaxKeysPerNode());
//
//    private int GetMinKeysForInternalNode() => BPlusFillFactor.ComputeMinKeys(GetMaxKeysForInternalNode());
//
//    /// <summary>
//    /// Metodo ricorsivo per la cancellazione che scende l'albero e gestisce il ribilanciamento
//    /// (merge o ridistribuzione) durante la risalita.
//    /// </summary>
//    /// <param name="pageOffset">L'offset della pagina corrente da processare.</param>
//    /// <param name="parent">Il nodo padre della pagina corrente (null per la radice).</param>
//    /// <param name="parentIndex">L'indice del puntatore, nel padre, che punta alla pagina corrente.</param>
//    /// <param name="key">La chiave da rimuovere.</param>
//    /// <param name="recordOffset">Lo specifico offset del record da rimuovere.</param>
//    private void DeleteRecursive(long pageOffset, BPlusInternalNode? parent, int parentIndex, byte[] key,
//        long recordOffset)
//    {
//        var page = LoadNode(pageOffset);
//
//        if (page.IsLeaf)
//        {
//            // --- CASO BASE: Siamo in una foglia ---
//            var leaf = (BPlusLeafNode)page;
//            if (leaf.Remove(key, recordOffset))
//            {
//                SaveNode(leaf);
//            }
//
//            return;
//        }
//
//        // --- CASO RICORSIVO: Siamo in un nodo interno ---
//        var internalNode = (BPlusInternalNode)page;
//        var childIndex = internalNode.FindChildIndexFor(key);
//        var childOffset = internalNode.ChildrenOffsets[childIndex];
//
//        // Scende ricorsivamente al figlio.
//        DeleteRecursive(childOffset, internalNode, childIndex, key, recordOffset);
//
//        // Dopo la chiamata, controlla se il figlio è in underflow.
//        var childNode = LoadNode(childOffset);
//        if (childNode.KeyCount < GetMinKeysPerNode())
//        {
//            // Se il figlio è in underflow, gestisci il ribilanciamento.
//            HandleUnderflow(internalNode, childIndex);
//        }
//    }
//
//    /// <summary>
//    /// Gestisce una pagina figlio che è andata in "underflow" (troppo poche chiavi).
//    /// </summary>
//    /// <summary>
//    /// Gestisce una pagina figlio che è andata in "underflow" (troppo poche chiavi) dopo una cancellazione,
//    /// tentando prima una ridistribuzione (prestito) da un fratello e, se non possibile, un merge (unione).
//    /// </summary>
//    /// <param name="parent">La pagina padre del nodo in underflow.</param>
//    /// <param name="childInUnderflowIndex">L'indice del puntatore, all'interno del padre, che punta al nodo in underflow.</param>
//    private void HandleUnderflow(BPlusInternalNode parent, int childInUnderflowIndex)
//    {
//        // Tenta di ridistribuire dal fratello sinistro
//        if (childInUnderflowIndex > 0)
//        {
//            var leftSibling = LoadNode(parent.ChildrenOffsets[childInUnderflowIndex - 1]);
//            if (leftSibling.KeyCount > GetMinKeysPerNode())
//            {
//                Redistribute(parent, childInUnderflowIndex - 1, leftSibling,
//                    LoadNode(parent.ChildrenOffsets[childInUnderflowIndex]));
//                return;
//            }
//        }
//
//        // Tenta di ridistribuire dal fratello destro
//        if (childInUnderflowIndex < parent.KeyCount)
//        {
//            var rightSibling = LoadNode(parent.ChildrenOffsets[childInUnderflowIndex + 1]);
//            if (rightSibling.KeyCount > GetMinKeysPerNode())
//            {
//                Redistribute(parent, childInUnderflowIndex, LoadNode(parent.ChildrenOffsets[childInUnderflowIndex]),
//                    rightSibling);
//                return;
//            }
//        }
//
//        // Se non è possibile, esegue il merge
//        if (childInUnderflowIndex > 0)
//        {
//            Merge(parent, childInUnderflowIndex - 1);
//        }
//        else
//        {
//            Merge(parent, childInUnderflowIndex);
//        }
//    }
//
//    /// <summary>
//    /// Ribalancia due nodi fratelli adiacenti quando uno è in underflow e l'altro ha chiavi da cedere.
//    /// L'operazione sposta una chiave/puntatore dal fratello più pieno a quello in underflow, passando attraverso il padre.
//    /// </summary>
//    /// <param name="parent">La pagina padre comune.</param>
//    /// <param name="leftNodeIndexInParent">L'indice del puntatore, nel padre, che punta al nodo di sinistra dei due fratelli.</param>
//    /// <param name="leftChild">Il nodo fratello di sinistra.</param>
//    /// <param name="rightChild">Il nodo fratello di destra.</param>
//    internal void Redistribute(BPlusInternalNode parent, int leftNodeIndexInParent, BPlusNode leftChild,
//        BPlusNode rightChild)
//    {
//        // Se il figlio sinistro ha bisogno di chiavi, le prende dal destro
//        if (leftChild.KeyCount < GetMinKeysPerNode())
//        {
//            if (leftChild.IsLeaf)
//            {
//                var leftLeaf = (BPlusLeafNode)leftChild;
//                var rightLeaf = (BPlusLeafNode)rightChild;
//
//                // Prendi la prima entry dal fratello destro
//                var borrowedKey = rightLeaf.Keys.First();
//                var borrowedOffsets = rightLeaf.RecordOffsets.First();
//                rightLeaf.Keys.RemoveAt(0);
//                rightLeaf.RecordOffsets.RemoveAt(0);
//
//                // Aggiungila in coda al fratello sinistro
//                leftLeaf.Keys.Add(borrowedKey);
//                leftLeaf.RecordOffsets.Add(borrowedOffsets);
//
//                // Aggiorna la chiave separatrice nel padre con la nuova prima chiave del fratello destro
//                parent.Keys[leftNodeIndexInParent] = rightLeaf.Keys.First();
//            }
//            else // Nodi Interni
//            {
//                var leftInternal = (BPlusInternalNode)leftChild;
//                var rightInternal = (BPlusInternalNode)rightChild;
//
//                // Tira giù la chiave separatrice dal padre
//                leftInternal.Keys.Add(parent.Keys[leftNodeIndexInParent]);
//                // Prendi il primo puntatore dal fratello destro
//                leftInternal.ChildrenOffsets.Add(rightInternal.ChildrenOffsets.First());
//                rightInternal.ChildrenOffsets.RemoveAt(0);
//
//                // La prima chiave del fratello destro diventa la nuova chiave separatrice
//                parent.Keys[leftNodeIndexInParent] = rightInternal.Keys.First();
//                rightInternal.Keys.RemoveAt(0);
//            }
//        }
//        else // Altrimenti, il figlio destro ha bisogno di chiavi e le prende dal sinistro
//        {
//            if (leftChild.IsLeaf)
//            {
//                var leftLeaf = (BPlusLeafNode)leftChild;
//                var rightLeaf = (BPlusLeafNode)rightChild;
//
//                // Prendi l'ultima entry dal fratello sinistro
//                var borrowedKey = leftLeaf.Keys.Last();
//                var borrowedOffsets = leftLeaf.RecordOffsets.Last();
//                leftLeaf.Keys.RemoveAt(leftLeaf.KeyCount - 1);
//                leftLeaf.RecordOffsets.RemoveAt(leftLeaf.RecordOffsets.Count - 1);
//
//                // Aggiungila all'inizio del fratello destro
//                rightLeaf.Keys.Insert(0, borrowedKey);
//                rightLeaf.RecordOffsets.Insert(0, borrowedOffsets);
//
//                // Aggiorna la chiave separatrice nel padre con la nuova prima chiave del fratello destro
//                parent.Keys[leftNodeIndexInParent] = rightLeaf.Keys.First();
//            }
//            else // Nodi Interni
//            {
//                var leftInternal = (BPlusInternalNode)leftChild;
//                var rightInternal = (BPlusInternalNode)rightChild;
//
//                // Tira giù la chiave separatrice
//                rightInternal.Keys.Insert(0, parent.Keys[leftNodeIndexInParent]);
//                // Prendi l'ultimo puntatore dal fratello sinistro
//                rightInternal.ChildrenOffsets.Insert(0, leftInternal.ChildrenOffsets.Last());
//                leftInternal.ChildrenOffsets.RemoveAt(leftInternal.ChildrenOffsets.Count - 1);
//
//                // L'ultima chiave del fratello sinistro diventa la nuova chiave separatrice
//                parent.Keys[leftNodeIndexInParent] = leftInternal.Keys.Last();
//                leftInternal.Keys.RemoveAt(leftInternal.KeyCount - 1);
//            }
//        }
//
//        // Salva tutte e tre le pagine modificate su disco
//        SaveNode(parent);
//        SaveNode(leftChild);
//        SaveNode(rightChild);
//    }
//
//    /// <summary>
//    /// Unisce due nodi fratelli adiacenti (sinistro e destro) quando entrambi sono al minimo numero di chiavi o meno.
//    /// L'operazione sposta tutto il contenuto del nodo destro nel nodo sinistro, tira giù la chiave separatrice
//    /// dal nodo padre e rimuove il puntatore al nodo destro dal padre.
//    /// </summary>
//    /// <param name="parent">Il nodo padre che contiene i puntatori ai due figli da unire.</param>
//    /// <param name="leftNodeIndexInParent">
//    /// L'indice, nella lista dei puntatori del padre, che punta al nodo di sinistra dei due fratelli.
//    /// Il nodo di destra si trova implicitamente all'indice `leftNodeIndexInParent + 1`.
//    /// </param>
//    /// <remarks>
//    /// Questa operazione è l'inverso dello split e viene invocata per gestire l'underflow quando la ridistribuzione non è possibile.
//    /// La pagina del nodo destro viene aggiunta alla free list per il riutilizzo.
//    /// Se l'unione coinvolge nodi foglia, la lista doppiamente concatenata viene aggiornata di conseguenza.
//    /// </remarks>
//    internal void Merge(BPlusInternalNode parent, int leftNodeIndexInParent)
//    {
//        var leftChild = LoadNode(parent.ChildrenOffsets[leftNodeIndexInParent]);
//        var rightChild = LoadNode(parent.ChildrenOffsets[leftNodeIndexInParent + 1]);
//
//        // Tira giù la chiave separatrice dal padre
//        var separatorKey = parent.Keys[leftNodeIndexInParent];
//
//        if (leftChild.IsLeaf)
//        {
//            var leftLeaf = (BPlusLeafNode)leftChild;
//            var rightLeaf = (BPlusLeafNode)rightChild;
//
//            // Aggiunge tutte le entry del fratello destro a quello sinistro
//            leftLeaf.Keys.AddRange(rightLeaf.Keys);
//            leftLeaf.RecordOffsets.AddRange(rightLeaf.RecordOffsets);
//
//            // Aggiorna la lista concatenata per "scavalcare" il nodo destro
//            leftLeaf.NextLeafOffset = rightLeaf.NextLeafOffset;
//            if (rightLeaf.NextLeafOffset != -1)
//            {
//                var nextNext = (BPlusLeafNode)LoadNode(rightLeaf.NextLeafOffset);
//                // Aggiorna il puntatore all'indietro del nodo successivo perché punti al nodo sinistro
//                nextNext.PrevLeafOffset = leftChild.NodeOffset;
//                SaveNode(nextNext);
//            }
//        }
//        else // Nodi interni
//        {
//            var leftInternal = (BPlusInternalNode)leftChild;
//            var rightInternal = (BPlusInternalNode)rightChild;
//
//            // Tira giù la chiave separatrice e la aggiunge al nodo sinistro
//            leftInternal.Keys.Add(separatorKey);
//            // Aggiunge tutte le chiavi e i puntatori del fratello destro
//            leftInternal.Keys.AddRange(rightInternal.Keys);
//            leftInternal.ChildrenOffsets.AddRange(rightInternal.ChildrenOffsets);
//        }
//
//        // Rimuove la chiave e il puntatore dal padre, che ora sono ridondanti
//        parent.Keys.RemoveAt(leftNodeIndexInParent);
//        parent.ChildrenOffsets.RemoveAt(leftNodeIndexInParent + 1);
//
//        SaveNode(leftChild);
//        SaveNode(parent);
//        _fileHandle.AddPageToFreeList(rightChild.NodeOffset);
//    }
//
//    /// <summary>
//    /// Naviga l'albero B+ a partire dalla radice per trovare il nodo foglia più a sinistra.
//    /// Questo nodo è il primo nella sequenza logica dell'indice e contiene le chiavi con i valori più bassi.
//    /// </summary>
//    /// <returns>
//    /// L'istanza di <see cref="BPlusLeafNode"/> che rappresenta il primo nodo foglia dell'albero.
//    /// </returns>
//    /// <remarks>
//    /// L'operazione consiste nel seguire ricorsivamente il primo puntatore (indice 0) di ogni
//    /// nodo interno fino a raggiungere un nodo foglia. Questo metodo è il punto di partenza
//    /// per operazioni come GO TOP e per le scansioni sequenziali complete dell'indice.
//    /// </remarks>
//    internal BPlusLeafNode FindFirstLeaf()
//    {
//        if (_rootNodeOffset == -1)
//        {
//            return new BPlusLeafNode { NodeOffset = -1 };
//        }
//
//        var node = LoadNode(_rootNodeOffset);
//        while (!node.IsLeaf)
//        {
//            node = LoadNode(((BPlusInternalNode)node).ChildrenOffsets[0]);
//        }
//
//        return (BPlusLeafNode)node;
//    }
//
//    /// <summary>
//    /// Navigates backwards from a given leaf node to find its immediate predecessor in the doubly-linked list.
//    /// </summary>
//    /// <param name="currentLeaf">The leaf node from which to start the backward navigation.</param>
//    /// <returns>The preceding <see cref="BPlusLeafNode"/>, or <c>null</c> if <paramref name="currentLeaf"/> is the
//    /// first leaf in the sequence.</returns>
//    /// <remarks>
//    /// This operation is highly efficient (O(1)) as it directly uses the <c>PrevLeafOffset</c> pointer
//    /// stored within the leaf node structure, avoiding a full scan of the index.
//    /// </remarks>
//    private BPlusLeafNode? FindPreviousLeaf(BPlusLeafNode currentLeaf)
//    {
//        // Utilizza il puntatore diretto 'PrevLeafOffset' per un'operazione O(1)
//        // // invece di rieseguire la scansione dell'intera lista di foglie dall'inizio (O(N)).
//        if (currentLeaf.PrevLeafOffset == -1)
//        {
//            // Questo è il primo nodo foglia, non ha predecessori.
//            return null;
//        }
//
//        // Carica direttamente il nodo precedente usando l'offset memorizzato.
//        return (BPlusLeafNode)LoadNode(currentLeaf.PrevLeafOffset);
//    }
//
//    /// <summary>
//    /// Esegue una ricerca inversa per trovare la posizione precisa di una entry dell'indice di un record
//    /// all'interno dell'albero B+. Dato l'offset fisico di un record, questo metodo determina quale
//    /// nodo foglia contiene la sua chiave, l'indice di quella chiave all'interno del nodo e la sua
//    /// posizione nella lista degli offset duplicati.
//    /// </summary>
//    /// <param name="recordOffset">L'offset fisico in byte del record dati da localizzare all'interno dell'indice.</param>
//    /// <param name="evaluator">Il callback fornito dall'Engine per calcolare la chiave del record.</param>
//    /// <returns>
//    /// Una tupla contenente:
//    /// - <c>leaf</c>: Il <see cref="BPlusLeafNode"/> dove risiede la entry dell'indice.
//    /// - <c>keyIndex</c>: L'indice 0-based della chiave all'interno della lista di chiavi della foglia.
//    /// - <c>offsetIndex</c>: L'indice 0-based di <paramref name="recordOffset"/> all'interno della lista di duplicati per quella chiave.
//    /// Restituisce <c>(null, -1, -1)</c> se la chiave del record non viene trovata nell'indice o se viene rilevata un'inconsistenza.
//    /// </returns>
//    /// <remarks>
//    /// Questo metodo è il pilastro della navigazione sequenziale (es. SKIP). Funziona leggendo prima l'intero
//    /// record dal disco, usando l'evaluator per estrarne il valore della chiave, quindi attraversando l'albero B+
//    /// per localizzare il nodo foglia e la entry specifici.
//    /// </remarks>
//    private (BPlusLeafNode? leaf, int keyIndex, int offsetIndex) FindRecordPosition(long recordOffset,
//        KeyExpressionEvaluatorCallback evaluator)
//    {
//        // 1. Legge il record dal disco.
//        var buffer = new byte[_fileHandle.Header.RecordSize];
//        var stream = _fileHandle.GetStream();
//        stream.Seek(recordOffset, SeekOrigin.Begin);
//        stream.ReadExactly(buffer, 0, buffer.Length);
//
//        var udtDef = _fileHandle.UdtDefinition;
//        var (_, udtInstance) = RecordMarshaller.DeserializeRecord(_schema, buffer, udtDef, _fileHandle);
//
//        // Poiché IndexDefinition usa ancora una stringa per il nome, si confronta con 'f.Name.Lexeme'.
//        var udtField = (udtDef.Fields as IEnumerable<TypeField>).FirstOrDefault(f =>
//            f.Name.Lexeme.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));
//
//        var targetType = udtField?.DataType ?? new StringType(_indexDefinition.KeyLength);
//
//        // CORREZIONE ARCHITETTURALE: Marshalling da ID-based a string-based.
//        // Il delegato 'evaluator' si aspetta un dizionario con chiavi stringa.
//        // Creiamo una mappa di conversione una-tantum dai SymbolId ai nomi dei campi.
//        var idToNameMap = udtDef.Fields.ToDictionary(f => f.SymbolId, f => f.Name.Lexeme);
//
//        // Convertiamo il dizionario Fields dell'istanza UDT nel formato richiesto.
//        var stringKeyedFields = udtInstance.Fields.ToDictionary(
//            kvp => idToNameMap[kvp.Key], // Converte SymbolId (int) in nome (string)
//            kvp => kvp.Value
//        );
//
//        var keyValue = evaluator(_indexDefinition.KeyExpression, stringKeyedFields, targetType);
//        var key = SchemaManager.SerializeKeyValue(keyValue, _indexDefinition);
//
//        if (_rootNodeOffset == -1) return (null, -1, -1);
//
//        // 2. Scende l'albero per trovare il nodo foglia corretto per questa chiave.
//        var node = LoadNode(_rootNodeOffset);
//        while (!node.IsLeaf)
//        {
//            var internalNode = (BPlusInternalNode)node;
//            var index = internalNode.Keys.BinarySearch(key, new ByteArrayComparer());
//            if (index < 0) index = ~index;
//            else index += 1;
//
//            node = LoadNode(internalNode.ChildrenOffsets[index]);
//        }
//
//        var leaf = (BPlusLeafNode)node;
//
//        // 3. Cerca la chiave all'interno del nodo foglia.
//        var keyIndex = leaf.Keys.BinarySearch(key, new ByteArrayComparer());
//        if (keyIndex < 0)
//        {
//            return (null, -1, -1); // Inconsistenza tra dati e indice.
//        }
//
//        // 4. Cerca l'offset specifico del record all'interno della lista di duplicati.
//        var offsetIndex = leaf.RecordOffsets[keyIndex].IndexOf(recordOffset);
//        if (offsetIndex < 0)
//        {
//            return (null, -1, -1); // Altra inconsistenza.
//        }
//
//        return (leaf, keyIndex, offsetIndex);
//    }
//
//    /// <summary>
//    /// Persists the current in-memory root node offset to the file's main header.
//    /// This ensures that the database correctly points to the root of the B+Tree when it is next opened.
//    /// </summary>
//    /// <remarks>
//    /// The method correctly identifies whether the update is for the primary key index or for one of the
//    /// secondary indexes by comparing the manager's index field name against the schema definition.
//    /// It then modifies the appropriate field in the header structure (<c>PrimaryKeyRootOffset</c> or an
//    /// element of <c>SecondaryIndexRootOffsets</c>) and delegates the write operation to the file handle.
//    /// This is a critical operation that must be called after any structural change to the tree root, such as
//    /// a root split or a tree becoming empty.
//    /// </remarks>
//    private bool IsValidRecordOffset(long recordOffset)
//    {
//        var dataAreaStart = _fileHandle.DataAreaStartOffset;
//        if (recordOffset < dataAreaStart)
//        {
//            return false;
//        }
//
//        var relative = recordOffset - dataAreaStart;
//        return relative % _fileHandle.Header.RecordSize == 0;
//    }
//
//    private void RefreshRootOffsetFromHeader()
//    {
//        var header = _fileHandle.Header;
//        if (_indexDefinition.Name.Equals(_schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
//        {
//            _rootNodeOffset = header.PrimaryKeyRootOffset;
//            return;
//        }
//
//        var secondaryIndexPosition = _schema.SecondaryKeys.FindIndex(def =>
//            def.Name.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));
//        if (secondaryIndexPosition != -1)
//        {
//            _rootNodeOffset = header.SecondaryIndexRootOffsets[secondaryIndexPosition];
//        }
//    }
//
//    private void UpdateRootOffsetInHeader()
//    {
//        using (_fileHandle.EnterStructuralLock())
//        {
//            var header = _fileHandle.Header;
//
//            if (_indexDefinition.Name.Equals(_schema.PrimaryKey.Name, StringComparison.OrdinalIgnoreCase))
//            {
//                header.PrimaryKeyRootOffset = _rootNodeOffset;
//            }
//            else
//            {
//                var secondaryIndexPosition = _schema.SecondaryKeys.FindIndex(def =>
//                    def.Name.Equals(_indexDefinition.Name, StringComparison.OrdinalIgnoreCase));
//
//                if (secondaryIndexPosition != -1)
//                {
//                    header.SecondaryIndexRootOffsets[secondaryIndexPosition] = _rootNodeOffset;
//                }
//                else
//                {
//                    throw new InvalidOperationException(
//                        $"Indice secondario '{_indexDefinition.Name}' non trovato nello schema.");
//                }
//            }
//
//            _fileHandle.UpdateHeader(header);
//        }
//    }
//
//    private byte[] ReadNodeBytes(long offset, long fileLength)
//    {
//        const int pageSize = 4096;
//        var bytesToRead = (int)Math.Min(pageSize, Math.Max(0, fileLength - offset));
//        if (bytesToRead <= 0)
//        {
//            throw new EndOfStreamException($"Offset nodo B+ non valido: {offset}, lunghezza file: {fileLength}.");
//        }
//
//        var buffer = new byte[bytesToRead];
//        _fileHandle.ReadBytesAt(offset, buffer);
//        return buffer;
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define BPlusPage_Size 4096

typedef struct {
    uint8_t* p;
    int n, cap;
} Bw;
typedef struct {
    const uint8_t* p;
    int n, i;
} Br;

static void bw_grow(Bw* b, int add)
{
    if (b->n + add <= b->cap) return;
    b->cap = b->cap ? b->cap * 2 : 256;
    while (b->n + add > b->cap) b->cap *= 2;
    b->p = (uint8_t*)realloc(b->p, (size_t)b->cap);
}
static void bw_u8(Bw* b, uint8_t v) { bw_grow(b, 1); b->p[b->n++] = v; }
static void bw_i32(Bw* b, int32_t v)
{
    bw_grow(b, 4);
    b->p[b->n++] = (uint8_t)v; b->p[b->n++] = (uint8_t)(v >> 8);
    b->p[b->n++] = (uint8_t)(v >> 16); b->p[b->n++] = (uint8_t)(v >> 24);
}
static void bw_i64(Bw* b, int64_t v)
{
    int i;
    bw_grow(b, 8);
    for (i = 0; i < 8; i++) b->p[b->n++] = (uint8_t)((uint64_t)v >> (8 * i));
}
static void bw_bytes(Bw* b, const uint8_t* d, int n)
{
    if (n <= 0) return;
    bw_grow(b, n);
    memcpy(b->p + b->n, d, (size_t)n);
    b->n += n;
}

static int br_u8(Br* r, uint8_t* v)
{
    if (r->i >= r->n) return -1;
    *v = r->p[r->i++];
    return 0;
}
static int br_i32(Br* r, int32_t* v)
{
    if (r->i + 4 > r->n) return -1;
    *v = (int32_t)(r->p[r->i] | (r->p[r->i + 1] << 8) | (r->p[r->i + 2] << 16) | (r->p[r->i + 3] << 24));
    r->i += 4;
    return 0;
}
static int br_i64(Br* r, int64_t* v)
{
    int i;
    uint64_t u = 0;
    if (r->i + 8 > r->n) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)r->p[r->i++] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

typedef struct BPlusNode {
    int64_t NodeOffset;
    int IsLeaf;
    int KeyCount, KeyCap;
    uint8_t** Keys;
    int* KeyLens;
    int64_t NextLeafOffset, PrevLeafOffset;
    int64_t** Recs;
    int* RecCounts;
    int64_t* Children;
    int ChildCount, ChildCap;
} BPlusNode;

typedef struct {
    int NodeUpdated;
    BPlusNode* NewNode;
    int64_t NewNodeOffset;
    uint8_t* PromotedKey;
    int PromotedKeyLen;
} BPlusInsertResult;

struct BPlusTreeManager {
    FILE* fs;
    IsamFileHandle* handle; /* shared header when Engine owns the file */
    char FilePath[PATH_MAX];
    int KeyLength;
    int Unique;
    int ownsFile;
    int secondarySlot; /* -1 = PK root; 0..14 = SecondaryIndexRootOffsets[slot] */
    int64_t rootNodeOffset;
    IsamLruCache* nodeCache;
    pthread_mutex_t syncRoot;
};

static BPlusNode* BPlusNode_New(int isLeaf)
{
    BPlusNode* n = (BPlusNode*)calloc(1, sizeof(*n));
    n->IsLeaf = isLeaf;
    n->NodeOffset = 0;
    n->NextLeafOffset = -1;
    n->PrevLeafOffset = -1;
    return n;
}

static void BPlusNode_EnsureKeys(BPlusNode* n, int need)
{
    if (n->KeyCap >= need) return;
    n->KeyCap = n->KeyCap ? n->KeyCap * 2 : 8;
    while (n->KeyCap < need) n->KeyCap *= 2;
    n->Keys = (uint8_t**)realloc(n->Keys, (size_t)n->KeyCap * sizeof(uint8_t*));
    n->KeyLens = (int*)realloc(n->KeyLens, (size_t)n->KeyCap * sizeof(int));
    if (n->IsLeaf) {
        n->Recs = (int64_t**)realloc(n->Recs, (size_t)n->KeyCap * sizeof(int64_t*));
        n->RecCounts = (int*)realloc(n->RecCounts, (size_t)n->KeyCap * sizeof(int));
    }
}

static void BPlusNode_EnsureChildren(BPlusNode* n, int need)
{
    if (n->ChildCap >= need) return;
    n->ChildCap = n->ChildCap ? n->ChildCap * 2 : 8;
    while (n->ChildCap < need) n->ChildCap *= 2;
    n->Children = (int64_t*)realloc(n->Children, (size_t)n->ChildCap * sizeof(int64_t));
}

static uint8_t* bplus_dup_key(const uint8_t* k, int n)
{
    uint8_t* p = (uint8_t*)malloc((size_t)n);
    memcpy(p, k, (size_t)n);
    return p;
}

static int BPlusNode_FindKey(const BPlusNode* n, const uint8_t* key, int keyLen, int* found)
{
    int low = 0, high = n->KeyCount - 1;
    *found = 0;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = ByteArrayComparer_Compare(n->Keys[mid], n->KeyLens[mid], key, keyLen);
        if (cmp == 0) { *found = 1; return mid; }
        if (cmp < 0) low = mid + 1;
        else high = mid - 1;
    }
    return low;
}

int BPlusTreeManager_GetMaxKeysPerNode(const BPlusTreeManager* self)
{
    int leafHeaderSize = 1 + 8 + 8 + 4;
    int perLeafKey = 4 + self->KeyLength + 4 + 8;
    int n = (BPlusPage_Size - leafHeaderSize) / perLeafKey;
    return n < 2 ? 2 : n;
}

int BPlusTreeManager_GetMaxKeysForInternalNode(const BPlusTreeManager* self)
{
    int internalHeaderSize = 1 + 4, childListOverhead = 4, perChild = 8;
    int perKey = 4 + self->KeyLength;
    int bytesPerKeySlot = perKey + perChild;
    int fixedOverhead = internalHeaderSize + childListOverhead + perChild;
    int n = (BPlusPage_Size - fixedOverhead) / bytesPerKeySlot;
    return n < 2 ? 2 : n;
}

int64_t BPlusTreeManager_GetRootOffset(const BPlusTreeManager* self) { return self->rootNodeOffset; }

static int BPlusTreeManager_ReadHeader(BPlusTreeManager* self, IsamFileHeader* h)
{
    if (self->handle) {
        *h = self->handle->Header;
        return GMB_DB_OK;
    }
    return IsamFileHeader_Read(self->fs, h);
}

static int BPlusTreeManager_WriteHeader(BPlusTreeManager* self, const IsamFileHeader* h)
{
    if (self->handle) self->handle->Header = *h;
    return IsamFileHeader_Write(self->fs, h);
}

static int64_t BPlusTreeManager_AllocateNewPage(BPlusTreeManager* self)
{
    IsamFileHeader h;
    int64_t offset;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return -1;
    if (h.PageFreeListHeadOffset != -1) {
        int64_t next = -1;
        uint8_t buf[8];
        fseek(self->fs, (long)h.PageFreeListHeadOffset, SEEK_SET);
        if (fread(buf, 1, 8, self->fs) == 8) {
            int i;
            uint64_t u = 0;
            for (i = 0; i < 8; i++) u |= (uint64_t)buf[i] << (8 * i);
            next = (int64_t)u;
            if (next <= 0) next = -1;
        }
        offset = h.PageFreeListHeadOffset;
        h.PageFreeListHeadOffset = next;
        BPlusTreeManager_WriteHeader(self, &h);
        IsamLruCache_Remove(self->nodeCache, (uint64_t)offset);
        if (self->handle) IsamFileHandle_RegisterHeapAllocation(self->handle, offset);
        return offset;
    }
    offset = h.NextAvailablePageOffset;
    h.NextAvailablePageOffset = offset + BPlusPage_Size;
    BPlusTreeManager_WriteHeader(self, &h);
    if (ftruncate(fileno(self->fs), (off_t)h.NextAvailablePageOffset) != 0) return -1;
    if (self->handle) IsamFileHandle_RegisterHeapAllocation(self->handle, offset);
    return offset;
}

static void BPlusLeaf_Serialize(const BPlusNode* n, Bw* b)
{
    int i, j;
    bw_u8(b, 1);
    bw_i64(b, n->NextLeafOffset);
    bw_i64(b, n->PrevLeafOffset);
    bw_i32(b, n->KeyCount);
    for (i = 0; i < n->KeyCount; i++) {
        bw_i32(b, n->KeyLens[i]);
        bw_bytes(b, n->Keys[i], n->KeyLens[i]);
        bw_i32(b, n->RecCounts[i]);
        for (j = 0; j < n->RecCounts[i]; j++) bw_i64(b, n->Recs[i][j]);
    }
}

static void BPlusInternal_Serialize(const BPlusNode* n, Bw* b)
{
    int i;
    bw_u8(b, 0);
    bw_i32(b, n->KeyCount);
    for (i = 0; i < n->KeyCount; i++) {
        bw_i32(b, n->KeyLens[i]);
        bw_bytes(b, n->Keys[i], n->KeyLens[i]);
    }
    bw_i32(b, n->ChildCount);
    for (i = 0; i < n->ChildCount; i++) bw_i64(b, n->Children[i]);
}

static int BPlusLeaf_Deserialize(BPlusNode* n, Br* r)
{
    uint8_t leaf;
    int32_t keyCount, i, j, klen, oc;
    if (br_u8(r, &leaf) || br_i64(r, &n->NextLeafOffset) || br_i64(r, &n->PrevLeafOffset) || br_i32(r, &keyCount))
        return -1;
    n->IsLeaf = 1;
    BPlusNode_EnsureKeys(n, keyCount);
    for (i = 0; i < keyCount; i++) {
        if (br_i32(r, &klen) || r->i + klen > r->n) return -1;
        n->Keys[i] = (uint8_t*)malloc((size_t)klen);
        memcpy(n->Keys[i], r->p + r->i, (size_t)klen);
        r->i += klen;
        n->KeyLens[i] = klen;
        if (br_i32(r, &oc)) return -1;
        n->RecCounts[i] = oc;
        n->Recs[i] = (int64_t*)malloc((size_t)oc * sizeof(int64_t));
        for (j = 0; j < oc; j++)
            if (br_i64(r, &n->Recs[i][j])) return -1;
    }
    n->KeyCount = keyCount;
    return 0;
}

static int BPlusInternal_Deserialize(BPlusNode* n, Br* r)
{
    uint8_t leaf;
    int32_t keyCount, childCount, i, klen;
    if (br_u8(r, &leaf) || br_i32(r, &keyCount)) return -1;
    n->IsLeaf = 0;
    BPlusNode_EnsureKeys(n, keyCount);
    for (i = 0; i < keyCount; i++) {
        if (br_i32(r, &klen) || r->i + klen > r->n) return -1;
        n->Keys[i] = (uint8_t*)malloc((size_t)klen);
        memcpy(n->Keys[i], r->p + r->i, (size_t)klen);
        r->i += klen;
        n->KeyLens[i] = klen;
    }
    n->KeyCount = keyCount;
    if (br_i32(r, &childCount)) return -1;
    BPlusNode_EnsureChildren(n, childCount);
    for (i = 0; i < childCount; i++)
        if (br_i64(r, &n->Children[i])) return -1;
    n->ChildCount = childCount;
    return 0;
}

static BPlusNode* BPlusTreeManager_LoadNodeFromDisk(BPlusTreeManager* self, int64_t offset)
{
    uint8_t page[BPlusPage_Size];
    uint32_t crc;
    BPlusNode* n;
    Br r;
    if (self->handle) {
        if (IsamFileHandle_ReadBytesAt(self->handle, offset, page, BPlusPage_Size) != GMB_DB_OK)
            return NULL;
    } else {
        fseek(self->fs, (long)offset, SEEK_SET);
        if (fread(page, 1, BPlusPage_Size, self->fs) != BPlusPage_Size) return NULL;
    }
    if (!IsamPageChecksum_TryValidate(page, BPlusPage_Size, &crc) && crc != 0) return NULL;
    if (page[0] != 0 && page[0] != 1) return NULL;
    n = BPlusNode_New(page[0] == 1);
    n->NodeOffset = offset;
    r.p = page; r.n = BPlusPage_Size; r.i = 0;
    if (n->IsLeaf) {
        if (BPlusLeaf_Deserialize(n, &r) != 0) return NULL;
    } else {
        if (BPlusInternal_Deserialize(n, &r) != 0) return NULL;
    }
    return n;
}

static BPlusNode* BPlusTreeManager_LoadNode(BPlusTreeManager* self, int64_t offset)
{
    void* cached = NULL;
    BPlusNode* n;
    if (offset < 0) return NULL;
    if (IsamLruCache_TryGetValue(self->nodeCache, (uint64_t)offset, &cached) && cached)
        return (BPlusNode*)cached;
    n = BPlusTreeManager_LoadNodeFromDisk(self, offset);
    if (n) IsamLruCache_AddOrUpdate(self->nodeCache, (uint64_t)offset, n);
    return n;
}

static int64_t BPlusTreeManager_SaveNode(BPlusTreeManager* self, BPlusNode* node)
{
    uint8_t page[BPlusPage_Size];
    Bw b = {0};
    if (node->NodeOffset <= 0) {
        node->NodeOffset = BPlusTreeManager_AllocateNewPage(self);
        if (node->NodeOffset < 0) return -1;
    }
    if (node->IsLeaf) BPlusLeaf_Serialize(node, &b);
    else BPlusInternal_Serialize(node, &b);
    if (b.n > BPlusPage_Size) { free(b.p); return -1; }
    memset(page, 0, sizeof(page));
    memcpy(page, b.p, (size_t)b.n);
    free(b.p);
    IsamPageChecksum_WriteChecksum(page, BPlusPage_Size);
    fseek(self->fs, (long)node->NodeOffset, SEEK_SET);
    if (fwrite(page, 1, BPlusPage_Size, self->fs) != BPlusPage_Size) return -1;
    fflush(self->fs);
    IsamLruCache_AddOrUpdate(self->nodeCache, (uint64_t)node->NodeOffset, node);
    return node->NodeOffset;
}

static void BPlusTreeManager_UpdateRootOffsetInHeader(BPlusTreeManager* self)
{
    IsamFileHeader h;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return;
    if (self->secondarySlot >= 0 && self->secondarySlot < 15)
        h.SecondaryIndexRootOffsets[self->secondarySlot] = self->rootNodeOffset;
    else
        h.PrimaryKeyRootOffset = self->rootNodeOffset;
    BPlusTreeManager_WriteHeader(self, &h);
    fflush(self->fs);
}

static BPlusInsertResult BPlusLeaf_SplitAndPromote(BPlusTreeManager* self, BPlusNode* leaf)
{
    BPlusNode* sibling = BPlusNode_New(1);
    int splitIndex = BPlusFillFactor_ComputeLeafSplitIndex(leaf->KeyCount);
    int right = leaf->KeyCount - splitIndex, i;
    BPlusInsertResult r;
    int64_t originalNext = leaf->NextLeafOffset;
    BPlusNode_EnsureKeys(sibling, right);
    for (i = 0; i < right; i++) {
        int src = splitIndex + i;
        sibling->Keys[i] = leaf->Keys[src];
        sibling->KeyLens[i] = leaf->KeyLens[src];
        sibling->Recs[i] = leaf->Recs[src];
        sibling->RecCounts[i] = leaf->RecCounts[src];
    }
    sibling->KeyCount = right;
    leaf->KeyCount = splitIndex;
    if (leaf->NodeOffset <= 0) BPlusTreeManager_SaveNode(self, leaf);
    sibling->NextLeafOffset = originalNext;
    sibling->PrevLeafOffset = leaf->NodeOffset;
    BPlusTreeManager_SaveNode(self, sibling);
    leaf->NextLeafOffset = sibling->NodeOffset;
    if (originalNext != -1) {
        BPlusNode* nxt = BPlusTreeManager_LoadNode(self, originalNext);
        if (nxt && nxt->IsLeaf) {
            nxt->PrevLeafOffset = sibling->NodeOffset;
            BPlusTreeManager_SaveNode(self, nxt);
        }
    }
    BPlusTreeManager_SaveNode(self, sibling);
    BPlusTreeManager_SaveNode(self, leaf);
    memset(&r, 0, sizeof(r));
    r.NodeUpdated = 1;
    r.NewNode = sibling;
    r.NewNodeOffset = sibling->NodeOffset;
    r.PromotedKey = sibling->Keys[0];
    r.PromotedKeyLen = sibling->KeyLens[0];
    return r;
}

static BPlusInsertResult BPlusLeaf_Insert(BPlusTreeManager* self, BPlusNode* leaf,
                                          const uint8_t* key, int keyLen, int64_t recordOffset)
{
    int found, index, i;
    BPlusInsertResult none;
    memset(&none, 0, sizeof(none));
    none.NewNodeOffset = -1;
    index = BPlusNode_FindKey(leaf, key, keyLen, &found);
    if (found) {
        if (self->Unique) {
            int j;
            for (j = 0; j < leaf->RecCounts[index]; j++)
                if (leaf->Recs[index][j] == recordOffset) {
                    none.NodeUpdated = 1;
                    BPlusTreeManager_SaveNode(self, leaf);
                    return none;
                }
            none.NodeUpdated = 0;
            return none; /* uniqueness violation → ERR via NodeUpdated=0 and NewNode=NULL; caller ignores */
        }
        leaf->Recs[index] = (int64_t*)realloc(leaf->Recs[index], (size_t)(leaf->RecCounts[index] + 1) * sizeof(int64_t));
        leaf->Recs[index][leaf->RecCounts[index]++] = recordOffset;
        BPlusTreeManager_SaveNode(self, leaf);
        none.NodeUpdated = 1;
        return none;
    }
    BPlusNode_EnsureKeys(leaf, leaf->KeyCount + 1);
    for (i = leaf->KeyCount; i > index; i--) {
        leaf->Keys[i] = leaf->Keys[i - 1];
        leaf->KeyLens[i] = leaf->KeyLens[i - 1];
        leaf->Recs[i] = leaf->Recs[i - 1];
        leaf->RecCounts[i] = leaf->RecCounts[i - 1];
    }
    leaf->Keys[index] = bplus_dup_key(key, keyLen);
    leaf->KeyLens[index] = keyLen;
    leaf->Recs[index] = (int64_t*)malloc(sizeof(int64_t));
    leaf->Recs[index][0] = recordOffset;
    leaf->RecCounts[index] = 1;
    leaf->KeyCount++;
    if (leaf->KeyCount <= BPlusTreeManager_GetMaxKeysPerNode(self)) {
        BPlusTreeManager_SaveNode(self, leaf);
        none.NodeUpdated = 1;
        return none;
    }
    return BPlusLeaf_SplitAndPromote(self, leaf);
}

static BPlusInsertResult BPlusInternal_Insert(BPlusTreeManager* self, BPlusNode* rootInternal,
                                              const uint8_t* key, int keyLen, int64_t recordOffset)
{
    typedef struct { BPlusNode* Node; int ChildIndex; } Spine;
    Spine spine[64];
    int depth = 0, visitedN = 0;
    int64_t visited[64];
    BPlusNode* node = rootInternal;
    BPlusInsertResult result, split, none;
    int i, found;
    memset(&none, 0, sizeof(none));
    none.NewNodeOffset = -1;
    while (!node->IsLeaf) {
        int index;
        int64_t childOffset;
        if (depth >= 64) return none;
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        /* C#: se trovato, NON +1 in Insert (solo Search va a destra) */
        childOffset = node->Children[index];
        if (childOffset <= 0) return none;
        for (i = 0; i < visitedN; i++) if (visited[i] == childOffset) return none;
        visited[visitedN++] = childOffset;
        spine[depth].Node = node;
        spine[depth].ChildIndex = index;
        depth++;
        node = BPlusTreeManager_LoadNode(self, childOffset);
        if (!node) return none;
    }
    result = BPlusLeaf_Insert(self, node, key, keyLen, recordOffset);
    if (!result.NewNode) return none;
    if (!result.PromotedKey) return none;
    split = result;
    for (i = depth - 1; i >= 0; i--) {
        BPlusNode* parent = spine[i].Node;
        int childIndex = spine[i].ChildIndex, k, maxInternal;
        if (split.NewNodeOffset < 0)
            split.NewNodeOffset = BPlusTreeManager_SaveNode(self, split.NewNode);
        BPlusNode_EnsureKeys(parent, parent->KeyCount + 1);
        BPlusNode_EnsureChildren(parent, parent->ChildCount + 1);
        for (k = parent->KeyCount; k > childIndex; k--) {
            parent->Keys[k] = parent->Keys[k - 1];
            parent->KeyLens[k] = parent->KeyLens[k - 1];
        }
        parent->Keys[childIndex] = bplus_dup_key(split.PromotedKey, split.PromotedKeyLen);
        parent->KeyLens[childIndex] = split.PromotedKeyLen;
        parent->KeyCount++;
        for (k = parent->ChildCount; k > childIndex + 1; k--)
            parent->Children[k] = parent->Children[k - 1];
        parent->Children[childIndex + 1] = split.NewNodeOffset;
        parent->ChildCount++;
        maxInternal = BPlusTreeManager_GetMaxKeysForInternalNode(self);
        if (parent->KeyCount <= maxInternal) {
            BPlusTreeManager_SaveNode(self, parent);
            return none;
        }
        {
            BPlusNode* sibling = BPlusNode_New(0);
            int splitIndex = BPlusFillFactor_ComputeInternalSplitIndex(parent->KeyCount);
            uint8_t* promoted = parent->Keys[splitIndex];
            int promotedLen = parent->KeyLens[splitIndex];
            int rightKeys = parent->KeyCount - splitIndex - 1;
            int rightChildren = parent->ChildCount - (splitIndex + 1);
            int j;
            BPlusNode_EnsureKeys(sibling, rightKeys);
            BPlusNode_EnsureChildren(sibling, rightChildren);
            for (j = 0; j < rightKeys; j++) {
                sibling->Keys[j] = parent->Keys[splitIndex + 1 + j];
                sibling->KeyLens[j] = parent->KeyLens[splitIndex + 1 + j];
            }
            sibling->KeyCount = rightKeys;
            for (j = 0; j < rightChildren; j++)
                sibling->Children[j] = parent->Children[splitIndex + 1 + j];
            sibling->ChildCount = rightChildren;
            parent->KeyCount = splitIndex;
            parent->ChildCount = splitIndex + 1;
            BPlusTreeManager_SaveNode(self, parent);
            BPlusTreeManager_SaveNode(self, sibling);
            memset(&split, 0, sizeof(split));
            split.NodeUpdated = 1;
            split.NewNode = sibling;
            split.NewNodeOffset = sibling->NodeOffset;
            split.PromotedKey = promoted;
            split.PromotedKeyLen = promotedLen;
        }
    }
    return split;
}

static int BPlusTreeManager_SearchNode(BPlusTreeManager* self, BPlusNode* node,
                                       const uint8_t* key, int keyLen, int64_t* offsets, int cap, int* count)
{
    int found, index;
    if (node->IsLeaf) {
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        *count = 0;
        if (!found) return GMB_DB_OK;
        if (offsets && cap > 0) {
            int n = node->RecCounts[index] < cap ? node->RecCounts[index] : cap;
            int i;
            for (i = 0; i < n; i++) offsets[i] = node->Recs[index][i];
            *count = n;
        } else {
            *count = node->RecCounts[index];
        }
        return GMB_DB_OK;
    }
    index = BPlusNode_FindKey(node, key, keyLen, &found);
    if (found) index += 1;
    return BPlusTreeManager_SearchNode(self, BPlusTreeManager_LoadNode(self, node->Children[index]),
                                       key, keyLen, offsets, cap, count);
}

BPlusTreeManager* BPlusTreeManager_New(const char* filePath, int keyLength, int isUnique)
{
    BPlusTreeManager* self = (BPlusTreeManager*)calloc(1, sizeof(*self));
    IsamFileHeader h;
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->KeyLength = keyLength;
    self->Unique = isUnique;
    self->ownsFile = 1;
    self->secondarySlot = -1;
    self->nodeCache = IsamLruCache_New();
    pthread_mutex_init(&self->syncRoot, NULL);
    self->fs = fopen(filePath, "rb+");
    if (!self->fs || BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) {
        if (self->fs) fclose(self->fs);
        IsamLruCache_Dispose(self->nodeCache);
        free(self);
        return NULL;
    }
    self->rootNodeOffset = h.PrimaryKeyRootOffset;
    if (self->rootNodeOffset == 0) self->rootNodeOffset = -1;
    return self;
}

BPlusTreeManager* BPlusTreeManager_NewOnHandleAt(IsamFileHandle* handle, int keyLength, int isUnique,
                                                 int secondarySlot)
{
    BPlusTreeManager* self;
    int64_t root;
    if (!handle || !handle->fs) return NULL;
    self = (BPlusTreeManager*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", handle->FilePath);
    self->handle = handle;
    self->fs = handle->fs;
    self->KeyLength = keyLength;
    self->Unique = isUnique;
    self->ownsFile = 0;
    self->secondarySlot = secondarySlot;
    self->nodeCache = IsamLruCache_New();
    pthread_mutex_init(&self->syncRoot, NULL);
    if (secondarySlot >= 0 && secondarySlot < 15)
        root = handle->Header.SecondaryIndexRootOffsets[secondarySlot];
    else
        root = handle->Header.PrimaryKeyRootOffset;
    self->rootNodeOffset = (root == 0) ? -1 : root;
    return self;
}

BPlusTreeManager* BPlusTreeManager_NewOnHandle(IsamFileHandle* handle, int keyLength, int isUnique)
{
    return BPlusTreeManager_NewOnHandleAt(handle, keyLength, isUnique, -1);
}

void BPlusTreeManager_Dispose(BPlusTreeManager* self)
{
    if (!self) return;
    pthread_mutex_lock(&self->syncRoot);
    if (self->ownsFile && self->fs) fclose(self->fs);
    pthread_mutex_unlock(&self->syncRoot);
    IsamLruCache_Dispose(self->nodeCache);
    pthread_mutex_destroy(&self->syncRoot);
    free(self);
}

static BPlusNode* BPlusTreeManager_FindLastLeaf(BPlusTreeManager* self);

static int bplus_child_index(const BPlusNode* n, int64_t childOffset)
{
    int i;
    for (i = 0; i < n->ChildCount; i++) if (n->Children[i] == childOffset) return i;
    return -1;
}

static BPlusNode* bplus_find_parent_internal(BPlusTreeManager* self, int64_t childOffset)
{
    int64_t stack[1024];
    int sp = 0;
    if (self->rootNodeOffset < 0 || self->rootNodeOffset == childOffset) return NULL;
    stack[sp++] = self->rootNodeOffset;
    while (sp > 0) {
        int64_t off = stack[--sp];
        BPlusNode* node = BPlusTreeManager_LoadNode(self, off);
        int i;
        if (!node || node->IsLeaf) continue;
        if (bplus_child_index(node, childOffset) >= 0) return node;
        for (i = 0; i < node->ChildCount && sp < 1024; i++) stack[sp++] = node->Children[i];
    }
    return NULL;
}

static void bplus_promote_leaf_split_as_root(BPlusTreeManager* self, BPlusNode* leftChild, BPlusInsertResult split)
{
    BPlusNode* newRoot = BPlusNode_New(0);
    BPlusNode_EnsureKeys(newRoot, 1);
    BPlusNode_EnsureChildren(newRoot, 2);
    newRoot->Keys[0] = bplus_dup_key(split.PromotedKey, split.PromotedKeyLen);
    newRoot->KeyLens[0] = split.PromotedKeyLen;
    newRoot->KeyCount = 1;
    newRoot->Children[0] = leftChild->NodeOffset;
    newRoot->Children[1] = split.NewNodeOffset;
    newRoot->ChildCount = 2;
    self->rootNodeOffset = BPlusTreeManager_SaveNode(self, newRoot);
    BPlusTreeManager_UpdateRootOffsetInHeader(self);
}

static void bplus_insert_promoted_into_internal(BPlusTreeManager* self, BPlusNode* parent,
                                                int64_t leftChildOffset, BPlusInsertResult childSplit)
{
    for (;;) {
        int childIndex = bplus_child_index(parent, leftChildOffset);
        int k;
        if (childIndex < 0) return;
        BPlusNode_EnsureKeys(parent, parent->KeyCount + 1);
        BPlusNode_EnsureChildren(parent, parent->ChildCount + 1);
        for (k = parent->KeyCount; k > childIndex; k--) {
            parent->Keys[k] = parent->Keys[k - 1];
            parent->KeyLens[k] = parent->KeyLens[k - 1];
        }
        parent->Keys[childIndex] = bplus_dup_key(childSplit.PromotedKey, childSplit.PromotedKeyLen);
        parent->KeyLens[childIndex] = childSplit.PromotedKeyLen;
        parent->KeyCount++;
        for (k = parent->ChildCount; k > childIndex + 1; k--) parent->Children[k] = parent->Children[k - 1];
        parent->Children[childIndex + 1] = childSplit.NewNodeOffset;
        parent->ChildCount++;
        if (parent->KeyCount <= BPlusTreeManager_GetMaxKeysForInternalNode(self)) {
            BPlusTreeManager_SaveNode(self, parent);
            return;
        }
        {
            BPlusNode* sibling = BPlusNode_New(0);
            int splitIndex = BPlusFillFactor_ComputeInternalSplitIndex(parent->KeyCount);
            uint8_t* promoted = parent->Keys[splitIndex];
            int promotedLen = parent->KeyLens[splitIndex];
            int rightKeys = parent->KeyCount - splitIndex - 1;
            int rightChildren = parent->ChildCount - (splitIndex + 1);
            int j;
            BPlusNode_EnsureKeys(sibling, rightKeys);
            BPlusNode_EnsureChildren(sibling, rightChildren);
            for (j = 0; j < rightKeys; j++) { sibling->Keys[j] = parent->Keys[splitIndex + 1 + j]; sibling->KeyLens[j] = parent->KeyLens[splitIndex + 1 + j]; }
            sibling->KeyCount = rightKeys;
            for (j = 0; j < rightChildren; j++) sibling->Children[j] = parent->Children[splitIndex + 1 + j];
            sibling->ChildCount = rightChildren;
            parent->KeyCount = splitIndex;
            parent->ChildCount = splitIndex + 1;
            BPlusTreeManager_SaveNode(self, parent);
            {
                int64_t siblingOff = BPlusTreeManager_SaveNode(self, sibling);
                BPlusInsertResult up;
                memset(&up, 0, sizeof(up));
                up.NodeUpdated = 1;
                up.NewNode = sibling;
                up.NewNodeOffset = siblingOff;
                up.PromotedKey = promoted;
                up.PromotedKeyLen = promotedLen;
                if (parent->NodeOffset == self->rootNodeOffset) {
                    bplus_promote_leaf_split_as_root(self, parent, up);
                    return;
                }
                leftChildOffset = parent->NodeOffset;
                childSplit = up;
                parent = bplus_find_parent_internal(self, leftChildOffset);
                if (!parent) return;
            }
        }
    }
}

static void bplus_promote_leaf_split(BPlusTreeManager* self, BPlusNode* leftLeaf, BPlusInsertResult split)
{
    BPlusNode* parent;
    if (self->rootNodeOffset == leftLeaf->NodeOffset) { bplus_promote_leaf_split_as_root(self, leftLeaf, split); return; }
    parent = bplus_find_parent_internal(self, leftLeaf->NodeOffset);
    if (!parent) return;
    bplus_insert_promoted_into_internal(self, parent, leftLeaf->NodeOffset, split);
}

/* Bulk append di entries con chiavi strettamente crescenti e > ultima chiave dell'indice,
 * con un SaveNode per pagina piena (port di BPlusTreeManager.InsertManyAppendOrdered). */
int BPlusTreeManager_InsertManyAppendOrdered(BPlusTreeManager* self,
        const uint8_t* const* keys, const int* keyLens, const int64_t* offs, int n)
{
    BPlusNode* leaf;
    int i, dirty = 0;
    if (!self || n < 0) return GMB_DB_ERR;
    if (n == 0) return GMB_DB_OK;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) { leaf = BPlusNode_New(1); }
    else {
        leaf = BPlusTreeManager_FindLastLeaf(self);
        if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return GMB_DB_ERR; }
    }
    for (i = 0; i < n; i++) {
        BPlusInsertResult split;
        BPlusNode_EnsureKeys(leaf, leaf->KeyCount + 1);
        leaf->Keys[leaf->KeyCount] = bplus_dup_key(keys[i], keyLens[i]);
        leaf->KeyLens[leaf->KeyCount] = keyLens[i];
        leaf->Recs[leaf->KeyCount] = (int64_t*)malloc(sizeof(int64_t));
        leaf->Recs[leaf->KeyCount][0] = offs[i];
        leaf->RecCounts[leaf->KeyCount] = 1;
        leaf->KeyCount++;
        if (leaf->KeyCount <= BPlusTreeManager_GetMaxKeysPerNode(self)) { dirty = 1; continue; }
        split = BPlusLeaf_SplitAndPromote(self, leaf);
        if (self->rootNodeOffset == -1) bplus_promote_leaf_split_as_root(self, leaf, split);
        else bplus_promote_leaf_split(self, leaf, split);
        leaf = split.NewNode;
        dirty = 0;
    }
    if (self->rootNodeOffset == -1) {
        self->rootNodeOffset = BPlusTreeManager_SaveNode(self, leaf);
        BPlusTreeManager_UpdateRootOffsetInHeader(self);
    } else if (dirty) {
        BPlusTreeManager_SaveNode(self, leaf);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

int BPlusTreeManager_Insert(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusInsertResult result;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        BPlusNode* root = BPlusNode_New(1);
        BPlusLeaf_Insert(self, root, key, keyLen, recordOffset);
        self->rootNodeOffset = BPlusTreeManager_SaveNode(self, root);
        BPlusTreeManager_UpdateRootOffsetInHeader(self);
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    {
        BPlusNode* rootNode = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
        if (!rootNode) { pthread_mutex_unlock(&self->syncRoot); return GMB_DB_ERR; }
        result = rootNode->IsLeaf
            ? BPlusLeaf_Insert(self, rootNode, key, keyLen, recordOffset)
            : BPlusInternal_Insert(self, rootNode, key, keyLen, recordOffset);
        if (result.NewNode) {
            int64_t siblingOff = result.NewNodeOffset;
            BPlusNode* newRoot;
            if (siblingOff < 0) siblingOff = BPlusTreeManager_SaveNode(self, result.NewNode);
            newRoot = BPlusNode_New(0);
            BPlusNode_EnsureKeys(newRoot, 1);
            BPlusNode_EnsureChildren(newRoot, 2);
            newRoot->Keys[0] = bplus_dup_key(result.PromotedKey, result.PromotedKeyLen);
            newRoot->KeyLens[0] = result.PromotedKeyLen;
            newRoot->KeyCount = 1;
            newRoot->Children[0] = self->rootNodeOffset;
            newRoot->Children[1] = siblingOff;
            newRoot->ChildCount = 2;
            self->rootNodeOffset = BPlusTreeManager_SaveNode(self, newRoot);
            BPlusTreeManager_UpdateRootOffsetInHeader(self);
        }
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

int BPlusTreeManager_Search(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                            int64_t* offsets, int cap, int* count)
{
    BPlusNode* root;
    int rc;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) { *count = 0; pthread_mutex_unlock(&self->syncRoot); return GMB_DB_OK; }
    root = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (!root) { pthread_mutex_unlock(&self->syncRoot); return GMB_DB_ERR; }
    rc = BPlusTreeManager_SearchNode(self, root, key, keyLen, offsets, cap, count);
    pthread_mutex_unlock(&self->syncRoot);
    return rc;
}

static int BPlusTreeManager_SearchLowerBoundNode(BPlusTreeManager* self, BPlusNode* node,
                                                 const uint8_t* key, int keyLen,
                                                 int64_t* offsets, int cap, int* count)
{
    int found, index, n, i;
    *count = 0;
    if (!node) return GMB_DB_ERR;
    if (node->IsLeaf) {
        index = BPlusNode_FindKey(node, key, keyLen, &found);
        if (index >= node->KeyCount) {
            if (node->NextLeafOffset < 0) return GMB_DB_OK;
            node = BPlusTreeManager_LoadNode(self, node->NextLeafOffset);
            if (!node || node->KeyCount <= 0) return GMB_DB_OK;
            index = 0;
        }
        n = node->RecCounts[index] < cap ? node->RecCounts[index] : cap;
        for (i = 0; i < n; i++) offsets[i] = node->Recs[index][i];
        *count = n;
        return GMB_DB_OK;
    }
    index = BPlusNode_FindKey(node, key, keyLen, &found);
    if (found) index += 1;
    return BPlusTreeManager_SearchLowerBoundNode(self,
        BPlusTreeManager_LoadNode(self, node->Children[index]), key, keyLen, offsets, cap, count);
}

int BPlusTreeManager_SearchLowerBound(BPlusTreeManager* self, const uint8_t* key, int keyLen,
                                      int64_t* offsets, int cap, int* count)
{
    BPlusNode* root;
    int rc;
    if (!self || !key || !count) return GMB_DB_ERR;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        *count = 0;
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    root = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (!root) {
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_ERR;
    }
    rc = BPlusTreeManager_SearchLowerBoundNode(self, root, key, keyLen, offsets, cap, count);
    pthread_mutex_unlock(&self->syncRoot);
    return rc;
}

static void bplus_selfcheck_key(uint8_t key[10], int i)
{
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int BPlusTreeManager_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_bplus_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    BPlusTreeManager* mgr;
    uint8_t key[10];
    int i, n;
    int64_t off;
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -2;
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr) return -3;
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Insert(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -4;
        }
    }
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        n = 0; off = -1;
        if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 1 || off != 10000 + i) {
            BPlusTreeManager_Dispose(mgr); return -100 - i;
        }
    }
    BPlusTreeManager_Dispose(mgr);
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr || mgr->rootNodeOffset <= 0) { if (mgr) BPlusTreeManager_Dispose(mgr); return -5; }
    bplus_selfcheck_key(key, 150);
    n = 0; off = -1;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || off != 10150) {
        BPlusTreeManager_Dispose(mgr); return -6;
    }
    for (i = 0; i < 200; i += 2) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Delete(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -7;
        }
    }
    for (i = 0; i < 200; i++) {
        bplus_selfcheck_key(key, i);
        n = 0; off = -1;
        if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -200 - i;
        }
        if (i % 2 == 0) {
            if (n != 0) { BPlusTreeManager_Dispose(mgr); return -300 - i; }
        } else if (n != 1 || off != 10000 + i) {
            BPlusTreeManager_Dispose(mgr); return -400 - i;
        }
    }
    for (i = 1; i < 200; i += 2) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Delete(mgr, key, 10, 10000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -8;
        }
    }
    bplus_selfcheck_key(key, 99);
    n = 1; off = 0;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 0) {
        BPlusTreeManager_Dispose(mgr); return -9;
    }
    for (i = 0; i < 20; i++) {
        bplus_selfcheck_key(key, i);
        if (BPlusTreeManager_Insert(mgr, key, 10, 20000 + i) != GMB_DB_OK) {
            BPlusTreeManager_Dispose(mgr); return -10;
        }
    }
    BPlusTreeManager_Dispose(mgr);
    mgr = BPlusTreeManager_New(path, 10, 1);
    if (!mgr) return -11;
    bplus_selfcheck_key(key, 10);
    n = 0; off = -1;
    if (BPlusTreeManager_Search(mgr, key, 10, &off, 1, &n) != GMB_DB_OK || n != 1 || off != 20010) {
        BPlusTreeManager_Dispose(mgr); return -12;
    }
    BPlusTreeManager_Dispose(mgr);
    return GMB_DB_OK;
}

static int BPlusTreeManager_IsActiveRecord(BPlusTreeManager* self, int64_t recordOffset)
{
    uint8_t status = 0;
    if (recordOffset < 0 || !self->fs) return 0;
    if (self->handle) {
        RecordMetadata meta;
        if (IsamFileHandle_ReadRecordMetadata(self->handle, recordOffset, &meta) != GMB_DB_OK) return 0;
        return meta.Status == Config_RecordStatusActive;
    }
    fseek(self->fs, (long)recordOffset, SEEK_SET);
    if (fread(&status, 1, 1, self->fs) != 1) return 0;
    return status == Config_RecordStatusActive;
}

static BPlusNode* BPlusTreeManager_FindFirstLeaf(BPlusTreeManager* self)
{
    BPlusNode* node;
    if (self->rootNodeOffset < 0) return NULL;
    node = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    while (node && !node->IsLeaf) {
        if (node->ChildCount <= 0) return NULL;
        node = BPlusTreeManager_LoadNode(self, node->Children[0]);
    }
    return node;
}

static BPlusNode* BPlusTreeManager_FindLastLeaf(BPlusTreeManager* self)
{
    BPlusNode* node;
    if (self->rootNodeOffset < 0) return NULL;
    node = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    while (node && !node->IsLeaf) {
        if (node->ChildCount <= 0) return NULL;
        node = BPlusTreeManager_LoadNode(self, node->Children[node->ChildCount - 1]);
    }
    return node;
}

int BPlusTreeManager_GetLastKey(BPlusTreeManager* self, uint8_t* out, int cap)
{
    BPlusNode* leaf = BPlusTreeManager_FindLastLeaf(self);
    int len;
    if (!leaf || leaf->KeyCount <= 0) return 0;
    len = leaf->KeyLens[leaf->KeyCount - 1];
    if (len <= 0 || len > cap) return -1;
    memcpy(out, leaf->Keys[leaf->KeyCount - 1], (size_t)len);
    return len;
}

int64_t BPlusTreeManager_GetFirstRecordOffset(BPlusTreeManager* self)
{
    BPlusNode* leaf;
    int i, j;
    if (!self || self->rootNodeOffset < 0) return -1;
    pthread_mutex_lock(&self->syncRoot);
    leaf = BPlusTreeManager_FindFirstLeaf(self);
    while (leaf) {
        for (i = 0; i < leaf->KeyCount; i++) {
            for (j = 0; j < leaf->RecCounts[i]; j++) {
                int64_t off = leaf->Recs[i][j];
                if (BPlusTreeManager_IsActiveRecord(self, off)) {
                    pthread_mutex_unlock(&self->syncRoot);
                    return off;
                }
            }
        }
        if (leaf->NextLeafOffset < 0) break;
        leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return -1;
}

int64_t BPlusTreeManager_GetLastRecordOffset(BPlusTreeManager* self)
{
    BPlusNode* leaf;
    int i, j;
    if (!self || self->rootNodeOffset < 0) return -1;
    pthread_mutex_lock(&self->syncRoot);
    leaf = BPlusTreeManager_FindLastLeaf(self);
    while (leaf) {
        for (i = leaf->KeyCount - 1; i >= 0; i--) {
            for (j = leaf->RecCounts[i] - 1; j >= 0; j--) {
                int64_t off = leaf->Recs[i][j];
                if (BPlusTreeManager_IsActiveRecord(self, off)) {
                    pthread_mutex_unlock(&self->syncRoot);
                    return off;
                }
            }
        }
        if (leaf->PrevLeafOffset < 0) break;
        leaf = BPlusTreeManager_LoadNode(self, leaf->PrevLeafOffset);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return -1;
}

static int bplus_find_offset_in_leaf(const BPlusNode* leaf, int64_t recOff, int* keyIndex, int* offIndex)
{
    int i, j;
    for (i = 0; i < leaf->KeyCount; i++) {
        for (j = 0; j < leaf->RecCounts[i]; j++) {
            if (leaf->Recs[i][j] == recOff) {
                *keyIndex = i;
                *offIndex = j;
                return 1;
            }
        }
    }
    return 0;
}

static BPlusNode* bplus_locate_record(BPlusTreeManager* self, int64_t recOff, int* keyIndex, int* offIndex)
{
    /* ponytail: C# FindRecordPosition ricalcola la chiave via evaluator; scan foglie per offset. */
    BPlusNode* leaf = BPlusTreeManager_FindFirstLeaf(self);
    while (leaf) {
        if (bplus_find_offset_in_leaf(leaf, recOff, keyIndex, offIndex)) return leaf;
        if (leaf->NextLeafOffset < 0) return NULL;
        leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
    }
    return NULL;
}

static int64_t bplus_step_from(BPlusTreeManager* self, BPlusNode* leaf, int keyIndex, int offIndex, int direction)
{
    if (direction > 0) {
        offIndex++;
        if (offIndex >= leaf->RecCounts[keyIndex]) {
            keyIndex++;
            offIndex = 0;
            if (keyIndex >= leaf->KeyCount) {
                if (leaf->NextLeafOffset < 0) return -1;
                leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
                if (!leaf || leaf->KeyCount <= 0) return -1;
                keyIndex = 0;
            }
        }
    } else {
        offIndex--;
        if (offIndex < 0) {
            keyIndex--;
            if (keyIndex < 0) {
                if (leaf->PrevLeafOffset < 0) return -1;
                leaf = BPlusTreeManager_LoadNode(self, leaf->PrevLeafOffset);
                if (!leaf || leaf->KeyCount <= 0) return -1;
                keyIndex = leaf->KeyCount - 1;
            }
            offIndex = leaf->RecCounts[keyIndex] - 1;
        }
    }
    if (keyIndex < 0 || keyIndex >= leaf->KeyCount) return -1;
    if (offIndex < 0 || offIndex >= leaf->RecCounts[keyIndex]) return -1;
    return leaf->Recs[keyIndex][offIndex];
}

int64_t BPlusTreeManager_FindNextRecordOffset(BPlusTreeManager* self, int64_t currentRecordOffset, int skipCount)
{
    BPlusNode* leaf;
    int keyIndex = 0, offIndex = 0, i, dir, guard;
    int64_t candidate;
    if (!self || skipCount == 0) return currentRecordOffset;
    pthread_mutex_lock(&self->syncRoot);
    leaf = bplus_locate_record(self, currentRecordOffset, &keyIndex, &offIndex);
    if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    dir = skipCount > 0 ? 1 : -1;
    candidate = currentRecordOffset;
    for (i = 0; i < (skipCount < 0 ? -skipCount : skipCount); i++) {
        candidate = bplus_step_from(self, leaf, keyIndex, offIndex, dir);
        if (candidate < 0) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        leaf = bplus_locate_record(self, candidate, &keyIndex, &offIndex);
        if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    }
    guard = 0;
    while (candidate >= 0 && !BPlusTreeManager_IsActiveRecord(self, candidate) && guard++ < 1000000) {
        int64_t advanced;
        leaf = bplus_locate_record(self, candidate, &keyIndex, &offIndex);
        if (!leaf) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        advanced = bplus_step_from(self, leaf, keyIndex, offIndex, dir);
        if (advanced < 0 || advanced == candidate) { pthread_mutex_unlock(&self->syncRoot); return advanced; }
        candidate = advanced;
    }
    pthread_mutex_unlock(&self->syncRoot);
    return candidate;
}

static int bplus_min_keys(const BPlusTreeManager* self)
{
    return BPlusFillFactor_ComputeMinKeys(BPlusTreeManager_GetMaxKeysPerNode(self));
}

static int BPlusInternal_FindChildIndex(const BPlusNode* n, const uint8_t* key, int keyLen)
{
    int found, index = BPlusNode_FindKey(n, key, keyLen, &found);
    if (found) index += 1;
    if (n->ChildCount <= 0) return 0;
    if (index >= n->ChildCount) index = n->ChildCount - 1;
    if (index < 0) index = 0;
    return index;
}

static int BPlusLeaf_Remove(BPlusNode* leaf, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    int found, index, j, k;
    index = BPlusNode_FindKey(leaf, key, keyLen, &found);
    if (!found) return 0;
    for (j = 0; j < leaf->RecCounts[index]; j++) {
        if (leaf->Recs[index][j] != recordOffset) continue;
        for (k = j; k < leaf->RecCounts[index] - 1; k++)
            leaf->Recs[index][k] = leaf->Recs[index][k + 1];
        leaf->RecCounts[index]--;
        if (leaf->RecCounts[index] == 0) {
            free(leaf->Keys[index]);
            free(leaf->Recs[index]);
            for (k = index; k < leaf->KeyCount - 1; k++) {
                leaf->Keys[k] = leaf->Keys[k + 1];
                leaf->KeyLens[k] = leaf->KeyLens[k + 1];
                leaf->Recs[k] = leaf->Recs[k + 1];
                leaf->RecCounts[k] = leaf->RecCounts[k + 1];
            }
            leaf->KeyCount--;
        }
        return 1;
    }
    return 0;
}

static void BPlusTreeManager_AddPageToFreeList(BPlusTreeManager* self, int64_t pageOffset)
{
    IsamFileHeader h;
    uint8_t page[BPlusPage_Size];
    int i;
    if (pageOffset < 0) return;
    if (BPlusTreeManager_ReadHeader(self, &h) != GMB_DB_OK) return;
    memset(page, 0, sizeof(page));
    for (i = 0; i < 8; i++)
        page[i] = (uint8_t)((uint64_t)h.PageFreeListHeadOffset >> (8 * i));
    page[8] = 0xFE;
    fseek(self->fs, (long)pageOffset, SEEK_SET);
    fwrite(page, 1, BPlusPage_Size, self->fs);
    h.PageFreeListHeadOffset = pageOffset;
    BPlusTreeManager_WriteHeader(self, &h);
    fflush(self->fs);
    IsamLruCache_Remove(self->nodeCache, (uint64_t)pageOffset);
}

static void BPlusInternal_RemoveKeyAt(BPlusNode* n, int i)
{
    int k;
    if (i < 0 || i >= n->KeyCount) return;
    for (k = i; k < n->KeyCount - 1; k++) {
        n->Keys[k] = n->Keys[k + 1];
        n->KeyLens[k] = n->KeyLens[k + 1];
    }
    n->KeyCount--;
}

static void BPlusInternal_RemoveChildAt(BPlusNode* n, int i)
{
    int k;
    if (i < 0 || i >= n->ChildCount) return;
    for (k = i; k < n->ChildCount - 1; k++)
        n->Children[k] = n->Children[k + 1];
    n->ChildCount--;
}

static void BPlusLeaf_MoveFirstToLeft(BPlusNode* left, BPlusNode* right)
{
    int i = left->KeyCount;
    BPlusNode_EnsureKeys(left, i + 1);
    left->Keys[i] = right->Keys[0];
    left->KeyLens[i] = right->KeyLens[0];
    left->Recs[i] = right->Recs[0];
    left->RecCounts[i] = right->RecCounts[0];
    left->KeyCount++;
    for (i = 0; i < right->KeyCount - 1; i++) {
        right->Keys[i] = right->Keys[i + 1];
        right->KeyLens[i] = right->KeyLens[i + 1];
        right->Recs[i] = right->Recs[i + 1];
        right->RecCounts[i] = right->RecCounts[i + 1];
    }
    right->KeyCount--;
}

static void BPlusLeaf_MoveLastToRight(BPlusNode* left, BPlusNode* right)
{
    int last = left->KeyCount - 1, i;
    BPlusNode_EnsureKeys(right, right->KeyCount + 1);
    for (i = right->KeyCount; i > 0; i--) {
        right->Keys[i] = right->Keys[i - 1];
        right->KeyLens[i] = right->KeyLens[i - 1];
        right->Recs[i] = right->Recs[i - 1];
        right->RecCounts[i] = right->RecCounts[i - 1];
    }
    right->Keys[0] = left->Keys[last];
    right->KeyLens[0] = left->KeyLens[last];
    right->Recs[0] = left->Recs[last];
    right->RecCounts[0] = left->RecCounts[last];
    right->KeyCount++;
    left->KeyCount--;
}

static void BPlusInternal_MoveFirstChildToLeft(BPlusNode* left, BPlusNode* right, BPlusNode* parent, int sep)
{
    BPlusNode_EnsureKeys(left, left->KeyCount + 1);
    BPlusNode_EnsureChildren(left, left->ChildCount + 1);
    left->Keys[left->KeyCount] = parent->Keys[sep];
    left->KeyLens[left->KeyCount] = parent->KeyLens[sep];
    left->KeyCount++;
    left->Children[left->ChildCount++] = right->Children[0];
    BPlusInternal_RemoveChildAt(right, 0);
    parent->Keys[sep] = right->Keys[0];
    parent->KeyLens[sep] = right->KeyLens[0];
    BPlusInternal_RemoveKeyAt(right, 0);
}

static void BPlusInternal_MoveLastChildToRight(BPlusNode* left, BPlusNode* right, BPlusNode* parent, int sep)
{
    int lastKey = left->KeyCount - 1, lastChild = left->ChildCount - 1, i;
    BPlusNode_EnsureKeys(right, right->KeyCount + 1);
    BPlusNode_EnsureChildren(right, right->ChildCount + 1);
    for (i = right->KeyCount; i > 0; i--) {
        right->Keys[i] = right->Keys[i - 1];
        right->KeyLens[i] = right->KeyLens[i - 1];
    }
    for (i = right->ChildCount; i > 0; i--)
        right->Children[i] = right->Children[i - 1];
    right->Keys[0] = parent->Keys[sep];
    right->KeyLens[0] = parent->KeyLens[sep];
    right->KeyCount++;
    right->Children[0] = left->Children[lastChild];
    right->ChildCount++;
    parent->Keys[sep] = left->Keys[lastKey];
    parent->KeyLens[sep] = left->KeyLens[lastKey];
    left->KeyCount--;
    left->ChildCount--;
}

static void BPlusTreeManager_ReplaceSeparator(BPlusNode* parent, int sep, const BPlusNode* right)
{
    free(parent->Keys[sep]);
    parent->Keys[sep] = bplus_dup_key(right->Keys[0], right->KeyLens[0]);
    parent->KeyLens[sep] = right->KeyLens[0];
}

static void BPlusTreeManager_Redistribute(BPlusTreeManager* self, BPlusNode* parent, int leftIndex,
                                         BPlusNode* leftChild, BPlusNode* rightChild)
{
    if (leftChild->KeyCount < bplus_min_keys(self)) {
        if (leftChild->IsLeaf) {
            BPlusLeaf_MoveFirstToLeft(leftChild, rightChild);
            BPlusTreeManager_ReplaceSeparator(parent, leftIndex, rightChild);
        } else {
            BPlusInternal_MoveFirstChildToLeft(leftChild, rightChild, parent, leftIndex);
        }
    } else if (leftChild->IsLeaf) {
        BPlusLeaf_MoveLastToRight(leftChild, rightChild);
        BPlusTreeManager_ReplaceSeparator(parent, leftIndex, rightChild);
    } else {
        BPlusInternal_MoveLastChildToRight(leftChild, rightChild, parent, leftIndex);
    }
    BPlusTreeManager_SaveNode(self, parent);
    BPlusTreeManager_SaveNode(self, leftChild);
    BPlusTreeManager_SaveNode(self, rightChild);
}

static void BPlusTreeManager_Merge(BPlusTreeManager* self, BPlusNode* parent, int leftIndex)
{
    BPlusNode* leftChild = BPlusTreeManager_LoadNode(self, parent->Children[leftIndex]);
    BPlusNode* rightChild;
    int i;
    if (leftIndex < 0 || leftIndex + 1 >= parent->ChildCount) return;
    rightChild = BPlusTreeManager_LoadNode(self, parent->Children[leftIndex + 1]);
    if (!leftChild || !rightChild) return;
    if (leftChild->IsLeaf) {
        BPlusNode_EnsureKeys(leftChild, leftChild->KeyCount + rightChild->KeyCount);
        for (i = 0; i < rightChild->KeyCount; i++) {
            int d = leftChild->KeyCount + i;
            leftChild->Keys[d] = rightChild->Keys[i];
            leftChild->KeyLens[d] = rightChild->KeyLens[i];
            leftChild->Recs[d] = rightChild->Recs[i];
            leftChild->RecCounts[d] = rightChild->RecCounts[i];
        }
        leftChild->KeyCount += rightChild->KeyCount;
        rightChild->KeyCount = 0;
        leftChild->NextLeafOffset = rightChild->NextLeafOffset;
        if (rightChild->NextLeafOffset != -1) {
            BPlusNode* nxt = BPlusTreeManager_LoadNode(self, rightChild->NextLeafOffset);
            if (nxt && nxt->IsLeaf) {
                nxt->PrevLeafOffset = leftChild->NodeOffset;
                BPlusTreeManager_SaveNode(self, nxt);
            }
        }
    } else {
        BPlusNode_EnsureKeys(leftChild, leftChild->KeyCount + 1 + rightChild->KeyCount);
        BPlusNode_EnsureChildren(leftChild, leftChild->ChildCount + rightChild->ChildCount);
        leftChild->Keys[leftChild->KeyCount] = parent->Keys[leftIndex];
        leftChild->KeyLens[leftChild->KeyCount] = parent->KeyLens[leftIndex];
        leftChild->KeyCount++;
        for (i = 0; i < rightChild->KeyCount; i++) {
            leftChild->Keys[leftChild->KeyCount] = rightChild->Keys[i];
            leftChild->KeyLens[leftChild->KeyCount] = rightChild->KeyLens[i];
            leftChild->KeyCount++;
        }
        for (i = 0; i < rightChild->ChildCount; i++)
            leftChild->Children[leftChild->ChildCount++] = rightChild->Children[i];
        rightChild->KeyCount = 0;
        rightChild->ChildCount = 0;
    }
    if (leftChild->IsLeaf)
        free(parent->Keys[leftIndex]);
    BPlusInternal_RemoveKeyAt(parent, leftIndex);
    BPlusInternal_RemoveChildAt(parent, leftIndex + 1);
    BPlusTreeManager_SaveNode(self, leftChild);
    BPlusTreeManager_SaveNode(self, parent);
    BPlusTreeManager_AddPageToFreeList(self, rightChild->NodeOffset);
}

static void BPlusTreeManager_HandleUnderflow(BPlusTreeManager* self, BPlusNode* parent, int childIndex)
{
    int minKeys = bplus_min_keys(self);
    if (childIndex > 0) {
        BPlusNode* left = BPlusTreeManager_LoadNode(self, parent->Children[childIndex - 1]);
        if (left && left->KeyCount > minKeys) {
            BPlusTreeManager_Redistribute(self, parent, childIndex - 1, left,
                BPlusTreeManager_LoadNode(self, parent->Children[childIndex]));
            return;
        }
    }
    if (childIndex < parent->KeyCount) {
        BPlusNode* right = BPlusTreeManager_LoadNode(self, parent->Children[childIndex + 1]);
        if (right && right->KeyCount > minKeys) {
            BPlusTreeManager_Redistribute(self, parent, childIndex,
                BPlusTreeManager_LoadNode(self, parent->Children[childIndex]), right);
            return;
        }
    }
    if (childIndex > 0)
        BPlusTreeManager_Merge(self, parent, childIndex - 1);
    else
        BPlusTreeManager_Merge(self, parent, childIndex);
}

static void BPlusTreeManager_DeleteRecursive(BPlusTreeManager* self, int64_t pageOffset,
                                            const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusNode* page = BPlusTreeManager_LoadNode(self, pageOffset);
    int childIndex;
    int64_t childOffset;
    BPlusNode* child;
    if (!page) return;
    if (page->IsLeaf) {
        if (BPlusLeaf_Remove(page, key, keyLen, recordOffset))
            BPlusTreeManager_SaveNode(self, page);
        return;
    }
    childIndex = BPlusInternal_FindChildIndex(page, key, keyLen);
    childOffset = page->Children[childIndex];
    BPlusTreeManager_DeleteRecursive(self, childOffset, key, keyLen, recordOffset);
    child = BPlusTreeManager_LoadNode(self, childOffset);
    if (child && child->KeyCount < bplus_min_keys(self))
        BPlusTreeManager_HandleUnderflow(self, page, childIndex);
}

int BPlusTreeManager_Delete(BPlusTreeManager* self, const uint8_t* key, int keyLen, int64_t recordOffset)
{
    BPlusNode* rootPage;
    if (!self || !key) return GMB_DB_ERR;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootNodeOffset == -1) {
        pthread_mutex_unlock(&self->syncRoot);
        return GMB_DB_OK;
    }
    BPlusTreeManager_DeleteRecursive(self, self->rootNodeOffset, key, keyLen, recordOffset);
    rootPage = BPlusTreeManager_LoadNode(self, self->rootNodeOffset);
    if (rootPage && !rootPage->IsLeaf && rootPage->KeyCount == 0 && rootPage->ChildCount > 0) {
        int64_t oldRoot = self->rootNodeOffset;
        self->rootNodeOffset = rootPage->Children[0];
        BPlusTreeManager_UpdateRootOffsetInHeader(self);
        BPlusTreeManager_AddPageToFreeList(self, oldRoot);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

/* leftover inventory stubs (nomi void, non collidono con l'API sopra) */

static void bplus_add_tree_pages(BPlusTreeManager* self, int64_t pageOffset)
{
    BPlusNode* node;
    int i;
    if (pageOffset < 0) return;
    node = BPlusTreeManager_LoadNode(self, pageOffset);
    if (!node) return;
    if (!node->IsLeaf) {
        for (i = 0; i < node->ChildCount; i++)
            bplus_add_tree_pages(self, node->Children[i]);
    }
    BPlusTreeManager_AddPageToFreeList(self, pageOffset);
}

void BPlusTreeManager_AddTreeToFreeList(BPlusTreeManager* self)
{
    if (!self || self->rootNodeOffset < 0) return;
    bplus_add_tree_pages(self, self->rootNodeOffset);
    self->rootNodeOffset = -1;
    IsamLruCache_Clear(self->nodeCache);
}

void BPlusTreeManager_ReloadRootFromHeader(BPlusTreeManager* self)
{
    int64_t r;
    if (!self || !self->handle) return;
    if (self->secondarySlot >= 0 && self->secondarySlot < 15)
        r = self->handle->Header.SecondaryIndexRootOffsets[self->secondarySlot];
    else
        r = self->handle->Header.PrimaryKeyRootOffset;
    self->rootNodeOffset = (r == 0) ? -1 : r;
}

int BPlusTreeManager_RepairPointersAfterHeapMove(BPlusTreeManager* self,
                                                 int64_t heapStart, int64_t heapEnd, int64_t delta)
{
    int64_t queue[1024];
    int64_t seen[1024];
    int qh = 0, qt = 0, nseen = 0;
    if (!self) return GMB_DB_ERR;
    IsamLruCache_Clear(self->nodeCache);
    if (self->rootNodeOffset < 0) return GMB_DB_OK;
    queue[qt++] = self->rootNodeOffset;
    while (qh < qt) {
        int64_t off = queue[qh++];
        BPlusNode* node;
        int s, dup = 0, i;
        for (s = 0; s < nseen; s++) {
            if (seen[s] == off) { dup = 1; break; }
        }
        if (dup) continue;
        if (nseen < 1024) seen[nseen++] = off;
        node = BPlusTreeManager_LoadNode(self, off);
        if (!node) continue;
        node->NodeOffset = off;
        if (!node->IsLeaf) {
            for (i = 0; i < node->ChildCount; i++) {
                if (node->Children[i] >= heapStart && node->Children[i] < heapEnd)
                    node->Children[i] += delta;
                if (node->Children[i] >= heapStart && qt < 1024)
                    queue[qt++] = node->Children[i];
            }
        } else {
            if (node->NextLeafOffset >= heapStart && node->NextLeafOffset < heapEnd)
                node->NextLeafOffset += delta;
            if (node->PrevLeafOffset >= heapStart && node->PrevLeafOffset < heapEnd)
                node->PrevLeafOffset += delta;
            for (i = 0; i < node->KeyCount; i++) {
                int r;
                for (r = 0; r < node->RecCounts[i]; r++) {
                    if (node->Recs[i][r] >= heapStart && node->Recs[i][r] < heapEnd)
                        node->Recs[i][r] += delta;
                }
            }
            if (node->NextLeafOffset >= heapStart && qt < 1024)
                queue[qt++] = node->NextLeafOffset;
        }
        BPlusTreeManager_SaveNode(self, node);
    }
    return GMB_DB_OK;
}


/* Visitor dei record del B+ (tutte le foglie, record attivi e cancellati):
 * usato dal checker di integrita' (IsamIntegrityChecker). Ritorna GMB_DB_OK;
 * se il callback ritorna != 0 (errore segnalato), interrompe con GMB_DB_ERR. */
int BPlusTreeManager_VisitRecordOffsets(BPlusTreeManager* self, int (*cb)(void* ctx, int64_t offset), void* ctx)
{
    BPlusNode* leaf;
    int i, j;
    if (!self || !cb || self->rootNodeOffset < 0) return GMB_DB_OK;
    pthread_mutex_lock(&self->syncRoot);
    leaf = BPlusTreeManager_FindFirstLeaf(self);
    while (leaf) {
        for (i = 0; i < leaf->KeyCount; i++) {
            for (j = 0; j < leaf->RecCounts[i]; j++) {
                if (cb(ctx, leaf->Recs[i][j])) {
                    pthread_mutex_unlock(&self->syncRoot);
                    return GMB_DB_ERR;
                }
            }
        }
        if (leaf->NextLeafOffset < 0) break;
        leaf = BPlusTreeManager_LoadNode(self, leaf->NextLeafOffset);
    }
    pthread_mutex_unlock(&self->syncRoot);
    return GMB_DB_OK;
}

