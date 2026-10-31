// FILE: Ast.DataTypes.Collections.cs



// ReSharper disable InconsistentNaming

namespace Basic;

// ========================================================================
// --- NUOVI TIPI DI DATO PER COLLEZIONI NATIVE ---
// ========================================================================

/// <summary>
/// Rappresenta una singola coppia chiave-valore all'interno di un dictionary literal.
/// </summary>
[JsonDerivedType(typeof(DictionaryEntry), nameof(DictionaryEntry))]
public record DictionaryEntry(Expr Key, Expr Value)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta della coppia chiave-valore.
    /// </summary>
    public override string ToString() => $"{Key}: {Value}";

    /// <summary>
    /// Crea una copia profonda di questa voce di dizionario.
    /// </summary>
    public DictionaryEntry DeepClone()
    {
        return new DictionaryEntry((Expr)Key.DeepClone(), (Expr)Value.DeepClone());
    }
}

/// <summary>
/// Rappresenta il tipo di dato di una coppia chiave-valore.
/// </summary>
[JsonDerivedType(typeof(KeyValuePairType), nameof(KeyValuePairType))]
public record KeyValuePairType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale del tipo.
    /// </summary>
    public override string ToString() => $"KEYVALUEPAIR OF {KeyType}, {ValueType}";
    
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new KeyValuePairType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato LIST, una collezione ordinata di elementi.
/// </summary>
[JsonDerivedType(typeof(ListType), nameof(ListType))]
public record ListType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"LIST OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new ListType((DataType)ElementType.DeepClone());
} 

/// <summary>
/// Rappresenta il tipo di dato DICT, una collezione di coppie chiave-valore.
/// </summary>
[JsonDerivedType(typeof(DictType), nameof(DictType))]
public record DictType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"DICT OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new DictType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato HASHSET, una collezione di elementi unici non ordinati.
/// </summary>
[JsonDerivedType(typeof(HashSetType), nameof(HashSetType))]
public record HashSetType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"HASHSET OF {ElementType}";
    
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new HashSetType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato QUEUE, una collezione FIFO (First-In, First-Out).
/// </summary>
[JsonDerivedType(typeof(QueueType), nameof(QueueType))]
public record QueueType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"QUEUE OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new QueueType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato STACK, una collezione LIFO (Last-In, First-Out).
/// </summary>
[JsonDerivedType(typeof(StackType), nameof(StackType))]
public record StackType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"STACK OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new StackType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato SORTED, una collezione ordinata di coppie chiave-valore.
/// </summary>
[JsonDerivedType(typeof(SortedDictType), nameof(SortedDictType))]
public record SortedDictType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"SORTED OF {KeyType}, {ValueType}";
    
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new SortedDictType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato TREE, un albero binario di ricerca non bilanciato.
/// </summary>
[JsonDerivedType(typeof(TreeType), nameof(TreeType))]
public record TreeType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"TREE OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new TreeType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato AVLTREE, un albero binario di ricerca auto-bilanciato.
/// </summary>
[JsonDerivedType(typeof(AVLTreeType), nameof(AVLTreeType))]
public record AVLTreeType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"AVLTREE OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new AVLTreeType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato REDBLACK, un albero rosso-nero auto-bilanciato.
/// </summary>
[JsonDerivedType(typeof(RedBlackTreeType), nameof(RedBlackTreeType))]
public record RedBlackTreeType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"REDBLACK OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new RedBlackTreeType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato BITREE, un Albero di Fenwick per somme prefisse.
/// </summary>
[JsonDerivedType(typeof(BinaryIndexedTreeType), nameof(BinaryIndexedTreeType))]
public record BinaryIndexedTreeType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"BITREE OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new BinaryIndexedTreeType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato TRIE, un albero di prefissi per stringhe.
/// </summary>
public record TrieType(Token? SourceToken = null) 
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "TRIE";
    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato LINKEDLIST, una lista concatenata.
/// </summary>
[JsonDerivedType(typeof(LinkedListType), nameof(LinkedListType))]
public record LinkedListType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"LINKEDLIST OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new LinkedListType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato DOUBLYLINKEDLIST, una lista doppiamente concatenata.
/// </summary>
[JsonDerivedType(typeof(DoublyLinkedListType), nameof(DoublyLinkedListType))]
public record DoublyLinkedListType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"DOUBLYLINKEDLIST OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new DoublyLinkedListType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato CIRCULAR, una lista circolare doppiamente concatenata.
/// </summary>
[JsonDerivedType(typeof(CircularLinkedListType), nameof(CircularLinkedListType))]
public record CircularLinkedListType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"CIRCULAR OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new CircularLinkedListType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato HEAP (Min-Heap) o MAXHEAP (Max-Heap).
/// </summary>
[JsonDerivedType(typeof(BinaryHeapType), nameof(BinaryHeapType))]
public record BinaryHeapType(DataType ElementType, bool IsMaxHeap) : DataType(ElementType.Position)
{
    public override string ToString() => $"{(IsMaxHeap ? "MAXHEAP" : "HEAP")} OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new BinaryHeapType((DataType)ElementType.DeepClone(), IsMaxHeap);
}

/// <summary>
/// Rappresenta il tipo di dato BAG (Multiset).
/// </summary>
[JsonDerivedType(typeof(BagType), nameof(BagType))]
public record BagType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"BAG OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new BagType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato CACHE.
/// </summary>
[JsonDerivedType(typeof(CacheType), nameof(CacheType))]
public record CacheType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"CACHE OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new CacheType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato GRAPH.
/// </summary>
[JsonDerivedType(typeof(GraphType), nameof(GraphType))]
public record GraphType(DataType NodeType) : DataType(NodeType.Position)
{
    public override string ToString() => $"GRAPH OF {NodeType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new GraphType((DataType)NodeType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato TIMELINE.
/// </summary>
[JsonDerivedType(typeof(TimelineType), nameof(TimelineType))]
public record TimelineType(DataType ValueType) : DataType(ValueType.Position)
{
    public override string ToString() => $"TIMELINE OF {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new TimelineType((DataType)ValueType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato HASHRING, un anello di hashing coerente.
/// </summary>
[JsonDerivedType(typeof(HashRingType), nameof(HashRingType))]
public record HashRingType(DataType NodeType) : DataType(NodeType.Position)
{
    public override string ToString() => $"HASHRING OF {NodeType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new HashRingType((DataType)NodeType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato RINGBUFFER, una coda circolare a dimensione fissa.
/// </summary>
[JsonDerivedType(typeof(RingBufferType), nameof(RingBufferType))]
public record RingBufferType(DataType ElementType) : DataType(ElementType.Position)
{
    public override string ToString() => $"RINGBUFFER OF {ElementType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new RingBufferType((DataType)ElementType.DeepClone());
}

/// <summary>
/// Rappresenta il tipo di dato SPARSESET. Non è generico.
/// </summary>
public record SparseSetType(Token? SourceToken = null) 
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "SPARSESET";
    /// <summary>
    /// Crea una copia profonda di questo nodo.
    /// </summary>
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato BPLUSTREE.
/// </summary>
[JsonDerivedType(typeof(BPlusTreeType), nameof(BPlusTreeType))]
public record BPlusTreeType(DataType KeyType, DataType ValueType) : DataType(KeyType.Position)
{
    public override string ToString() => $"BPLUSTREE OF {KeyType}, {ValueType}";
    /// <summary>
    /// Crea una copia profonda di questo nodo DataType.
    /// </summary>
    public override AstNode DeepClone() => new BPlusTreeType((DataType)KeyType.DeepClone(), (DataType)ValueType.DeepClone());
}