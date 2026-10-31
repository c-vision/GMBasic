/* FILE: native/gmb_db_c/src/DBF/NtxFile.c */
/* Port 1:1 di modules/Database/DBF/NtxFile.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.NtxFile.cs
//
//using System.Diagnostics;
//using System.Text;
//using Basic.Databases;
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic.DBF;
//
///*
// * FORMATO FISICO DEL FILE NTX (Implementazione Personalizzata)
// *
// * HEADER DEL FILE (Primi 1024 byte - Dimensione della Pagina)
// * -----------------------------------------------------------
// * Offset  | Lunghezza | Descrizione
// * -----------------------------------------------------------
// * 0       | 4 byte    | uint: Numero della pagina radice (Root Page).
// * 4       | 4 byte    | uint: Numero della prima pagina foglia (First Leaf Page) per la scansione sequenziale.
// * 8       | 2 byte    | ushort: Lunghezza della chiave di indice (KeyLength).
// * 10      | 1 byte    | byte: Flags (es. 0x01 = Indice UNIQUE).
// * 11      | 1 byte    | byte: Riservato.
// * 12      | 4 byte    | uint: Numero della pagina in testa alla Free List.
// * 16      | 256 byte  | stringa ASCII (null-terminated): Espressione della chiave di indice.
// * 272     | 752 byte  | Riservato per usi futuri.
// * -----------------------------------------------------------
// *
// * STRUTTURA DELLE PAGINE (Nodi del B+Tree)
// * -----------------------------------------------------------
// * Tutte le pagine hanno una dimensione fissa di 1024 byte.
// *
// * HEADER DELLA PAGINA (Primi 12 byte)
// * -----------------------------------
// * Offset  | Lunghezza | Descrizione
// * -----------------------------------
// * 0       | 2 byte    | ushort: Numero di chiavi nella pagina (KeyCount).
// * 2       | 2 byte    | ushort: Flags di pagina (0x0001 = Pagina Foglia (IsLeaf)).
// * 4       | 4 byte    | uint: (Solo per foglie) Puntatore alla pagina foglia successiva (NextPageNum).
// * 8       | 4 byte    | uint: (Solo per foglie) Puntatore alla pagina foglia precedente (PrevPageNum).
// * -----------------------------------
// *
// * CONTENUTO PAGINA FOGLIA (Leaf Page)
// * -----------------------------------
// * Header (12 byte), seguito da una sequenza di entry [Chiave][Recno]:
// * [Chiave (KeyLength byte)] [Numero Record (uint, 4 byte)]
// * [Chiave (KeyLength byte)] [Numero Record (uint, 4 byte)]
// * ...
// *
// * CONTENUTO PAGINA INTERNA (Internal Page)
// * ----------------------------------------
// * Header (12 byte), seguito da una sequenza di [Puntatore][Chiave]:
// * [Puntatore a Figlio Sinistro (uint, 4 byte)]
// * [Chiave (KeyLength byte)] [Puntatore a Figlio Destro (uint, 4 byte)]
// * [Chiave (KeyLength byte)] [Puntatore a Figlio Destro (uint, 4 byte)]
// */
///// <summary>
///// Rappresenta una pagina fisica (blocco) all'interno del file di indice.
///// L'header è stato esteso per includere puntatori per la lista concatenata delle foglie.
///// </summary>
//internal sealed class NtxPage
//{
//    /// <summary>
//    /// Dimensione dell'header: KeyCount (2b), Flags/Tipo (2b), NextLeaf (4b), PrevLeaf (4b).
//    /// </summary>
//    public const int HeaderSize = 12;
//
//    public readonly int PageSize;
//    public ushort KeyCount { get; private set; }
//    public bool IsLeaf { get; private set; }
//
//    /// <summary>
//    /// Puntatore (numero di pagina) al prossimo nodo foglia nella sequenza. Vale 0 se non c'è.
//    /// </summary>
//    public uint NextPageNum { get; private set; }
//
//    public uint NodeOffset { get; set; }
//
//    /// <summary>
//    /// Puntatore (numero di pagina) al precedente nodo foglia nella sequenza. Vale 0 se non c'è.
//    /// </summary>
//    public uint PrevPageNum { get; private set; }
//
//    public byte[] Data { get; }
//
//    public NtxPage(int pageSize)
//    {
//        PageSize = pageSize;
//        Data = new byte[pageSize];
//    }
//
//    /// <summary>
//    /// Carica i dati da un buffer e popola le proprietà della pagina, inclusi i puntatori sequenziali.
//    /// </summary>
//    public void Load(ReadOnlySpan<byte> src)
//    {
//        src.CopyTo(Data);
//        KeyCount = BitConverter.ToUInt16(src[..2]);
//        IsLeaf = (BitConverter.ToUInt16(src.Slice(2, 2)) & 1) == 1;
//
//        // I puntatori Next/Prev sono validi solo per i nodi foglia.
//        if (IsLeaf)
//        {
//            NextPageNum = BitConverter.ToUInt32(src.Slice(4, 4));
//            PrevPageNum = BitConverter.ToUInt32(src.Slice(8, 4));
//        }
//    }
//}
//
///// <summary>
///// Implementa l'interfaccia IIndexHandle per la gestione di un file di indice in formato NTX.
///// </summary>
//internal sealed class NtxFile : IWritableIndexHandle
//{
//    private readonly FileStream _fs;
//    private readonly int _pageSize;
//    private readonly string _keyExpressionString;
//    private readonly ILockManager _lockManager;
//    private uint _rootPageNum;
//    private uint _firstLeafPageNum;
//    private uint _freeListPageHead;
//    private readonly LruCache<uint, NtxPage> _nodeCache = new(256);
//
//    public string Name { get; }
//    public bool Unique { get; }
//    public bool Descending { get; }
//    public int KeyLength { get; }
//
//    /// <summary>
//    /// Ottiene il percorso completo del file di indice.
//    /// </summary>
//    public string FilePath { get; }
//
//    /// <summary>
//    /// Oggetto per la sincronizzazione thread-safe dell'accesso all'indice.
//    /// </summary>
//    private readonly object _syncRoot = new();
//
//    /// <summary>
//    /// Flag che determina se la cache dei nodi (pagine) è abilitata.
//    /// Viene disabilitata in modalità CrossProcess per prevenire incoerenze.
//    /// </summary>
//    private readonly bool _useNodeCache;
//    
//    /// <summary>
//    /// Inizializza una nuova istanza della classe NtxFile aprendo un file di indice esistente.
//    /// </summary>
//    /// <param name="path">Il percorso del file di indice.</param>
//    /// <param name="lockManager">L'istanza del gestore di lock multi-processo associato al file DBF.</param>
//    /// <param name="lockingStrategy">La strategia di locking in uso, per decidere se abilitare la cache.</param>
//    public NtxFile(string path, ILockManager lockManager, LockingStrategy lockingStrategy)
//    {
//        FilePath = path;
//        _pageSize = 1024;
//        _lockManager = lockManager;
//        
//        _useNodeCache = lockingStrategy == LockingStrategy.IntraProcess;
//
//        _fs = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.Read, _pageSize,
//            FileOptions.RandomAccess);
//        
//        if (_fs.Length < _pageSize)
//            throw new IOException($"File di indice '{path}' non valido o corrotto.");
//
//        var headerBuffer = new byte[_pageSize];
//        _fs.Position = 0;
//        _fs.ReadExactly(headerBuffer, 0, headerBuffer.Length);
//
//        _rootPageNum = BitConverter.ToUInt32(headerBuffer, 0);
//        _firstLeafPageNum = BitConverter.ToUInt32(headerBuffer, 4);
//        _freeListPageHead = BitConverter.ToUInt32(headerBuffer, 12);
//        KeyLength = BitConverter.ToUInt16(headerBuffer, 8);
//        Unique = (headerBuffer[10] & 1) == 1;
//
//        var exprEnd = Array.IndexOf(headerBuffer, (byte)0, 16);
//        if (exprEnd == -1) exprEnd = 16 + 256;
//        
//        // Memorizza la stringa dell'espressione di chiave originale.
//        _keyExpressionString = Encoding.ASCII.GetString(headerBuffer, 16, exprEnd - 16);
//
//        // Parsa l'espressione solo per una validazione iniziale, ma non la memorizza.
//        //ParseKeyExpression(_keyExpressionString);
//        
//        Name = Path.GetFileNameWithoutExtension(path);
//        Descending = false;
//    }
//
//    /*
//    /// <summary>
//    /// Parsa la stringa dell'espressione di chiave in un albero Expr
//    /// utilizzando il parser centrale dell'interprete.
//    /// </summary>
//    private Expr ParseKeyExpression(string expression)
//    {
//        try
//        {
//            return Parser.ParseSingleExpression(expression);
//        }
//        catch (Exception ex)
//        {
//            // Trasforma l'eccezione del parser in un'eccezione più informativa
//            // per l'utente finale, indicando che la chiave dell'indice è corrotta o non supportata.
//            throw new InvalidDataException(
//                $"Impossibile parsare l'espressione di chiave dell'indice ('{expression}'). " +
//                $"L'indice potrebbe essere corrotto o usare una sintassi non supportata. Dettagli: {ex.Message}", ex);
//        }
//    }
//    */
//
//    /// <summary>
//    /// Restituisce l'espressione di chiave originale come stringa,
//    /// così come è memorizzata nell'header dell'indice.
//    /// </summary>
//    public string GetKeyExpressionString() => _keyExpressionString;
//    
//    /// <summary>
//    /// Cerca una chiave nell'indice e restituisce il numero del primo record corrispondente,
//    /// gestendo il locking a livello di chiave per il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="keyBytes">La chiave da cercare, già normalizzata.</param>
//    /// <param name="soft">Se true, in caso di non corrispondenza esatta, restituisce il record con la chiave immediatamente successiva (soft seek).</param>
//    /// <param name="context">Il contesto della transazione attiva. Se `Serializable`, viene acquisito
//    /// un lock di lettura sulla chiave per prevenire le letture fantasma.</param>
//    /// <returns>Il numero del record (1-based) o -1 se non trovato.</returns>
//    public int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
//    {
//        // Se in una transazione Serializable, acquisisci un lock di lettura sull'intervallo di chiave.
//        if (context is { IsolationLevel: IsolationLevel.Serializable })
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
//            _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}'");
//            context.ReadLockedKeys.Add(lockKey);
//        }
//
//        lock (_syncRoot)
//        {
//            if (_rootPageNum == 0) return -1; // L'indice è vuoto o non valido.
//
//            var pageNum = _rootPageNum;
//            while (true)
//            {
//                var page = ReadPage(pageNum);
//                if (page.IsLeaf)
//                {
//                    // Se siamo arrivati a un nodo foglia, la ricerca termina qui.
//                    return FindInLeaf(page, keyBytes, soft);
//                }
//                else
//                {
//                    // Se siamo in un nodo interno, troviamo il puntatore al prossimo
//                    // livello dell'albero e continuiamo la discesa.
//                    pageNum = DescendInternal(page, keyBytes);
//                }
//            }
//        }
//    }
//
//    /// <summary>
//    /// Inserisce una nuova coppia chiave/record nell'indice, gestendo il locking a livello di chiave
//    /// per supportare il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="keyBytes">La chiave da inserire, già normalizzata.</param>
//    /// <param name="recno">Il numero del record (1-based) associato alla chiave.</param>
//    /// <param name="context">Il contesto della transazione attiva. Se `Serializable`, viene acquisito
//    /// un lock di scrittura sulla chiave per prevenire le letture fantasma.</param>
//    public void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        AcquireSerializableWriteLock(keyBytes, context);
//
//        lock (_syncRoot)
//        {
//            InsertUnderLock(keyBytes, recno);
//        }
//    }
//
//    /// <inheritdoc />
//    public void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context)
//    {
//        if (entries.Count == 0)
//        {
//            return;
//        }
//
//        foreach (var (key, _) in entries)
//        {
//            AcquireSerializableWriteLock(key.Span, context);
//        }
//
//        lock (_syncRoot)
//        {
//            foreach (var (key, recno) in entries)
//            {
//                InsertUnderLock(key.Span, recno);
//            }
//        }
//    }
//
//    private void AcquireSerializableWriteLock(ReadOnlySpan<byte> keyBytes, TransactionContext? context)
//    {
//        if (context is not { IsolationLevel: IsolationLevel.Serializable })
//        {
//            return;
//        }
//
//        var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
//        _lockManager.AcquireWriteLock(lockKey, 5000, $"chiave di indice '{Name}'");
//        context.WriteLockedKeys.Add(lockKey);
//    }
//
//    private void InsertUnderLock(ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        if (_rootPageNum == 0) throw new InvalidOperationException("Indice non inizializzato o corrotto.");
//
//        var oldRootPage = ReadPage(_rootPageNum);
//        var promoted = InsertRecursive(_rootPageNum, keyBytes, recno);
//
//        if (promoted.key != null && promoted.newPageNum != 0)
//        {
//            var newRootPage = new NtxPage(_pageSize);
//            var span = newRootPage.Data.AsSpan();
//
//            BitConverter.GetBytes((ushort)1).CopyTo(span[..2]);
//            BitConverter.GetBytes((ushort)PageType.Internal).CopyTo(span.Slice(2, 2));
//
//            BitConverter.GetBytes(_rootPageNum).CopyTo(span[NtxPage.HeaderSize..]);
//            promoted.key.CopyTo(span[(NtxPage.HeaderSize + 4)..]);
//            BitConverter.GetBytes(promoted.newPageNum).CopyTo(span[(NtxPage.HeaderSize + 4 + KeyLength)..]);
//
//            var newRootPageNum = AllocateNewPage();
//            WritePage(newRootPageNum, newRootPage);
//
//            UpdateRootPageInHeader(newRootPageNum);
//
//            if (oldRootPage.IsLeaf)
//            {
//                UpdateFirstLeafInHeader(_rootPageNum);
//            }
//
//            _rootPageNum = newRootPageNum;
//        }
//    }
//
//    /// <summary>
//    /// Rimuove una coppia chiave/record dall'indice, gestendo il locking a livello di chiave
//    /// per supportare il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="keyBytes">La chiave da rimuovere, già normalizzata.</param>
//    /// <param name="recno">Lo specifico numero di record da rimuovere (per chiavi duplicate).</param>
//    /// <param name="context">Il contesto della transazione attiva. Se `Serializable`, viene acquisito
//    /// un lock di scrittura sulla chiave per prevenire conflitti con letture di intervalli.</param>
//    public void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context)
//    {
//        // Se in una transazione Serializable, acquisisci un lock di scrittura sulla chiave.
//        if (context is { IsolationLevel: IsolationLevel.Serializable })
//        {
//            var lockKey = LockManagerBase.GetKeyLockKey(Name, keyBytes.ToArray());
//            _lockManager.AcquireWriteLock(lockKey, 5000, $"chiave di indice '{Name}'");
//            context.WriteLockedKeys.Add(lockKey);
//        }
//
//        lock (_syncRoot)
//        {
//            if (_rootPageNum == 0) return;
//            DeleteRecursive(_rootPageNum, keyBytes, recno);
//
//            var rootPage = ReadPage(_rootPageNum);
//            // Controlla se la radice si è svuotata (possibile dopo un merge che risale l'albero).
//            if (rootPage.IsLeaf || rootPage.KeyCount != 0) return;
//            var oldRootPageNum = _rootPageNum;
//            _rootPageNum = GetInternalPointer(rootPage, 0);
//            UpdateRootPageInHeader(_rootPageNum);
//
//            AddPageToFreeList(oldRootPageNum);
//        }
//    }
//
//    /// <summary>
//    /// Aggiorna una voce dell'indice rimuovendo la vecchia chiave e inserendo quella nuova,
//    /// gestendo il locking a livello di chiave per il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="oldKey">La vecchia chiave da rimuovere.</param>
//    /// <param name="newKey">La nuova chiave da inserire.</param>
//    /// <param name="recno">Il numero del record (1-based).</param>
//    /// <param name="context">Il contesto della transazione attiva. Se `Serializable`, vengono acquisiti
//    /// lock di scrittura sia sulla vecchia che sulla nuova chiave.</param>
//    /// <remarks>
//    /// Questa operazione è intrinsecamente thread-safe perché esegue la cancellazione e l'inserimento
//    /// all'interno di un unico blocco `lock`, garantendone l'atomicità a livello di thread.
//    /// Il locking transazionale a livello di chiave viene delegato ai metodi `Delete` e `Insert`.
//    /// </remarks>
//    public void Update(ReadOnlySpan<byte> oldKey, ReadOnlySpan<byte> newKey, int recno, TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            Delete(oldKey, recno, context);
//            Insert(newKey, recno, context);
//        }
//    }
//
//    /// <summary>
//    /// Ricostruisce l'intero indice da zero in modo efficiente a partire da una sorgente dati ordinata.
//    /// Questa operazione è thread-safe.
//    /// </summary>
//    /// <param name="producer">Una funzione che restituisce un'enumerazione di coppie (chiave, recno) già ordinate.</param>
//    public void Rebuild(Func<IEnumerable<(ReadOnlyMemory<byte> key, int recno)>> producer)
//    {
//        lock (_syncRoot)
//        {
//            InitializeNewIndex();
//            var entries = producer().ToList();
//            if (entries.Count == 0) return;
//
//            var leafLevel = BuildLeafLevel(entries);
//            var parentLevel = leafLevel;
//            while (parentLevel.Count > 1)
//            {
//                parentLevel = BuildInternalLevel(parentLevel);
//            }
//
//            var finalRootPage = parentLevel.FirstOrDefault().pageNum;
//            var firstLeafPage = leafLevel.FirstOrDefault().pageNum;
//
//            UpdateRootPageInHeader(finalRootPage);
//            UpdateFirstLeafInHeader(firstLeafPage);
//
//            _rootPageNum = finalRootPage;
//            _firstLeafPageNum = firstLeafPage;
//        }
//    }
//
//    /// <summary>
//    /// Ottiene il numero del primo record secondo l'ordine dell'indice.
//    /// Questa operazione è thread-safe.
//    /// </summary>
//    public int GetFirstRecordNumber()
//    {
//        lock (_syncRoot)
//        {
//            if (_rootPageNum == 0) return 0;
//
//            var pageNum = _rootPageNum;
//            while (true)
//            {
//                var page = ReadPage(pageNum);
//                if (page.IsLeaf)
//                {
//                    return page.KeyCount > 0 ? (int)GetLeafRecno(page, 0) : 0;
//                }
//
//                pageNum = BitConverter.ToUInt32(page.Data.AsSpan(NtxPage.HeaderSize));
//            }
//        }
//    }
//
//    /// <summary>
//    /// Ottiene il numero dell'ultimo record secondo l'ordine dell'indice.
//    /// Questa operazione è thread-safe.
//    /// </summary>
//    public int GetLastRecordNumber()
//    {
//        lock (_syncRoot)
//        {
//            if (_rootPageNum == 0) return 0;
//
//            var pageNum = _rootPageNum;
//            while (true)
//            {
//                var page = ReadPage(pageNum);
//                if (page.IsLeaf)
//                {
//                    return page.KeyCount > 0 ? (int)GetLeafRecno(page, page.KeyCount - 1) : 0;
//                }
//
//                var lastPointerOffset = NtxPage.HeaderSize + (page.KeyCount * (4 + KeyLength));
//                pageNum = BitConverter.ToUInt32(page.Data.AsSpan(lastPointerOffset));
//            }
//        }
//    }
//
//    /// <summary>
//    /// Restituisce un'enumerazione di tutti i numeri di record presenti nell'indice,
//    /// nell'ordine corretto della chiave. Questa operazione è thread-safe.
//    /// </summary>
//    public IEnumerable<int> GetAllRecordNumbers()
//    {
//        // NOTA: Il lock viene acquisito implicitamente perché questo metodo è un iteratore.
//        // Il lock rimarrà attivo per l'intera durata dell'enumerazione.
//        lock (_syncRoot)
//        {
//            if (_firstLeafPageNum == 0) yield break;
//
//            var currentPageNum = _firstLeafPageNum;
//
//            while (currentPageNum != 0)
//            {
//                var leafPage = ReadPage(currentPageNum);
//
//                if (!leafPage.IsLeaf) yield break;
//
//                for (var i = 0; i < leafPage.KeyCount; i++)
//                {
//                    yield return (int)GetLeafRecno(leafPage, i);
//                }
//
//                currentPageNum = leafPage.NextPageNum;
//            }
//        }
//    }
//
//    /// <summary>
//    /// Restituisce i numeri di record per un intervallo di chiavi. (Non implementato)
//    /// </summary>
//    public IEnumerable<int> Range(ReadOnlySpan<byte> lo, bool loInc, ReadOnlySpan<byte> hi, bool hiInc)
//    {
//        yield break;
//    }
//
//    /// <summary>
//    /// Esegue una serie di controlli per verificare l'integrità strutturale dell'indice B+Tree.
//    /// L'operazione è thread-safe.
//    /// </summary>
//    /// <returns>True se l'indice è strutturalmente valido, altrimenti False.</returns>
//    public bool ValidateIndex()
//    {
//        lock (_syncRoot)
//        {
//            try
//            {
//                if (_rootPageNum == 0) return _fs.Length == _pageSize; // Un albero vuoto è valido
//                
//                // Controlla la coerenza strutturale dell'albero B+
//                if (!ValidatePageRecursive(_rootPageNum, []))
//                    return false;
//
//                // Controlla la coerenza della lista doppiamente concatenata delle foglie
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
//    /// Ottiene la posizione del primo record nell'ordine dell'indice, acquisendo un lock di lettura
//    /// sulla sua chiave se in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
//    /// <returns>Una tupla contenente il numero del record, il numero della pagina foglia e l'indice all'interno della pagina.</returns>
//    public (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            if (_firstLeafPageNum == 0) return (0, 0, 0);
//
//            var firstLeaf = ReadPage(_firstLeafPageNum);
//            if (firstLeaf.KeyCount == 0) return (0, 0, 0);
//
//            var recno = (int)GetLeafRecno(firstLeaf, 0);
//            
//            // Se in una transazione Serializable, acquisisci un lock di lettura sulla prima chiave.
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                var firstKey = GetLeafKey(firstLeaf, 0).ToArray();
//                var lockKey = LockManagerBase.GetKeyLockKey(Name, firstKey);
//                _lockManager.AcquireReadLock(lockKey, 5000, $"prima chiave dell'indice '{Name}'");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (recno, _firstLeafPageNum, 0);
//        }
//    }
//    
//    /// <summary>
//    /// Ottiene la posizione dell'ultimo record nell'ordine dell'indice, acquisendo un lock di lettura
//    /// sulla sua chiave se in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
//    /// <returns>Una tupla contenente il numero del record, il numero della pagina foglia e l'indice all'interno della pagina.</returns>
//    public (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            if (_rootPageNum == 0) return (0, 0, 0);
//
//            var pageNum = _rootPageNum;
//            NtxPage page;
//            while (true)
//            {
//                page = ReadPage(pageNum);
//                if (page.IsLeaf) break;
//
//                pageNum = GetInternalPointer(page, page.KeyCount);
//            }
//
//            if (page.KeyCount == 0) return (0, 0, 0);
//
//            var lastIndex = page.KeyCount - 1;
//            var recno = (int)GetLeafRecno(page, lastIndex);
//            
//            // Se in una transazione Serializable, acquisisci un lock di lettura sull'ultima chiave.
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                var lastKey = GetLeafKey(page, lastIndex).ToArray();
//                var lockKey = LockManagerBase.GetKeyLockKey(Name, lastKey);
//                _lockManager.AcquireReadLock(lockKey, 5000, $"ultima chiave dell'indice '{Name}'");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (recno, pageNum, lastIndex);
//        }
//    }
//
//    /// <summary>
//    /// A partire da una posizione specifica, trova il record successivo o precedente nell'ordine dell'indice,
//    /// acquisendo un lock di lettura sulle nuove chiavi attraversate in una transazione `Serializable`.
//    /// </summary>
//    /// <param name="currentPageNum">Il numero della pagina foglia di partenza.</param>
//    /// <param name="currentIndexInLeaf">L'indice all'interno della pagina foglia di partenza.</param>
//    /// <param name="direction">La direzione dello spostamento (1 per successivo, -1 per precedente).</param>
//    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
//    /// <returns>Una tupla contenente il nuovo numero di record, la nuova pagina e il nuovo indice. Restituisce (0, 0, 0) se si raggiunge BOF o EOF.</returns>
//    public (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(uint currentPageNum, int currentIndexInLeaf,
//        int direction, TransactionContext? context)
//    {
//        lock (_syncRoot)
//        {
//            if (currentPageNum == 0) return (0, 0, 0);
//
//            var currentPage = ReadPage(currentPageNum);
//            var newIndex = currentIndexInLeaf + direction;
//            
//            uint newPageNum;
//            int newIndexInLeaf;
//            NtxPage finalPage;
//
//            if (newIndex >= 0 && newIndex < currentPage.KeyCount)
//            {
//                newPageNum = currentPageNum;
//                newIndexInLeaf = newIndex;
//                finalPage = currentPage;
//            }
//            else
//            {
//                newPageNum = (direction > 0) ? currentPage.NextPageNum : currentPage.PrevPageNum;
//                if (newPageNum == 0) return (0, 0, 0);
//
//                finalPage = ReadPage(newPageNum);
//                if (finalPage.KeyCount == 0) return (0, 0, 0);
//                
//                newIndexInLeaf = (direction > 0) ? 0 : finalPage.KeyCount - 1;
//            }
//
//            var recno = (int)GetLeafRecno(finalPage, newIndexInLeaf);
//            
//            // Se in una transazione Serializable, acquisisci un lock di lettura sulla chiave del nuovo record trovato.
//            if (context is { IsolationLevel: IsolationLevel.Serializable })
//            {
//                var newKey = GetLeafKey(finalPage, newIndexInLeaf).ToArray();
//                var lockKey = LockManagerBase.GetKeyLockKey(Name, newKey);
//                _lockManager.AcquireReadLock(lockKey, 5000, $"chiave di indice '{Name}' durante la scansione");
//                context.ReadLockedKeys.Add(lockKey);
//            }
//
//            return (recno, newPageNum, newIndexInLeaf);
//        }
//    }
//
//    /// <summary>
//    /// Crea un nuovo file di indice (.ntx) vuoto su disco e restituisce un'istanza di NtxFile per gestirlo.
//    /// Se il file esiste già, verrà sovrascritto.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del file di indice da creare.</param>
//    /// <param name="keyExpression">L'espressione di chiave che definirà l'ordine dell'indice.</param>
//    /// <param name="keyLength">La lunghezza fissa in byte della chiave dell'indice.</param>
//    /// <param name="isUnique">True se l'indice non ammette chiavi duplicate, altrimenti False.</param>
//    /// <param name="lockManager">L'istanza del gestore di lock multi-processo associato al file DBF.</param>
//    /// <returns>Una nuova istanza di <see cref="NtxFile"/> pronta per l'inserimento di record.</returns>
//    /// /// <param name="lockingStrategy">La strategia di locking in uso, per decidere se abilitare la cache.</param>
//    public static NtxFile CreateNew(string filePath, Expr keyExpression, int keyLength, bool isUnique, 
//        ILockManager lockManager, LockingStrategy lockingStrategy)
//    {
//        const int pageSize = 1024;
//
//        // Crea fisicamente il file, sovrascrivendolo se esiste.
//        using (var fs = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None))
//        {
//            // --- Scrive l'Header del File ---
//            var headerBuffer = new byte[pageSize];
//            var headerSpan = headerBuffer.AsSpan();
//
//            const uint rootPageNum = 1;
//            const uint firstLeafPageNum = 1;
//
//            BitConverter.GetBytes(rootPageNum).CopyTo(headerSpan[..4]);
//            BitConverter.GetBytes(firstLeafPageNum).CopyTo(headerSpan.Slice(4, 4));
//            BitConverter.GetBytes((ushort)keyLength).CopyTo(headerSpan.Slice(8, 2));
//            headerSpan[10] = isUnique ? (byte)1 : (byte)0;
//            BitConverter.GetBytes((uint)0).CopyTo(headerSpan.Slice(12, 4));
//
//            var keyExprString = DbfTotalExpressions.ToPlainString(keyExpression);
//            Encoding.ASCII.GetBytes(keyExprString, 0, Math.Min(keyExprString.Length, 255), headerBuffer, 16);
//
//            fs.Write(headerBuffer, 0, headerBuffer.Length);
//
//            // --- Crea e Scrive la Prima Pagina (Radice/Foglia) ---
//            var pageBuffer = new byte[pageSize];
//            var pageSpan = pageBuffer.AsSpan();
//            pageSpan.Clear();
//
//            BitConverter.GetBytes((ushort)0).CopyTo(pageSpan[..2]);
//            BitConverter.GetBytes((ushort)1).CopyTo(pageSpan.Slice(2, 2));
//            BitConverter.GetBytes((uint)0).CopyTo(pageSpan.Slice(4, 4));
//            BitConverter.GetBytes((uint)0).CopyTo(pageSpan.Slice(8, 4));
//
//            fs.Write(pageBuffer, 0, pageBuffer.Length);
//            fs.Flush();
//        }
//
//        // Ora che il file è stato creato, lo apre usando il costruttore standard,
//        // passando il lock manager e la locking strategy richiesti.
//        return new NtxFile(filePath, lockManager, lockingStrategy);
//    }
//
//    /// <summary>
//    /// Invalida lo stato interno. Per NtxFile, che non usa una cache di record,
//    /// questa operazione non ha effetti.
//    /// </summary>
//    public void InvalidateCache()
//    {
//        lock (_syncRoot)
//        {
//            // Nessuna azione richiesta, NtxFile non si basa su una cache di record.
//        }
//    }
//
//    /// <summary>
//    /// Rilascia le risorse del file stream in modo thread-safe.
//    /// </summary>
//    public void Dispose()
//    {
//        lock (_syncRoot)
//        {
//            _fs.Dispose();
//        }
//    }
//
//    #region Private B+Tree Implementation
//
//    /// <summary>
//    /// Costruisce l'intero livello delle foglie e le collega in una lista doppiamente concatenata.
//    /// </summary>
//    private List<(uint pageNum, byte[] key)> BuildLeafLevel(List<(ReadOnlyMemory<byte> key, int recno)> entries)
//    {
//        var leafPagesInfo = new List<(uint pageNum, byte[] key)>();
//        var page = new NtxPage(_pageSize);
//
//        foreach (var (key, recno) in entries)
//        {
//            if (page.KeyCount >= GetMaxKeysPerLeaf())
//            {
//                var pageNum = AllocateNewPage();
//                leafPagesInfo.Add((pageNum, GetLeafKey(page, 0).ToArray()));
//                WritePage(pageNum, page);
//                page = new NtxPage(_pageSize);
//            }
//
//            InsertIntoLeaf(page, key.Span, recno);
//        }
//
//        if (page.KeyCount > 0)
//        {
//            var pageNum = AllocateNewPage();
//            leafPagesInfo.Add((pageNum, GetLeafKey(page, 0).ToArray()));
//            WritePage(pageNum, page);
//        }
//
//        // Collega le pagine foglia
//        for (var i = 0; i < leafPagesInfo.Count; i++)
//        {
//            var pageNum = leafPagesInfo[i].pageNum;
//            var currentPage = ReadPage(pageNum);
//            var prevNum = (i > 0) ? leafPagesInfo[i - 1].pageNum : 0;
//            var nextNum = (i < leafPagesInfo.Count - 1) ? leafPagesInfo[i + 1].pageNum : 0;
//            UpdateLeafPointers(currentPage, nextNum, prevNum);
//            WritePage(pageNum, currentPage);
//        }
//
//        return leafPagesInfo;
//    }
//
//    /// <summary>
//    /// Costruisce un livello di nodi interni a partire dal livello figlio sottostante.
//    /// </summary>
//    private List<(uint pageNum, byte[] key)> BuildInternalLevel(List<(uint pageNum, byte[] key)> childLevel)
//    {
//        var parentLevelInfo = new List<(uint pageNum, byte[] key)>();
//        var parentPage = new NtxPage(_pageSize);
//
//        // Il primo puntatore del primo nodo padre è la prima pagina del livello figlio.
//        var firstPointer = childLevel.First().pageNum;
//
//        for (var i = 1; i < childLevel.Count; i++)
//        {
//            var childInfo = childLevel[i];
//            if (parentPage.KeyCount >= GetMaxKeysPerInternal())
//            {
//                var pageNum = AllocateNewPage();
//                // La "chiave" di un nodo padre è la prima chiave che contiene, per riferimento.
//                parentLevelInfo.Add((pageNum, GetInternalKey(parentPage, 0).ToArray()));
//                WritePage(pageNum, parentPage);
//                parentPage = new NtxPage(_pageSize);
//                firstPointer = childInfo.pageNum; // Il primo puntatore del nuovo nodo padre
//            }
//
//            // Aggiunge la chiave (che è la prima chiave del figlio destro) e il puntatore a quel figlio
//            InsertIntoInternal(parentPage, childInfo.key, firstPointer);
//            firstPointer = childInfo.pageNum;
//        }
//
//        if (parentPage.KeyCount > 0)
//        {
//            var pageNum = AllocateNewPage();
//            parentLevelInfo.Add((pageNum, GetInternalKey(parentPage, 0).ToArray()));
//            WritePage(pageNum, parentPage);
//        }
//
//        return parentLevelInfo;
//    }
//
//    /// <summary>
//    /// Metodo helper ricorsivo per la validazione delle pagine dell'indice.
//    /// </summary>
//    /// <param name="pageNum">Il numero della pagina da validare.</param>
//    /// <param name="visitedPages">Un set di numeri di pagina già visitati per rilevare i cicli.</param>
//    /// <returns>True se la pagina e tutti i suoi sotto-alberi sono validi.</returns>
//    private bool ValidatePageRecursive(uint pageNum, HashSet<uint> visitedPages)
//    {
//        // Rileva cicli: se abbiamo già visitato questa pagina, l'indice è corrotto.
//        if (!visitedPages.Add(pageNum)) return false;
//
//        try
//        {
//            var page = ReadPage(pageNum);
//            // Controlla che il numero di chiavi sia entro i limiti validi.
//            if (page.KeyCount > GetMaxKeysPerLeaf()) return false;
//
//            if (!page.IsLeaf)
//            {
//                // Se è un nodo interno, valida ricorsivamente tutti i suoi figli.
//                for (var i = 0; i <= page.KeyCount; i++)
//                {
//                    var childPageNum = GetInternalPointer(page, i);
//                    if (childPageNum == 0 || !ValidatePageRecursive(childPageNum, visitedPages))
//                        return false;
//                }
//            }
//
//            return true; // La pagina e i suoi sotto-alberi sono validi.
//        }
//        catch
//        {
//            // Errori di I/O o di formato durante la lettura indicano corruzione.
//            return false;
//        }
//    }
//
//    /// <summary>
//    /// Aggiorna il puntatore alla prima pagina foglia nell'header del file di indice.
//    /// </summary>
//    /// <param name="firstLeafPageNum">Il numero della nuova prima pagina foglia.</param>
//    private void UpdateFirstLeafInHeader(uint firstLeafPageNum)
//    {
//        // L'offset del puntatore alla prima foglia nell'header NTX è 4.
//        _fs.Position = 4;
//        using var writer = new BinaryWriter(_fs, Encoding.ASCII, leaveOpen: true);
//        writer.Write(firstLeafPageNum);
//        _firstLeafPageNum = firstLeafPageNum; // Aggiorna anche il campo in memoria
//    }
//
//    /// <summary>
//    /// Aggiorna il puntatore alla testa della free list nell'header del file di indice.
//    /// </summary>
//    /// <param name="newHeadPageNum">Il numero della nuova pagina in testa alla lista.</param>
//    private void UpdateFreeListHeadInHeader(uint newHeadPageNum)
//    {
//        // L'offset del puntatore alla free list nell'header NTX è 12.
//        _fs.Position = 12;
//        using var writer = new BinaryWriter(_fs, Encoding.ASCII, leaveOpen: true);
//        writer.Write(newHeadPageNum);
//        _freeListPageHead = newHeadPageNum; // Aggiorna anche il campo in memoria
//    }
//
//    /// <summary>
//    /// Rimuove l'ULTIMA entry (chiave, recno) da un nodo foglia.
//    /// </summary>
//    private void RemoveLastKeyFromLeaf(NtxPage page)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        if (keyCount == 0) return;
//
//        var entrySize = KeyLength + 4;
//
//        // Pulisce lo spazio dell'ultima entry.
//        pageSpan[(NtxPage.HeaderSize + (keyCount - 1) * entrySize)..].Clear();
//
//        // Aggiorna il contatore.
//        BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Rimuove l'ULTIMA entry (chiave e puntatore destro) da un nodo interno.
//    /// </summary>
//    private void RemoveLastKeyFromInternal(NtxPage page)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        if (keyCount == 0) return;
//
//        var entrySize = KeyLength + 4;
//
//        // Pulisce lo spazio dell'ultima entry [Key][Ptr].
//        pageSpan[(NtxPage.HeaderSize + (keyCount - 1) * entrySize + 4)..].Clear();
//
//        // Aggiorna il contatore.
//        BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Metodo ricorsivo che attraversa l'albero per inserire una chiave.
//    /// Scende fino al nodo foglia, esegue l'inserimento, e se un nodo si divide (splitta),
//    /// restituisce la chiave mediana e il puntatore alla nuova pagina al suo chiamante (il nodo padre).
//    /// </summary>
//    private (byte[]? key, uint newPageNum) InsertRecursive(uint pageNum, ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        var page = ReadPage(pageNum);
//        if (page.IsLeaf)
//        {
//            if (page.KeyCount < GetMaxKeysPerLeaf())
//            {
//                InsertIntoLeaf(page, keyBytes, recno);
//                WritePage(pageNum, page);
//                return (null, 0);
//            }
//            else
//            {
//                return SplitAndInsertLeaf(pageNum, page, keyBytes, recno);
//            }
//        }
//        else // Nodo Interno
//        {
//            var childPageNum = DescendInternal(page, keyBytes);
//            var promoted = InsertRecursive(childPageNum, keyBytes, recno);
//
//            if (promoted.key is null) return (null, 0);
//
//            if (page.KeyCount < GetMaxKeysPerInternal())
//            {
//                InsertIntoInternal(page, promoted.key, promoted.newPageNum);
//                WritePage(pageNum, page);
//                return (null, 0);
//            }
//            else
//            {
//                // Il nodo interno è pieno. Dividilo.
//                return SplitAndInsertInternal(pageNum, page, promoted.key, promoted.newPageNum);
//            }
//        }
//    }
//
//    /// <summary>
//    /// Gestisce la divisione (split) di un nodo interno pieno. L'operazione inserisce una nuova chiave (promossa
//    /// da un livello inferiore), divide il set risultante di chiavi e puntatori a metà, promuove la chiave mediana
//    /// al nodo padre e crea un nuovo nodo fratello per ospitare la seconda metà dei dati.
//    /// </summary>
//    /// <param name="pageNum">Il numero della pagina originale (piena) da dividere.</param>
//    /// <param name="page">L'istanza della pagina originale.</param>
//    /// <param name="keyToInsert">La nuova chiave, promossa da un figlio, da inserire in questo nodo.</param>
//    /// <param name="newChildPageNum">Il numero di pagina del nuovo figlio associato alla <paramref name="keyToInsert"/>.</param>
//    /// <returns>
//    /// Una tupla contenente:
//    /// - <c>key</c>: La chiave mediana che deve essere promossa al nodo padre.
//    /// - <c>newPageNum</c>: Il numero di pagina del nuovo nodo fratello creato durante lo split.
//    /// </returns>
//    private (byte[]? key, uint newPageNum) SplitAndInsertInternal(uint pageNum, NtxPage page, byte[] keyToInsert,
//        uint newChildPageNum)
//    {
//        // 1. Raccoglie tutte le chiavi e i puntatori (esistenti + nuovo) in una lista temporanea per ordinarli.
//        // La struttura di un nodo interno è [Ptr0][Key0][Ptr1][Key1]...[PtrN].
//        // Ogni chiave ha un puntatore alla sua destra.
//        var tempEntries = new List<(byte[] key, uint pointer)>(page.KeyCount + 1);
//        for (var i = 0; i < page.KeyCount; i++)
//        {
//            tempEntries.Add((GetInternalKey(page, i).ToArray(), GetInternalPointer(page, i + 1)));
//        }
//
//        tempEntries.Add((keyToInsert, newChildPageNum));
//        tempEntries.Sort((a, b) => a.key.AsSpan().SequenceCompareTo(b.key.AsSpan()));
//
//        // Il primo puntatore (il più a sinistra) viene gestito separatamente.
//        var firstPointer = GetInternalPointer(page, 0);
//
//        // 2. Trova la chiave mediana. Questa sarà promossa al livello superiore.
//        var midPoint = tempEntries.Count / 2;
//        var promotedKey = tempEntries[midPoint].key;
//
//        // Il puntatore associato alla chiave mediana diventa il primo puntatore del nuovo nodo fratello.
//        var newSiblingFirstPointer = tempEntries[midPoint].pointer;
//
//        // 3. Crea e popola il nuovo nodo fratello (sibling) con la seconda metà delle entry.
//        var siblingPageNum = AllocateNewPage();
//        var siblingPage = new NtxPage(_pageSize);
//        var siblingKeys = tempEntries.Skip(midPoint + 1).Select(e => e.key);
//        var siblingPointers = tempEntries.Skip(midPoint + 1).Select(e => e.pointer).Prepend(newSiblingFirstPointer);
//        RewriteInternalNode(siblingPage, siblingKeys, siblingPointers);
//
//        // 4. Riscrive la pagina originale con la prima metà delle entry.
//        var originalKeys = tempEntries.Take(midPoint).Select(e => e.key);
//        var originalPointers = tempEntries.Take(midPoint).Select(e => e.pointer).Prepend(firstPointer);
//        RewriteInternalNode(page, originalKeys, originalPointers);
//
//        // 5. Salva entrambe le pagine modificate su disco.
//        WritePage(pageNum, page);
//        WritePage(siblingPageNum, siblingPage);
//
//        // 6. Restituisce la chiave promossa e il puntatore al nuovo fratello.
//        return (promotedKey, siblingPageNum);
//    }
//
//    /// <summary>
//    /// Helper per riscrivere completamente il contenuto di un nodo interno a partire
//    /// da una lista di chiavi e puntatori.
//    /// </summary>
//    private void RewriteInternalNode(NtxPage page, IEnumerable<byte[]> keys, IEnumerable<uint> pointers)
//    {
//        var pageSpan = page.Data.AsSpan();
//        pageSpan.Clear(); // Pulisce la pagina
//
//        var keyList = keys.ToList();
//        var pointerList = pointers.ToList();
//
//        // Scrive il nuovo header della pagina
//        BitConverter.GetBytes((ushort)keyList.Count).CopyTo(pageSpan[..2]);
//        BitConverter.GetBytes((ushort)PageType.Internal).CopyTo(pageSpan.Slice(2, 2));
//
//        var currentOffset = NtxPage.HeaderSize;
//
//        // Scrive la sequenza [Ptr][Key][Ptr][Key]...[Ptr]
//        for (var i = 0; i < keyList.Count; i++)
//        {
//            BitConverter.GetBytes(pointerList[i]).CopyTo(pageSpan[currentOffset..]);
//            currentOffset += 4;
//            keyList[i].CopyTo(pageSpan[currentOffset..]);
//            currentOffset += KeyLength;
//        }
//
//        // Scrive l'ultimo puntatore a destra
//        BitConverter.GetBytes(pointerList.Last()).CopyTo(pageSpan[currentOffset..]);
//
//        // Ricarica le proprietà della pagina dai dati appena scritti
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Rimuove una chiave e il suo puntatore destro da un nodo interno.
//    /// </summary>
//    /// <param name="page">La pagina del nodo interno da modificare.</param>
//    /// <param name="keyIndex">L'indice della chiave da rimuovere.</param>
//    private void RemoveKeyFromInternal(NtxPage page, int keyIndex)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        if (keyIndex < 0 || keyIndex >= keyCount) return;
//
//        var entrySize = KeyLength + 4;
//
//        // Offset dell'inizio della chiave da rimuovere
//        var startOffset = NtxPage.HeaderSize + keyIndex * entrySize + 4;
//
//        // Sposta tutto ciò che si trova a destra (la chiave successiva e i puntatori) a sinistra
//        var sourceSpan =
//            pageSpan[(startOffset + entrySize - 4)..]; // Include il puntatore a destra della chiave successiva
//        var destSpan = pageSpan[(startOffset - 4)..]; // Sovrascrive il puntatore a sinistra della chiave
//        sourceSpan.CopyTo(destSpan);
//
//        // Pulisce lo spazio rimanente
//        destSpan[sourceSpan.Length..].Clear();
//
//        // Aggiorna il contatore
//        BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Metodo ricorsivo che attraversa l'albero per trovare e rimuovere una chiave.
//    /// </summary>
//    private void DeleteRecursive(uint pageNum, ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        var page = ReadPage(pageNum);
//        if (page.IsLeaf)
//        {
//            RemoveFromLeaf(pageNum, page, keyBytes, recno);
//            // Non c'è bisogno di controllare il valore di ritorno, se la chiave non esiste non succede nulla.
//            // La gestione dell'underflow avverrà comunque se necessario.
//            return;
//        }
//
//        var childIndex = FindChildIndexFor(page, keyBytes);
//        var childPageNum = GetInternalPointer(page, childIndex);
//
//        DeleteRecursive(childPageNum, keyBytes, recno);
//
//        // Dopo la chiamata ricorsiva, controlliamo se il figlio è andato in underflow.
//        var childPage = ReadPage(childPageNum);
//        if (childPage.KeyCount < GetMinKeysPerNode())
//        {
//            HandleUnderflow(page, childIndex);
//        }
//    }
//
//    /// <summary>
//    /// Aggiunge una pagina non più utilizzata alla testa della lista delle pagine libere (free list).
//    /// </summary>
//    /// <remarks>
//    /// Questa operazione scrive sulla pagina liberata per trasformarla in un nodo della lista concatenata
//    /// e aggiorna l'header del file per puntare a questa nuova testa.
//    /// Questo metodo deve essere chiamato all'interno di un lock.
//    /// </remarks>
//    /// <param name="pageNum">Il numero della pagina da liberare e aggiungere alla lista.</param>
//    private void AddPageToFreeList(uint pageNum)
//    {
//        var pageBuffer = new byte[_pageSize];
//        BitConverter.GetBytes(_freeListPageHead).CopyTo(pageBuffer, 0);
//
//        var offset = pageNum * _pageSize;
//        _fs.Seek(offset, SeekOrigin.Begin);
//        _fs.Write(pageBuffer, 0, _pageSize);
//        
//        UpdateFreeListHeadInHeader(pageNum);
//
//        // Rimuove la pagina dalla cache dei nodi. Ora che fa parte della free list,
//        // il suo vecchio contenuto come nodo B+Tree non è più valido e non deve essere
//        // riutilizzato per errore.
//        _nodeCache.Remove(pageNum);
//    }
//
//    /// <summary>
//    /// Gestisce una pagina figlio che è andata in "underflow" (troppo poche chiavi) dopo una cancellazione,
//    /// tentando prima una ridistribuzione (prestito) da un fratello e, se non possibile, un merge (unione).
//    /// </summary>
//    /// <param name="parent">La pagina padre del nodo in underflow.</param>
//    /// <param name="childIndex">L'indice del puntatore, all'interno del padre, che punta al nodo in underflow.</param>
//    private void HandleUnderflow(NtxPage parent, int childIndex)
//    {
//        // Tenta di ridistribuire dal fratello sinistro
//        if (childIndex > 0)
//        {
//            var childPageNum = GetInternalPointer(parent, childIndex);
//            var child = ReadPage(childPageNum);
//            var leftSiblingPageNum = GetInternalPointer(parent, childIndex - 1);
//            var leftSibling = ReadPage(leftSiblingPageNum);
//
//            if (leftSibling.KeyCount > GetMinKeysPerNode())
//            {
//                Redistribute(parent, childIndex, child, leftSibling, true);
//                return;
//            }
//        }
//
//        // Tenta di ridistribuire dal fratello destro
//        if (childIndex < parent.KeyCount)
//        {
//            var childPageNum = GetInternalPointer(parent, childIndex);
//            var child = ReadPage(childPageNum);
//            var rightSiblingPageNum = GetInternalPointer(parent, childIndex + 1);
//            var rightSibling = ReadPage(rightSiblingPageNum);
//
//            if (rightSibling.KeyCount > GetMinKeysPerNode())
//            {
//                Redistribute(parent, childIndex, child, rightSibling, false);
//                return;
//            }
//        }
//
//        // Se la ridistribuzione non è possibile, esegui il merge
//        if (childIndex > 0)
//        {
//            // Esegue il merge con il fratello sinistro
//            Merge(parent, childIndex - 1);
//        }
//        else
//        {
//            // Esegue il merge con il fratello destro
//            Merge(parent, childIndex);
//        }
//    }
//
//    /// <summary>
//    /// Ribalancia due nodi fratelli quando uno è in underflow, spostando una chiave
//    /// dal fratello più pieno a quello più vuoto attraverso il padre.
//    /// </summary>
//    /// <param name="parent">La pagina padre comune.</param>
//    /// <param name="childIndex">L'indice del puntatore, nel padre, che punta al nodo in underflow.</param>
//    /// <param name="child">Il nodo in underflow che riceverà una chiave.</param>
//    /// <param name="sibling">Il nodo fratello che cederà una chiave.</param>
//    /// <param name="fromLeft">True se il fratello è a sinistra, False se è a destra.</param>
//    private void Redistribute(NtxPage parent, int childIndex, NtxPage child, NtxPage sibling, bool fromLeft)
//    {
//        if (child.IsLeaf)
//        {
//            if (fromLeft)
//            {
//                var borrowedKey = GetLeafKey(sibling, sibling.KeyCount - 1);
//                var borrowedRecno = GetLeafRecno(sibling, sibling.KeyCount - 1);
//                InsertIntoLeafFront(child, borrowedKey, borrowedRecno);
//                RemoveLastKeyFromLeaf(sibling);
//                SetInternalKey(parent, childIndex - 1, GetLeafKey(child, 0));
//            }
//            else // fromRight
//            {
//                var borrowedKey = GetLeafKey(sibling, 0);
//                var borrowedRecno = GetLeafRecno(sibling, 0);
//                InsertIntoLeaf(child, borrowedKey, (int)borrowedRecno); // Inserisce in coda
//                RemoveFirstKeyFromLeaf(sibling);
//                SetInternalKey(parent, childIndex, GetLeafKey(sibling, 0));
//            }
//        }
//        else // Nodo Interno
//        {
//            if (fromLeft)
//            {
//                var borrowedKey = GetInternalKey(sibling, sibling.KeyCount - 1).ToArray();
//                var borrowedPtr = GetInternalPointer(sibling, sibling.KeyCount);
//                var sepKey = GetInternalKey(parent, childIndex - 1).ToArray();
//                InsertAtFrontInternal(child, sepKey, borrowedPtr);
//                SetInternalKey(parent, childIndex - 1, borrowedKey.AsSpan());
//                RemoveLastKeyFromInternal(sibling);
//            }
//            else // fromRight
//            {
//                var borrowedKey = GetInternalKey(sibling, 0).ToArray();
//                var borrowedPtr = GetInternalPointer(sibling, 0);
//                var sepKey = GetInternalKey(parent, childIndex).ToArray();
//                InsertIntoInternal(child, sepKey, borrowedPtr);
//                SetInternalKey(parent, childIndex, borrowedKey.AsSpan());
//                RemoveFirstKeyFromInternal(sibling);
//            }
//        }
//
//        // Salva tutte le pagine modificate
//        WritePage(parent.NodeOffset, parent);
//        WritePage(child.NodeOffset, child);
//        WritePage(sibling.NodeOffset, sibling);
//    }
//
//    /// <summary>
//    /// Unisce due nodi fratelli adiacenti (sinistro e destro) quando entrambi sono al minimo numero di chiavi o meno.
//    /// L'operazione sposta tutto il contenuto del nodo destro nel nodo sinistro, tira giù la chiave separatrice
//    /// dal nodo padre e rimuove il puntatore al nodo destro dal padre.
//    /// </summary>
//    /// <param name="parent">La pagina padre che contiene i puntatori ai due figli da unire.</param>
//    /// <param name="leftNodeIndexInParent">
//    /// L'indice, nella lista dei puntatori del padre, che punta al nodo di sinistra dei due fratelli.
//    /// Il nodo di destra si trova implicitamente all'indice `leftNodeIndexInParent + 1`.
//    /// </param>
//    private void Merge(NtxPage parent, int leftNodeIndexInParent)
//    {
//        var leftPageNum = GetInternalPointer(parent, leftNodeIndexInParent);
//        var rightPageNum = GetInternalPointer(parent, leftNodeIndexInParent + 1);
//
//        var left = ReadPage(leftPageNum);
//        var right = ReadPage(rightPageNum);
//
//        if (left.IsLeaf)
//        {
//            // Copia tutte le chiavi/record dal nodo destro a quello sinistro.
//            for (var i = 0; i < right.KeyCount; i++)
//            {
//                var key = GetLeafKey(right, i);
//                var recno = GetLeafRecno(right, i);
//                InsertIntoLeaf(left, key, (int)recno);
//            }
//
//            // Aggiorna la lista doppiamente concatenata per "scavalcare" il nodo destro.
//            var nextNum = right.NextPageNum;
//            UpdateLeafPointers(left, nextPageNum: nextNum);
//            if (nextNum != 0)
//            {
//                var next = ReadPage(nextNum);
//                UpdateLeafPointers(next, prevPageNum: leftPageNum);
//                WritePage(nextNum, next);
//            }
//        }
//        else // Nodo interno
//        {
//            // Tira giù la chiave separatrice dal padre e la inserisce nel nodo sinistro.
//            var sepKey = GetInternalKey(parent, leftNodeIndexInParent).ToArray();
//            InsertIntoInternal(left, sepKey, GetInternalPointer(right, 0));
//
//            // Copia tutte le chiavi e i puntatori dal nodo destro a quello sinistro.
//            for (var i = 0; i < right.KeyCount; i++)
//            {
//                var k = GetInternalKey(right, i).ToArray();
//                var ptr = GetInternalPointer(right, i + 1);
//                InsertIntoInternal(left, k, ptr);
//            }
//        }
//
//        // Aggiorna il padre: rimuove la chiave separatrice e il puntatore al nodo destro.
//        RemoveKeyFromInternal(parent, leftNodeIndexInParent);
//
//        WritePage(leftPageNum, left);
//        WritePage(parent.NodeOffset, parent);
//
//        // Aggiunge la pagina del fratello destro, ora vuota e non più referenziata, alla free list.
//        AddPageToFreeList(rightPageNum);
//    }
//
//    /// <summary>
//    /// Calcola il numero minimo di chiavi che un nodo deve avere.
//    /// </summary>
//    private int GetMinKeysPerNode() => GetMaxKeysPerInternal() / 2;
//
//    /// <summary>
//    /// Trova l'indice del puntatore al figlio corretto in un nodo interno.
//    /// </summary>
//    private int FindChildIndexFor(NtxPage page, ReadOnlySpan<byte> keyBytes)
//    {
//        for (var i = 0; i < page.KeyCount; i++)
//        {
//            if (keyBytes.SequenceCompareTo(GetInternalKey(page, i)) < 0)
//                return i;
//        }
//
//        return page.KeyCount;
//    }
//
//    #endregion
//
//    #region Private Page Manipulation Helpers
//
//    /// <summary>
//    /// Metodo helper per verificare la coerenza della lista doppiamente concatenata dei nodi foglia.
//    /// </summary>
//    private bool CheckLeafListConsistency()
//    {
//        if (_firstLeafPageNum == 0) return true; // Lista vuota è consistente
//
//        var firstLeaf = ReadPage(_firstLeafPageNum);
//        var current = firstLeaf;
//        uint prevPageNum = 0;
//
//        // Scorre in avanti, controllando i puntatori 'prev'.
//        while (current.NextPageNum != 0)
//        {
//            if (current.PrevPageNum != prevPageNum) return false;
//
//            prevPageNum = current.NodeOffset;
//            var nextNode = ReadPage(current.NextPageNum);
//            if (!nextNode.IsLeaf) return false; // Un nodo nella lista deve essere una foglia
//            
//            current = nextNode;
//        }
//
//        // Ora scorre all'indietro partendo dall'ultimo nodo, controllando i puntatori 'next'.
//        while (current.PrevPageNum != 0)
//        {
//            var prevNode = ReadPage(current.PrevPageNum);
//            if (!prevNode.IsLeaf) return false;
//            if (prevNode.NextPageNum != current.NodeOffset) return false;
//
//            current = prevNode;
//        }
//        
//        // Se il ciclo all'indietro ci ha riportato al primo nodo, la lista è consistente.
//        return current.NodeOffset == firstLeaf.NodeOffset;
//    }
//    
//    /// <summary>
//    /// Inizializza un nuovo file di indice, creando l'header e la prima pagina radice (che è anche la prima foglia).
//    /// </summary>
//    private void InitializeNewIndex()
//    {
//        // Svuota l'intera cache dei nodi poiché la struttura dell'indice sta per essere
//        // completamente riscritta da zero.
//        _nodeCache.Clear();
//        
//        _fs.SetLength(0); // Svuota il file per iniziare da capo.
//        var headerBuffer = new byte[_pageSize];
//
//        // La prima pagina dati (pagina 1) sarà sia la radice che la prima (e unica) foglia.
//        _rootPageNum = 1;
//        _firstLeafPageNum = 1;
//
//        // --- Scrive l'Header del File ---
//        BitConverter.GetBytes(_rootPageNum).CopyTo(headerBuffer, 0); // Offset 0: Puntatore alla radice
//        BitConverter.GetBytes(_firstLeafPageNum).CopyTo(headerBuffer, 4); // Offset 4: Puntatore alla prima foglia
//        BitConverter.GetBytes((ushort)KeyLength).CopyTo(headerBuffer, 8); // Offset 8: Lunghezza chiave
//        headerBuffer[10] = Unique ? (byte)1 : (byte)0; // Offset 10: Flag Unique
//
//        // Usa il campo stringa _keyExpressionString.
//        var keyExprString = _keyExpressionString;
//        Encoding.ASCII.GetBytes(keyExprString, 0, Math.Min(keyExprString.Length, 255), headerBuffer, 16);
//
//        _fs.Position = 0;
//        _fs.Write(headerBuffer, 0, headerBuffer.Length);
//
//        // --- Crea e Scrive la Prima Pagina (Radice/Foglia) ---
//        var rootPage = new NtxPage(_pageSize);
//        var span = rootPage.Data.AsSpan();
//        BitConverter.GetBytes((ushort)0).CopyTo(span[..2]); // 0 chiavi
//        BitConverter.GetBytes((ushort)1).CopyTo(span.Slice(2, 2)); // Flag IsLeaf = true
//        BitConverter.GetBytes((uint)0).CopyTo(span.Slice(4, 4)); // NextPageNum = 0 (nessuna pagina successiva)
//        BitConverter.GetBytes((uint)0).CopyTo(span.Slice(8, 4)); // PrevPageNum = 0 (nessuna pagina precedente)
//
//        WritePage(_rootPageNum, rootPage);
//        _fs.Flush();
//    }
//
//    /// <summary>
//    /// Legge una pagina (nodo) dal file di indice.
//    /// Questo metodo deve essere chiamato all'interno di un lock.
//    /// </summary>
//    /// <param name="pageNum">Il numero (0-based) della pagina da leggere.</param>
//    /// <returns>Un'istanza di <see cref="NtxPage"/> con i dati caricati.</returns>
//    /// <exception cref="Executor.BasicException">
//    /// Lanciata con codice <see cref="Executor.ErrorCode.DatabaseDbfError"/> se il numero di pagina
//    /// punta a una posizione non valida (oltre la fine del file), indicando una corruzione dell'indice.
//    /// </exception>
//    private NtxPage ReadPage(uint pageNum)
//    {
//        // Controlla se la cache deve essere usata.
//        if (_useNodeCache && _nodeCache.TryGetValue(pageNum, out var cachedPage) && cachedPage != null)
//        {
//            return cachedPage;
//        }
//        
//        var offset = pageNum * _pageSize;
//        if (offset < 0 || offset + _pageSize > _fs.Length)
//        {
//            throw new BasicException(
//                ErrorCode.DatabaseDbfError, SourcePosition.Unknown,
//                $"Indice '{Name}' corrotto: il puntatore alla pagina {pageNum} (offset {offset}) è fuori dai limiti del file.",
//                null,
//                new EndOfStreamException()
//            );
//        }
//
//        var p = new NtxPage(_pageSize) { NodeOffset = pageNum };
//        _fs.Seek(offset, SeekOrigin.Begin);
//        _fs.ReadExactly(p.Data, 0, _pageSize);
//        p.Load(p.Data);
//        
//        // Aggiunge alla cache solo se è abilitata.
//        if (_useNodeCache)
//        {
//            _nodeCache.AddOrUpdate(pageNum, p);
//        }
//        
//        return p;
//    }
//
//    /// <summary>
//    /// Scrive i dati di una pagina su disco alla posizione specificata.
//    /// In modalità DEBUG, valida gli invarianti della pagina prima della scrittura.
//    /// Questo metodo deve essere chiamato all'interno di un lock.
//    /// </summary>
//    /// <param name="pageNum">Il numero (0-based) della pagina da scrivere.</param>
//    /// <param name="page">L'oggetto <see cref="NtxPage"/> contenente i dati da scrivere.</param>
//    private void WritePage(uint pageNum, NtxPage page)
//    {
//        ValidatePageInvariants(page, pageNum);
//
//        var offset = pageNum * _pageSize;
//        _fs.Seek(offset, SeekOrigin.Begin);
//        _fs.Write(page.Data, 0, _pageSize);
//
//        // Aggiorna la pagina anche nella cache per mantenere la coerenza.
//        _nodeCache.AddOrUpdate(pageNum, page);
//    }
//
//    /// <summary>
//    /// Alloca una nuova pagina per l'indice. Tenta prima di riutilizzare una pagina dalla
//    /// lista delle pagine libere (free list); se la lista è vuota, estende il file.
//    /// Questo metodo deve essere chiamato all'interno di un lock.
//    /// </summary>
//    /// <returns>Il numero della pagina allocata (0-based).</returns>
//    private uint AllocateNewPage()
//    {
//        // Controlla se ci sono pagine disponibili nella free list.
//        if (_freeListPageHead != 0)
//        {
//            var reclaimedPageNum = _freeListPageHead;
//
//            // La prima parte di una pagina libera (i primi 4 byte) contiene il puntatore 
//            // alla *prossima* pagina libera nella catena.
//            var offset = reclaimedPageNum * _pageSize;
//            _fs.Seek(offset, SeekOrigin.Begin);
//
//            // Legge solo i primi 4 byte per efficienza.
//            Span<byte> nextPointerBuffer = stackalloc byte[4];
//            _fs.ReadExactly(nextPointerBuffer);
//            var nextFreePageNum = BitConverter.ToUInt32(nextPointerBuffer);
//
//            // Aggiorna l'header per puntare alla prossima pagina libera nella catena.
//            UpdateFreeListHeadInHeader(nextFreePageNum);
//
//            // Rimuove la pagina dalla cache dei nodi, poiché il suo vecchio contenuto 
//            // (come nodo della free list) non è più valido.
//            _nodeCache.Remove(reclaimedPageNum);
//
//            return reclaimedPageNum;
//        }
//        else
//        {
//            // Nessuna pagina libera, estende il file.
//            // Il numero della nuova pagina è l'offset corrente diviso la dimensione della pagina.
//            var newPageOffset = _fs.Length;
//            var newPageNum = (uint)(newPageOffset / _pageSize);
//            _fs.SetLength(newPageOffset + _pageSize);
//            return newPageNum;
//        }
//    }
//
//    private void UpdateRootPageInHeader(uint newRootPageNum)
//    {
//        _fs.Position = 0;
//        using var writer = new BinaryWriter(_fs, Encoding.ASCII, leaveOpen: true);
//        writer.Write(newRootPageNum);
//    }
//
//    /// <summary>
//    /// Metodo helper privato per cercare una chiave all'interno di un nodo foglia.
//    /// </summary>
//    private int FindInLeaf(NtxPage page, ReadOnlySpan<byte> keyBytes, bool soft)
//    {
//        // Esegue una ricerca binaria sulle chiavi all'interno della pagina.
//        var low = 0;
//        var high = page.KeyCount - 1;
//        var resultIndex = -1;
//
//        while (low <= high)
//        {
//            var mid = low + (high - low) / 2;
//            var currentKey = GetLeafKey(page, mid);
//            var cmp = currentKey.SequenceCompareTo(keyBytes);
//
//            if (cmp == 0)
//            {
//                // Corrispondenza esatta trovata.
//                return (int)GetLeafRecno(page, mid);
//            }
//
//            if (cmp < 0)
//            {
//                low = mid + 1;
//            }
//            else
//            {
//                resultIndex = mid;
//                high = mid - 1;
//            }
//        }
//
//        // Se non è stata trovata una corrispondenza esatta e il soft seek è attivo,
//        // restituiamo il record della prima chiave maggiore di quella cercata.
//        return soft && resultIndex != -1 ? (int)GetLeafRecno(page, resultIndex) : -1;
//    }
//
//    private void InsertIntoLeaf(NtxPage page, ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int i;
//        for (i = 0; i < page.KeyCount; i++)
//            if (keyBytes.SequenceCompareTo(GetLeafKey(page, i)) < 0)
//                break;
//
//        var entrySize = KeyLength + 4;
//        var insertOffset = NtxPage.HeaderSize + i * entrySize;
//
//        var source = pageSpan.Slice(insertOffset, (page.KeyCount - i) * entrySize);
//        var dest = pageSpan[(insertOffset + entrySize)..];
//        source.CopyTo(dest);
//
//        keyBytes.CopyTo(pageSpan[insertOffset..]);
//        BitConverter.GetBytes((uint)recno).CopyTo(pageSpan[(insertOffset + KeyLength)..]);
//
//        BitConverter.GetBytes((ushort)(page.KeyCount + 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Inserisce una nuova entry (chiave, recno) all'inizio di un nodo foglia.
//    /// </summary>
//    private void InsertIntoLeafFront(NtxPage page, ReadOnlySpan<byte> keyBytes, uint recno)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        var entrySize = KeyLength + 4;
//
//        // Sposta tutto a destra per fare spazio all'inizio
//        var sourceSpan = pageSpan.Slice(NtxPage.HeaderSize, keyCount * entrySize);
//        var destSpan = pageSpan[(NtxPage.HeaderSize + entrySize)..];
//        sourceSpan.CopyTo(destSpan);
//
//        // Inserisce la nuova entry
//        keyBytes.CopyTo(pageSpan[NtxPage.HeaderSize..]);
//        BitConverter.GetBytes(recno).CopyTo(pageSpan[(NtxPage.HeaderSize + KeyLength)..]);
//
//        // Aggiorna il contatore
//        BitConverter.GetBytes((ushort)(keyCount + 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Rimuove la prima entry (chiave, recno) da un nodo foglia.
//    /// </summary>
//    private void RemoveFirstKeyFromLeaf(NtxPage page)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        if (keyCount == 0) return;
//
//        var entrySize = KeyLength + 4;
//
//        // Sposta tutto il blocco di entry (dalla seconda in poi) a sinistra,
//        // sovrascrivendo la prima entry.
//        var sourceSpan = pageSpan.Slice(NtxPage.HeaderSize + entrySize, (keyCount - 1) * entrySize);
//        var destSpan = pageSpan[NtxPage.HeaderSize..];
//        sourceSpan.CopyTo(destSpan);
//
//        // Pulisce lo spazio lasciato vuoto alla fine
//        destSpan[sourceSpan.Length..].Clear();
//
//        // Aggiorna il contatore
//        BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Aggiorna il valore di una chiave specifica all'interno di un nodo interno.
//    /// </summary>
//    /// <param name="page">La pagina del nodo interno da modificare.</param>
//    /// <param name="keyIndex">L'indice (0-based) della chiave da sostituire.</param>
//    /// <param name="newKey">Lo span di byte contenente la nuova chiave.</param>
//    private void SetInternalKey(NtxPage page, int keyIndex, ReadOnlySpan<byte> newKey)
//    {
//        if (page.IsLeaf || keyIndex < 0 || keyIndex >= page.KeyCount)
//        {
//            // Non dovrebbe mai accadere se la logica è corretta
//            throw new ArgumentException("Indice di chiave non valido per il nodo interno.");
//        }
//
//        // Calcola l'offset della chiave da aggiornare
//        // Struttura: [Ptr0][Key0][Ptr1][Key1]...
//        var offset = NtxPage.HeaderSize + (keyIndex * (4 + KeyLength)) + 4;
//
//        // Sovrascrive la vecchia chiave con quella nuova
//        newKey.CopyTo(page.Data.AsSpan(offset, KeyLength));
//    }
//
//    /// <summary>
//    /// Inserisce una nuova entry (chiave e puntatore figlio sinistro) all'inizio di un nodo interno.
//    /// </summary>
//    /// <param name="page">La pagina del nodo interno da modificare.</param>
//    /// <param name="keyBytes">La chiave da inserire (che scende dal padre).</param>
//    /// <param name="leftChildPointer">Il puntatore al nuovo sotto-albero sinistro (che arriva dal fratello).</param>
//    private void InsertAtFrontInternal(NtxPage page, ReadOnlySpan<byte> keyBytes, uint leftChildPointer)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        var entrySize = KeyLength + 4;
//
//        // Sposta tutto a destra per fare spazio a una nuova entry [Ptr][Key]
//        var sourceSpan =
//            pageSpan.Slice(NtxPage.HeaderSize, (keyCount * entrySize) + 4); // +4 per l'ultimo puntatore a destra
//        var destSpan = pageSpan[(NtxPage.HeaderSize + entrySize)..];
//        sourceSpan.CopyTo(destSpan);
//
//        // Inserisce il nuovo puntatore e la nuova chiave all'inizio
//        BitConverter.GetBytes(leftChildPointer).CopyTo(pageSpan[NtxPage.HeaderSize..]);
//        keyBytes.CopyTo(pageSpan[(NtxPage.HeaderSize + 4)..]);
//
//        // Aggiorna il contatore
//        BitConverter.GetBytes((ushort)(keyCount + 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Rimuove la prima entry (puntatore sinistro e chiave) da un nodo interno.
//    /// </summary>
//    /// <param name="page">La pagina del nodo interno da cui rimuovere l'entry.</param>
//    private void RemoveFirstKeyFromInternal(NtxPage page)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        if (keyCount == 0) return;
//
//        var entrySize = KeyLength + 4;
//
//        // Sposta tutto il blocco di entry (dalla seconda in poi) a sinistra,
//        // sovrascrivendo la prima entry [Ptr][Key].
//        var sourceSpan = pageSpan[(NtxPage.HeaderSize + entrySize)..];
//        var destSpan = pageSpan[NtxPage.HeaderSize..];
//        sourceSpan.CopyTo(destSpan);
//
//        // Pulisce lo spazio lasciato vuoto alla fine.
//        destSpan[sourceSpan.Length..].Clear();
//
//        // Aggiorna il contatore.
//        BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Gestisce la divisione (split) di un nodo foglia pieno. L'operazione inserisce la nuova coppia chiave/record,
//    /// divide il set di entry risultante a metà, crea un nuovo nodo fratello per la seconda metà,
//    /// e aggiorna la lista doppiamente concatenata dei nodi foglia.
//    /// </summary>
//    /// <param name="pageNum">Il numero della pagina originale (piena) da dividere.</param>
//    /// <param name="page">L'istanza della pagina originale.</param>
//    /// <param name="keyBytes">La nuova chiave da inserire.</param>
//    /// <param name="recno">Il nuovo numero di record da inserire.</param>
//    /// <returns>
//    /// Una tupla contenente la prima chiave del nuovo nodo fratello (da promuovere al padre)
//    /// e il numero di pagina del nuovo nodo fratello creato.
//    /// </returns>
//    private (byte[] key, uint newPageNum) SplitAndInsertLeaf(uint pageNum, NtxPage page, ReadOnlySpan<byte> keyBytes,
//        int recno)
//    {
//        var siblingPageNum = AllocateNewPage();
//        var siblingPage = new NtxPage(_pageSize);
//
//        // 1. Crea un buffer temporaneo con tutte le entry (vecchie + nuova) già ordinate.
//        var allEntries = new List<(byte[] key, uint recno)>(page.KeyCount + 1);
//        for (var i = 0; i < page.KeyCount; i++)
//        {
//            allEntries.Add((GetLeafKey(page, i).ToArray(), GetLeafRecno(page, i)));
//        }
//
//        allEntries.Add((keyBytes.ToArray(), (uint)recno));
//        allEntries.Sort((a, b) => a.key.AsSpan().SequenceCompareTo(b.key.AsSpan()));
//
//        // 2. Divide le entry a metà.
//        var midPoint = allEntries.Count / 2;
//        var firstHalf = allEntries.Take(midPoint).ToList();
//        var secondHalf = allEntries.Skip(midPoint).ToList();
//
//        // Memorizza quale era il prossimo nodo prima di modificare i puntatori.
//        var originalNext = page.NextPageNum;
//
//        // 3. Riscrive la pagina originale e quella nuova con i dati divisi.
//        RewriteLeafNode(page, firstHalf);
//        RewriteLeafNode(siblingPage, secondHalf);
//
//        // 4. Aggiorna la lista doppiamente concatenata.
//        // Il nuovo fratello si inserisce tra la pagina originale e la successiva.
//        UpdateLeafPointers(page, nextPageNum: siblingPageNum);
//        UpdateLeafPointers(siblingPage, nextPageNum: originalNext, prevPageNum: pageNum);
//
//        // 5. Se c'era una pagina successiva, aggiorna il suo puntatore 'prev' perché punti al nuovo fratello.
//        if (originalNext != 0)
//        {
//            var nextPage = ReadPage(originalNext);
//            UpdateLeafPointers(nextPage, prevPageNum: siblingPageNum);
//            WritePage(originalNext, nextPage);
//        }
//
//        // 6. Se la pagina originale era la prima foglia dell'albero, non cambia.
//        // Se però l'albero era vuoto e questa è la prima operazione, assicuriamoci che _firstLeafPageNum sia corretto.
//        if (_firstLeafPageNum == 0 || _firstLeafPageNum == siblingPageNum)
//        {
//            UpdateFirstLeafInHeader(pageNum);
//        }
//
//        // 7. Salva la pagina originale e il nuovo fratello su disco.
//        WritePage(pageNum, page);
//        WritePage(siblingPageNum, siblingPage);
//
//        // 8. Restituisce la prima chiave del nuovo fratello, che sarà promossa al nodo padre.
//        return (GetLeafKey(siblingPage, 0).ToArray(), siblingPageNum);
//    }
//
//    /// <summary>
//    /// Helper per riscrivere completamente il contenuto di un nodo foglia a partire
//    /// da una lista di entry (chiave, recno).
//    /// </summary>
//    private void RewriteLeafNode(NtxPage page, List<(byte[] key, uint recno)> entries)
//    {
//        var pageSpan = page.Data.AsSpan();
//        pageSpan.Clear(); // Pulisce completamente la pagina prima di riscriverla.
//        var entrySize = KeyLength + 4;
//        var currentOffset = NtxPage.HeaderSize;
//
//        // Scrive le entry una dopo l'altra.
//        foreach (var (key, recno) in entries)
//        {
//            key.AsSpan().CopyTo(pageSpan[currentOffset..]);
//            BitConverter.GetBytes(recno).CopyTo(pageSpan[(currentOffset + KeyLength)..]);
//            currentOffset += entrySize;
//        }
//
//        // Scrive il nuovo header della pagina (conteggio chiavi e tipo).
//        BitConverter.GetBytes((ushort)entries.Count).CopyTo(pageSpan[..2]);
//        BitConverter.GetBytes((ushort)PageType.Leaf).CopyTo(pageSpan.Slice(2, 2));
//
//        // Ricarica le proprietà della pagina dai dati appena scritti.
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Helper per aggiornare i puntatori sequenziali (Next/Prev) nell'header di una pagina foglia.
//    /// </summary>
//    private void UpdateLeafPointers(NtxPage page, uint? nextPageNum = null, uint? prevPageNum = null)
//    {
//        var pageSpan = page.Data.AsSpan();
//        if (nextPageNum.HasValue)
//        {
//            BitConverter.GetBytes(nextPageNum.Value).CopyTo(pageSpan.Slice(4, 4));
//        }
//
//        if (prevPageNum.HasValue)
//        {
//            BitConverter.GetBytes(prevPageNum.Value).CopyTo(pageSpan.Slice(8, 4));
//        }
//
//        // Ricarica le proprietà per mantenere l'oggetto consistente.
//        page.Load(pageSpan);
//    }
//
//    /// <summary>
//    /// Rimuove un'entry (chiave/recno) da un nodo foglia e salva la pagina modificata su disco.
//    /// </summary>
//    /// <param name="pageNum">Il numero della pagina da modificare.</param>
//    /// <param name="page">L'oggetto NtxPage che rappresenta la pagina.</param>
//    /// <param name="keyBytes">La chiave dell'entry da rimuovere.</param>
//    /// <param name="recno">Il numero di record specifico da rimuovere.</param>
//    /// <returns>True se l'entry è stata trovata e rimossa, altrimenti False.</returns>
//    private bool RemoveFromLeaf(uint pageNum, NtxPage page, ReadOnlySpan<byte> keyBytes, int recno)
//    {
//        var pageSpan = page.Data.AsSpan();
//        int keyCount = page.KeyCount;
//        var entrySize = KeyLength + 4;
//
//        for (var i = 0; i < keyCount; i++)
//        {
//            var currentKey = GetLeafKey(page, i);
//            if (keyBytes.SequenceEqual(currentKey))
//            {
//                var currentRecno = GetLeafRecno(page, i);
//                if (currentRecno == recno)
//                {
//                    // Trovata l'entry esatta da cancellare.
//                    var startOffset = NtxPage.HeaderSize + i * entrySize;
//
//                    // Sovrascrive l'entry spostando tutti gli elementi successivi a sinistra di una posizione.
//                    var sourceSpan = pageSpan.Slice(startOffset + entrySize, (keyCount - 1 - i) * entrySize);
//                    var destSpan = pageSpan[startOffset..];
//                    sourceSpan.CopyTo(destSpan);
//
//                    // Pulisce lo spazio lasciato vuoto alla fine della lista di entry.
//                    pageSpan[(NtxPage.HeaderSize + (keyCount - 1) * entrySize)..].Clear();
//
//                    // Aggiorna il contatore delle chiavi nell'header della pagina.
//                    BitConverter.GetBytes((ushort)(keyCount - 1)).CopyTo(pageSpan[..2]);
//                    page.Load(pageSpan); // Ricarica le proprietà (KeyCount) dall'array di byte modificato.
//
//                    // Scrive la pagina aggiornata su disco.
//                    WritePage(pageNum, page);
//                    return true;
//                }
//            }
//        }
//
//        return false; // Chiave o numero di record non trovati in questa pagina.
//    }
//
//    /// <summary>
//    /// Metodo helper privato per trovare il puntatore alla pagina figlia corretta in un nodo interno.
//    /// </summary>
//    private uint DescendInternal(NtxPage page, ReadOnlySpan<byte> keyBytes)
//    {
//        // Scansiona le chiavi nel nodo per trovare la prima che è maggiore o uguale.
//        for (var i = 0; i < page.KeyCount; i++)
//        {
//            var currentKey = GetInternalKey(page, i);
//            if (keyBytes.SequenceCompareTo(currentKey) < 0)
//            {
//                // La chiave è minore, quindi seguiamo il puntatore che si trova PRIMA di questa chiave.
//                return GetInternalPointer(page, i);
//            }
//        }
//
//        // Se la chiave è maggiore di tutte, seguiamo l'ultimo puntatore (a destra di tutte le chiavi).
//        return GetInternalPointer(page, page.KeyCount);
//    }
//
//    /// <summary>
//    /// Ottiene la chiave da un nodo interno a un indice specifico.
//    /// </summary>
//    private ReadOnlySpan<byte> GetInternalKey(NtxPage page, int index)
//    {
//        // Struttura Nodo Interno: [Ptr0][Key0][Ptr1][Key1]...[PtrN]
//        var offset = NtxPage.HeaderSize + (index * (4 + KeyLength)) + 4;
//        return page.Data.AsSpan(offset, KeyLength);
//    }
//
//    /// <summary>
//    /// Ottiene il puntatore a una pagina figlia da un nodo interno a un indice specifico.
//    /// </summary>
//    private uint GetInternalPointer(NtxPage page, int index)
//    {
//        var offset = NtxPage.HeaderSize + (index * (4 + KeyLength));
//        return BitConverter.ToUInt32(page.Data.AsSpan(offset, 4));
//    }
//
//    /// <summary>
//    /// Inserisce una chiave promossa e il puntatore alla sua nuova pagina destra
//    /// in un nodo interno che ha spazio disponibile.
//    /// </summary>
//    private void InsertIntoInternal(NtxPage page, byte[] key, uint newPageNum)
//    {
//        var pageSpan = page.Data.AsSpan();
//        var keyCount = page.KeyCount;
//        var entrySize = KeyLength + 4;
//
//        int i;
//        // Trova la posizione corretta per la nuova chiave
//        for (i = 0; i < keyCount; i++)
//        {
//            var currentKeyOffset = NtxPage.HeaderSize + (i * entrySize) + 4;
//            var currentKey = pageSpan.Slice(currentKeyOffset, KeyLength);
//            if (key.AsSpan().SequenceCompareTo(currentKey) < 0)
//            {
//                break;
//            }
//        }
//
//        var insertOffset = NtxPage.HeaderSize + i * entrySize;
//
//        // --- Sposta le entry esistenti per fare spazio ---
//        // La struttura è [Ptr][Key]. Dobbiamo inserire una [Key][Ptr].
//        // Quindi spostiamo prima il blocco [Key][Ptr]...[Key][Ptr]
//
//        // Sposta il blocco che inizia con il puntatore a destra della chiave da inserire
//        var rightPointerOffset = insertOffset + 4;
//        var sourceSpan = pageSpan.Slice(rightPointerOffset, (keyCount - i) * entrySize + 4);
//        var destSpan = pageSpan[(rightPointerOffset + entrySize)..];
//        sourceSpan.CopyTo(destSpan);
//
//        // --- Inserisce la nuova chiave e il puntatore alla nuova pagina ---
//        key.AsSpan().CopyTo(pageSpan[(insertOffset + 4)..]);
//        BitConverter.GetBytes(newPageNum).CopyTo(pageSpan[(insertOffset + 4 + KeyLength)..]);
//
//        // Aggiorna il contatore e ricarica le proprietà della pagina
//        BitConverter.GetBytes((ushort)(keyCount + 1)).CopyTo(pageSpan[..2]);
//        page.Load(pageSpan);
//    }
//
//    private ReadOnlySpan<byte> GetLeafKey(NtxPage page, int index) =>
//        page.Data.AsSpan().Slice(NtxPage.HeaderSize + index * (KeyLength + 4), KeyLength);
//
//    private uint GetLeafRecno(NtxPage page, int index) =>
//        BitConverter.ToUInt32(page.Data.AsSpan().Slice(NtxPage.HeaderSize + index * (KeyLength + 4) + KeyLength, 4));
//
//    private int GetMaxKeysPerLeaf() => (_pageSize - NtxPage.HeaderSize) / (KeyLength + 4);
//    private int GetMaxKeysPerInternal() => (_pageSize - NtxPage.HeaderSize - 4) / (KeyLength + 4);
//
//    /// <summary>
//    /// Esegue controlli di validità e asserzioni su una pagina per garantirne l'integrità strutturale
//    /// prima che venga scritta su disco.
//    /// </summary>
//    /// <remarks>
//    /// Questo metodo è compilato solo in modalità DEBUG per non impattare le performance in release.
//    /// Verifica che il numero di chiavi in un nodo rispetti gli invarianti del B+Tree.
//    /// </remarks>
//    /// <param name="page">La pagina da validare.</param>
//    /// <param name="pageNum">Il numero della pagina, usato per contestualizzare i messaggi di errore.</param>
//    [Conditional("DEBUG")]
//    private void ValidatePageInvariants(NtxPage page, uint pageNum)
//    {
//        var maxKeys = page.IsLeaf ? GetMaxKeysPerLeaf() : GetMaxKeysPerInternal();
//        Debug.Assert(page.KeyCount <= maxKeys,
//            $"La pagina {pageNum} ha superato il numero massimo di chiavi. Contiene {page.KeyCount}, massimo {maxKeys}.");
//
//        // La radice è l'unica pagina che può avere meno del numero minimo di chiavi.
//        if (pageNum != _rootPageNum)
//        {
//            // Un nodo vuoto è valido solo se è la radice.
//            if (page.KeyCount == 0)
//            {
//                Debug.Assert(page.KeyCount > 0, $"La pagina non-radice {pageNum} è vuota.");
//            }
//
//            var minKeys = GetMinKeysPerNode();
//            Debug.Assert(page.KeyCount >= minKeys,
//                $"La pagina {pageNum} è in underflow. Contiene {page.KeyCount}, minimo {minKeys}.");
//        }
//    }
//
//    #endregion
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#define NtxPage_HeaderSize 12
#define NtxPage_PageSize 1024
#define NtxPageType_Internal 0
#define NtxPageType_Leaf 1

