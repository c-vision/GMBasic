' FILE: Collections/CollectionsLinked.bas
' =================================================================================================
'       GUIDA TEORICA: STRUTTURE DATI LINEARI E COLLEGATE (PARTE 3)
' =================================================================================================
'
' Questo test completa la panoramica sulle collezioni native di GM-BASIC, concentrandosi sulle
' strutture dati lineari basate su array (LIST, QUEUE, STACK) e su quelle basate su nodi (LINKEDLIST).
'
' 1. LIST (Lista Dinamica Generica)
'    È la collezione più comune: un array ridimensionabile automaticamente.
'    - Accesso diretto: O(1) tramite indice.
'    - Inserimento/Rimozione: O(N) (deve spostare gli elementi), tranne in coda O(1).
'
' 2. QUEUE (Coda FIFO) e STACK (Pila LIFO)
'    Specializzazioni di LIST per pattern di accesso specifici.
'    - QUEUE: Enqueue (aggiungi in coda), Dequeue (rimuovi dalla testa).
'    - STACK: Push (aggiungi in cima), Pop (rimuovi dalla cima).
'
' 3. DICT e SORTED (Mappe Chiave-Valore)
'    Associano una chiave univoca a un valore.
'    - DICT: Basato su hash table. Accesso O(1). Ordine non garantito.
'    - SORTED: Basato su albero bilanciato. Accesso O(log N). Chiavi sempre ordinate.
'
' 4. HASHSET (Insieme Unico)
'    Collezione di elementi unici senza valori associati. Ideale per controlli di appartenenza veloci
'    e operazioni insiemistiche (Unione, Intersezione).
'
' 5. LISTE COLLEGATE (LINKED LISTS)
'    Strutture dove ogni elemento è un nodo che punta al successivo (e al precedente).
'    - LINKEDLIST: Singolarmente concatenata. Veloce inserimento/rimozione in testa.
'    - DOUBLYLINKEDLIST: Doppiamente concatenata. Navigabile in entrambe le direzioni.
'    - CIRCULAR: L'ultimo nodo punta al primo. Ideale per buffer circolari o turnazioni.
'      Metodo `Rotate(n)` per spostare la "testa" logica della lista.
'
' 6. ITERAZIONE E CONVERSIONE
'    Tutte le collezioni supportano:
'    - `ToArray()`: Restituisce un array nativo degli elementi per l'iterazione classica `FOR i`.
'    - `Count()`: Restituisce il numero di elementi.
'    - `FOR EACH` (non usato in questo file specifico per dimostrare l'approccio classico con `ToArray`).
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ CollectionsLinked - Parte 3: Collezioni di Base e Liste                     ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' Dispatcher principale: esegue tutti i test in sequenza.
100 GOSUB TestList
110 GOSUB TestDict
120 GOSUB TestSortedDict
130 GOSUB TestHashSet
140 GOSUB TestQueue
150 GOSUB TestStack
160 GOSUB TestLinkedList         ' Singly-Linked
170 GOSUB TestDoublyLinkedList   ' Doubly-Linked
180 GOSUB TestCircularLinkedList

190 PRINT "--- Test Parte 1 completati ---"
200 END

' -------------------------------------------------------------------------
' 1. LIST: Vettore dinamico
' -------------------------------------------------------------------------
1000 TestList:
1010 PRINT "--- 1. Testing LIST<STRING> ---"
    ' Inizializzazione con literal
1020 DIM MyList AS LIST<STRING> = ["beta", "gamma"]
1030 MyList.Add("delta")
1040 MyList.Insert(0, "alpha")
1050 PRINT "Lista iniziale: "; JSON.Stringify(MyList.ToArray())
1060 MyList.Sort()             ' Ordinamento
1070 PRINT "Dopo Sort(): "; MyList.Join(", ")
1080 MyList.Reverse()          ' Inversione
1090 PRINT "Dopo Reverse(): "; MyList.Join(", ")
1100 MyList.Remove("gamma")    ' Rimozione per valore
1110 MyList.RemoveAt(0)        ' Rimozione per indice
1120 PRINT "Dopo rimozioni: "; JSON.Stringify(MyList.ToArray()) ' Atteso: ["beta", "alpha"]
1130 PRINT "Contains('alpha'): "; MyList.Contains("alpha"); ", IndexOf('beta'): "; MyList.IndexOf("beta")
    ' Copia profonda (o shallow copy degli elementi)
