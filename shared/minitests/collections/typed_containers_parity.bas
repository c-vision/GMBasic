' EXPECT: 4|["a",10]|["a","c","d"]|[20,"venti"]|[5,"cinque"]|[10,"dieci"]|[{"Key":"a","Value":true},{"Key":"b","Value":false},{"Key":"c","Value":false}]|1|1|2|9|5|3|3|2|{"a":2,"b":1,"c":1}|["a","b","c"]|-1|["car","cart","cat","catalog"]
' Contenitori TIPIZZATI — metodi che sulla gamba C erano NIL muto / divergenti (chiusi il 2026-10-10).
' L'EXPECT è il CANONE documentato dai commenti del sample (CollectionsModern.bas) e prodotto dalla
' gamba **P-Code**, l'unica corretta su TUTTE queste righe.
'
' GAMBE: VERDE su TUTTE — P-Code (canone), IL, VM/C e C99Native (dal 2026-10-26).
'   RISOLTO 2026-10-26 sulla gamba IL (emitter C#: verde):
'     Min/Max/ExtractMin/ExtractMax su TREE/AVL/REDBLACK = COPPIA [chiave,valore] (prima il solo
'     VALORE, e su AVLTREE<INTEGER,STRING>Max() lanciava ArgumentException "Object must be of type
'     String" per il probe __httpRequest sul SortedDictionary); REDBLACK.ToArray serializza
'     {"Key":…,"Value":…} (KeyValuePair ha Key/Value come PROPRIETÀ, non campi → usciva "{}");
'     HEAP/MAXHEAP sono BinaryHeap<T> con heapify (Peek/Extract = RADICE); BAG è un multiset
'     (Count() = molteplicità TOTALE, Remove toglie UNA istanza).
'   RISOLTO 2026-10-26 anche sulla gamba VM/C (era il dispatch generico delle collezioni):
'     Min/Max/Extract* = coppia [k,v] (gmb_coll_kv_entry_*), Keys su COLLECTION non veniva
'     consegnata (gmb_bf_630 faceva fallthrough nel reset → "null"), ToArray dei contenitori
'     chiave→valore = dict {Key,Value} (gmb_bf_632 usava gmb_coll_to_array → solo VALORI),
'     BAG.Count(item) veniva azzerato dopo il conteggio, BAG.Remove toglieva la PRIMA istanza
'     (ToJson/UniqueItems in ordine sbagliato), B.ToJson() = JSON.STRINGIFY(collection) usciva
'     "null" (gmb_vm_json_stringify non conosceva il multiset).
'   RISOLTO 2026-10-26 anche sulla gamba C99Native (era C generato che NON compilava: 18 errori):
'     l'emitter risolveva il metodo sulla GRAFIA e trattava HEAP/MAXHEAP come array tipizzato
'     (`H.Add(2)` → gmb_array_resize con `H_len` mai dichiarato; `H.Peek()` →
'     `gmb_list_first(H).v_double` passato a gmb_print_val) e BAG/TRIE come liste
'     (`B.Count("a")` → gmb_val_index + "Count"; TR.FindAll → gmb_list_find_all).
'     Ora il dispatch è per TokenKind (gmb_member_tok, tabella canonica) e i quattro tipi sono
'     COLLEZIONI del runtime condiviso con handle (gmb_coll_heap_peek/extract, gmb_coll_add,
'     gmb_coll_bag_*, gmb_coll_trie_*): DIM/letterale → gmb_collection_new(type_code, …),
'     dichiarazione gmb_val_t, e JSON.STRINGIFY delega allo stesso dispatch.
'   CACHE.Keys()/Values()       NON asseriti: l'oracolo è un `ConcurrentDictionary` → ordine NON
'                               deterministico fra processi (4 run: si alterna).
' Riproduzione: la gamba C con `tools/parity4 samples/Collections/CollectionsModern.bas`
' (o il gate `--file=`) e le altre con
'   dotnet tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll --engine=il   minitests/collections/typed_containers_parity.bas
'   dotnet tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll --engine=pcode minitests/collections/typed_containers_parity.bas
'   dotnet tools/runners/test_vmrun/bin/Debug/net10.0/test_vmrun.dll               minitests/collections/typed_containers_parity.bas
IMPORT COLLECTIONS
SUB Main()
  ' TREE (7) — Min() = COPPIA [chiave,valore]; Keys() = chiavi ordinate; Remove(arg) = per CHIAVE.
  DIM T AS TREE<STRING, INTEGER> = { "b": 20, "a": 10, "c": 30 }
  T.Add("d", 40)
  PRINT T.Count()
  PRINT JSON.Stringify(T.Min())
  T.Remove("b")
  PRINT JSON.Stringify(T.Keys())
  ' AVLTREE (8) — Max()/Min() coppia; ExtractMin() rimuove e restituisce la coppia.
  DIM A AS AVLTREE<INTEGER, STRING> = NEW AVLTREE<INTEGER, STRING>
  A.Add(10, "dieci") : A.Add(20, "venti") : A.Add(5, "cinque")
  PRINT JSON.Stringify(A.Max())
  PRINT JSON.Stringify(A.ExtractMin())
  PRINT JSON.Stringify(A.Min())
  ' REDBLACK (9) — ToArray() = array di KeyValuePair {Key,Value}.
  DIM R AS REDBLACK<STRING, BOOLEAN> = { "b": FALSE, "a": TRUE, "c": TRUE }
  R["c"] = FALSE
  PRINT JSON.Stringify(R.ToArray())
  ' HEAP (14) / MAXHEAP (27) — il letterale `= [4,1,7,3]` sono ELEMENTI; Peek/Extract = RADICE.
  DIM H AS HEAP<INTEGER> = [4, 1, 7, 3] : H.Add(2)
  PRINT H.Peek()
  PRINT H.Extract()
  PRINT H.Peek()
  DIM X AS MAXHEAP<INTEGER> = [4, 1, 7, 3] : X.Add(9)
  PRINT X.Peek()
  ' BAG (15) — multiset: Count() = totale, UniqueCount(), Count(item) = molteplicita',
  ' Remove toglie UNA istanza (l'ultima), ToJson() = {elemento:molteplicita'}, UniqueItems().
  DIM B AS BAG<STRING> = ["a", "b", "a", "c", "a"]
  PRINT B.Count()
  PRINT B.UniqueCount()
  PRINT B.Count("a")
  B.Remove("a")
  PRINT B.Count("a")
  PRINT B.ToJson()
  PRINT JSON.Stringify(B.UniqueItems())
  ' TRIE (11) — StartsWith(prefix) / FindAll(prefix).
  DIM TR AS TRIE = ["car", "cat", "cart"]
  TR.Add("catalog")
  PRINT TR.StartsWith("car")
  PRINT JSON.Stringify(TR.FindAll("ca"))
END SUB