static void ntx_u16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t ntx_ru16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void ntx_u32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t ntx_ru32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct NtxPage {
    int PageSize;
    uint16_t KeyCount;
    int IsLeaf;
    uint32_t NextPageNum;
    uint32_t PrevPageNum;
    uint32_t NodeOffset;
    uint8_t Data[NtxPage_PageSize];
} NtxPage;

static NtxPage* NtxPage_New(void)
{
    NtxPage* p = (NtxPage*)calloc(1, sizeof(*p));
    p->PageSize = NtxPage_PageSize;
    return p;
}

static void NtxPage_Load(NtxPage* page)
{
    page->KeyCount = ntx_ru16(page->Data);
    page->IsLeaf = (ntx_ru16(page->Data + 2) & 1) == 1;
    if (page->IsLeaf) {
        page->NextPageNum = ntx_ru32(page->Data + 4);
        page->PrevPageNum = ntx_ru32(page->Data + 8);
    }
}

struct NtxFile {
    FILE* fs;
    int pageSize;
    char keyExpressionString[256];
    char FilePath[PATH_MAX];
    char Name[256];
    int Unique;
    int Descending;
    int KeyLength;
    uint32_t rootPageNum;
    uint32_t firstLeafPageNum;
    uint32_t freeListPageHead;
    LruCache* nodeCache;
    int useNodeCache;
    pthread_mutex_t syncRoot;
};

