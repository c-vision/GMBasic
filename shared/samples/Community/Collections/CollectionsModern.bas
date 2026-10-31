' FILE: Collections/CollectionsModern.bas
' =================================================================================================
'       GUIDA TEORICA: COLLEZIONI AVANZATE E ALGORITMI
' =================================================================================================
'
' Questo test copre le strutture dati avanzate e specialistiche introdotte in GM-BASIC per
' supportare algoritmi complessi, grafica, analisi dati e sistemi distribuiti.
'
' 1. STRUTTURE AD ALBERO
'    - TREE (Binary Search Tree): Albero binario semplice, non bilanciato. Veloce per dati casuali.
'    - AVLTREE: Albero binario auto-bilanciato. Ottimizzato per lookup frequenti.
'    - REDBLACK: Albero rosso-nero. Bilanciamento più rilassato, ottimizzato per inserimenti/rimozioni frequenti.
'    - BPLUSTREE: Albero bilanciato ottimizzato per sistemi di archiviazione e range query.
'    - TRIE (Prefix Tree): Albero per stringhe, ottimizzato per autocompletamento e ricerche per prefisso.
'    - BITREE (Fenwick Tree): Albero binario indicizzato per calcolo efficiente di somme prefisse e aggiornamenti.
'
' 2. STRUTTURE PROBABILISTICHE E DI BUFFERING
'    - HASHRING: Implementa il Consistent Hashing. Fondamentale per sistemi distribuiti (sharding).
'    - SPARSESET: Gestisce insiemi di interi sparsi in modo efficiente per memoria e cache (ideale per ECS nei giochi).
'    - RINGBUFFER: Coda circolare a dimensione fissa. Sovrascrive i vecchi dati quando piena.
'
' 3. ALTRE STRUTTURE SPECIALIZZATE
'    - HEAP / MAXHEAP: Coda di priorità (Min-Heap o Max-Heap). Accesso O(1) al min/max.
'    - BAG (Multiset): Insieme che permette duplicati. Utile per istogrammi e conteggi.
'    - CACHE: Mappa chiave-valore con policy di scadenza (TTL) e dimensione massima (LRU).
'    - GRAPH: Rappresenta un grafo orientato pesato. Supporta algoritmi come Dijkstra.
'    - TIMELINE: Collezione ordinata temporalmente. Ottimizzata per eventi e serie temporali.
'
' 4. COSTRUTTI DEL LINGUAGGIO
'    - FOR EACH: Itera su qualsiasi collezione. Supporta il destructuring (k, v) per dizionari.
'    - RAW STRINGS: Letterali stringa delimitati da backtick (`) o triple quotes (""") che preservano il formato.
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ CollectionsModern - Parte 2: Collezioni Avanzate e Sintassi Moderna         ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' Dispatcher dei test
100 GOSUB TestTree
110 GOSUB TestAVLTree
120 GOSUB TestRedBlackTree
130 GOSUB TestTrie
140 GOSUB TestHeap
150 GOSUB TestBag
160 GOSUB TestCache
170 GOSUB TestGraph
180 GOSUB TestTimeline
190 GOSUB TestBITree
200 GOSUB TestHashRing
210 GOSUB TestRingBuffer
220 GOSUB TestSparseSet
230 GOSUB TestBPlusTree
240 GOSUB TestForEachConstruct
250 GOSUB TestAdvancedSyntax

260 PRINT "--- Test Parte 2 completati ---"
270 END


' -------------------------------------------------------------------------
' 1. TREE: Albero Binario di Ricerca (BST)
' -------------------------------------------------------------------------
1000 TestTree:
1010 PRINT "--- 1. Testing TREE ---"
    ' Inizializzazione con literal {k:v}
1020 DIM MyTree AS TREE<STRING, INTEGER> = { "b": 20, "a": 10, "c": 30 }
1030 MyTree.Add("d", 40)
    ' Min() restituisce la coppia chiave-valore con la chiave minima
