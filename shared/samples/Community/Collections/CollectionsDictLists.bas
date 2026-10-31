' FILE: Collections/CollectionsDictLists.bas
' =================================================================================================
'       GUIDA TEORICA: STRUTTURE DATI FONDAMENTALI (PARTE 1)
' =================================================================================================
'
' Questo test copre l'uso delle collezioni di base in GM-BASIC.
' GM-BASIC offre un ricco set di strutture dati native, ottimizzate e tipizzate.
'
' 1. LIST (Lista Dinamica)
'    Un array ridimensionabile. Mantiene l'ordine di inserimento.
'    - Accesso indicizzato: `list(0)` o `list[0]`.
'    - Metodi principali: `Add`, `Insert`, `Remove`, `Sort`, `Reverse`, `Contains`.
'    - Conversione: `ToArray`, `ToList` (copia).
'
' 2. DICT (Dizionario / Hash Map)
'    Una collezione di coppie chiave-valore. Le chiavi sono univoche.
'    - Inizializzazione Estesa: Le chiavi stringa possono essere scritte senza virgolette se 
'      sono identificatori o keyword valide (es. { nome: "Mario", età: 30 }).
'    - Accesso per chiave: `dict("chiave")` o `dict["chiave"]`.
'    - Metodi principali: `ContainsKey`, `Remove`, `Keys`, `Values`.
'    - Non garantisce l'ordine degli elementi.
'
' 3. SORTED (Dizionario Ordinato)
'    Simile a DICT, ma mantiene le chiavi sempre ordinate (albero RB/AVL).
'    - Utile quando l'ordine di inerzione è importante.
'
' 4. HASHSET (Insieme)
'    Una collezione di elementi unici. Ottimizzato per operazioni di insieme.
'    - Operazioni insiemistiche: `Union`, `Intersect`, `Except`.
'    - Velocissimo per `Contains`, ma non mantiene l'ordine.
'
' 5. QUEUE (Coda FIFO)
'    First-In, First-Out. Gli elementi vengono aggiunti in coda e rimossi dalla testa.
'    - Metodi: `Enqueue` (aggiunge), `Dequeue` (rimuove e restituisce), `Peek` (legge senza rimuovere).
'
' 6. STACK (Pila LIFO)
'    Last-In, First-Out. Gli elementi vengono aggiunti e rimossi dalla cima.
'    - Metodi: `Push` (aggiunge), `Pop` (rimuove e restituisce), `Peek` (legge senza rimuovere).
'
' 7. LINKED LISTS (Liste Concatenate)
'    Strutture dati dove ogni elemento punta al successivo.
'    - LINKEDLIST (Singly-Linked): Ogni nodo ha un riferimento al successivo. `AddFirst`, `AddLast`.
'    - DOUBLYLINKEDLIST (Doubly-Linked): Riferimenti sia al successivo che al precedente.
'    - CIRCULAR (Lista Circolare): L'ultimo elemento punta al primo. Supporta `Rotate()`.
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ CollectionsDictLists - Parte 1: Collezioni di Base e Liste                  ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- Dispatcher dei Test ---
100 GOSUB TestList
110 GOSUB TestDict
120 GOSUB TestSortedDict
130 GOSUB TestHashSet
140 GOSUB TestQueue
150 GOSUB TestStack
160 GOSUB TestLinkedList         ' Singly-Linked
170 GOSUB TestDoublyLinkedList   ' Doubly-Linked
180 GOSUB TestCircularLinkedList
185 GOSUB TestExtendedDictSyntax ' Nuova sintassi TODO #39

190 PRINT "--- Test Parte 1 completati ---"
200 END


' -------------------------------------------------------------------------
' 1. LIST: Vettore dinamico ridimensionabile
' -------------------------------------------------------------------------
1000 TestList:
1010 PRINT "--- 1. Testing LIST<STRING> ---"
    ' Inizializzazione con sintassi letterale [ ]
1020 DIM MyList AS LIST<STRING> = ["beta", "gamma"]
1030 MyList.Add("delta")       ' Aggiunge in coda
1040 MyList.Insert(0, "alpha") ' Inserisce in testa
1050 PRINT "Lista iniziale: "; JSON.Stringify(MyList.ToArray())
1060 MyList.Sort()             ' Ordina in-place
1070 PRINT "Dopo Sort(): "; MyList.Join(", ")
1080 MyList.Reverse()          ' Inverte l'ordine in-place
1090 PRINT "Dopo Reverse(): "; MyList.Join(", ")
1100 MyList.Remove("gamma")    ' Rimuove per valore (prima occorrenza)
1110 MyList.RemoveAt(0)        ' Rimuove per indice
1120 PRINT "Dopo rimozioni: "; JSON.Stringify(MyList.ToArray()) 
1130 PRINT "Contains('alpha'): "; MyList.Contains("alpha"); ", IndexOf('beta'): "; MyList.IndexOf("beta")
    ' .ToList() crea una copia indipendente (shallow copy)