static long NtxFile_len(FILE* f)
{
    long cur = ftell(f), end;
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static const uint8_t* NtxFile_GetLeafKey(const NtxFile* self, const NtxPage* page, int index)
{
    return page->Data + NtxPage_HeaderSize + index * (self->KeyLength + 4);
}
static uint32_t NtxFile_GetLeafRecno(const NtxFile* self, const NtxPage* page, int index)
{
    return ntx_ru32(NtxFile_GetLeafKey(self, page, index) + self->KeyLength);
}
static const uint8_t* NtxFile_GetInternalKey(const NtxFile* self, const NtxPage* page, int index)
{
    return page->Data + NtxPage_HeaderSize + (index * (4 + self->KeyLength)) + 4;
}
static uint32_t NtxFile_GetInternalPointer(const NtxFile* self, const NtxPage* page, int index)
{
    return ntx_ru32(page->Data + NtxPage_HeaderSize + index * (4 + self->KeyLength));
}
static int NtxFile_GetMaxKeysPerLeaf(const NtxFile* self)
{
    return (self->pageSize - NtxPage_HeaderSize) / (self->KeyLength + 4);
}
static int NtxFile_GetMaxKeysPerInternal(const NtxFile* self)
{
    return (self->pageSize - NtxPage_HeaderSize - 4) / (self->KeyLength + 4);
}

static NtxPage* NtxFile_ReadPage(NtxFile* self, uint32_t pageNum)
{
    void* cached = NULL;
    NtxPage* p;
    long offset;
    if (self->useNodeCache && LruCache_TryGetValue(self->nodeCache, pageNum, &cached) && cached)
        return (NtxPage*)cached;
    offset = (long)pageNum * self->pageSize;
    if (offset < 0 || offset + self->pageSize > NtxFile_len(self->fs)) return NULL;
    p = NtxPage_New();
    p->NodeOffset = pageNum;
    fseek(self->fs, offset, SEEK_SET);
    if (fread(p->Data, 1, (size_t)self->pageSize, self->fs) != (size_t)self->pageSize) {
        free(p);
        return NULL;
    }
    NtxPage_Load(p);
    if (self->useNodeCache) LruCache_AddOrUpdate(self->nodeCache, pageNum, p);
    return p;
}

static void NtxFile_WritePage(NtxFile* self, uint32_t pageNum, NtxPage* page)
{
    long offset = (long)pageNum * self->pageSize;
    fseek(self->fs, offset, SEEK_SET);
    fwrite(page->Data, 1, (size_t)self->pageSize, self->fs);
    if (self->useNodeCache) LruCache_AddOrUpdate(self->nodeCache, pageNum, page);
}

static void NtxFile_UpdateRootPageInHeader(NtxFile* self, uint32_t newRootPageNum)
{
    uint8_t b[4];
    self->rootPageNum = newRootPageNum;
    ntx_u32(b, newRootPageNum);
    fseek(self->fs, 0, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static void NtxFile_UpdateFirstLeafInHeader(NtxFile* self, uint32_t firstLeaf)
{
    uint8_t b[4];
    self->firstLeafPageNum = firstLeaf;
    ntx_u32(b, firstLeaf);
    fseek(self->fs, 4, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static void NtxFile_UpdateFreeListHeadInHeader(NtxFile* self, uint32_t newHead)
{
    uint8_t b[4];
    self->freeListPageHead = newHead;
    ntx_u32(b, newHead);
    fseek(self->fs, 12, SEEK_SET);
    fwrite(b, 1, 4, self->fs);
}

static uint32_t NtxFile_AllocateNewPage(NtxFile* self)
{
    if (self->freeListPageHead != 0) {
        uint32_t reclaimed = self->freeListPageHead;
        uint8_t nextBuf[4];
        long offset = (long)reclaimed * self->pageSize;
        fseek(self->fs, offset, SEEK_SET);
        fread(nextBuf, 1, 4, self->fs);
        NtxFile_UpdateFreeListHeadInHeader(self, ntx_ru32(nextBuf));
        if (self->useNodeCache) LruCache_Remove(self->nodeCache, reclaimed);
        return reclaimed;
    }
    {
        long newOff = NtxFile_len(self->fs);
        uint32_t newPageNum = (uint32_t)(newOff / self->pageSize);
        ftruncate(fileno(self->fs), (off_t)(newOff + self->pageSize));
        return newPageNum;
    }
}

static void NtxFile_RewriteLeafNode(NtxFile* self, NtxPage* page, const uint8_t** keys, const uint32_t* recnos, int n)
{
    int i, entrySize = self->KeyLength + 4, off = NtxPage_HeaderSize;
    memset(page->Data, 0, (size_t)self->pageSize);
    for (i = 0; i < n; i++) {
        memcpy(page->Data + off, keys[i], (size_t)self->KeyLength);
        ntx_u32(page->Data + off + self->KeyLength, recnos[i]);
        off += entrySize;
    }
    ntx_u16(page->Data, (uint16_t)n);
    ntx_u16(page->Data + 2, (uint16_t)NtxPageType_Leaf);
    NtxPage_Load(page);
}

static void NtxFile_UpdateLeafPointers(NtxPage* page, int hasNext, uint32_t next, int hasPrev, uint32_t prev)
{
    if (hasNext) ntx_u32(page->Data + 4, next);
    if (hasPrev) ntx_u32(page->Data + 8, prev);
    NtxPage_Load(page);
}

static void NtxFile_InsertIntoLeaf(NtxFile* self, NtxPage* page, const uint8_t* keyBytes, int recno)
{
    int i, entrySize = self->KeyLength + 4, insertOffset, move;
    uint8_t* span = page->Data;
    for (i = 0; i < page->KeyCount; i++)
        if (ByteArrayComparer_Compare(keyBytes, self->KeyLength, NtxFile_GetLeafKey(self, page, i), self->KeyLength) < 0)
            break;
    insertOffset = NtxPage_HeaderSize + i * entrySize;
    move = (page->KeyCount - i) * entrySize;
    if (move > 0) memmove(span + insertOffset + entrySize, span + insertOffset, (size_t)move);
    memcpy(span + insertOffset, keyBytes, (size_t)self->KeyLength);
    ntx_u32(span + insertOffset + self->KeyLength, (uint32_t)recno);
    ntx_u16(span, (uint16_t)(page->KeyCount + 1));
    NtxPage_Load(page);
}

static int NtxFile_FindInLeaf(NtxFile* self, const NtxPage* page, const uint8_t* keyBytes, int soft)
{
    int low = 0, high = page->KeyCount - 1, resultIndex = -1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = ByteArrayComparer_Compare(NtxFile_GetLeafKey(self, page, mid), self->KeyLength,
                                            keyBytes, self->KeyLength);
        if (cmp == 0) return (int)NtxFile_GetLeafRecno(self, page, mid);
        if (cmp < 0) low = mid + 1;
        else { resultIndex = mid; high = mid - 1; }
    }
    return (soft && resultIndex != -1) ? (int)NtxFile_GetLeafRecno(self, page, resultIndex) : -1;
}

static uint32_t NtxFile_DescendInternal(NtxFile* self, const NtxPage* page, const uint8_t* keyBytes)
{
    int i;
    for (i = 0; i < page->KeyCount; i++) {
        if (ByteArrayComparer_Compare(keyBytes, self->KeyLength,
                                      NtxFile_GetInternalKey(self, page, i), self->KeyLength) < 0)
            return NtxFile_GetInternalPointer(self, page, i);
    }
    return NtxFile_GetInternalPointer(self, page, page->KeyCount);
}

static void NtxFile_RewriteInternalNode(NtxFile* self, NtxPage* page, const uint8_t** keys, const uint32_t* ptrs, int keyN)
{
    int i, off = NtxPage_HeaderSize;
    memset(page->Data, 0, (size_t)self->pageSize);
    ntx_u16(page->Data, (uint16_t)keyN);
    ntx_u16(page->Data + 2, (uint16_t)NtxPageType_Internal);
    for (i = 0; i < keyN; i++) {
        ntx_u32(page->Data + off, ptrs[i]);
        off += 4;
        memcpy(page->Data + off, keys[i], (size_t)self->KeyLength);
        off += self->KeyLength;
    }
    ntx_u32(page->Data + off, ptrs[keyN]);
    NtxPage_Load(page);
}

static void NtxFile_InsertIntoInternal(NtxFile* self, NtxPage* page, const uint8_t* key, uint32_t newPageNum)
{
    int i, entrySize = self->KeyLength + 4, insertOffset, keyCount = page->KeyCount;
    uint8_t* span = page->Data;
    int rightPointerOffset, srcLen;
    for (i = 0; i < keyCount; i++) {
        if (ByteArrayComparer_Compare(key, self->KeyLength, NtxFile_GetInternalKey(self, page, i), self->KeyLength) < 0)
            break;
    }
    insertOffset = NtxPage_HeaderSize + i * entrySize;
    rightPointerOffset = insertOffset + 4;
    srcLen = (keyCount - i) * entrySize + 4;
    memmove(span + rightPointerOffset + entrySize, span + rightPointerOffset, (size_t)srcLen);
    memcpy(span + insertOffset + 4, key, (size_t)self->KeyLength);
    ntx_u32(span + insertOffset + 4 + self->KeyLength, newPageNum);
    ntx_u16(span, (uint16_t)(keyCount + 1));
    NtxPage_Load(page);
}

typedef struct { uint8_t* key; uint32_t newPageNum; } NtxPromoted;

static NtxPromoted NtxFile_SplitAndInsertLeaf(NtxFile* self, uint32_t pageNum, NtxPage* page,
                                              const uint8_t* keyBytes, int recno)
{
    uint32_t siblingPageNum = NtxFile_AllocateNewPage(self);
    NtxPage* sibling = NtxPage_New();
    int n = page->KeyCount + 1, i, mid, origNext;
    uint8_t** keys = (uint8_t**)malloc((size_t)n * sizeof(uint8_t*));
    uint32_t* recs = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    uint8_t* newKeyCopy = (uint8_t*)malloc((size_t)self->KeyLength);
    NtxPromoted r;
    memcpy(newKeyCopy, keyBytes, (size_t)self->KeyLength);
    for (i = 0; i < page->KeyCount; i++) {
        keys[i] = (uint8_t*)malloc((size_t)self->KeyLength);
        memcpy(keys[i], NtxFile_GetLeafKey(self, page, i), (size_t)self->KeyLength);
        recs[i] = NtxFile_GetLeafRecno(self, page, i);
    }
    keys[n - 1] = newKeyCopy;
    recs[n - 1] = (uint32_t)recno;
    /* sort */
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++)
            if (ByteArrayComparer_Compare(keys[j], self->KeyLength, keys[i], self->KeyLength) < 0) {
                uint8_t* tk = keys[i]; uint32_t tr = recs[i];
                keys[i] = keys[j]; recs[i] = recs[j]; keys[j] = tk; recs[j] = tr;
            }
    }
    mid = n / 2;
    origNext = (int)page->NextPageNum;
    NtxFile_RewriteLeafNode(self, page, (const uint8_t**)keys, recs, mid);
    NtxFile_RewriteLeafNode(self, sibling, (const uint8_t**)(keys + mid), recs + mid, n - mid);
    NtxFile_UpdateLeafPointers(page, 1, siblingPageNum, 0, 0);
    NtxFile_UpdateLeafPointers(sibling, 1, (uint32_t)origNext, 1, pageNum);
    if (origNext != 0) {
        NtxPage* nextPage = NtxFile_ReadPage(self, (uint32_t)origNext);
        if (nextPage) {
            NtxFile_UpdateLeafPointers(nextPage, 0, 0, 1, siblingPageNum);
            NtxFile_WritePage(self, (uint32_t)origNext, nextPage);
        }
    }
    if (self->firstLeafPageNum == 0 || self->firstLeafPageNum == siblingPageNum)
        NtxFile_UpdateFirstLeafInHeader(self, pageNum);
    NtxFile_WritePage(self, pageNum, page);
    NtxFile_WritePage(self, siblingPageNum, sibling);
    r.key = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(r.key, NtxFile_GetLeafKey(self, sibling, 0), (size_t)self->KeyLength);
    r.newPageNum = siblingPageNum;
    for (i = 0; i < n; i++) free(keys[i]);
    free(keys); free(recs);
    return r;
}

static NtxPromoted NtxFile_SplitAndInsertInternal(NtxFile* self, uint32_t pageNum, NtxPage* page,
                                                  const uint8_t* keyToInsert, uint32_t newChildPageNum)
{
    int n = page->KeyCount + 1, i, mid;
    uint8_t** keys = (uint8_t**)malloc((size_t)n * sizeof(uint8_t*));
    uint32_t* ptrsRight = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    uint32_t firstPointer = NtxFile_GetInternalPointer(self, page, 0);
    uint32_t siblingPageNum;
    NtxPage* sibling;
    uint32_t *origPtrs, *sibPtrs;
    const uint8_t **origKeys, **sibKeys;
    int origN, sibN;
    NtxPromoted r;
    for (i = 0; i < page->KeyCount; i++) {
        keys[i] = (uint8_t*)malloc((size_t)self->KeyLength);
        memcpy(keys[i], NtxFile_GetInternalKey(self, page, i), (size_t)self->KeyLength);
        ptrsRight[i] = NtxFile_GetInternalPointer(self, page, i + 1);
    }
    keys[n - 1] = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(keys[n - 1], keyToInsert, (size_t)self->KeyLength);
    ptrsRight[n - 1] = newChildPageNum;
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++)
            if (ByteArrayComparer_Compare(keys[j], self->KeyLength, keys[i], self->KeyLength) < 0) {
                uint8_t* tk = keys[i]; uint32_t tp = ptrsRight[i];
                keys[i] = keys[j]; ptrsRight[i] = ptrsRight[j]; keys[j] = tk; ptrsRight[j] = tp;
            }
    }
    mid = n / 2;
    siblingPageNum = NtxFile_AllocateNewPage(self);
    sibling = NtxPage_New();
    origN = mid;
    sibN = n - mid - 1;
    origKeys = (const uint8_t**)keys;
    sibKeys = (const uint8_t**)(keys + mid + 1);
    origPtrs = (uint32_t*)malloc((size_t)(origN + 1) * sizeof(uint32_t));
    sibPtrs = (uint32_t*)malloc((size_t)(sibN + 1) * sizeof(uint32_t));
    origPtrs[0] = firstPointer;
    for (i = 0; i < origN; i++) origPtrs[i + 1] = ptrsRight[i];
    sibPtrs[0] = ptrsRight[mid];
    for (i = 0; i < sibN; i++) sibPtrs[i + 1] = ptrsRight[mid + 1 + i];
    NtxFile_RewriteInternalNode(self, page, origKeys, origPtrs, origN);
    NtxFile_RewriteInternalNode(self, sibling, sibKeys, sibPtrs, sibN);
    NtxFile_WritePage(self, pageNum, page);
    NtxFile_WritePage(self, siblingPageNum, sibling);
    r.key = (uint8_t*)malloc((size_t)self->KeyLength);
    memcpy(r.key, keys[mid], (size_t)self->KeyLength);
    r.newPageNum = siblingPageNum;
    for (i = 0; i < n; i++) free(keys[i]);
    free(keys); free(ptrsRight); free(origPtrs); free(sibPtrs);
    return r;
}

