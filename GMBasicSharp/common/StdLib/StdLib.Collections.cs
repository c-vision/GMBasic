// FILE: Interpreter.StdLib.Collections.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Collections — namespace 'COLLECTIONS'
 * 0 funzioni | 0 comandi | 30 tipi | 0 costanti
 *
 * TIPI (30):
 *   AVLTreeType
 *   BITREE
 *   BPlusTreeType
 *   BagType
 *   BinaryHeapType
 *   BinaryIndexedTreeType
 *   CIRCULAR
 *   CacheType
 *   CircularLinkedListType
 *   DictType
 *   DoublyLinkedListType
 *   GraphType
 *   HEAP
 *   HashRingType
 *   HashSetType
 *   KeyValuePairType
 *   LinkedListType
 *   ListType
 *   MAXHEAP
 *   QueueType
 *   REDBLACK
 *   RedBlackTreeType
 *   RingBufferType
 *   SORTED
 *   SortedDictType
 *   SparseSetType
 *   StackType
 *   TimelineType
 *   TreeType
 *   TrieType
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- COLLECTIONS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'COLLECTIONS'.
    /// QUESTA VERSIONE È CORRETTA e non usa più `Union()` per unire i dizionari dei metodi,
    /// risolvendo il warning di performance del compilatore C#.
    /// </summary>
    private void CreateCollectionsNamespace()
    {
        const string namespaceName = "COLLECTIONS";

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "LIST", new ListType(new AnyType()) },
            { "DICT", new DictType(new AnyType(), new AnyType()) },
            { "HASHSET", new HashSetType(new AnyType()) },
            { "QUEUE", new QueueType(new AnyType()) },
            { "STACK", new StackType(new AnyType()) },
            { "SORTED", new SortedDictType(new AnyType(), new AnyType()) },
            { "TREE", new TreeType(new AnyType(), new AnyType()) },
            { "AVLTREE", new AVLTreeType(new AnyType(), new AnyType()) },
            { "REDBLACK", new RedBlackTreeType(new AnyType(), new AnyType()) },
            { "BITREE", new BinaryIndexedTreeType(new AnyType()) },
            { "TRIE", new TrieType() }, // Trie è specifico per stringhe, non cambia
            { "LINKEDLIST", new LinkedListType(new AnyType()) },
            { "DOUBLYLINKEDLIST", new DoublyLinkedListType(new AnyType()) },
            { "CIRCULAR", new CircularLinkedListType(new AnyType()) },
            { "HEAP", new BinaryHeapType(new AnyType(), IsMaxHeap: false) },
            { "MAXHEAP", new BinaryHeapType(new AnyType(), IsMaxHeap: true) },
            { "BAG", new BagType(new AnyType()) },
            { "CACHE", new CacheType(new AnyType(), new AnyType()) },
            { "GRAPH", new GraphType(new AnyType()) },
            { "TIMELINE", new TimelineType(new AnyType()) },
            { "HASHRING", new HashRingType(new AnyType()) },
            { "RINGBUFFER", new RingBufferType(new AnyType()) },
            { "SPARSESET", new SparseSetType() },
            { "BPLUSTREE", new BPlusTreeType(new AnyType(), new AnyType()) },
            { "KEYVALUEPAIR", new KeyValuePairType(new AnyType(), new AnyType()) }
        }.ToImmutableDictionary(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, [], types);

        // --- Definizione dei tipi di base per riutilizzo ---
        var voidType = new VoidType();
        var longType = new LongType();
        var bitType = new BitType();
        var inferred = new InferredType();
        var stringType = new StringType();
        var jsonType = new JsonType();
        var inferredArray = new ArrayType(inferred);
        var inferredList = new ListType(inferred);

        // --- Registrazione degli Schemi per i Tipi di Collezione ---

        // Schema per KeyValuePair
        RegisterTypeSchema(typeof(KeyValuePairType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Key", inferred },
                { "Value", inferred }
            });

        // --- COLLEZIONI A SINGOLO ELEMENTO ---
        var singleElementCollectionMethods = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ToArray", inferredArray },
            { "ToList", inferredList },
            { "ToJSON", jsonType }
        };

        var listMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Insert", voidType }, { "RemoveAt", voidType }, { "Clear", voidType },
                { "Sort", voidType }, { "Reverse", voidType }, { "AddRange", voidType },
                { "Remove", bitType }, { "Contains", bitType },
                { "IndexOf", longType }, { "Join", stringType }
            };
        RegisterTypeSchema(typeof(ListType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, listMethods);

        var hashSetMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Union", new HashSetType(inferred) }, { "Intersect", new HashSetType(inferred) },
                { "Except", new HashSetType(inferred) },
                { "Add", bitType }, { "Remove", bitType }, { "Contains", bitType }, { "Clear", voidType }
            };
        RegisterTypeSchema(typeof(HashSetType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } },
            hashSetMethods);

        var queueMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Enqueue", voidType }, { "Clear", voidType },
                { "Dequeue", inferred }, { "Peek", inferred },
                { "Contains", bitType }
            };
        RegisterTypeSchema(typeof(QueueType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, queueMethods);

        var stackMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Push", voidType }, { "Clear", voidType },
                { "Pop", inferred }, { "Peek", inferred },
                { "Contains", bitType }
            };
        RegisterTypeSchema(typeof(StackType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, stackMethods);

        // --- COLLEZIONI CHIAVE-VALORE ---
        var keyValueCollectionMethods = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ToArray", new ArrayType(new KeyValuePairType(inferred, inferred)) },
            { "ToDict", new DictType(inferred, inferred) },
            { "ToJSON", jsonType }
        };

        // Snippet 1: Aggiornamento dictMethods per DICT e SORTED
        var dictMethods = new Dictionary<string, DataType>(keyValueCollectionMethods, StringComparer.OrdinalIgnoreCase)
        {
            { "Add", voidType }, { "Clear", voidType },
            { "Remove", bitType }, { "ContainsKey", bitType },
            { "TryGet", bitType }
        };
        RegisterTypeSchema(typeof(DictType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Count", longType }, { "Keys", new ListType(inferred) }, { "Values", new ListType(inferred) }
            }, dictMethods);

        RegisterTypeSchema(typeof(SortedDictType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Count", longType }, { "Keys", new ListType(inferred) }, { "Values", new ListType(inferred) }
            }, dictMethods);

        // --- ALBERI ---
        var treeMethods = new Dictionary<string, DataType>(keyValueCollectionMethods, StringComparer.OrdinalIgnoreCase)
        {
            { "Add", voidType }, { "Clear", voidType },
            { "Min", new KeyValuePairType(inferred, inferred) }, { "Max", new KeyValuePairType(inferred, inferred) },
            { "First", new KeyValuePairType(inferred, inferred) }, { "Last", new KeyValuePairType(inferred, inferred) },
            { "ExtractMin", new KeyValuePairType(inferred, inferred) },
            { "ExtractMax", new KeyValuePairType(inferred, inferred) },
            { "Get", inferred },
            { "TryGetValue", bitType }, { "TryGet", bitType }, { "Remove", bitType }, { "ContainsKey", bitType }
        };
        RegisterTypeSchema(typeof(TreeType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, treeMethods);
        RegisterTypeSchema(typeof(AVLTreeType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, treeMethods);
        RegisterTypeSchema(typeof(RedBlackTreeType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, treeMethods);
        RegisterTypeSchema(typeof(BPlusTreeType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, treeMethods);

        // --- ALTRE COLLEZIONI ---
        RegisterTypeSchema(typeof(TrieType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Add", bitType }, { "Remove", bitType }, { "Contains", bitType }, { "StartsWith", bitType },
                { "Clear", voidType },
                { "FindAll", new ListType(stringType) }, { "ToArray", new ArrayType(stringType) },
                { "ToList", new ListType(stringType) }, { "ToJSON", jsonType }
            });

        var biTreeMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Update", voidType }, { "Query", inferred }, { "QueryRange", inferred }
            };
        RegisterTypeSchema(typeof(BinaryIndexedTreeType), null, biTreeMethods);

        var linkedListMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "AddFirst", voidType }, { "AddLast", voidType }, { "RemoveFirst", voidType },
                { "RemoveLast", voidType },
                { "Clear", voidType }, { "Sort", voidType }, { "Reverse", voidType },
                { "Remove", bitType }, { "Contains", bitType },
                { "IndexOf", longType }
            };
        RegisterTypeSchema(typeof(LinkedListType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "First", inferred }, { "Last", inferred } }, linkedListMethods);
        RegisterTypeSchema(typeof(DoublyLinkedListType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "First", inferred }, { "Last", inferred } }, linkedListMethods);

        var circularListMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Rotate", voidType }, { "Clear", voidType },
                { "Remove", bitType }, { "Contains", bitType }, { "IndexOf", longType }
            };
        RegisterTypeSchema(typeof(CircularLinkedListType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "Current", inferred } }, circularListMethods);

        var heapMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Clear", voidType }, { "Extract", inferred }, { "Peek", inferred }
            };
        RegisterTypeSchema(typeof(BinaryHeapType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } }, heapMethods);

        var bagMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Clear", voidType },
                { "Remove", bitType }, { "Contains", bitType },
                { "ToDict", new DictType(inferred, longType) }
            };
        RegisterTypeSchema(typeof(BagType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Count", longType }, { "UniqueCount", longType }, { "UniqueItems", new HashSetType(inferred) }
            }, bagMethods);

        var cacheMethods = new Dictionary<string, DataType>(keyValueCollectionMethods, StringComparer.OrdinalIgnoreCase)
        {
            { "Set", voidType }, { "Clear", voidType },
            { "Min", new KeyValuePairType(inferred, inferred) }, { "Max", new KeyValuePairType(inferred, inferred) },
            { "First", new KeyValuePairType(inferred, inferred) }, { "Last", new KeyValuePairType(inferred, inferred) },
            { "Get", inferred }, { "TryGetValue", bitType }, { "TryGet", bitType }, { "Remove", bitType }, { "ContainsKey", bitType }
        };
        RegisterTypeSchema(typeof(CacheType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Count", longType }, { "Keys", new ListType(inferred) }, { "Values", new ListType(inferred) }
            }, cacheMethods);

        RegisterTypeSchema(typeof(GraphType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "NodeCount", longType }, { "Count", longType } },
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "AddNode", bitType }, { "RemoveNode", bitType }, { "HasNode", bitType },
                { "AddEdge", voidType }, { "RemoveEdge", bitType }, { "HasEdge", bitType },
                { "GetNeighbors", new ListType(inferred) }, { "GetEdgeWeight", new DoubleType() },
                { "GetInDegree", longType }, { "GetOutDegree", longType }, { "GetDegree", longType },
                { "BFS", new ListType(inferred) }, { "DFS", new ListType(inferred) },
                { "Dijkstra", new DictType(inferred, new DoubleType()) },
                { "ShortestPath", new ListType(inferred) }, { "DijkstraPath", new ListType(inferred) },
                { "FindPath", new ListType(inferred) }, { "AStar", new ListType(inferred) },
                { "BellmanFord", new DictType(inferred, new DoubleType()) },
                { "FloydWarshall", new DictType(inferred, new DictType(inferred, new DoubleType())) },
                { "ConnectedComponents", new ListType(new ListType(inferred)) },
                { "StronglyConnectedComponents", new ListType(new ListType(inferred)) },
                { "HasCycle", bitType }, { "TopologicalSort", new ListType(inferred) }, { "IsBipartite", bitType },
                { "ClosenessCentrality", new DoubleType() }, { "BetweennessCentrality", new DictType(inferred, new DoubleType()) },
                { "PageRank", new ListType(new DictType(inferred, new DoubleType())) },
                { "Diameter", new DoubleType() }, { "Eccentricity", new DoubleType() },
                { "ClusteringCoefficient", new DoubleType() }, { "MinimumSpanningTree", new ListType(new DictType(inferred, new DoubleType())) },
                { "Nodes", new ListType(inferred) }, { "Keys", new ListType(inferred) },
                { "ToArray", new ArrayType(inferred) }, { "ToDict", new DictType(inferred, new ListType(inferred)) },
                { "ToJSON", jsonType }
            });

        var timelineMethods =
            new Dictionary<string, DataType>(keyValueCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Clear", voidType },
                { "Get", inferred }, { "GetRange", new TimelineType(inferred) },
                { "Remove", bitType }, { "Contains", bitType },
                { "Keys", new ListType(new DateTimeType()) }, { "Values", new ListType(inferred) }
            };
        RegisterTypeSchema(typeof(TimelineType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "First", inferred }, { "Last", inferred } }, timelineMethods);

        var hashRingMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "AddNode", voidType }, { "RemoveNode", voidType }, { "GetNode", inferred }
            };
        RegisterTypeSchema(typeof(HashRingType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase) { { "Count", longType } },
            hashRingMethods);

        var ringBufferMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Enqueue", voidType }, { "Clear", voidType }, { "Dequeue", inferred }, { "Peek", inferred }
            };
        RegisterTypeSchema(typeof(RingBufferType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "Capacity", longType } }, ringBufferMethods);

        var sparseSetMethods =
            new Dictionary<string, DataType>(singleElementCollectionMethods, StringComparer.OrdinalIgnoreCase)
            {
                { "Add", voidType }, { "Clear", voidType }, { "Remove", bitType }, { "HasItem", bitType }
            };
        RegisterTypeSchema(typeof(SparseSetType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
                { { "Count", longType }, { "Capacity", longType } }, sparseSetMethods);
    }
}