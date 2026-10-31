' FILE: Collections/CollectionsBasics.bas
' =================================================================================================
'       GUIDA TEORICA: COLLEZIONI NATIVE, SINTASSI DUALE E LITERALS
' =================================================================================================
'
' Questo test esplora il supporto esteso di GM-BASIC per le strutture dati (Collezioni),
' evidenziando la flessibilità sintattica del linguaggio che supporta sia uno stile "Legacy"
' (ispirato a VB/BASIC classico) sia uno stile "Moderno" (ispirato a C#/Java/TypeScript).
'
' 1. TIPI DI COLLEZIONI SUPPORTATI
'    - LIST: Una lista ordinata di elementi indicizzabili (ridimensionabile dinamicamente).
'    - DICT: Un dizionario (Map) che associa chiavi univoche a valori. Non ordinato.
'    - SORTED: Un dizionario che mantiene le chiavi ordinate automaticamente.
'    - HASHSET: Un insieme di elementi unici, senza ordine garantito, ottimizzato per lookup veloci.
'    - QUEUE: Una coda FIFO (First-In, First-Out).
'    - STACK: Una pila LIFO (Last-In, First-Out).
'
' 2. SINTASSI DI DICHIARAZIONE: "OF" vs "<...>"
'    GM-BASIC permette due modi per dichiarare tipi generici:
'    - Legacy: `DIM x AS LIST OF STRING`. Verboso, stile naturale.
'    - Modern: `DIM x AS LIST<STRING>`. Conciso, stile C-like.
'    Entrambe le sintassi producono lo stesso identico tipo a runtime.
'
' 3. INIZIALIZZAZIONE CON LITERALS
'    - Liste/Set/Code/Pile: Usano le parentesi quadre `[...]`.
'      Es: `DIM n AS LIST<INT> = [1, 2, 3]`
'    - Dizionari: Usano le parentesi graffe `{...}` con sintassi JSON-like.
'      Es: `DIM d AS DICT<STRING, INT> = {"Uno": 1, "Due": 2}`
'
' 4. ACCESSO AGLI ELEMENTI: "()" vs "[]"
'    - Legacy: `lista(0)` o `dict("chiave")`. Usa le parentesi tonde, indistinguibile da una chiamata di funzione.
'    - Modern: `lista[0]` o `dict["chiave"]`. Usa le parentesi quadre, denota chiaramente un accesso indicizzato.
'
' 5. INTEGRAZIONE JSON
'    Tutte le collezioni native sono serializzabili direttamente tramite `JSON.Stringify`.
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ CollectionsBasics - Test Completo Collezioni Native (Sintassi Combinata)    ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' Dispatcher principale: invoca le subroutine di test per ogni tipo di collezione.
100 GOSUB TestListLegacy
105 GOSUB TestListModern
110 GOSUB TestDictLegacy
115 GOSUB TestDictModern
120 GOSUB TestSortedDictLegacy
125 GOSUB TestSortedDictModern
130 GOSUB TestHashSetLegacy
135 GOSUB TestHashSetModern
140 GOSUB TestQueueLegacy
145 GOSUB TestQueueModern
150 GOSUB TestStackLegacy
155 GOSUB TestStackModern
160 END

' -------------------------------------------------------------------------
' 1. LIST (Lista Dinamica)
' -------------------------------------------------------------------------

200 TestListLegacy:
210 PRINT "--- 1A. Testing LIST OF STRING (Legacy OF, Initialized with List Literal) ---"
    ' Dichiarazione stile VB (OF) e inizializzazione con literal [a, b, c]
220 DIM MyListLegacy AS LIST OF STRING = ["alpha", "beta", "gamma"]
    ' Accesso con parentesi tonde ()
240 PRINT "Legacy List (.Count ="; MyListLegacy.Count; "): "; MyListLegacy(0); ", "; MyListLegacy(1)
250 MyListLegacy(0) = "ALPHA_L"
260 PRINT "Assigned (0): "; MyListLegacy(0)
270 PRINT
280 RETURN

300 TestListModern:
310 PRINT "--- 1B. Testing LIST<STRING> (Modern Angolare, Literal) ---"
    ' Dichiarazione stile C# (<T>) e inizializzazione con literal
320 DIM MyListModern AS LIST<STRING> = ["one", "two", "three"]
    ' Accesso con parentesi quadre []
340 PRINT "Modern List (.Count ="; MyListModern.Count; "): "; MyListModern[0]; ", "; MyListModern[1]
350 MyListModern[1] = "TWO_M"
360 PRINT "Assigned (1) with []: "; MyListModern[1]
    ' Serializzazione automatica in JSON
370 PRINT "List.ToJson(): "; JSON.Stringify(MyListModern)
380 PRINT
390 RETURN

' -------------------------------------------------------------------------
' 2. DICT (Dizionario Hash Map)
' -------------------------------------------------------------------------

400 TestDictLegacy:
410 PRINT "--- 2A. Testing DICT OF STRING, DOUBLE (Legacy OF, New Dict) ---"
    ' Inizializzazione esplicita con NEW