static NtxPromoted NtxFile_InsertRecursive(NtxFile* self, uint32_t pageNum, const uint8_t* keyBytes, int recno)
{
    NtxPage* page = NtxFile_ReadPage(self, pageNum);
    NtxPromoted none = { NULL, 0 };
    if (!page) return none;
    if (page->IsLeaf) {
        if (page->KeyCount < NtxFile_GetMaxKeysPerLeaf(self)) {
            NtxFile_InsertIntoLeaf(self, page, keyBytes, recno);
            NtxFile_WritePage(self, pageNum, page);
            return none;
        }
        return NtxFile_SplitAndInsertLeaf(self, pageNum, page, keyBytes, recno);
    }
    {
        uint32_t child = NtxFile_DescendInternal(self, page, keyBytes);
        NtxPromoted promoted = NtxFile_InsertRecursive(self, child, keyBytes, recno);
        if (!promoted.key) return none;
        if (page->KeyCount < NtxFile_GetMaxKeysPerInternal(self)) {
            NtxFile_InsertIntoInternal(self, page, promoted.key, promoted.newPageNum);
            NtxFile_WritePage(self, pageNum, page);
            free(promoted.key);
            return none;
        }
        {
            NtxPromoted up = NtxFile_SplitAndInsertInternal(self, pageNum, page, promoted.key, promoted.newPageNum);
            free(promoted.key);
            return up;
        }
    }
}