1040 PRINT "Tree.Count="; MyTree.Count(); ", Min="; JSON.Stringify(MyTree.Min())
1050 PRINT "Valore di 'c': "; MyTree("c")
1060 MyTree.Remove("b")
    ' Keys restituisce le chiavi ordinate (in-order traversal)
1070 PRINT "Keys dopo Remove: "; JSON.Stringify(MyTree.Keys())
1080 PRINT
1090 RETURN


' -------------------------------------------------------------------------
' 2. AVLTREE: Albero AVL (Auto-bilanciante rigoroso)
' -------------------------------------------------------------------------
1200 TestAVLTree:
1210 PRINT "--- 2. Testing AVLTREE ---"
1220 DIM MyAVL AS AVLTREE<INTEGER, STRING> = NEW AVLTREE<INTEGER, STRING>
1230 MyAVL.Add(10, "dieci") : MyAVL.Add(20, "venti") : MyAVL.Add(5, "cinque")
    ' Max() restituisce la coppia con la chiave massima
1240 PRINT "AVLTree.Count="; MyAVL.Count(); ", Max="; JSON.Stringify(MyAVL.Max())
1250 PRINT "Valore di 20: "; MyAVL(20)
    ' ExtractMin() rimuove e restituisce il minimo
1260 PRINT "Estratto Min: "; JSON.Stringify(MyAVL.ExtractMin())
1270 PRINT "Nuovo Min: "; JSON.Stringify(MyAVL.Min())
1280 PRINT
1290 RETURN


' -------------------------------------------------------------------------
' 3. REDBLACK: Albero Rosso-Nero (Auto-bilanciante veloce)
' -------------------------------------------------------------------------
1400 TestRedBlackTree:
1410 PRINT "--- 3. Testing REDBLACK ---"
1420 DIM MyRB AS REDBLACK<STRING, BOOLEAN> = { "b": FALSE, "a": TRUE, "c": TRUE }
1430 MyRB["c"] = FALSE ' Accesso con parentesi quadre
1440 PRINT "Valore aggiornato di 'c': "; MyRB["c"]
    ' ToArray restituisce array di KeyValuePair
1450 PRINT "ToArray: "; JSON.Stringify(MyRB.ToArray())
1460 PRINT "Iterazione FOR EACH:"
1470 FOR EACH key, value IN MyRB
1480     PRINT "  - "; key; " -> "; value
1490 NEXT
1500 PRINT
1510 RETURN


' -------------------------------------------------------------------------
' 4. TRIE: Albero di Prefissi
' -------------------------------------------------------------------------
1600 TestTrie:
1610 PRINT "--- 4. Testing TRIE ---"
1620 DIM MyTrie AS TRIE = ["car", "cat", "cart"]
1630 MyTrie.Add("catalog")
1640 PRINT "Trie.Count="; MyTrie.Count(); ", StartsWith('car'): "; MyTrie.StartsWith("car")
    ' FindAll trova tutte le parole che iniziano con un prefisso
1650 PRINT "FindAll('ca'): "; JSON.Stringify(MyTrie.FindAll("ca"))
1660 PRINT "Iterazione FOR EACH:"
1670 FOR EACH word$ IN MyTrie
1680     PRINT "  - "; UPPER(word$)
1690 NEXT
1700 PRINT
1710 RETURN


' -------------------------------------------------------------------------
' 5. HEAP / MAXHEAP: Coda di Priorità
' -------------------------------------------------------------------------
1800 TestHeap:
1810 PRINT "--- 5. Testing HEAP & MAXHEAP ---"
    ' HEAP è un Min-Heap (il più piccolo in cima)
1820 DIM MinHeap AS HEAP<INTEGER> = [4, 1, 7, 3] : MinHeap.Add(2)
    ' MAXHEAP è un Max-Heap (il più grande in cima)