420 DIM PricesLegacy AS DICT OF STRING, DOUBLE = NEW DICT OF STRING, DOUBLE
440 PricesLegacy("KeyA") = 1.05
450 PricesLegacy("KeyB") = 2.10
460 PRINT "Legacy Dict Keys (.Count ="; PricesLegacy.Count; "): KeyA="; PricesLegacy("KeyA"); ", KeyB="; PricesLegacy("KeyB")
470 PRINT
480 RETURN

500 TestDictModern:
510 PRINT "--- 2B. Testing DICT<STRING, DOUBLE> (Modern Angolare, Literal) ---"
    ' Inizializzazione con literal JSON-like { k:v }
520 DIM PricesModern AS DICT<STRING, DOUBLE> = {"Apple": 1.25, "Banana": 0.75}
    ' Accesso con chiave stringa in quadre
540 PricesModern["Apple"] = 1.30
550 PricesModern["Orange"] = 2.50
560 PRINT "Modern Dict Keys (.Count ="; PricesModern.Count; "): Apple="; PricesModern["Apple"]; ", Orange="; PricesModern["Orange"]
    ' Introspezione delle chiavi
570 PRINT "Dict.Keys(): "; JSON.Stringify(PricesModern.Keys())
580 PRINT
590 RETURN

' -------------------------------------------------------------------------
' 3. SORTED (Dizionario Ordinato per Chiave)
' -------------------------------------------------------------------------

600 TestSortedDictLegacy:
610 PRINT "--- 3A. Testing SORTED OF STRING, INTEGER (Legacy OF) ---"
620 DIM SortedL AS SORTED OF STRING, INTEGER = NEW SORTED OF STRING, INTEGER
640 SortedL("Beta") = 2
650 SortedL("Alpha") = 1
    ' Le chiavi verranno stampate in ordine alfabetico (Alpha, Beta)
660 PRINT "Sorted Legacy Keys: "; JSON.Stringify(SortedL.Keys())
670 RETURN

700 TestSortedDictModern:
710 PRINT "--- 3B. Testing SORTED<STRING, INTEGER> (Modern Angolare, Literal) ---"
720 DIM SortedM AS SORTED<STRING, INTEGER> = {"Zebra": 10, "Apple": 20, "Cat": 30}
    ' L'ordine di inserimento non conta, conta l'ordine delle chiavi
740 PRINT "Sorted Modern Keys: "; JSON.Stringify(SortedM.Keys())
750 PRINT
760 RETURN

' -------------------------------------------------------------------------
' 4. HASHSET (Insieme di Valori Unici)
' -------------------------------------------------------------------------

800 TestHashSetLegacy:
810 PRINT "--- 4A. Testing HASHSET OF INTEGER (Legacy OF) ---"
820 DIM SetL AS HASHSET OF INTEGER = [1, 2, 3]
840 SetL.Add(2) ' Aggiunta di un duplicato: viene ignorata silenziosamente
850 PRINT "Set Legacy .Count: "; SetL.Count
860 PRINT
870 RETURN

900 TestHashSetModern:
910 PRINT "--- 4B. Testing HASHSET<INTEGER> (Modern Angolare, Literal) ---"
920 DIM SetM AS HASHSET<INTEGER> = [5, 6, 5] ' Il 5 duplicato nel literal viene collassato
940 PRINT "Set Modern .Count: "; SetM.Count
950 PRINT "Contains 6? "; SetM.Contains(6)
960 PRINT
970 RETURN

' -------------------------------------------------------------------------
' 5. QUEUE (FIFO - First In First Out)
' -------------------------------------------------------------------------

1000 TestQueueLegacy:
1010 PRINT "--- 5A. Testing QUEUE OF STRING (Legacy OF) ---"
1020 DIM TasksL AS QUEUE OF STRING = ["Job1", "Job2"]
1040 TasksL.Enqueue("Job3")
1050 PRINT "Queue Legacy Dequeued: "; TasksL.Dequeue() ' Deve restituire "Job1"
1060 PRINT
1070 RETURN

1100 TestQueueModern:
1110 PRINT "--- 5B. Testing QUEUE<STRING> (Modern Angolare, Literal) ---"
1120 DIM TasksM AS QUEUE<STRING> = ["TaskA", "TaskB"]
1140 TasksM.Enqueue("TaskC")
1150 PRINT "Queue Modern Dequeued: "; TasksM.Dequeue() ' Deve restituire "TaskA"
1160 PRINT
1170 RETURN

' -------------------------------------------------------------------------
' 6. STACK (LIFO - Last In First Out)
' -------------------------------------------------------------------------

1200 TestStackLegacy:
1210 PRINT "--- 6A. Testing STACK OF STRING (Legacy OF) ---"
1220 DIM HistoryL AS STACK OF STRING = ["Hist1", "Hist2"]
1240 HistoryL.Push("Hist3")
1250 PRINT "Stack Legacy Popped: "; HistoryL.Pop() ' Deve restituire "Hist3"
1260 PRINT
1270 RETURN

1300 TestStackModern:
1310 PRINT "--- 6B. Testing STACK<STRING> (Modern Angolare, Literal) ---"
1320 DIM HistoryM AS STACK<STRING> = ["PageX", "PageY"]
1340 HistoryM.Push("PageZ")
1350 PRINT "Stack Modern Popped: "; HistoryM.Pop() ' Deve restituire "PageZ"
1360 PRINT
1370 RETURN