static void NtxFile_InsertUnderLock(NtxFile* self, const uint8_t* keyBytes, int recno)
{
    NtxPage* oldRoot;
    NtxPromoted promoted;
    if (self->rootPageNum == 0) return;
    oldRoot = NtxFile_ReadPage(self, self->rootPageNum);
    promoted = NtxFile_InsertRecursive(self, self->rootPageNum, keyBytes, recno);
    if (promoted.key && promoted.newPageNum != 0) {
        NtxPage* newRoot = NtxPage_New();
        uint8_t* span = newRoot->Data;
        uint32_t newRootPageNum;
        ntx_u16(span, 1);
        ntx_u16(span + 2, (uint16_t)NtxPageType_Internal);
        ntx_u32(span + NtxPage_HeaderSize, self->rootPageNum);
        memcpy(span + NtxPage_HeaderSize + 4, promoted.key, (size_t)self->KeyLength);
        ntx_u32(span + NtxPage_HeaderSize + 4 + self->KeyLength, promoted.newPageNum);
        NtxPage_Load(newRoot);
        newRootPageNum = NtxFile_AllocateNewPage(self);
        NtxFile_WritePage(self, newRootPageNum, newRoot);
        if (oldRoot && oldRoot->IsLeaf)
            NtxFile_UpdateFirstLeafInHeader(self, self->rootPageNum);
        NtxFile_UpdateRootPageInHeader(self, newRootPageNum);
        free(promoted.key);
    }
}