1140 DIM MyList2 = MyList.ToList()
1150 MyList2(0) = "BETA"
1160 PRINT "Originale(0): "; MyList(0); ", Copia(0): "; MyList2(0)
1170 MyList.Clear()
1180 PRINT "Count dopo Clear: "; MyList.Count()
1190 PRINT
1200 RETURN

' -------------------------------------------------------------------------
' 2. DICT: Mappa hash
' -------------------------------------------------------------------------
2000 TestDict:
2010 PRINT "--- 2. Testing DICT<STRING, DOUBLE> ---"
2020 DIM Prices AS DICT<STRING, DOUBLE> = {"Apple": 1.25, "Banana": 0.75}
2030 Prices["Orange"] = 2.50
2040 IF Prices.ContainsKey("Apple") THEN Prices("Apple") = 1.30
2050 PRINT "Dict.Count="; Prices.Count(); ", Orange="; Prices["Orange"]
2060 PRINT "Keys: "; JSON.Stringify(Prices.Keys())
2070 PRINT "Values: "; JSON.Stringify(Prices.Values())
2080 DIM PricesCopy = Prices.ToDict()
2090 PricesCopy.Remove("Apple")
2100 PRINT "Originale contiene Apple: "; Prices.ContainsKey("Apple"); ", Copia contiene Apple: "; PricesCopy.ContainsKey("Apple")
2110 PRINT "Original ToJson: "; JSON.Stringify(Prices.ToJson())
2120 PRINT
2130 RETURN


' -------------------------------------------------------------------------
' 3. SORTED: Mappa ordinata
' -------------------------------------------------------------------------
3000 TestSortedDict:
3010 PRINT "--- 3. Testing SORTED<STRING, INTEGER> ---"
3020 DIM Scores AS SORTED<STRING, INTEGER> = {"Charlie": 85, "Alice": 95, "Bob": 90}
3030 PRINT "Keys (ordinato): "; JSON.Stringify(Scores.Keys())
3040 PRINT "Values (ordinato per chiave): "; JSON.Stringify(Scores.Values())
3050 PRINT "Iterazione (ordinata):"
    ' Iterazione classica tramite chiavi
3060 DIM sortedKeys = Scores.Keys()
3070 FOR i = 0 TO UBOUND(sortedKeys)
3080     DIM k = sortedKeys(i)
3090     PRINT "  - "; k; ": "; Scores(k)
3100 NEXT
3110 PRINT
3120 RETURN

' -------------------------------------------------------------------------
' 4. HASHSET: Insieme unico
' -------------------------------------------------------------------------
4000 TestHashSet:
4010 PRINT "--- 4. Testing HASHSET<INTEGER> ---"
4020 DIM SetA AS HASHSET<INTEGER> = [1, 2, 3, 3, 4]
4030 DIM SetB AS HASHSET<INTEGER> = [3, 4, 5, 6]
4040 PRINT "SetA.Count="; SetA.Count(); ", Contains 3: "; SetA.Contains(3)
4050 DIM UnionSet = SetA.Union(SetB)
4060 PRINT "Union: "; JSON.Stringify(UnionSet.ToArray())
4070 DIM IntersectSet = SetA.Intersect(SetB)
4080 PRINT "Intersection: "; JSON.Stringify(IntersectSet.ToArray())
4090 DIM ExceptSet = SetA.Except(SetB)
4100 PRINT "Except (A-B): "; JSON.Stringify(ExceptSet.ToArray())
4110 PRINT "Iterazione su SetA:"
4120 DIM setItems = SetA.ToArray()
4130 FOR i = 0 TO UBOUND(setItems)
4140     PRINT "  - "; setItems(i)
4150 NEXT
4160 PRINT
4170 RETURN