1830 DIM MyMaxHeap AS MAXHEAP<INTEGER> = [4, 1, 7, 3] : MyMaxHeap.Add(9)
1840 PRINT "MinHeap.Peek()="; MinHeap.Peek(); ", MaxHeap.Peek()="; MyMaxHeap.Peek()
1850 PRINT "Estratto da MinHeap: "; MinHeap.Extract(); ", Nuovo Peek: "; MinHeap.Peek()
1860 PRINT
1870 RETURN


' -------------------------------------------------------------------------
' 6. BAG: Multiset (Insieme con duplicati)
' -------------------------------------------------------------------------
1900 TestBag:
1910 PRINT "--- 6. Testing BAG ---"
1920 DIM MyBag AS BAG<STRING> = ["a", "b", "a", "c", "a"]
1930 PRINT "Bag.Count="; MyBag.Count(); ", UniqueCount="; MyBag.UniqueCount()
    ' Count(item) restituisce la molteplicità dell'elemento
1940 PRINT "Conteggio di 'a': "; MyBag.Count("a")
    ' Remove rimuove una singola istanza
1950 MyBag.Remove("a")
1960 PRINT "Nuovo conteggio 'a': "; MyBag.Count("a")
1970 PRINT "Bag.ToJson(): "; JSON.Stringify(MyBag.ToJson())
1980 PRINT
1990 RETURN


' -------------------------------------------------------------------------
' 7. CACHE: Mappa con TTL (Time-To-Live)
' -------------------------------------------------------------------------
2000 TestCache:
2010 PRINT "--- 7. Testing CACHE ---"
2020 DIM MyCache AS CACHE<STRING, STRING>
    ' Set accetta un parametro opzionale per i secondi di vita
2030 MyCache.Set("key1", "value1", 2) : MyCache.Set("key2", "value2")
2035 DIM v$ AS STRING
2040 IF MyCache.TryGetValue("key1", v$) THEN PRINT "Trovato key1: "; v$
2050 PRINT "Keys: "; JSON.Stringify(MyCache.Keys())
2060 PRINT "Attesa di 3 secondi...";: SLEEP 3
    ' Dopo 3 secondi, key1 dovrebbe essere scaduta
2070 IF NOT MyCache.ContainsKey("key1") THEN PRINT "key1 è scaduto correttamente."
2080 PRINT
2090 RETURN


' -------------------------------------------------------------------------
' 8. GRAPH: Grafo Orientato Pesato
' -------------------------------------------------------------------------
2100 TestGraph:
2110 PRINT "--- 8. Testing GRAPH ---"
2120 DIM MyGraph AS GRAPH<STRING>
2130 MyGraph.AddNode("A") : MyGraph.AddNode("B") : MyGraph.AddNode("C")
2140 MyGraph.AddEdge("A", "B", 10) : MyGraph.AddEdge("A", "C", 20) : MyGraph.AddEdge("B", "C", 5)
2150 PRINT "Graph.NodeCount="; MyGraph.NodeCount(); ", HasEdge(A,C): "; MyGraph.HasEdge("A", "C")
2160 PRINT "In-Degree di 'C': "; MyGraph.GetInDegree("C")
    ' Algoritmo di Dijkstra integrato per cammino minimo
2170 PRINT "Dijkstra da A: "; JSON.Stringify(MyGraph.Dijkstra("A"))
2180 PRINT
2190 RETURN


' -------------------------------------------------------------------------
' 9. TIMELINE: Serie Temporale Ordinata
' -------------------------------------------------------------------------
2200 TestTimeline:
2210 PRINT "--- 9. Testing TIMELINE ---"
2220 DIM MyTimeline AS TIMELINE<STRING>
    ' Le chiavi sono sempre Date/DateTime