NtxFile* NtxFile_New(const char* path, LockingStrategy lockingStrategy)
{
    NtxFile* self = (NtxFile*)calloc(1, sizeof(*self));
    uint8_t header[NtxPage_PageSize];
    int exprEnd, i;
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", path);
    self->pageSize = NtxPage_PageSize;
    self->useNodeCache = lockingStrategy == LockingStrategy_IntraProcess;
    self->nodeCache = LruCache_New(256);
    pthread_mutex_init(&self->syncRoot, NULL);
    self->fs = fopen(path, "rb+");
    if (!self->fs || NtxFile_len(self->fs) < self->pageSize) {
        if (self->fs) fclose(self->fs);
        LruCache_Dispose(self->nodeCache);
        free(self);
        return NULL;
    }
    fseek(self->fs, 0, SEEK_SET);
    fread(header, 1, (size_t)self->pageSize, self->fs);
    self->rootPageNum = ntx_ru32(header);
    self->firstLeafPageNum = ntx_ru32(header + 4);
    self->freeListPageHead = ntx_ru32(header + 12);
    self->KeyLength = ntx_ru16(header + 8);
    self->Unique = (header[10] & 1) == 1;
    exprEnd = -1;
    for (i = 16; i < 16 + 256; i++) if (header[i] == 0) { exprEnd = i; break; }
    if (exprEnd < 0) exprEnd = 16 + 256;
    memcpy(self->keyExpressionString, header + 16, (size_t)(exprEnd - 16));
    {
        const char* slash = strrchr(path, '/');
        const char* base = slash ? slash + 1 : path;
        const char* dot = strrchr(base, '.');
        size_t n = dot ? (size_t)(dot - base) : strlen(base);
        if (n >= sizeof(self->Name)) n = sizeof(self->Name) - 1;
        memcpy(self->Name, base, n);
        self->Name[n] = 0;
    }
    self->Descending = 0;
    return self;
}