1140 DIM MyList2 = MyList.ToList()
1150 MyList2(0) = "BETA"
1160 PRINT "Originale(0): "; MyList(0); ", Copia(0): "; MyList2(0)
1170 MyList.Clear()
1180 PRINT "Count dopo Clear: "; MyList.Count()
1190 PRINT
1200 RETURN


' -------------------------------------------------------------------------
' 2. DICT: Mappa chiave-valore non ordinata
' -------------------------------------------------------------------------
2000 TestDict:
2010 PRINT "--- 2. Testing DICT<STRING, DOUBLE> ---"
    ' Inizializzazione con sintassi letterale estesa (chiavi senza virgolette)
2020 DIM Prices AS DICT<STRING, DOUBLE> = {Apple: 1.25, Banana: 0.75}
2030 Prices["Orange"] = 2.50   ' Inserimento/Aggiornamento con sintassi array-like
2040 IF Prices.ContainsKey("Apple") THEN Prices("Apple") = 1.30
2050 PRINT "Dict.Count="; Prices.Count(); ", Orange="; Prices["Orange"]
2060 PRINT "Keys: "; JSON.Stringify(Prices.Keys())     ' Restituisce una LIST delle chiavi
2070 PRINT "Values: "; JSON.Stringify(Prices.Values()) ' Restituisce una LIST dei valori
2080 DIM PricesCopy = Prices.ToDict() ' Copia indipendente
2090 PricesCopy.Remove("Apple")
2100 PRINT "Originale contiene Apple: "; Prices.ContainsKey("Apple"); ", Copia contiene Apple: "; PricesCopy.ContainsKey("Apple")
2110 PRINT "Original ToJson: "; JSON.STRINGIFY(Prices.ToJson())
2120 PRINT
2130 RETURN


' -------------------------------------------------------------------------
' 3. SORTED: Mappa chiave-valore ordinata per chiave
' -------------------------------------------------------------------------
3000 TestSortedDict:
3010 PRINT "--- 3. Testing SORTED<STRING, INTEGER> ---"
    ' Sintassi estesa anche per i dizionari ordinati
3020 DIM Scores AS SORTED<STRING, INTEGER> = {Charlie: 85, Alice: 95, Bob: 90}
    ' Le chiavi e i valori sono restituiti in ordine alfabetico delle chiavi (Alice, Bob, Charlie)
3030 PRINT "Keys (ordinato): "; JSON.Stringify(Scores.Keys())
3040 PRINT "Values (ordinato per chiave): "; JSON.Stringify(Scores.Values())
3050 PRINT "Iterazione FOR EACH (ordinata):"
    ' FOR EACH su dizionari itera su coppie (chiave, valore)
3060 FOR EACH name, score IN Scores
3070     PRINT "  - "; name; ": "; score
3080 NEXT
3090 PRINT
3100 RETURN


' -------------------------------------------------------------------------
' 4. HASHSET: Insieme di valori unici
' -------------------------------------------------------------------------
4000 TestHashSet:
4010 PRINT "--- 4. Testing HASHSET<INTEGER> ---"
    ' I duplicati nel letterale [1, 2, 3, 3, 4] vengono eliminati automaticamente
4020 DIM SetA AS HASHSET<INTEGER> = [1, 2, 3, 3, 4]
4030 DIM SetB AS HASHSET<INTEGER> = [3, 4, 5, 6]
4040 PRINT "SetA.Count="; SetA.Count(); ", Contains 3: "; SetA.Contains(3)
    ' Operazioni insiemistiche
4050 DIM UnionSet = SetA.Union(SetB)
4060 PRINT "Union: "; JSON.Stringify(UnionSet.ToArray())
4070 DIM IntersectSet = SetA.Intersect(SetB)
4080 PRINT "Intersection: "; JSON.Stringify(IntersectSet.ToArray())
4090 DIM ExceptSet = SetA.Except(SetB)
4100 PRINT "Except (A-B): "; JSON.Stringify(ExceptSet.ToArray())
4110 PRINT "Iterazione FOR EACH su SetA:"
4120 FOR EACH item IN SetA
4130     PRINT "  - "; item
4140 NEXT
4150 PRINT
4160 RETURN