2230 MyTimeline.Add(DateSerial(2023, 1, 10), "Evento A")
2240 MyTimeline.Add(DateSerial(2023, 1, 15), "Evento B")
2250 MyTimeline.Add(DateSerial(2023, 1, 20), "Evento C")
2260 PRINT "Timeline.Count="; MyTimeline.Count(); ", Primo evento: "; JSON.Stringify(MyTimeline.First())
    ' GetRange restituisce una sotto-timeline
2270 PRINT "Eventi tra 12 e 18 Gen: "; JSON.Stringify(MyTimeline.GetRange(DateSerial(2023, 1, 12), DateSerial(2023, 1, 18)))
2280 PRINT
2290 RETURN


' -------------------------------------------------------------------------
' 10. BITREE: Binary Indexed Tree (Fenwick Tree)
' -------------------------------------------------------------------------
2300 TestBITree:
2310 PRINT "--- 10. Testing BITREE ---"
2330 DIM MyBIT AS BITREE<INTEGER> = NEW BITREE<INTEGER>([1, 2, 3, 4, 5])
    ' Query(idx) calcola la somma prefissa da 0 a idx
2340 PRINT "Somma fino a indice 2 (1+2+3): "; MyBIT.Query(2)
    ' QueryRange(start, end)
2350 PRINT "Somma intervallo [1, 3] (2+3+4): "; MyBIT.QueryRange(1, 3)
    ' Update aggiunge un delta a un valore esistente
2360 MyBIT.Update(1, 10) ' Aggiorna indice 1 da 2 a 12 (delta +10)
2370 PRINT "Nuova somma intervallo [1, 3]: "; MyBIT.QueryRange(1, 3)
2380 PRINT
2390 RETURN


' -------------------------------------------------------------------------
' 11. HASHRING: Consistent Hashing Ring
' -------------------------------------------------------------------------
2400 TestHashRing:
2410 PRINT "--- 11. Testing HASHRING ---"
2420 DIM Servers = ["10.0.0.1", "10.0.0.2", "10.0.0.3"]
2430 DIM MyRing AS HASHRING<STRING> = NEW HASHRING<STRING>(Servers)
    ' Mappa una chiave a un nodo (server)
2440 PRINT "La chiave 'user:123' è mappata su: "; MyRing.GetNode("user:123")
    ' Aggiunge un nodo dinamicamente minimizzando le rimappature
2450 MyRing.AddNode("10.0.0.4")
2460 PRINT "Dopo AddNode, la chiave 'user:123' è mappata su: "; MyRing.GetNode("user:123")
2470 PRINT
2480 RETURN


' -------------------------------------------------------------------------
' 12. RINGBUFFER: Buffer Circolare a dimensione fissa
' -------------------------------------------------------------------------
2500 TestRingBuffer:
2510 PRINT "--- 12. Testing RINGBUFFER ---"
    ' Crea un buffer di capacità 3
2520 DIM MyBuffer AS RINGBUFFER<STRING> = NEW RINGBUFFER<STRING>(3)
2530 MyBuffer.Enqueue("A") : MyBuffer.Enqueue("B") : MyBuffer.Enqueue("C")
2540 PRINT "Buffer pieno: "; JSON.Stringify(MyBuffer.ToArray())
    ' L'inserimento in un buffer pieno sovrascrive il più vecchio ("A")
2550 MyBuffer.Enqueue("D") 
2560 PRINT "Buffer dopo overflow: "; JSON.Stringify(MyBuffer.ToArray())
2570 PRINT "Dequeue: "; MyBuffer.Dequeue()
2580 PRINT
2590 RETURN


' -------------------------------------------------------------------------
' 13. SPARSESET: Insieme sparso di interi (Entity Component System)
' -------------------------------------------------------------------------
2600 TestSparseSet:
2610 PRINT "--- 13. Testing SPARSESET ---"
    ' (Capacità Massima, Dimensione Pagina)