NtxFile* NtxFile_CreateNew(const char* filePath, const char* keyExpression, int keyLength, int isUnique,
                           LockingStrategy lockingStrategy)
{
    FILE* fs;
    uint8_t header[NtxPage_PageSize], page[NtxPage_PageSize];
    uint32_t root = 1, first = 1;
    memset(header, 0, sizeof(header));
    memset(page, 0, sizeof(page));
    ntx_u32(header, root);
    ntx_u32(header + 4, first);
    ntx_u16(header + 8, (uint16_t)keyLength);
    header[10] = isUnique ? 1 : 0;
    ntx_u32(header + 12, 0);
    if (keyExpression) {
        size_t n = strlen(keyExpression);
        if (n > 255) n = 255;
        memcpy(header + 16, keyExpression, n);
    }
    ntx_u16(page, 0);
    ntx_u16(page + 2, 1);
    ntx_u32(page + 4, 0);
    ntx_u32(page + 8, 0);
    fs = fopen(filePath, "wb");
    if (!fs) return NULL;
    fwrite(header, 1, sizeof(header), fs);
    fwrite(page, 1, sizeof(page), fs);
    fflush(fs);
    fclose(fs);
    return NtxFile_New(filePath, lockingStrategy);
}

void NtxFile_Dispose(NtxFile* self)
{
    if (!self) return;
    pthread_mutex_lock(&self->syncRoot);
    if (self->fs) fclose(self->fs);
    pthread_mutex_unlock(&self->syncRoot);
    LruCache_Dispose(self->nodeCache);
    pthread_mutex_destroy(&self->syncRoot);
    free(self);
}