' -------------------------------------------------------------------------
' 5. QUEUE: Coda FIFO
' -------------------------------------------------------------------------
5000 TestQueue:
5010 PRINT "--- 5. Testing QUEUE ---"
5020 DIM MyQueue AS QUEUE OF STRING = ["first", "second"]
5030 MyQueue.Enqueue("third")  ' Aggiunge in coda
5040 PRINT "Queue Peek: "; MyQueue.Peek()      ' Legge la testa senza rimuovere ("first")
5050 PRINT "Queue Dequeue: "; MyQueue.Dequeue() ' Rimuove e restituisce la testa ("first")
5060 PRINT "Contains 'third': "; MyQueue.Contains("third")
5070 PRINT "Iterazione FOR EACH su Queue (ordine FIFO):"
5080 FOR EACH item$ IN MyQueue
5090     PRINT "  - "; item$
5100 NEXT
5110 PRINT
5120 RETURN


' -------------------------------------------------------------------------
' 6. STACK: Pila LIFO
' -------------------------------------------------------------------------
6000 TestStack:
6010 PRINT "--- 6. Testing STACK ---"
6020 DIM MyStack AS STACK OF INTEGER = [10, 20]
6030 MyStack.Push(30)          ' Aggiunge in cima
6040 PRINT "Stack Peek: "; MyStack.Peek()      ' Legge la cima senza rimuovere (30)
6050 PRINT "Stack Pop: "; MyStack.Pop()        ' Rimuove e restituisce la cima (30)
6060 PRINT "Contains 10: "; MyStack.Contains(10)
6070 PRINT "Iterazione FOR EACH su Stack (ordine LIFO):"
6080 FOR EACH item IN MyStack
6090     PRINT "  - "; item
6100 NEXT
6110 PRINT
6120 RETURN


' -------------------------------------------------------------------------
' 7. LINKEDLIST: Lista concatenata semplice
' -------------------------------------------------------------------------
7000 TestLinkedList:
7010 PRINT "--- 7. Testing LINKEDLIST (Singly-Linked) ---"
7020 DIM MySLL AS LINKEDLIST<STRING> = ["B"]
7030 MySLL.AddFirst("A") : MySLL.AddLast("C")
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
8060 MyDLL.Sort()    ' Supporta l'ordinamento
8070 MyDLL.Reverse() ' Supporta l'inversione
8080 PRINT "Dopo operazioni: "; JSON.Stringify(MyDLL.ToArray())
8090 PRINT
8100 RETURN


' -------------------------------------------------------------------------
' 9. CIRCULAR: Lista circolare (Buffer ad anello dinamico)
' -------------------------------------------------------------------------
9000 TestCircularLinkedList:
9010 PRINT "--- 9. Testing CIRCULAR ---"
9020 DIM MyCLL AS CIRCULAR<STRING> = ["A", "B", "C"]
9030 MyCLL.Add("D")
9040 PRINT "Current: "; MyCLL.Current() ' Elemento corrente (testa implicita)
9050 MyCLL.Rotate(1)  ' Ruota la lista di 1 posizione in avanti (Current diventa B)
9060 PRINT "Dopo Rotate(1), Current: "; MyCLL.Current()
9070 MyCLL.Rotate(-2) ' Ruota indietro di 2 (Current torna ad A, poi a D)
9080 PRINT "Dopo Rotate(-2), Current: "; MyCLL.Current()
9090 PRINT "Iterazione FOR EACH su Circular:"
9100 FOR EACH item$ IN MyCLL
9110     PRINT "  - "; item$
9120 NEXT
9130 PRINT
9140 RETURN


' -------------------------------------------------------------------------
' 10. SINTASSI ESTESA: Identificatori come chiavi non virgolettate
' -------------------------------------------------------------------------
9500 TestExtendedDictSyntax:
9510 PRINT "--- 10. Testing Extended DICT Syntax (Unquoted Keys) ---"
    ' Le chiavi possono essere identificatori o keyword (es: keys, print)
9520 DIM User AS DICT<STRING, ANY> = { id: 101, name: "Gaetano", active: TRUE, keys: "Master" }
9530 PRINT "User ID: "; User["id"]
9540 PRINT "User Name: "; User["name"]
9550 PRINT "User Active: "; User["active"]
9560 PRINT "User Key Type: "; User["keys"]
9570 PRINT "JSON Output: "; JSON.Stringify(User)
9580 PRINT
9590 RETURN