2620 DIM ActiveEntities AS SPARSESET = NEW SPARSESET(1000, 10)
2630 ActiveEntities.Add(100) : ActiveEntities.Add(500) : ActiveEntities.Add(999)
2640 PRINT "SparseSet.Count="; ActiveEntities.Count(); ", Contiene 500: "; ActiveEntities.HasItem(500)
2650 ActiveEntities.Remove(100)
2660 PRINT "Contiene 100 ora: "; ActiveEntities.HasItem(100)
2670 PRINT
2680 RETURN


' -------------------------------------------------------------------------
' 14. BPLUSTREE: Albero B+ (Ottimizzato per range query)
' -------------------------------------------------------------------------
2700 TestBPlusTree:
2710 PRINT "--- 14. Testing BPLUSTREE ---"
    ' Ordine dell'albero = 3
2720 DIM MyBPTree AS BPLUSTREE<STRING, INTEGER> = NEW BPLUSTREE<STRING, INTEGER>(3)
2730 MyBPTree.Add("zulu", 30) : MyBPTree.Add("alpha", 10) : MyBPTree.Add("bravo", 20)
2740 PRINT "BPlusTree.Count="; MyBPTree.Count(); ", Valore Min="; MyBPTree.Min()
2750 PRINT "Valore di 'bravo': "; MyBPTree.Get("bravo")
2760 PRINT "Estratto Max: "; JSON.Stringify(MyBPTree.ExtractMax())
2770 PRINT "Nuovo Max: "; MyBPTree.Max()
2780 PRINT
2790 RETURN


' -------------------------------------------------------------------------
' 15. FOR EACH: Costrutto di iterazione universale
' -------------------------------------------------------------------------
2800 TestForEachConstruct:
2810 PRINT "--- 15. Testing FOR EACH Costrutto ---"
2820 DIM a = [10, 20, 30]
2830 PRINT "FOR EACH su Array nativo:"
2840 FOR EACH n IN a
2850     PRINT "  - "; n
2860 NEXT
2870 PRINT
2880 DIM d AS DICT<STRING, INTEGER> = { "uno": 1, "due": 2 }
2890 PRINT "FOR EACH con spacchettamento su DICT:"
    ' Supporto per destructuring della coppia chiave-valore
2900 FOR EACH k, v IN d
2910     PRINT "  - "; k; " -> "; v
2920 NEXT
2930 PRINT
2940 RETURN


' -------------------------------------------------------------------------
' 16. Sintassi Avanzata (Raw Strings, Funzioni UDF con Collezioni)
' -------------------------------------------------------------------------
3000 TestAdvancedSyntax:
3010 PRINT "--- 16. Testing Sintassi Avanzata ---"
    ' Raw string con backtick
3020 DIM jsonString$ = `{"user": "gaetano", "permissions": ["read", "write"]}`
    ' Accesso a proprietà dinamiche su oggetto JSON
3030 PRINT "JSON String Parsed: "; JSON.Parse(jsonString$).user
3040 DIM sourceDict AS DICT<STRING, INTEGER> = {"a": 1, "b": 2, "c": 3}
3045 DIM resultList AS LIST<STRING> = NEW LIST<STRING>
3046 resultList.Add("TEST")
3060 PRINT "Result from Function: "; JSON.Stringify(resultList)
    ' Continuazione di riga con underscore
3070 PRINT "Concatenazione " + "con " + _
         "funzioni: " + resultList[0] + _
         ", e con " : PRINT " continuazione (:)"
3080 PRINT
3090 RETURN

' Funzione definita dall'utente che accetta e restituisce collezioni generiche
FUNCTION GetUppercaseKeys(myData AS DICT<STRING, INTEGER>) AS LIST<STRING>
    DIM keys AS LIST<STRING> = NEW LIST<STRING>(myData.Keys())
    DIM upperKeys AS LIST<STRING> = NEW LIST<STRING>
    FOR EACH key$ IN keys
        upperKeys.Add(UPPER(key$))
    NEXT
    FUNCTION = upperKeys
END FUNCTION