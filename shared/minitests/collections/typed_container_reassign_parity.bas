' EXPECT: 1|2|1|8|3|2|-1
' Contenitore TIPIZZATO riassegnato con un letterale FUORI da DIM: `H = [e1, …]`.
' Il letterale sono gli ELEMENTI di una collezione NUOVA (stessa semantica del DIM:
' `new BinaryHeap(elems)` / multiset / trie inizializzati), non un'APPEND sulla
' collezione già esistente e non un array nativo. Dopo H = [9,5,2] la radice è 2
' (non 1 della collezione precedente).
'
' GAMBE:
'   IL        VERDE (oracolo: la gamba C# sostituisce la collezione).
'   C99Native VERDE dal 2026-10-26: il pre-passo di registrazione riclassificava la var
'             già dichiarata (coll_handle) come array nativo e la riassegnazione finiva in
'             emit_arr_literal_assign (`gmb_array_resize(&H, &H_len, …)` su un gmb_val_t,
'             H_len mai dichiarato → C non compilava).
'   P-Code    APERTO: la riassegnazione sostituisce con una LISTA generica → la var perde
'             il tipo dichiarato (Peek/StartsWith → nil, Count(item) → totale).
'   VM/C      APERTO: il letterale fa APPEND sulla collezione esistente (H resta con la
'             radice 1, B.Count() diventa 5 invece di 3).
IMPORT COLLECTIONS
SUB Main()
  ' HEAP (14) — letterale di riassegnazione = nuova collezione, radice = minimo.
  DIM H AS HEAP<INTEGER> = [4, 1, 7, 3]
  PRINT H.Peek()
  H = [9, 5, 2]
  PRINT H.Peek()
  H.Add(1)
  PRINT H.Peek()
  ' MAXHEAP (27) — radice = massimo.
  DIM X AS MAXHEAP<INTEGER> = [1, 2]
  X = [3, 8]
  PRINT X.Peek()
  ' BAG (15) — multiset: Count() = totale, Count(item) = molteplicità, dopo la sostituzione.
  DIM B AS BAG<STRING> = ["a", "b"]
  B = ["x", "y", "x"]
  PRINT B.Count()
  PRINT B.Count("x")
  ' TRIE (11) — StartsWith lavora sul prefisso della collezione sostituita.
  DIM TR AS TRIE = ["car"]
  TR = ["dog", "dot"]
  PRINT TR.StartsWith("do")
END SUB