const char* NtxFile_GetKeyExpressionString(const NtxFile* self) { return self->keyExpressionString; }
const char* NtxFile_get_FilePath(const NtxFile* self) { return self->FilePath; }
const char* NtxFile_get_Name(const NtxFile* self) { return self->Name; }
int NtxFile_get_Unique(const NtxFile* self) { return self->Unique; }
int NtxFile_get_KeyLength(const NtxFile* self) { return self->KeyLength; }

int NtxFile_Seek(NtxFile* self, const uint8_t* keyBytes, int keyLen, int soft)
{
    uint32_t pageNum;
    (void)keyLen;
    pthread_mutex_lock(&self->syncRoot);
    if (self->rootPageNum == 0) { pthread_mutex_unlock(&self->syncRoot); return -1; }
    pageNum = self->rootPageNum;
    for (;;) {
        NtxPage* page = NtxFile_ReadPage(self, pageNum);
        if (!page) { pthread_mutex_unlock(&self->syncRoot); return -1; }
        if (page->IsLeaf) {
            int rec = NtxFile_FindInLeaf(self, page, keyBytes, soft);
            pthread_mutex_unlock(&self->syncRoot);
            return rec;
        }
        pageNum = NtxFile_DescendInternal(self, page, keyBytes);
    }
}

void NtxFile_Insert(NtxFile* self, const uint8_t* keyBytes, int keyLen, int recno)
{
    (void)keyLen;
    pthread_mutex_lock(&self->syncRoot);
    NtxFile_InsertUnderLock(self, keyBytes, recno);
    pthread_mutex_unlock(&self->syncRoot);
}

void NtxFile_InvalidateCache(NtxFile* self)
{
    pthread_mutex_lock(&self->syncRoot);
    (void)self;
    pthread_mutex_unlock(&self->syncRoot);
}

static void ntx_selfcheck_key(uint8_t key[10], int i)
{
    /* 10-byte Clipper-style key: "K000" padded with spaces (no embedded NUL). */
    memset(key, ' ', 10);
    key[0] = 'K';
    key[1] = (uint8_t)('0' + (i / 100) % 10);
    key[2] = (uint8_t)('0' + (i / 10) % 10);
    key[3] = (uint8_t)('0' + i % 10);
}

int NtxFile_SelfCheck(void)
{
    char dir[] = "native/gmb_db_c/build/ntxsc_XXXXXX";
    char path[PATH_MAX];
    NtxFile* idx;
    int i;
    uint8_t key[10];
    if (!mkdtemp(dir)) return -2;
    snprintf(path, sizeof(path), "%s/t.ntx", dir);
    idx = NtxFile_CreateNew(path, "NAME", 10, 0, LockingStrategy_IntraProcess);
    if (!idx) return -3;
    for (i = 0; i < 80; i++) {
        ntx_selfcheck_key(key, i);
        NtxFile_Insert(idx, key, 10, i + 1);
    }
    for (i = 0; i < 80; i++) {
        int rec;
        ntx_selfcheck_key(key, i);
        rec = NtxFile_Seek(idx, key, 10, 0);
        if (rec != i + 1) { NtxFile_Dispose(idx); return -100 - i; }
    }
    NtxFile_Dispose(idx);
    idx = NtxFile_New(path, LockingStrategy_IntraProcess);
    if (!idx) return -4;
    ntx_selfcheck_key(key, 40);
    if (NtxFile_Seek(idx, key, 10, 0) != 41) { NtxFile_Dispose(idx); return -5; }
    NtxFile_Dispose(idx);
    return GMB_DB_OK;
}