' -------------------------------------------------------------------------
' 5. QUEUE: Coda
' -------------------------------------------------------------------------
5000 TestQueue:
5010 PRINT "--- 5. Testing QUEUE ---"
5020 DIM MyQueue AS QUEUE OF STRING = ["first", "second"]
5030 MyQueue.Enqueue("third")
5040 PRINT "Queue Peek: "; MyQueue.Peek()
5050 PRINT "Queue Dequeue: "; MyQueue.Dequeue()
5060 PRINT "Contains 'third': "; MyQueue.Contains("third")
5070 PRINT "Iterazione su Queue (ordine FIFO):"
5080 DIM queueItems = MyQueue.ToArray()
5090 FOR i = 0 TO UBOUND(queueItems)
5100     PRINT "  - "; queueItems(i)
5110 NEXT
5120 PRINT
5130 RETURN


' -------------------------------------------------------------------------
' 6. STACK: Pila
' -------------------------------------------------------------------------
6000 TestStack:
6010 PRINT "--- 6. Testing STACK ---"
6020 DIM MyStack AS STACK OF INTEGER = [10, 20]
6030 MyStack.Push(30)
6040 PRINT "Stack Peek: "; MyStack.Peek()
6050 PRINT "Stack Pop: "; MyStack.Pop()
6060 PRINT "Contains 10: "; MyStack.Contains(10)
6070 PRINT "Iterazione su Stack (ordine LIFO):"
6080 DIM stackItems = MyStack.ToArray()
6090 FOR i = 0 TO UBOUND(stackItems)
6100     PRINT "  - "; stackItems(i)
6110 NEXT
6120 PRINT
6130 RETURN


' -------------------------------------------------------------------------
' 7. LINKEDLIST: Lista concatenata semplice
' -------------------------------------------------------------------------
7000 TestLinkedList:
7010 PRINT "--- 7. Testing LINKEDLIST (Singly-Linked) ---"
7020 DIM MySLL AS LINKEDLIST<STRING> = ["B"]
7030 MySLL.AddFirst("A"): MySLL.AddLast("C")
7040 PRINT "SLL.Count="; MySLL.Count(); ", First="; MySLL.First(); ", Last="; MySLL.Last()
7050 MySLL.Remove("B")
7060 PRINT "Dopo Remove('B'): "; JSON.Stringify(MySLL.ToArray())
7070 MySLL.RemoveFirst()
7080 PRINT "Dopo RemoveFirst(), First: "; MySLL.First()
7090 PRINT
7100 RETURN

' -------------------------------------------------------------------------
' 8. DOUBLYLINKEDLIST: Lista doppiamente concatenata
' -------------------------------------------------------------------------
8000 TestDoublyLinkedList:
8010 PRINT "--- 8. Testing DOUBLYLINKEDLIST ---"
8020 DIM MyDLL AS DOUBLYLINKEDLIST<INTEGER> = [20, 30]
8030 MyDLL.AddFirst(10)
8040 PRINT "IndexOf(20): "; MyDLL.IndexOf(20)
8050 MyDLL.RemoveLast()
8060 MyDLL.Sort() ' Dovrebbe essere [10, 20] -> [10, 20]
8070 MyDLL.Reverse() ' Dovrebbe essere [20, 10]
8080 PRINT "Dopo operazioni: "; JSON.Stringify(MyDLL.ToArray())
8090 PRINT
8100 RETURN


' -------------------------------------------------------------------------
' 9. CIRCULAR: Lista circolare
' -------------------------------------------------------------------------
9000 TestCircularLinkedList:
9010 PRINT "--- 9. Testing CIRCULAR ---"
9020 DIM MyCLL AS CIRCULAR<STRING> = ["A", "B", "C"]
9030 MyCLL.Add("D")
9040 PRINT "Current: "; MyCLL.Current()
9050 MyCLL.Rotate(1) ' Ruota in avanti
9060 PRINT "Dopo Rotate(1), Current: "; MyCLL.Current()
9070 MyCLL.Rotate(-2) ' Ruota indietro
9080 PRINT "Dopo Rotate(-2), Current: "; MyCLL.Current()
9090 PRINT "Iterazione su Circular:"
9100 DIM cllItems = MyCLL.ToArray()
9110 FOR i = 0 TO UBOUND(cllItems)
9120     PRINT "  - "; cllItems(i)
9130 NEXT
9140 PRINT
9150 RETURN