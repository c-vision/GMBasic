# II.9 — Array, collezioni e stringhe

## Cosa ottieni da questo capitolo

Le tre strutture che contengono dati, con le regole che si sbagliano davvero: la differenza fra
`()` e `[]`, l'indice 0-based, `LEN` contro `LENB`, e quando un letterale è una lista e quando è
una matrice.

## 1. Array: dichiarare

```basic
DIM M(3, 3) AS DOUBLE       ' matrice 4x4: indici 0..3
DIM A(0 TO 5) AS INTEGER    ' 6 elementi, 0..5
DIM B() AS STRING           ' dinamico: cresce con AADD/REDIM
```

`DIM A(n)` dichiara **`n+1`** elementi, da `0` a `n`: è la convenzione storica del BASIC e la
causa numero uno degli "off by one". Un array numerico non inizializzato vale `0`, uno `STRING`
vale `""`.

### 1.1 Lower bound espliciti e array N-D

La forma completa del `DIM` è `DIM nome(lo TO hi [, lo2 TO hi2, ...]) [AS tipo]`. Ogni asse ha
il proprio lower bound e il numero di dimensioni è libero (fino a 8):

```basic
DIM m(1 TO 3, 1 TO 4) AS DOUBLE          ' 3 righe x 4 colonne, indici 1..3 e 1..4
DIM q(-1 TO 1, 0 TO 2, 1 TO 2) AS DOUBLE ' 3-D, lower misti e negativi
DIM serie(1 TO n) AS DOUBLE              ' n calcolato a runtime
```

Regole:

- Il **lower** (`lo`) è una costante intera, eventualmente negativa. L'**upper** (`hi`) può
  essere un'espressione calcolata a runtime: la lunghezza dell'asse è `hi - lo + 1`.
- Un lower non costante è un errore di compilazione su tutti i backend (il backend IL memorizza
  i lower bound in una tabella compile-time).
- Gli indici scritti nel programma sono quelli **utente**: su `DIM m(1 TO 3, 1 TO 4)`, il primo
  elemento è `m(1,1)` e l'ultimo `m(3,4)`; `m(0,1)` è fuori dai limiti.
- Il layout interno è **denso row-major**: l'ultimo asse varia più velocemente. Non è osservabile
  dal programma, che usa sempre gli indici utente, ma spiega l'ordine dei letterali annidati.
- `DIM a(n)` è la forma abbreviata di `DIM a(0 TO n)`.
- Un array `STRING` non inizializzato ha elementi `""`; un array di UDT ha istanze con i campi
  al valore di default.

## 2. Letterali: `{ }` e `[ ]`

```basic
DIM a() AS DOUBLE = [1.5, 2.5, 3.5]        ' dimensione dedotta
DIM b(4) AS INTEGER = [2, 10, 1, 4, 0]     ' dimensione dichiarata
DIM m(1, 1) AS INTEGER = [[1, 2], [3, 4]]  ' matrice, letterale annidato
DIM m2(1, 1) AS INTEGER = {{1, 2}, {3, 4}} ' equivalente con graffe
```

- In un `DIM` che dichiara un array, graffe e quadre sono equivalenti.
- Il letterale 2-D deve essere **rettangolare**: tutte le righe della stessa lunghezza.
- Se il letterale ha **più** elementi della dimensione dichiarata, è errore di compilazione.
- **Fuori** da un `DIM` di array, `[...]` è una **lista**, non una matrice:
  `DIM lst = [3, 1, 4]` crea una lista di tre elementi.

## 3. Leggere, scrivere, misurare

```basic
A(0) = 10          ' scrittura con parentesi
PRINT A(0)
PRINT A[0]         ' lettura: anche con quadre
PRINT UBOUND(A)    ' ultimo indice
PRINT LBOUND(A)    ' primo indice
PRINT LEN(A)       ' numero di elementi
```

### 3.1 Limiti per asse: `LBOUND` e `UBOUND`

`LBOUND(A[, dim])` e `UBOUND(A[, dim])` restituiscono il primo e l'ultimo indice valido
dell'asse `dim`.

| Parametro | Tipo | Descrizione |
|---|---|---|
| `A` | array | array nativo `DIM a(...)`. Su una lista o un dict `UBOUND` restituisce `Count-1` e `LBOUND` restituisce `0`. |
| `dim` | `LONG`, opzionale | asse **1-based**: `1` è la prima dimensione, `2` la seconda, e così via. Default `1`. Può essere un'espressione o una variabile: l'asse viene selezionato a runtime. |
| valore | `LONG` | `LBOUND` = lower dell'asse; `UBOUND` = upper dell'asse. |

- `UBOUND(A)` senza `dim` restituisce l'upper del **primo** asse, non il numero di elementi:
  su `DIM a(3, 7)` vale `3`. Per il conteggio si usa `LEN(A)`.
- Con un lower esplicito `DIM a(lo TO hi)`, `LBOUND(a)=lo` e `UBOUND(a)=hi`: la lunghezza è
  `hi - lo + 1`, non `hi`.
- Ogni asse è indipendente: `DIM m(0 TO 1, 1 TO 3)` ha `LBOUND(m,1)=0`, `UBOUND(m,1)=1`,
  `LBOUND(m,2)=1`, `UBOUND(m,2)=3`.
- Un `dim` fuori dal rank dell'array è un errore: viene rilevato in compilazione su IL e
  C99Native, a runtime su PCodeVM e VM/C.
- `LEN(A)` su un array restituisce il numero complessivo di elementi; su un array passato come
  parametro a una procedura la dimensione viaggia con l'array.
- **Array passati a una procedura**: i lower bound si propagano al parametro `a()` delle
  procedure top-level. Nel callee `LBOUND`/`UBOUND`, gli indici e `FOR EACH` usano gli stessi
  limiti del chiamante; le modifiche restano visibili. Vale anche per BYREF, per un array
  passato come *function value* e per una lambda. MiniTest
  `shared/minitests/arrays/dim_to_params.bas`.
- **Metodi e costruttori**: le stesse regole valgono per `v.M(a)` / `CALL v.M(a)`,
  `NEW Cls(a)`, gli override virtuali e `BASE.NEW(a)`; il callee vede i limiti del chiamante.
  MiniTest `shared/minitests/arrays/dim_to_class_params.bas`.

**Slicing.** `a[start:end]` produce una nuova lista con gli elementi da `start` incluso a `end`
escluso:

| Con `a = [1,2,3,4]` | Risultato |
|---|---|
| `a[1:3]` | `[2,3]` |
| `a[:3]` | `[1,2,3]` |
| `a[1:]` | `[2,3,4]` |
| `a[2:9]` | `[3,4]` (fine clampata) |

## 4. Comprensioni: costruire in una riga

```basic
DIM nomi = {"Charlie", "Alice", "Bob"}
DIM corti = [ n FOR n IN nomi IF LEN(n) <= 5 ]      ' lista filtrata
DIM lungh = { n : LEN(n) FOR n IN nomi }            ' dict
DIM unici = { n FOR n IN {"a", "b", "a"} }          ' set (dedup)
DIM ord   = [ n FOR n IN nomi ORDER BY n DESC ]     ' ordinata
```

| Forma | Produce |
|---|---|
| `[ out FOR v IN src [IF c] ]` | lista |
| `{ k : v FOR v IN src [IF c] }` | dict |
| `{ out FOR v IN src [IF c] }` | set |

Più clausole `FOR` annidano i cicli (prodotto cartesiano). `ORDER BY` vale solo per le liste:
dict e set non sono ordinati.

## 5. Funzioni e comandi `ARRAYS`

| Funzione | Cosa fa | Non muta l'originale |
|---|---|---|
| `ASORT(a)` / `AREVERSE(a)` / `AUNIQUE(a)` | ordina, inverte, deduplica | sì |
| `ACLONE(a)` / `APAD(a, n)` / `ACHUNK(a, n)` / `AFLAT(a)` / `AZIP(a, b)` | copia, estende, raggruppa, appiattisce, accoppia | sì |
| `ACOUNT` `AMAX` `AMIN` `ASUM` | conta, massimo, minimo, somma | — |
| `ASCAN(a, v)` / `ABINARYSEARCH(ordinato, v)` | indice o `-1` | — |
| `AJOIN(a, sep)` / `ATOKENS(s, sep)` | array ↔ stringa | — |
| `AMAP` `AFILTER` `AREDUCE` `AEVAL` | proiezione, filtro, riduzione, visita | sì |

I comandi di mutazione restano quelli storici, con **indici 0-based**:

```basic
AADD a, 9          ' append
AINS a, 1, 7       ' inserisce in posizione 1
ADEL a, 2          ' rimuove l'elemento in posizione 2
AFILL a, 0         ' riempie
ACOPY src, dst     ' copia parziale o totale: la lunghezza di dst non cambia
ASORT a, , "D"     ' ordina in-place, decrescente
```

`ASORT` con comparatore accetta un code block, una lambda o il nome di una funzione; il
comparatore deve restituire un valore **negativo** quando il primo elemento precede il secondo.

## 6. Collezioni

| Tipo | Dichiarazione | Caratteristica |
|---|---|---|
| lista | `DIM l = [1, 2, 3]` oppure `DIM l AS LIST OF DOUBLE` | ordine di inserimento, accesso per indice |
| dict | `DIM d AS DICT OF STRING, DOUBLE` oppure `DICT<STRING, DOUBLE>` | chiave → valore |
| set | `DIM s = {1, 2, 2}` | nessun duplicato |

Le due grafie (`OF` storica, `<...>` moderna) sono equivalenti. Le proprietà utili:

```basic
PRINT l.Count
PRINT d.Keys()
PRINT l.ToJson()
PRINT d["Apple"]        ' lettura per chiave
```

Per il codice nuovo le collezioni sono spesso più comode degli array: crescono da sole e il dict
evita le ricerche lineari.

## 7. Stringhe

### `LEN` è in caratteri, `LENB` in byte

| Stringa | `LEN` | `LENB` |
|---|---:|---:|
| `"abc"` | 3 | 3 |
| `"é"` | 1 | 2 |
| `"─"` | 1 | 3 |
| `"😀"` | 1 | 4 |

- `LEN` serve a ciò che l'utente **vede**: larghezza di una colonna, numero di lettere.
- `LENB` serve ai **limiti fisici**: `Content-Length` HTTP, scrittura su file, capacità di un
  campo, payload di un socket.

`LEFT$`, `RIGHT$`, `MID$`, `INSTR` lavorano in **caratteri**: non spezzano mai un carattere
multibyte.

### Il resto della cassetta

| Funzione | Uso |
|---|---|
| `UPPER` / `LOWER` / `TRIM` / `LTRIM` / `RTRIM` | normalizzazione |
| `REPLACE(s, a, b)` | sostituisce **tutte** le occorrenze; con `a` vuoto inserisce `b` a ogni posizione |
| `PADL` / `PADR` / `PADC` | allineamento a larghezza fissa (non tronca: per troncare `LEFT`) |
| `SPLIT` / `JOIN` | stringa ↔ array |
| `STR$` / `VAL` | numero ↔ testo (`STR$` aggiunge lo spazio davanti ai positivi) |
| `ISEMAIL` `ISURL` `ISLENGTH` | validatori, ritornano `-1`/`0` |

## 8. Esempi eseguiti

**Array e matrici** — `shared/samples/Arrays/ArraysDim.bas`:

```
Lettura NUMERI(3):  40  (Atteso: 40)
Lettura MESI$(INDICE): FEB (Atteso: FEB)
Lettura MAT(2, 3):  23  (Atteso: 23)
Lettura MAT(X, Y):  12  (Atteso: 12)
Lettura DATI(2):  300  (Accesso con tonde - Atteso: 300)
Lettura MAT[X, Y]:  12  (Accesso a matrice con quadre - Atteso: 12)
```

Le ultime due righe sono il punto del capitolo: la stessa matrice si legge con `MAT(X, Y)` e con
`MAT[X, Y]`, e per la **scrittura** si usa la forma con le tonde.

**Collezioni** — `shared/samples/Collections/CollectionsBasics.bas`:

```
Modern List (.Count = 3 ): one, two
Assigned (1) with []: TWO_M
List.ToJson(): ["one","TWO_M","three"]
Legacy Dict Keys (.Count = 2 ): KeyA= 1.05 , KeyB= 2.1
Modern Dict Keys (.Count = 3 ): Apple= 1.3 , Orange= 2.5
Dict.Keys(): ["Apple","Banana","Orange"]
```

Le due grafie convivono (`LIST OF` e `LIST<...>`), `Count` conta gli elementi, `[]` in scrittura
aggiorna un elemento esistente e `ToJson()` serializza la collezione.

**Stringhe** — `shared/samples/Strings/StringsUtf8Len.bas`:

```
TESTO        LEN  LENB  PRIMO CARATTERE
ascii        3    3     [a]
accento      1    2     [é]
box-drawing  1    3     [─]
emoji        1    4     [😀]

roundtrip: 😀  (due caratteri: emoji + x)
instr emoji:  2
```

La tabella è la misura del §7: `LEN` conta i caratteri, `LENB` i byte. `instr emoji: 2` mostra che
`INSTR` restituisce la posizione in **caratteri** (la emoji è il primo, la `x` il secondo), non in
byte — che sarebbero 5.

**Matrici e tensori con lower bound** — `shared/samples/Arrays/MultidimBounds.bas`:

```basic
DIM temp(1 TO 7) AS DOUBLE
DIM griglia(1 TO righe, 1 TO colonne) AS DOUBLE
DIM campo(1 TO nx, 1 TO ny, 1 TO nz) AS DOUBLE
DIM hyper(1 TO 2, 1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE
DIM kernel(1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE = {{{1,2},{3,4}},{{5,6},{7,8}}}
```

```
serie: lb= 1  ub= 7
       somma= 70  min= 4  max= 16
griglia: lb= 1 , 1  ub= 3 , 4
         angoli= 11 , 34
campo 3-D: ub= 3 , 4 , 2
           totale= 156  [1,1,1]= 2  [3,4,2]= 14
ipercubo: ub4= 2  primo= 15  ultimo= 30
          totale= 360
kernel: [1,1,1]= 1  [2,2,2]= 8
offset: lb=-1 ,-1  ub= 1 , 1
        diag=-1 , 0 , 1
```

L'esempio usa `UBOUND(asse)` come limite dei cicli invece di costanti scritte a mano, riempie
gli array con gli **indici utente** (non con offset) e mostra che `FOR EACH` su un array 1-D
parte da `LBOUND`. Il kernel 3-D è un letterale annidato: l'ordine è row-major e il programma lo
legge con `kernel(1,1,1) … kernel(2,2,2)`.

## 9. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| l'ultimo elemento "non esiste" | `DIM A(5)` dichiara 0..5, non 0..4 | usa `UBOUND`, non la costante scritta a mano |
| scrittura con `a[i] = v` non compila | la scrittura vuole le tonde | `a(i) = v`; le quadre per leggere |
| `AINS`/`ADEL` spostano di uno | indici 0-based | ricorda che il primo elemento è `0` |
| un letterale 2-D non compila | righe di lunghezze diverse | rendilo rettangolare |
| `DIM m(lo TO hi)` non compila | il lower `lo` è una variabile | usa una costante come lower, l'upper può essere runtime |
| `UBOUND(m, 3)` su una matrice 2-D fallisce | `dim` fuori dal rank | chiedi solo gli assi `1..2`, o `LEN(m)` per il conteggio |
| `m(0,1)` "fuori dai limiti" | la matrice è dichiarata `1 TO r, 1 TO c` | usa gli indici utente `1..r` e `1..c`; non sottrarre 1 |
| `DIM x = [[1,2],[3,4]]` non è una matrice | fuori da un DIM di array, `[...]` è una lista | dichiara `DIM m(1,1)`, o usa le graffe |
| un campo lungo 10 con `"AB"` | `STRING * 10` padda | `TRIM` in lettura |
| tronco un'emoji a metà | si è lavorato in byte | usa `LEN`/`LEFT$`, non `LENB` |

## 10. Dove andare adesso

- **II.5** — tipi degli elementi e scope.
- **II.10** — array in stile Clipper (`AADD`, `AFILL`) e macro.
- **V.27** — matrici e algebra lineare: operazioni su `MATRIX`.
- **IV.22** — leggere un file riga per riga e costruire strutture.













<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **ARRAYS.AADD(x1, x2)** — `PRINT "--- 1. Test AADD (Aggiunta) ---"` (shared/samples/Clipper/ClipperArrays.bas:79)
- **ARRAYS.ABINARYSEARCH(x1, x2)** — `DIM b = ARRAYS.ABINARYSEARCH(s, 2)` (shared/minitests/arrays/arrays2.bas:14)
- **ARRAYS.ACHUNK(x1, x2)** — `DIM c = ARRAYS.ACHUNK(b, 2)` (shared/samples/Arrays/ArraysMiniTest.bas:12)
- **ARRAYS.ACLONE(x1)** — `DIM c = ARRAYS.ACLONE(a)` (shared/minitests/arrays/arrays6.bas:9)
- **ARRAYS.ACOPY(x1, x2)** — `PRINT "--- 4. Test ACOPY (Copia) ---"` (shared/samples/Clipper/ClipperArrays.bas:132)
- **ARRAYS.ACOUNT(x1, x2)** — `DIM cnt = ARRAYS.ACOUNT(arr, FN(x) => x > 5)` (shared/samples/Arrays/ArraysFluent.bas:46)
- **ARRAYS.ADEL(x1, x2)** — `PRINT "--- 2. Test AINS / ADEL (Inserimento/Cancellazione) ---"` (shared/samples/Clipper/ClipperArrays.bas:96)
- **ARRAYS.AEVAL(x1, x2)** — `ARRAYS.AEVAL(a, SUB(x) { IF x = 2 THEN PRINT "aeval-ok" })` (shared/minitests/arrays/aeval.bas:5)
- **ARRAYS.AFILL(x1, x2)** — `PRINT "--- 3. Test AFILL (Riempimento) ---"` (shared/samples/Clipper/ClipperArrays.bas:119)
- **ARRAYS.AFILTER(x1, x2)** — `DIM even = ARRAYS.AFILTER(arr, FN(x) => x MOD 2 = 0)` (shared/samples/Arrays/ArraysFluent.bas:15)
- **ARRAYS.AFLAT(x1)** — `DIM f = ARRAYS.AFLAT(nested)` (shared/samples/Arrays/ArraysMiniTest.bas:29)
- **ARRAYS.AINS(x1, x2)** — `AINS a, 1` (shared/samples/Arrays/ArraysInsGaps.bas:10)
- **ARRAYS.AJOIN(x1)** — `names$ = ARRAYS.AJOIN(AI.PLUGINSLIST(), ",")` (shared/samples/Ai/AiPlugins.bas:24)
- **ARRAYS.AMAP(x1, x2)** — `DIM doubled = ARRAYS.AMAP(arr, FN(x) => x * 2)` (shared/samples/Arrays/ArraysFluent.bas:19)
- **ARRAYS.AMAX(x1)** — `PRINT "flat max: "; ARRAYS.AMAX(flat); " flat min: "; ARRAYS.AMIN(flat)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:30)
- **ARRAYS.AMIN(x1)** — `PRINT "flat max: "; ARRAYS.AMAX(flat); " flat min: "; ARRAYS.AMIN(flat)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:30)
- **ARRAYS.APAD(x1, x2)** — `DIM p = ARRAYS.APAD(data, 6, 0.0)` (shared/minitests/arrays/arrays8.bas:5)
- **ARRAYS.AREDUCE(x1, x2)** — `DIM sum = ARRAYS.AREDUCE(arr, FN(acc, x) => acc + x, 0)` (shared/samples/Arrays/ArraysFluent.bas:23)
- **ARRAYS.AREVERSE(x1)** — `DIM rev = ARRAYS.AREVERSE(arr)` (shared/samples/Arrays/ArraysFluent.bas:37)
- **ARRAYS.ASCAN(x1, x2)** — `DIM sc = ARRAYS.ASCAN(s, 3)` (shared/minitests/arrays/arrays3.bas:10)
- **ARRAYS.ASHUFFLE(x1)** — `PRINT "--- 14. Test AREVERSE e ASHUFFLE (Ordinamento inverso e casuale) ---"` (shared/samples/Clipper/ClipperArrays.bas:241)
- **ARRAYS.ASORT(x1)** — `DIM chiavi = ARRAYS.ASORT(d.Keys())` (shared/samples/Arrays/ArraysComprehensionFull.bas:132)
- **ARRAYS.ASUM(x1)** — `PRINT "sum="; ARRAYS.ASUM(data)` (shared/minitests/arrays/arrays7.bas:10)
- **ARRAYS.ATOKENS(x1)** — `DIM t = ARRAYS.ATOKENS("a,,b;c", ",;")` (shared/minitests/arrays/atokens.bas:3)
- **ARRAYS.AUNIQUE(x1)** — `DIM uniq = ARRAYS.AUNIQUE(withDupes)` (shared/samples/Arrays/ArraysFluent.bas:42)
- **ARRAYS.AZIP(x1, x2)** — `DIM z = ARRAYS.AZIP(a1, a2)` (shared/samples/Arrays/ArraysMiniTest.bas:43)
- **STRINGS.ASC(x1)** — `140 PRINT "2. TEST ASC(stringa)"` (shared/samples/Strings/StringsExamples.bas:71)
- **STRINGS.CAMEL(x1)** — `790 PRINT "CAMEL(T$) = '"; CAMEL(T$); "'"; " (Atteso: ciaoMondoFromBasic)"` (shared/samples/Strings/StringsExamples.bas:136)
- **STRINGS.CHR(x1)** — `DIM q$ AS STRING = "{" + CHR(34) + "query" + CHR(34) + ":" + CHR(34) + query + CHR(34) + "}"` (shared/samples/Connectors/AgentConnectorAutoDiscovery.bas:9)
- **STRINGS.CONTAINS(x1, x2)** — `1520 PRINT "CONTAINS(S1$, 'Mondo') = "; CONTAINS(S1$, "Mondo"); " (Atteso: -1)"` (shared/samples/Strings/StringsExamples.bas:214)
- **STRINGS.DUP(x1, x2)** — `PRINT DUP(64, "-")` (shared/samples/Strings/Utf8ReportLayout.bas:38)
- **STRINGS.ENDSWITH(x1, x2)** — `1480 REM --- TEST: CONTAINS, STARTSWITH, ENDSWITH (Globali) ---` (shared/samples/Strings/StringsExamples.bas:210)
- **STRINGS.FORMAT(x1, x2)** — `40 PRINT "Numero:"; N; " | Formato: '000000' | Risultato: '"; FORMAT(N, "000000"); "'"` (shared/samples/Strings/StringsFormat.bas:56)
- **STRINGS.HTMLENCODE(x1)** — `DIM safe$ = STRINGS.HTMLENCODE(attacker$)` (shared/samples/Security/XssSafeOutput.bas:15)
- **STRINGS.ISCREDITCARD(x1)** — `IF STRINGS.ISCREDITCARD("4111111111111111") THEN PRINT "CC OK" ELSE PRINT "CC FAIL"` (shared/samples/Security/ValidationDemo.bas:20)
- **STRINGS.ISEMAIL(x1)** — `IF STRINGS.ISEMAIL("a@b.co") THEN PRINT "EMAIL OK" ELSE PRINT "EMAIL FAIL"` (shared/samples/Security/ValidationDemo.bas:15)
- **STRINGS.ISLENGTH(x1, x2, x3)** — `IF STRINGS.ISLENGTH("abc", 2, 5) THEN PRINT "LEN OK" ELSE PRINT "LEN FAIL"` (shared/samples/Security/ValidationDemo.bas:23)
- **STRINGS.ISREQUIRED(x1)** — `IF STRINGS.ISREQUIRED("abc") THEN PRINT "REQ OK" ELSE PRINT "REQ FAIL"` (shared/samples/Security/ValidationDemo.bas:12)
- **STRINGS.ISURL(x1)** — `IF STRINGS.ISURL("https://example.com") THEN PRINT "URL OK" ELSE PRINT "URL FAIL"` (shared/samples/Security/ValidationDemo.bas:18)
- **STRINGS.INSTR(x1, x2)** — `PRINT "disabled="; INSTR(AI.CONNECTORSTATUS("demo-conn"), "false")` (shared/samples/Connectors/ConnectorFullCoverage.bas:40)
- **STRINGS.INSTR(x1, x2, x3)** — `PRINT "disabled="; INSTR(AI.CONNECTORSTATUS("demo-conn"), "false")` (shared/samples/Connectors/ConnectorFullCoverage.bas:40)
- **STRINGS.JSENCODE(x1)** — `PRINT "quote: "; STRINGS.JSENCODE(jsQuote$)` (shared/samples/Security/EncodingDemo.bas:32)
- **STRINGS.JOIN(x1, x2)** — `190 PRINT "  Parti:"; JOIN(parts$, " | ")` (shared/samples/Strings/StringsRegex.bas:85)
- **STRINGS.KEBAB(x1)** — `810 PRINT "KEBAB(T$) = '"; KEBAB(T$); "'"; " (Atteso: ciao-mondo-from-basic-v10)"` (shared/samples/Strings/StringsExamples.bas:138)
- **STRINGS.LCASE(x1)** — `DIM nota = STRINGS.LCASE("NORD-OVEST")` (shared/samples/Math/MathAtnVectorAngle.bas:69)
- **STRINGS.LTRIM(x1)** — `910 PRINT "LTRIM(SP$) = '"; LTRIM(SP$); "'"` (shared/samples/Strings/StringsExamples.bas:148)
- **STRINGS.LEFT(x1, x2)** — `PRINT "gmail.search: "; LEFT(mail$, 200)` (shared/samples/Connectors/ConnectorComposability.bas:21)
- **STRINGS.LEN(x1)** — `PRINT STRINGS.LEN("abc")` (shared/minitests/strings/str_len_chars.bas:9)
- **STRINGS.LENB(x1)** — `PRINT STRINGS.LENB("😀")` (shared/minitests/strings/str_lenb.bas:9)
- **STRINGS.LOWER(x1)** — `760 PRINT "LOWER(T$) = '"; LOWER(T$); "'"` (shared/samples/Strings/StringsExamples.bas:133)
- **STRINGS.MID(x1, x2)** — `450 PRINT "MID(S$, 8, 4) = '"; MID(S$, 8, 4); "'"; " (Atteso: 'MAZI')"` (shared/samples/Strings/StringsExamples.bas:102)
- **STRINGS.PADC(x1, x2)** — `1140 PRINT "PADC(PAD$, 10, ""-"") = '"; PADC(PAD$, 10, "-"); "'"; " (Atteso: '---CIAO---')"` (shared/samples/Strings/StringsExamples.bas:171)
- **STRINGS.PADL(x1, x2)** — `1120 PRINT "PADL(PAD$, 10, ""-"") = '"; PADL(PAD$, 10, "-"); "'"; " (Atteso: '------CIAO')"` (shared/samples/Strings/StringsExamples.bas:169)
- **STRINGS.PADR(x1, x2)** — `1130 PRINT "PADR(PAD$, 10, ""-"") = '"; PADR(PAD$, 10, "-"); "'"; " (Atteso: 'CIAO------')"` (shared/samples/Strings/StringsExamples.bas:170)
- **STRINGS.PASCAL(x1)** — `780 PRINT "PASCAL(T$) = '"; PASCAL(T$); "'"; " (Atteso: CiaoMondoFromBasic)"` (shared/samples/Strings/StringsExamples.bas:135)
- **STRINGS.PETSCII(x1)** — `PRINT "clr="; LEN(STRINGS.PETSCII("{clr}Hello{home}"))` (shared/minitests/strings/petscii.bas:4)
- **STRINGS.RTRIM(x1)** — `920 PRINT "RTRIM(SP$) = '"; RTRIM(SP$); "'"` (shared/samples/Strings/StringsExamples.bas:149)
- **STRINGS.REMOVE(x1, x2)** — `180 sb6.Remove(3, 4)` (shared/samples/Strings/StringsStringBuilder.bas:83)
- **STRINGS.REPEAT(x1, x2)** — `980 REM --- TEST: REPLACE, REMOVE, REPEAT (Namespace STRINGS) ---` (shared/samples/Strings/StringsExamples.bas:155)
- **STRINGS.REPLACE(x1, x2, x3)** — `PRINT "replace: "; RDD.REPLACE("CURRENT", "NOME", "X", TRUE)` (shared/samples/Databases/RddCompleteCoverage.bas:54)
- **STRINGS.REVERSE(x1)** — `860 REM --- TEST: TRIM, LTRIM, RTRIM, REVERSE (Namespace STRINGS) ---` (shared/samples/Strings/StringsExamples.bas:143)
- **STRINGS.RIGHT(x1, x2)** — `440 PRINT "RIGHT(S$, 5) = '"; RIGHT(S$, 5); "'"; " (Atteso: 'ZIONE')"` (shared/samples/Strings/StringsExamples.bas:101)
- **STRINGS.SEARCH(x1, x2)** — `PRINT "gmail.search (ciò che farebbe l'agente): "; LEFT(r1$, 200)` (shared/samples/Connectors/AgentConnectorAutoDiscovery.bas:38)
- **STRINGS.SENTENCE(x1)** — `830 PRINT "SENTENCE(T$) = '"; SENTENCE(T$); "'"; " (Atteso: Ciao mondo from basic v10)"` (shared/samples/Strings/StringsExamples.bas:140)
- **STRINGS.SNAKE(x1)** — `800 PRINT "SNAKE(T$) = '"; SNAKE(T$); "'"; " (Atteso: ciao_mondo_from_basic_v10)"` (shared/samples/Strings/StringsExamples.bas:137)
- **STRINGS.SPACE(x1)** — `600 PRINT "SPACE(15) = '"; SPACE(15); "'"; " (Atteso: 15 spazi)"` (shared/samples/Strings/StringsExamples.bas:117)
- **STRINGS.SPLIT(x1)** — `parts = STRINGS.SPLIT(SampleData(id), ",")` (shared/samples/Ai/McpCapabilityServer.bas:46)
- **STRINGS.STARTSWITH(x1, x2)** — `1550 PRINT "STARTSWITH(S1$, 'Ciao') = "; STARTSWITH(S1$, "Ciao"); " (Atteso: -1)"` (shared/samples/Strings/StringsExamples.bas:217)
- **STRINGS.STR(x1)** — `PRINT "  SPEED SELECT (Legacy): OK. Risultati: " + STR(UBOUND(ris) + 1)` (shared/samples/Databases/DbSpeedFullTests.bas:262)
- **STRINGS.TITLE(x1)** — `820 PRINT "TITLE(T$) = '"; TITLE(T$); "'"; " (Atteso: Ciao Mondo From Basic)"` (shared/samples/Strings/StringsExamples.bas:139)
- **STRINGS.TRIM(x1)** — `900 PRINT "TRIM(SP$) = '"; TRIM(SP$); "'"` (shared/samples/Strings/StringsExamples.bas:147)
- **STRINGS.UCASE(x1)** — `DIM etichetta = STRINGS.UCASE("rotta-")` (shared/samples/Math/MathAtnVectorAngle.bas:68)
- **STRINGS.URLENCODE(x1)** — `DIM link$ = "https://host/path?q=" + STRINGS.URLENCODE("a b&c=d")` (shared/samples/Security/XssSafeOutput.bas:30)
- **STRINGS.UPPER(x1)** — `PRINT ((UPPER("abc")) = "ABC")` (shared/samples/Strings/StringsRelationalCompare.bas:96)
- **STRINGS.WRAP(x1, x2)** — `PRINT "WRAP(2 arg)  = ["; STRINGS.WRAP("ciao mondo lungo", 5); "]"` (shared/samples/graphics/CoordsAndTextGaps.bas:19)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Collezioni-Guida-Utente

*Fonte: `Collezioni-Guida-Utente`*


## 1. Panoramica

' Legacy "OF"
DIM a AS LIST OF STRING
' Moderna "<...>"
DIM b AS LIST<STRING>

| Famiglia | Tipi |
|---|---|
| Liste dinamiche | `LIST`, `QUEUE`, `STACK`, `HASHSET` |
| Mappe | `DICT`, `SORTED` |
| Alberi ordinati (mappe) | `TREE`, `AVLTREE`, `REDBLACK`, `BPLUSTREE` |
| Specialistiche | `TRIE`, `HEAP`, `MAXHEAP`, `BAG`, `CACHE`, `GRAPH`, `TIMELINE`, `BITREE`, `HASHRING`, `RINGBUFFER`, `SPARSESET`, `LINKEDLIST`, `DOUBLYLINKEDLIST`, `CIRCULAR` |

## 2. Sintassi comune

### 2.1 Dichiarazione

DIM nome AS LIST<INTEGER>          ' vuota
DIM nome AS LIST OF STRING = ["a","b"]
DIM m  AS DICT<STRING, DOUBLE>     ' vuota
DIM d  AS DICT OF STRING, INTEGER = { "uno": 1, "due": 2 }
DIM s  AS SORTED<STRING, INTEGER> = { b: 2, a: 1 }


DIM x AS LIST OF INTEGER = NEW LIST OF INTEGER
DIM y AS DICT OF STRING, DOUBLE = NEW DICT OF STRING, DOUBLE


DIM batch AS LIST OF Riga
batch = NEW LIST OF Riga {}       ' lista vuota tipizzata
batch.Add(r1) : batch.Add(r2)


### 2.2 Accesso

PRINT lista(0)        ' parentesi tonde
PRINT lista[0]        ' parentesi quadre (moderno)
lista(0) = "nuovo"
PRINT mappa["chiave"]
mappa("chiave") = 7


### 2.3 Iterazione

FOR EACH v IN lista       ' elementi di una lista/set/coda/pila/albero/trie
FOR EACH v$ IN parole     ' elementi stringa
FOR EACH k, v IN mappa    ' coppie chiave/valore di un dizionario
FOR EACH KEY k IN mappa   ' solo chiavi
FOR EACH VALUE v IN mappa ' solo valori
FOR EACH ch IN "abc"      ' CARATTERI di una stringa (3 iterazioni, ch = stringa di 1)
FOR EACH row IN m         ' RIGHE di una matrice 2D (row = la riga)


### 2.4 Appartenenza

IF "x" IN lista  THEN ...   ' elemento presente
IF "k" IN mappa  THEN ...   ' chiave presente
IF "abc" IN testo$ THEN ... ' sottostringa


### 2.5 Accesso "sicuro" (nil)

IF (d["chiave"]? IS NIL) THEN PRINT "assente"
v = d["chiave"]? ?? -1      ' valore di default se assente

| Operando | `IS NIL` è vero quando… |
|---|---|
| **stringa** | il riferimento è nullo — una stringa **vuota `""` NON è NIL** |
| **collection** | chiave/indice assente o fuori intervallo (accesso con `?`) |
| **numero** | il valore è il sentinella NIL — una variabile `DIM` vale `0`, quindi **non** è NIL |

s$ = "valore"      : PRINT s$ IS NIL    '  0
s$ = ""            : PRINT s$ IS NIL    '  0   (vuota, ma non NIL)
DIM n AS DOUBLE    : PRINT n IS NIL     '  0   (un DIM è 0, non NIL)
PRINT d["assente"]? IS NIL              ' -1


### 2.6 JSON

PRINT JSON.Stringify(lista)
PRINT JSON.Stringify(mappa)
PRINT JSON.Stringify(lista.Keys())    ' o mappa.Keys()


### 2.7 Stampa diretta di una collection

PRINT lista    ' [1,2,3]
PRINT mappa    ' {"x":1,"y":2}


## 3. LIST

DIM numeri AS LIST OF INTEGER = [5, 10, 15]
PRINT numeri.Count; numeri(0)
numeri.Add(20)
numeri(1) = 99
PRINT JSON.Stringify(numeri)


 3  5 
[5,99,15,20]


### Metodi e proprietà
| Firma | Descrizione |
|---|---|
| `.Add(v)` | aggiunge in coda |
| `.Insert(i, v)` | inserisce alla posizione `i` |
| `.Remove(v)` | rimuove la prima occorrenza di `v` |
| `.RemoveAt(i)` | rimuove l'elemento alla posizione `i` |
| `.IndexOf(v)` | indice della prima occorrenza (`-1` se assente) |
| `.Contains(v)` | `-1` (vero) se presente |
| `.Count` / `.Count()` | numero di elementi |
| `.Sort()` | ordina in place |
| `.Reverse()` | inverte in place |
| `.Clear()` | svuota |
| `.ToArray()` / `.ToList()` | copia profonda della lista |
| `.Join(sep$)` | stringa con gli elementi uniti da `sep$` (numeri e stringhe) |
| `.Max()` | elemento massimo (numerico) |
| `.Min()` | elemento minimo (numerico) |
| `.Sum()` | somma degli elementi (numerico) |
| `x(i)` / `x[i]` | lettura/scrittura per indice |
| `FOR EACH v IN x` | iterazione in ordine |

### Metodi sugli ARRAY nativi (`DIM x = [ ... ]` / `DIM x(n)`)

DIM t = []
t.Add(30) : t.Add(10) : t.Add(20)
PRINT t.Count()          ' 3
PRINT t.Join(",")        ' 30,10,20   (Join 1 arg: separatore)
PRINT t.Max(); t.Min(); t.Sum()   ' 30  10  60
t.Sort()                 ' ordina in place (ASC)
t.Clear()                ' svuota (Count -> 0)


### `ORDER BY` nelle comprehension (`FROM ... SELECT`)

DIM a = {3, 1, 2}
DIM asc  = FROM n IN a ORDER BY n SELECT n       ' 1,2,3
DIM desc = FROM n IN a ORDER BY n DESC SELECT n  ' 3,2,1

| Clausola | Parametri | Note |
|---|---|---|
| `ORDER BY expr` | `expr` = chiave (ASC di default) | ordina la lista risultante |
| `ORDER BY expr DESC` | | ordine decrescente |

## 4. QUEUE (FIFO)

DIM coda AS QUEUE OF STRING = ["job1","job2"]
coda.Enqueue("job3")
PRINT coda.Peek; coda.Dequeue; coda.Count


job1 job1  2 

| Firma | Descrizione |
|---|---|
| `.Enqueue(v)` | accoda in coda |
| `.Dequeue()` / `.Dequeue` | rimuove e restituisce l'elemento frontale |
| `.Peek` / `.Peek()` | elemento frontale senza rimuoverlo |
| `.Contains(v)`, `.Count`, `.ToArray()` | come LIST |

## 5. STACK (LIFO)

DIM pila AS STACK OF INTEGER = [10, 20]
pila.Push(30)
PRINT pila.Pop; pila.Peek


 30   20

| Firma | Descrizione |
|---|---|
| `.Push(v)` | mette in cima |
| `.Pop()` / `.Pop` | rimuove e restituisce la cima |
| `.Peek` / `.Peek()` | cima senza rimuovere |

## 6. HASHSET

DIM s AS HASHSET<INTEGER> = [5, 6, 5]
s.Add(7) : s.Add(6)         ' 6 già presente → ignorato
PRINT s.Count; s.Contains(6)
PRINT JSON.Stringify(s.ToArray())


 3 -1
[5,6,7]


DIM a AS HASHSET<INTEGER> = [1,2,3]
DIM b AS HASHSET<INTEGER> = [3,4,5]
PRINT JSON.Stringify(a.Union(b))
PRINT JSON.Stringify(a.Intersect(b))
PRINT JSON.Stringify(a.Except(b))
PRINT JSON.Stringify(a.SymmetricExcept(b))


[1,2,3,4,5]
[3]
[1,2]

| Firma | Descrizione |
|---|---|
| `.Add(v)` | aggiunge se assente |
| `.Contains(v)` | appartenenza |
| `.Remove(v)`, `.Count`, `.Clear()` | usuali |
| `.Union(set)`, `.Intersect(set)`, `.Except(set)`, `.SymmetricExcept(set)` | operazioni insiemistiche (ritornano un nuovo HASHSET/lista) |

### Operazioni insiemistiche: parametri e semantica

Tutte le operazioni ricevono **un solo argomento**: un'altra `HASHSET` con lo stesso tipo
elemento. Non modificano né il ricevente né l'argomento: restituiscono un **nuovo** insieme.

| Metodo | Risultato |
|---|---|
| `Union(other)` | elementi presenti in `this` **o** in `other` (unione, senza duplicati) |
| `Intersect(other)` | elementi presenti in **entrambi** |
| `Except(other)` | elementi presenti in `this` e **non** in `other` |
| `SymmetricExcept(other)` | elementi presenti in **uno solo** dei due (differenza simmetrica) |

La cardinalità e l'appartenenza sono deterministiche; l'ordine di enumerazione di un
`HASHSET` non è garantito. Casi limite: `A.SymmetricExcept(A)` → insieme vuoto;
`A.SymmetricExcept(B)` con A e B disgiunti → `|A| + |B|` elementi.

```basic
IMPORT COLLECTIONS
DIM a AS HASHSET<INTEGER> = [1,2,3,4]
DIM b AS HASHSET<INTEGER> = [3,4,5,6]
DIM s = a.SymmetricExcept(b)       ' {1,2,5,6}
PRINT s.Count()                    ' 4
PRINT s.Contains(3)                ' 0
```

MiniTest: `shared/minitests/collections/hashset_setops.bas`. Sample:
`shared/samples/Collections/HashSetSetOps.bas`. Disponibile su IL/PCodeVM/VM-C/C99Native.
| `.UniqueItems()` | elenco degli elementi unici |

## 7. DICT

DIM d AS DICT<STRING, DOUBLE> = { mela: 1.25, banana: 0.75 }
d["arancia"] = 2.50
PRINT d.Count; d["mela"]
d.Remove("banana")
PRINT d.ContainsKey("banana"); JSON.Stringify(d.Keys())


 3  1.25 
 0 ["mela","arancia"]

| Firma | Descrizione |
|---|---|
| `d("k")` / `d["k"]` | lettura/scrittura per chiave |
| `.Add(k, v)` | inserisce/aggiorna una coppia |
| `.Remove(k)` | rimuove la chiave e ritorna un **predicato**: `-1` se esisteva, `0` se assente |
| `.ContainsKey(k)` | `-1` se la chiave esiste |
| `.Count` / `.Count()` | numero di coppie |
| `.Keys()` | lista (val boxed) delle chiavi |
| `.Values()` | lista dei valori |
| `.ToDict()` / `.ToJson()` | copia indipendente |
| `FOR EACH k, v IN d` | coppie |
| `FOR EACH KEY k IN d` | solo chiavi |

### 7.1 Forme del letterale dizionario `{ chiave: valore, ... }`

DIM tipizzato AS DICT OF STRING, INTEGER = { "uno": 1, "due": 2 }   ' tipo esplicito
DIM inferito  = { "a": 10, "b": 20 }                                ' tipo dedotto dal letterale
DIM legacy    AS DICT<STRING, DOUBLE> = { mela: 1.25, banana: 0.75 } ' chiavi identificatore


### 7.2 `Remove(k)` come predicato (uso in `IF` / `WHILE`)

DIM d AS DICT OF STRING, STRING
d("a") = "1"
d("b") = "2"

IF d.Remove("a") THEN PRINT "rimosso" ELSE PRINT "assente"     ' rimosso
IF d.Remove("zz") THEN PRINT "presente" ELSE PRINT "assente"   ' assente

WHILE d.Remove("b")            ' scorre finché la chiave esiste
  PRINT "rimossa b"
WEND


## 8. SORTED

DIM s AS SORTED<STRING, INTEGER> = { z: 10, a: 20, m: 30 }
PRINT JSON.Stringify(s.Keys())
PRINT s.Min(); s.Max()


["a","m","z"]
["a",20] ["z",30]


## 9. Alberi ordinati — TREE, AVLTREE, REDBLACK, BPLUSTREE

DIM t AS TREE<STRING, INTEGER> = { b: 20, a: 10, c: 30 }
t.Add("d", 40)
PRINT t.Min(); t("c")
t.Remove("b")
PRINT JSON.Stringify(t.Keys())


["a",10]  30 
["a","c","d"]


DIM avl AS AVLTREE<INTEGER, STRING> = NEW AVLTREE<INTEGER, STRING>
avl.Add(10, "dieci") : avl.Add(20, "venti") : avl.Add(5, "cinque")
PRINT avl.Min(); avl.Max()
PRINT JSON.Stringify(avl.ExtractMin())
PRINT avl.Min()


[5,"cinque"] [20,"venti"]
[5,"cinque"]
[10,"dieci"]


DIM rb AS REDBLACK<STRING, BOOLEAN> = { b: FALSE, a: TRUE }
PRINT JSON.Stringify(rb.ToArray())


[{"Key":"a","Value":True},{"Key":"b","Value":False}]

| Firma | Descrizione |
|---|---|
| `.Add(k, v)`, `.Set(k, v)`, `.SetKeyValue(k, v)`, `.Remove(k)`, `.ContainsKey(k)`, `.Get(k)` | gestione coppie (`Set`/`SetKeyValue` sovrascrivono la chiave) |
| `.MaxPair()` / `.MinPair()` | chiave della voce con valore massimo/minimo |
| `.Min()` / `.Max()` | coppia estremale per chiave |
| `.ExtractMin()` / `.ExtractMax()` | estrae la coppia estremale |
| `.Keys()` (ordinate), `.Values()`, `.Count()` | usuali |
| `.ToArray()` | elenco ordinato `{Key, Value}` |

## 10. TRIE

DIM t AS TRIE = ["car","cat","cart"]
t.Add("catalog")
PRINT t.Count; t.StartsWith("car")
PRINT JSON.Stringify(t.FindAll("ca"))


 4 -1
["car","cat","cart","catalog"]

| Firma | Descrizione |
|---|---|
| `.Add(parola$)` | inserisce (se assente) |
| `.Count()` | numero di parole |
| `.StartsWith(prefisso$)` | `-1` se esiste una parola con quel prefisso |
| `.FindAll(prefisso$)` | lista delle parole che iniziano col prefisso |
| `FOR EACH parola$ IN t` | iterazione sulle parole |

## 11. HEAP / MAXHEAP

DIM h AS HEAP<INTEGER> = [4, 1, 7, 3]
h.Add(2)
PRINT h.Peek(); h.Extract(); h.Peek()


 1  1  2 

| Firma | Descrizione |
|---|---|
| `.Add(v)` | inserisce |
| `.Peek()` | estremo senza rimuovere |
| `.Extract()` | rimuove e restituisce l'estremo |

## 12. BAG

DIM b AS BAG<STRING> = ["a","b","a","c","a"]
PRINT b.Count; b.UniqueCount; b.Count("a")
b.Remove("a")
PRINT b.Count("a")
PRINT JSON.Stringify(b.ToJson())


 5  3  3 
 2 
{"a":2,"b":1,"c":1}

| Firma | Descrizione |
|---|---|
| `.Add(v)`, `.Remove(v)` | aggiunge/rimuove **una** istanza |
| `.Count` / `.Count()` | totale elementi |
| `.Count(v)` | molteplicità di `v` |
| `.UniqueCount()` | numero di valori distinti |
| `.ToJson()` | conteggi come oggetto `{valore: quantità}` |

## 13. CACHE

DIM c AS CACHE<STRING, STRING>
c.Set("key1", "value1", 2)      ' scade dopo 2 secondi
c.Set("key2", "value2")
PRINT c.ContainsKey("key1"); c.Count()


 -1  2 

| Firma | Descrizione |
|---|---|
| `.Set(k, v [, ttl])` | inserisce con TTL opzionale |
| `.TryGetValue(k, out v)` | legge se presente e non scaduta |
| `.ContainsKey(k)`, `.Remove(k)`, `.Keys()`, `.Count()` | usuali |

## 14. GRAPH

DIM g AS GRAPH<STRING>
g.AddNode("A") : g.AddNode("B") : g.AddNode("C")
g.AddEdge("A","B",10) : g.AddEdge("B","C",5)
PRINT g.NodeCount; g.HasNode("A"); g.HasEdge("A","B")
PRINT g.GetInDegree("C")


 3 -1 -1
 1

| Firma | Descrizione |
|---|---|
| `.AddNode(n)`, `.RemoveNode(n)`, `.HasNode(n)` | gestione nodi |
| `.AddNodeConsistent(n)` | aggiunge il nodo se assente; ritorna `-1` se il nodo ESISTE dopo la chiamata (nuovo o già presente), `0` su errore |
| `.AddEdge(a,b,peso)`, `.HasEdge(a,b)`, `.RemoveEdge(a,b)` | gestione archi |
| `.GetEdgeWeight(a,b)` | peso dell'arco (`0` se assente) |
| `.GetNeighbors(n)` | vicini di un nodo: **lista di record** `[{node, weight}, …]` (es. `JSON.Stringify(g.GetNeighbors("A"))` → `[{"node":"B","weight":10},{"node":"C","weight":20}]`) |
| `.NodeCount()`, `.GetInDegree(n)` | metriche |
| `.BFS(n)` / `.DFS(n)` | visite in ampiezza/profondità |
| `.Dijkstra(sorgente)` | cammini minimi dalla sorgente |

## 15. TIMELINE

DIM tl AS TIMELINE<STRING>
tl.Add(DateSerial(2023,1,10), "Evento A")
tl.Add(DateSerial(2023,1,20), "Evento B")
PRINT tl.Count; JSON.Stringify(tl.First())


 3 [{},"Evento A"]

| Firma | Descrizione |
|---|---|
| `.Add(data, evento)` | inserisce in ordine di data |
| `.Count()` | numero di eventi |
| `.First()` / `.Last()` | primo/ultimo evento |
| `.GetRange(da, a)` | sotto-timeline nel range |

## 16. BITREE (Fenwick)

DIM b AS BITREE<INTEGER> = NEW BITREE<INTEGER>([1,2,3,4,5])
PRINT b.Query(2)            ' 1+2+3
PRINT b.QueryRange(1,3)     ' 2+3+4
b.Update(1, 10)             ' indice 1: 2 → 12
PRINT b.QueryRange(1,3)     ' 12+3+4


 6 
 9 
 19 

| Firma | Descrizione |
|---|---|
| `.Query(i)` | somma `[0..i]` |
| `.QueryRange(a,b)` | somma `[a..b]` |
| `.Update(i, delta)` | aggiunge `delta` all'indice `i` |

## 17. HASHRING

DIM server = ["10.0.0.1","10.0.0.2","10.0.0.3"]
DIM anello AS HASHRING<STRING> = NEW HASHRING<STRING>(server)
PRINT anello.GetNode("user:123")
anello.AddNode("10.0.0.4")
PRINT anello.GetNode("user:123")


10.0.0.1
10.0.0.1

| Firma | Descrizione |
|---|---|
| `.AddNode(n)`, `.GetNode(chiave)` | gestione nodi / mappatura chiave |

## 18. RINGBUFFER

DIM b AS RINGBUFFER<STRING> = NEW RINGBUFFER<STRING>(3)
b.Enqueue("A") : b.Enqueue("B") : b.Enqueue("C")
PRINT JSON.Stringify(b.ToArray())
b.Enqueue("D")                ' sovrascrive "A"
PRINT JSON.Stringify(b.ToArray())
PRINT b.Dequeue()


["A","B","C"]
["B","C","D"]
B

| Firma | Descrizione |
|---|---|
| `.Enqueue(v)`, `.Dequeue()` | accoda/estrae (FIFO) |
| `.ToArray()`, `.Count()` | contenuto e dimensione |
| `.Capacity()` | capacità fissa. **Default 64** con `DIM r AS RINGBUFFER<T>` (senza `NEW`); con `NEW RINGBUFFER<T>(n)` è `n`. |

## 19. SPARSESET

DIM s AS SPARSESET = NEW SPARSESET(1000, 10)
s.Add(100) : s.Add(500) : s.Add(999)
PRINT s.Count; s.HasItem(500)
s.Remove(100)
PRINT s.HasItem(100)


 3 -1
 0

| Firma | Descrizione |
|---|---|
| `.Add(i)`, `.Remove(i)`, `.HasItem(i)` | gestione |
| `.Count()` | numero di elementi |

## 20. Liste collegate — LINKEDLIST, DOUBLYLINKEDLIST, CIRCULAR

DIM s AS LINKEDLIST<STRING> = ["B"]
s.AddFirst("A") : s.AddLast("C")
PRINT s.First; s.Last
s.Remove("B")
PRINT JSON.Stringify(s.ToArray())
s.RemoveFirst()
PRINT s.First


A C
["A","C"]
C


DIM c AS CIRCULAR<STRING> = ["A","B","C"]
c.Add("D")
PRINT c.Current
c.Rotate(1)
PRINT c.Current


A
B

| Firma | Descrizione |
|---|---|
| `.AddFirst(v)` / `.AddLast(v)` / `.Add(v)` | inserimenti |
| `.RemoveFirst()` / `.RemoveLast()` / `.Remove(v)` | rimozioni |
| `.First()` / `.Last()` | estremi |
| `.Current()` / `.Rotate(n)` | (CIRCULAR) testa logica e rotazione |
| `.Sort()`, `.Reverse()`, `.ToArray()`, `.Count()` | usuali |

## 21. Conversione, JSON e stampa

IMPORT JSON

DIM l AS LIST OF INTEGER = [1,2,3]
PRINT l                    ' stampa diretta
PRINT JSON.Stringify(l)
PRINT l.ToArray()          ' usabile dentro ARRAYS.AJOIN / ARRAYS
PRINT ARRAYS.AJOIN(l.ToArray(), ",")


[1,2,3]
[1,2,3]
1,2,3


## 22. Riferimento rapido dei metodi condivisi
| Metodo | LIST | QUEUE | STACK | HASHSET |
|---|---|---|---|---|
| `Add(v)` | ✔ | — | — | ✔ (unico) |
| `Enqueue(v)` / `Dequeue()` | — | ✔ | — | — |
| `Push(v)` / `Pop()` | — | — | ✔ | — |
| `Contains(v)` | ✔ | ✔ | ✔ | ✔ |
| `Remove(v)` / `RemoveAt(i)` | ✔ | — | — | ✔ / — |
| `IndexOf(v)` | ✔ | ✔ | ✔ | — |
| `Sort()` / `Reverse()` | ✔ | ✔ | ✔ | ✔ |
| `Clear()` | ✔ | ✔ | ✔ | ✔ |
| `Count` / `Count()` | ✔ | ✔ | ✔ | ✔ |
| `ToArray()` / `ToList()` | ✔ | ✔ | ✔ | ✔ |
| `IsEmpty()` | ✔ | ✔ | ✔ | ✔ |
| `Front()` / `Back()` | ✔ | ✔ | ✔ | — |
| `AddUnique(v)` | ✔ | — | — | = `Add(v)` |
| `AddRange(altra)` | ✔ | ✔ | ✔ | — |
| `AddAfter(i, v)` / `AddBefore(i, v)` | ✔ | — | — | — |
| Metodo | Dove | Semantica |
|---|---|---|
| `LIST.AddUnique(v)` | LIST | aggiunge solo se assente (HashSet-like) |
| `LIST.AddAfter(i, v)` | LIST | inserisce DOPO la posizione i (0-based) |
| `LIST.AddBefore(i, v)` | LIST | inserisce PRIMA della posizione i |
| `IsEmpty()` | LIST/QUEUE/STACK | 1 se vuota, 0 altrimenti |
| `Front()` / `Back()` | LIST/QUEUE | testa (prossimo da `Dequeue`) / coda |
| `DICT.Set(k, v)` / `SetKeyValue(k, v)` | DICT/SORTED | sovrascrive la chiave (alias di `Add`) |
| `DICT.Get(k)` | DICT/SORTED | lookup per chiave (NIL se assente) |
| `DICT.MaxPair()` / `MinPair()` | DICT/SORTED | chiave con valore massimo/minimo |

## 23. Note di compatibilità

### Arrays-Guida-Utente

*Fonte: `Arrays-Guida-Utente`*


## 0. Dichiarazione e letterali (inizializzazione)

### 0.1 Dichiarazione

DIM M(3, 3)                    ' matrice 4x4 (indici 0..3), elementi a 0
DIM A(0 TO 5)                  ' 6 elementi, indici 0..5
DIM B()                        ' array dinamico (cresce con AADD/REDIM)
DIM C(2, 2) AS INTEGER         ' tipo elemento esplicito


### 0.2 Inizializzazione col letterale — due forme: `{ ... }` e `[ ... ]`

' 1-D, implicito (size = numero di elementi)
DIM a() AS DOUBLE  = [1.5, 2.5, 3.5]      ' oppure  {1.5, 2.5, 3.5}
DIM nomi() AS STRING = ["pena", "matita"]

' 1-D, dimensionato (i primi elementi dai valori, i restanti restano 0)
DIM b(4) AS INTEGER = [2, 10, 1, 4, 0]    ' 5 slot, tutti riempiti

' 2-D (matrice), letterale ANNIDATO rettangolare
DIM m(1, 1) AS INTEGER = [[1, 2], [3, 4]]         ' quadre
DIM m2(1, 1) AS INTEGER = {{1, 2}, {3, 4}}        ' graffe


### 0.3 `[ ... ]` come LISTA (fuori da un DIM di array)

DIM lst = [3, 1, 4]        ' lst è una LIST OF DOUBLE
DIM ll  = [[1,2], [3,4]]   ' ll è una LIST OF LIST (non una matrice!)


### 0.4 Lettura/scrittura

### 0.5 Comprensioni (LIST / DICT / SET)
| Forma | Sintassi | Produce |
|---|---|---|
| Lista | `[ <output> FOR <v> IN <src> [IF <cond>] ]` | array |
| Dict  | `{ <chiave> : <valore> FOR <v> IN <src> [IF <cond>] }` | `DICT<K,V>` |
| Set   | `{ <output> FOR <v> IN <src> [IF <cond>] }` | `HASHSET<T>` (dedup) |

DIM nomi = {"Charlie", "Alice", "Bob"}
DIM copia   = [ nome FOR nome IN nomi ]                  ' array di stringhe
DIM corti   = [ nome FOR nome IN nomi IF LEN(nome) <= 5 ] ' filtro
DIM up      = [ UPPER(s) FOR s IN {"a", "b"} ]            ' sorgente letterale
DIM lungh   = { nome : LEN(nome) FOR nome IN nomi }       ' chiave stringa, valore numerico
DIM saluti  = { nome : "Ciao " + nome FOR nome IN nomi }  ' chiave e valore stringa
DIM distinti= { s FOR s IN {"a", "b", "a"} }              ' set di stringhe (dedup)


## 1. Trasformazioni

IMPORT ARRAYS

a = {3, 1, 4, 1, 2}
PRINT AREVERSE(a)        ' {2,1,4,1,3}
PRINT ASORT(a)           ' {1,1,2,3,4}
PRINT AUNIQUE(a)         ' {3,1,4,2}  (dedup, ordine di prima comparsa)
PRINT ACLONE(a)          ' copia
PRINT APAD(a, 7)         ' estende a 7 elementi
PRINT ACHUNK(a, 2)       ' a blocchi di 2
PRINT AFLAT(ACHUNK(a, 2))' appiattisce
PRINT AZIP({1,2}, {10,20})' {[1,10],[2,20]}


## 2. Query e riduzione

PRINT ACOUNT(a)                       ' numero di elementi
PRINT AMAX(a) ; PRINT AMIN(a)         ' massimo / minimo
PRINT ASUM(a)                         ' somma
PRINT ABINARYSEARCH(ASORT(a), 3)      ' indice (array ordinato)
PRINT ASCAN(a, 4)                     ' indice del valore (o -1)
PRINT AREDUCE(a, FN(x, y) => x + y)   ' riduzione (fold)
PRINT AFILTER(a, FN(x) => x > 1)      ' elementi che soddisfano il predicato
PRINT AMAP(a, FN(x) => x * 2)         ' applica la funzione a ogni elemento
PRINT AEVAL(a, SUB(x) { PRINT x })    ' applica una SUB a ogni elemento


## 3. Comandi di mutazione (legacy)

AADD a, 9          ' aggiunge in coda
AINS a, 1          ' inserisce in posizione 1 (indice 0-based; slot nuovo = 0 / null)
ADEL a, 2          ' rimuove l'elemento in posizione 2 (indice 0-based)
AFILL a, 0         ' riempie con 0 (opz. start/count)
ACOPY src, dst     ' copia totale (dst resta della sua lunghezza)
ACOPY src, dst, srcStart, count, destStart   ' copia PARZIALE in-place


## 4. Conversioni

PRINT AJOIN(a, ",")          ' join in stringa
PRINT ATOKENS("a,b,c", ",")  ' split in array di stringhe


## 4bis. Slice `a[start:end]` (ArraySlice)
| forma | risultato (con `a = [1,2,3,4]`) |
|---|---|
| `a[1:3]` | `[2,3]` |
| `a[0:4]` | `[1,2,3,4]` |
| `a[2:9]` | `[3,4]` (end oltre la lunghezza → clampato) |
| `a[:3]` | start implicito **0** → `[1,2,3]` |
| `a[1:]` | end implicito = **lunghezza** → `[2,3,4]` |

## 5. Verifiche sui quattro backend

## 5. Riferimento comandi/funzioni (parametri)

### 5.1 Funzioni senza callback
| Funzione | Parametri | Ritorno | Note |
|---|---|---|---|
| `AREVERSE(a)` | `a` array/lista | nuovo array invertito | non muta `a` |
| `ASORT(a)` | `a` array/lista | nuovo array ordinato crescente | numerico |
| `AUNIQUE(a)` | `a` array/lista | nuovo array deduplicato | ordine di prima comparsa |
| `ACLONE(a)` | `a` array/lista | copia profonda | |
| `APAD(a, n [, pad])` | `n` = lunghezza target (int); `pad` = valore di riempimento (default: 0 / "") | nuovo array di lunghezza `n` | tronca se `n` < lunghezza |
| `ACHUNK(a, n)` | `n` = dimensione blocco (int) | lista di sotto-liste | |
| `AFLAT(a)` | `a` con sotto-liste | lista appiattita | |
| `AZIP(a, b, …)` | 2+ array | lista di tuple `[a_i, b_i, …]` | lunghezza = minima |
| `ASHUFFLE(a)` | `a` array/lista | copia mescolata | RNG deterministico (seed) |
| `ACOUNT(a [, pred])` | `pred` opzionale (callback) | int | senza `pred`: numero elementi; con `pred`: quanti la soddisfano |
| `AMAX(a)` / `AMIN(a)` | `a` numerico | double | |
| `ASUM(a)` | `a` numerico | double | |
| `ABINARYSEARCH(sorted, val)` | `sorted` = array GIÀ ordinato; `val` = valore | indice (0-based) o -1 | |
| `ASCAN(a, val\|pred)` | 2° arg: valore da cercare **oppure** callback predicato | indice 0-based o -1 | |
| `AJOIN(a, sep)` | `sep` = separatore (stringa) | stringa | |
| `ATOKENS(s, sep)` | `s` stringa, `sep` separatore | array di stringhe | split |

### 5.1bis `LEN` su array, stringhe e parametri
| forma | parametro | risultato |
|---|---|---|
| `LEN(a)` | array/lista/dict/set (locale o letterale) | numero di elementi |
| `LEN(a)` | **parametro array** di una SUB/FUNCTION | numero di elementi (l'ABI passa la dimensione) |
| `LEN(s)` | stringa | numero di caratteri |
| `LEN(x.Campo)` | campo lista/array di un oggetto boxed | numero di elementi |

### 5.2 Funzioni con callback (`FN`/`SUB`/lambda)
| Funzione | Parametri | Ritorno |
|---|---|---|
| `AMAP(a, fn)` | `fn(elem)` → valore | nuovo array (proiezione) |
| `AFILTER(a, pred)` | `pred(elem)` → booleano | nuovo array (elementi che soddisfano) |
| `AREDUCE(a, fn [, init])` | `fn(acc, elem)` → valore; `init` = accumulatore iniziale (default 0/None) | valore scalare (fold) |
| `AEVAL(a, sub)` / `AEVAL a, { \|v, i\| … }` | `sub(elem)` oppure block `{ \|v, i\| … }` → **elemento + indice 0-based** (2 parametri) | — (comando/espressione) |
| `ASCAN(a, pred)` | `pred(elem)` → booleano | indice del primo elemento che soddisfa, o -1 |

DIM a = {1, 2, 3, 4}
PRINT AMAP(a, FN(n AS INTEGER) AS INTEGER => n * n)        ' {1,4,9,16}
PRINT AFILTER(a, FN(n AS INTEGER) AS INTEGER => (n MOD 2) = 0)  ' {2,4}
PRINT AREDUCE(a, FN(acc AS INTEGER, x AS INTEGER) AS INTEGER => acc + x)  ' 10


### 5.3 `ASORT` con comparatore (statement)

DIM Clienti(1) AS Cliente
ASORT Clienti, { |a, b| RETURN a.Fatturato > b.Fatturato }   ' decrescente
DIM n() = {50, 10, 20}
ASORT n, , "D"                                               ' 50, 20, 10


### 5.4 `UBOUND`/`LBOUND` su matrici

### 5.5 Higher-order su array di **UDT**

TYPE Prodotto
    Nome   AS STRING * 20
    Prezzo AS CURRENCY
END TYPE

FUNCTION ConfrontaProdotti(P1 AS Prodotto, P2 AS Prodotto) AS INTEGER
    IF P1.Prezzo < P2.Prezzo THEN RETURN -1
    IF P1.Prezzo > P2.Prezzo THEN RETURN 1
    RETURN 0
END FUNCTION
FUNCTION ApplicaSconto(P AS Prodotto) AS Prodotto : RETURN P : END FUNCTION  ' UDT→UDT
FUNCTION FiltraInScorta(P AS Prodotto) AS BIT : RETURN TRIM(P.Nome) <> "" : END FUNCTION
FUNCTION SommaPrezzi(acc AS DOUBLE, P AS Prodotto) AS DOUBLE
    RETURN acc + P.Prezzo
END FUNCTION



### 5.6 `ASHUFFLE` su array STRING

## Vedi anche

### Conversion-Guida-Utente

*Fonte: `Conversion-Guida-Utente`*


## 1. Panoramica

IMPORT CONVERSIONS
SUB Main()
    PRINT HEX(255)
END SUB

| Famiglia | Funzioni |
|---|---|
| Basi numeriche | `HEX`, `OCT`, `BIN` |
| Conversione di tipo | `CINT`, `CLNG`, `CSNG`, `CDBL`, `CSTR`, `CCUR`, `CBYTE`, `VAL` |

## 2. Basi numeriche

### 2.1 HEX(255) → "FF"

PRINT HEX(255)   ' "FF"
PRINT HEX(10)    ' "A"


### 2.2 OCT(10) → "12"

PRINT OCT(10)    ' "12"


### 2.3 BIN(5) → "101"

PRINT BIN(5)     ' "101"


IMPORT CONVERSIONS
SUB Main()
    PRINT HEX(255)   ' "FF"
    PRINT OCT(10)    ' "12"
    PRINT BIN(5)     ' "101"
END SUB


 FF
 12
 101


## 3. Conversione di tipo numerico

### 3.1 CINT / CLNG / CSNG / CDBL

PRINT CDBL(2)      ' 2
PRINT CSTR(12.5)   ' "12.5"


### 3.1.1 `INTEGER` è un intero a 64 bit (deciso 2026-11-01)
| aspetto | valore |
|---|---|
| intervallo | praticamente illimitato (`LONG`/`CLNG` per l'uso esplicito a 64 bit) |
| `i = 70000` su `DIM i AS INTEGER` | **70000** (prima: `OverflowException` su IL, 70000 sulle backend C) |
| `i = LEN(s)` con `s` di 70.000 caratteri | **70000** (prima 4464 = 70000 MOD 65536, solo su IL) |
| `CINT(x)` | stesso valore intero (nessun troncamento a 16 bit) |
| arrotondamento di `CINT` | all'intero più vicino, **half-to-even** (`2.5→2`, `3.5→4`, `-2.5→-2`) |
| `SIZEOF(DIM x AS INTEGER)` | 2 (dichiarazione storica del tipo; non è la dimensione del valore) |

DIM i AS INTEGER
i = 70000
PRINT i                 ' 70000 su tutte e 4 le backend
PRINT CINT(70000)       ' 70000
PRINT CINT(2.5)         ' 2   (half-to-even)
PRINT CINT(3.5)         ' 4
PRINT CINT(-2.5)        ' -2


### 3.2 CSTR / VAL — numero ↔ stringa

s$ = CSTR(123)      ' "123"
n  = VAL("123.5")   ' 123.5


## 4. Note

## 5. Verifiche sui quattro backend (2026-09)

IMPORT CONVERSIONS
SUB Main()
  PRINT HEX(255)          ' "FF"
  PRINT OCT(10)           ' "12"
  PRINT BIN(5)            ' "101"
  PRINT CINT(3.7)         ' 4    (arrotonda: non tronca)
  PRINT CSTR(12.5)        ' "12.5"
  PRINT CDBL(2)           ' 2
END SUB


IMPORT CONVERSIONS
SUB Main()
  PRINT HEX(4095)         ' "FFF"
  PRINT OCT(64)           ' "100"
  PRINT BIN(255)          ' "11111111"
  PRINT CINT(3.49)        ' 3    (half-to-even)
  PRINT CINT(-3.5)        ' -4   (half-to-even; NON -3)
  PRINT CINT(-3.6)        ' -4
  PRINT CSNG(2.5)         ' 2.5
  PRINT CDBL(7)           ' 7
  PRINT INT(3.9)          ' 3    (floor)
  PRINT INT(-3.9)         ' -4   (floor; NON -3)
  PRINT FIX(3.9)          ' 3    (tronca verso zero)
  PRINT FIX(-3.9)         ' -3
  PRINT VAL("  -12.5")    ' -12.5
  PRINT ASC("A")          ' 65


IMPORT CONVERSIONS
SUB Main()
  PRINT CLNG(3.99)        ' 4
  PRINT CLNG(-3.5)        ' -4
  PRINT CLNG(123.4)       ' 123
END SUB


### Semantica di arrotondamento (parità verificata 2026-10-01)

### Riferimento funzioni di conversione (parametro → risultato)
| Funzione | Parametro | Risultato | Semantica |
|---|---|---|---|
| `CINT(x)` | `x` numerico o stringa numerica (letterale/variabile intera o double) | intero `INTEGER` (reso come DOUBLE) | **half-to-even** |
| `CLNG(x)` | idem | intero `LONG` (reso come DOUBLE) | **half-to-even** |
| `INT(x)` | `x` numerico | `DOUBLE` | **floor** (verso -∞) |
| `FIX(x)` | `x` numerico | `DOUBLE` | **tronca verso zero** |
| `TRUNC(x)` | `x` numerico | `DOUBLE` | **tronca verso zero** (≡ `FIX`) |
| `CDBL(x)` / `CSNG(x)` | `x` numerico/stringa | `DOUBLE` | identità sul double interno |
| `CBYTE(x)` | `x` numerico | `DOUBLE` | solo **[0, 255]**, fuori range → `Overflow` |
| `CCUR(x)` | `x` numerico | `DOUBLE` | pass-through (identità) |
| `CSTR(x)` | `x` numerico | `STRING` | stringa locale-invariante |
| `VAL(s)` | `s` `STRING` | `DOUBLE` | parse numerico invariante |

### Stringhe-Guida-Utente

*Fonte: `Stringhe-Guida-Utente`*


## Letterali e concatenazione

## Tipo e variabili

## Confronto relazionale (`<`, `>`, `<=`, `>=`, `=`, `<>`)
| Operatore | Parametri | Ritorno | Esempio |
|---|---|---|---|
| `a$ < b$` | `a$`, `b$` stringhe (letterali, variabili, concatenazioni, risultati di funzione) | `-1` se `a$` precede `b$`, `0` altrimenti | `("a" < "b")` → `-1` |
| `a$ > b$` | idem | `-1` se `a$` segue `b$`, `0` altrimenti | `("b" > "a")` → `-1` |
| `a$ <= b$` | idem | `-1` se `a$` precede o è uguale, `0` altrimenti | `("a" <= "a")` → `-1` |
| `a$ >= b$` | idem | `-1` se `a$` segue o è uguale, `0` altrimenti | `("a" >= "b")` → `0` |
| `a$ = b$` | idem | `-1` se uguali, `0` altrimenti | `("a" = "a")` → `-1` |
| `a$ <> b$` | idem | `-1` se diverse, `0` altrimenti | `("a" <> "b")` → `-1` |

DIM nomi(2) AS STRING
nomi(0) = "delta" : nomi(1) = "Alpha" : nomi(2) = "bravo"
IF nomi(0) > nomi(1) THEN PRINT "delta dopo Alpha"   ' -1 → stampa
IF ("ab" + "c") = "abc" THEN PRINT "concat-ok"       ' -1 → stampa
IF UPPER("x") >= "W" THEN PRINT "maiuscola-ok"      ' -1 → stampa


## Operazioni comuni

## `LEN` è in CARATTERI, `LENB` in BYTE (UTF-8)
| stringa | `LEN` | `LENB` |
|---|---:|---:|
| `"abc"` | 3 | 3 |
| `"é"` | 1 | 2 |
| `"─"` (box-drawing) | 1 | 3 |
| `"😀"` (emoji) | 1 | 4 |
| `""` | 0 | 0 |

### `LEN(source)` → LONG
| parametro | tipo | obbligatorio | significato |
|---|---|---|---|
| `source` | ANY (stringa, lista, dizionario, array, numerico) | sì | valore di cui misurare la lunghezza |

### `LENB(source)` → LONG
| parametro | tipo | obbligatorio | significato |
|---|---|---|---|
| `source` | ANY | sì | stringa di cui contare i **byte** della codifica UTF-8 |

### Sottostringhe e ricerca: indici in CARATTERI

PRINT LEN("😀"); " "; LENB("😀")            ' 1  4
PRINT LEFT$("😀x", 1)                        ' 😀 (carattere intero, non mezzo emoji)
PRINT MID$("a😀b", 2, 1)                     ' 😀
PRINT INSTR("a😀b", "😀")                    ' 2 (posizione in CARATTERI)
PRINT LENB("Caffè")                          ' 6 (5 caratteri, 6 byte)


### Esempio: colonne allineate e budget in byte

' colonna da 14 CARATTERI, payload con limite di 64 BYTE
DIM cella AS STRING
cella = LEFT$(nome, 14)
IF LEN(cella) < 14 THEN cella = cella + DUP(14 - LEN(cella), " ")
PRINT cella; " | "; LENB(payload); " byte"


## SPLIT e JOIN (stringa ↔ array)

## Escape e tipi di ritorno

## Validatori (`STRINGS.*`)
| Funzione | Parametri | Ritorno |
|---|---|---|
| `ISEMAIL(s)` | `s` stringa | BIT: `-1` (TRUE) se email valida, `0` altrimenti |
| `ISURL(s)` / `ISCREDITCARD(s)` / `ISREQUIRED(s)` | `s` stringa | BIT `-1`/`0` |
| `ISLENGTH(s, min, max)` | `min`, `max` lunghezze (int); `max < 0` = senza limite alto | BIT `-1`/`0`; `ISLENGTH("", 0, 0)` → `-1` |

## Accumulo in ciclo (`s$ = s$ + termine`)

s$ = ""
FOR i = 1 TO 4
    s$ = s$ + "ab"                 ' "abababab"
NEXT

t$ = ""
FOR i = 1 TO 3
    t$ = t$ + STR$(i)              ' " 1 2 3"  (STR$ prepende lo spazio ai positivi)
NEXT


## Padding a larghezza fissa — `PADL` / `PADR` / `PADC`
| Funzione | Forma | Ritorno |
|---|---|---|
| `PADL(exp, n [, fill])` | padding a **sinistra** | stringa lunga `n` |
| `PADR(exp, n [, fill])` | padding a **destra** | stringa lunga `n` |
| `PADC(exp, n [, fill])` | padding **centrato** | stringa lunga `n` |

PRINT "[" + PADR("ab", 5) + "]"        ' [ab   ]
PRINT "[" + PADL("42", 6, "0") + "]"   ' [000042]
PRINT "[" + PADC("mid", 7) + "]"       ' [  mid  ]
PRINT LEFT(PADR("abcdefgh", 5), 5)     ' abcde   (tronca esplicitamente)


### Strings

*Fonte: `Strings`*


## Panoramica

## Funzioni di base (estrazione e lunghezza)

### `LEN(str$) → LONG`

PRINT LEN("ciao")   ' 4


### `LEFT$(str$, n%) → STRING`

PRINT LEFT$("GMBasic", 2)  ' "GM"


### `RIGHT$(str$, n%) → STRING`

PRINT RIGHT$("GMBasic", 5) ' "Basic"


### `MID$(str$, start%, [len%]) → STRING`

PRINT MID$("GMBasic", 3, 4) ' "Basi"
PRINT MID$("GMBasic", 3)    ' "Basic"


### `INSTR([start%,] haystack$, needle$) → LONG`

PRINT INSTR("GMBasic", "Bas")  ' 3
PRINT INSTR(4, "GMBasic", "a") ' 4  ('a' è alla posizione 4)


### `DUP(count%, character) → STRING`

PRINT DUP(3, "a")   ' "aaa"


### `SPACE$(n%) → STRING`

PRINT "[" + SPACE$(3) + "]" ' "[   ]"


### `REVERSE$(str$) → STRING`

PRINT REVERSE$("ciao") ' "oaic"


## Case conversion e normalizzazione

### `LOWER$(str$) → STRING` e `UPPER$(str$) → STRING`

PRINT LOWER$("CIAO") ' "ciao"
PRINT UPPER$("ciao") ' "CIAO"


### `TRIM$(str$) → STRING` , `LTRIM$(str$)`, `RTRIM$(str$)`

PRINT "[" + TRIM$("  ciao  ") + "]"  ' "[ciao]"
PRINT "[" + LTRIM$("  ciao") + "]"   ' "[ciao]"


### `TITLE$(str$) → STRING` e `SENTENCE$(str$)`

PRINT TITLE$("ciao mondo") ' "Ciao Mondo"


### `REPLACE$(str$, old$, new$) → STRING`

PRINT REPLACE$("ciao mondo", "mondo", "GMBasic") ' "ciao GMBasic"


### `REPEAT$(str$, n%)` — alias di `DUP$`.

### `PASCAL(str$)` / `CAMEL(str$)` / `SNAKE(str$)` / `KEBAB(str$)`

PRINT PASCAL("ciao mondo") ' "CiaoMondo"
PRINT CAMEL("ciao mondo")  ' "ciaoMondo"
PRINT SNAKE("CiaoMondo")   ' "ciao_mondo"
PRINT KEBAB("CiaoMondo")   ' "ciao-mondo"


### `WRAP(str$, width% [, alignment%]) → STRING`

PRINT WRAP("ciao mondo lungo", 5)


 ciao
 mondo
 lungo


## Ricerca e test

### `SEARCH(str$, pattern$) → BOOLEAN`

PRINT SEARCH("GMBasic", "bas") ' -1  (true)
PRINT SEARCH("GMBasic", "xyz") ' 0


### `CONTAINS(str$, sub$) → BOOLEAN`

IF CONTAINS("GMBasic", "Bas") THEN PRINT "sì"
PRINT CONTAINS("GMBasic", "Bas")  ' -1 (true)
PRINT CONTAINS("GMBasic", "xyz")  ' 0


### `STARTSWITH(str$, prefix$) → BOOLEAN` e `ENDSWITH(str$, suffix$) → BOOLEAN`

PRINT STARTSWITH("GMBasic", "GM")  ' -1 (true)
PRINT STARTSWITH("GMBasic", "xy")  ' 0
PRINT ENDSWITH("GMBasic", "sic")   ' -1
PRINT ENDSWITH("GMBasic", "xy")    ' 0


### `REMOVE$(str$, toRemove$) → STRING`

PRINT REMOVE$("ciao mondo", "o") ' "cia mnd"


## Split / Join / Search

### `SPLIT$(str$, [sep$]) → STRING[]`

DIM a$() = SPLIT$("a,b,c", ",")
PRINT a$(1) ' "b"


### `JOIN$(arr$(), [sep$]) → STRING`

DIM a$() = {"a","b","c"}
PRINT JOIN$(a$(), "-") ' "a-b-c"


### `SEARCH` già descritto.

## Padding e formattazione

### `PADL$(str$, len%, [pad$]) → STRING` / `PADR$` / `PADC$`

PRINT "[" + PADL$("ciao", 8) + "]"      ' "[    ciao]"
PRINT "[" + PADR$("ciao", 8, "-") + "]" ' "[ciao----]"
PRINT "[" + PADC$("ciao", 8) + "]"      ' "[  ciao  ]"


### `FORMAT$(value, fmt$) → STRING`

## Encoding e sicurezza

### `HTMLENCODE(str$) / URLENCODE(str$) / JSENCODE(str$) → STRING`

PRINT HTMLENCODE("<ciao>")   ' "&lt;ciao&gt;"
PRINT URLENCODE("a b&c")     ' "a%20b%26c"


### `PETSCII$(str$) → STRING`

## Conversione e utilità

### `ASC(str$) → LONG` e `CHR$(code%) → STRING`

PRINT ASC("A")   ' 65
PRINT CHR$(65)  ' "A"


### `STR$(num) → STRING` e `VAL(str$) → DOUBLE`

PRINT STR$(123)      ' " 123" (spazio per segno)
PRINT VAL("123.5")   ' 123.5


### `SPACE$` già descritto.

## Esempi verificati sui quattro backend (2026-09)

IMPORT STRINGS
SUB Main()
    PRINT REVERSE("abc")
    PRINT LOWER("CIAO")
    PRINT UPPER("ciao")
    PRINT TRIM("  ciao  ")
    PRINT LTRIM("  ciao")
    PRINT RTRIM("ciao  ")
    PRINT TITLE("ciao mondo")
    PRINT PASCAL("ciao mondo")
    PRINT CAMEL("ciao mondo")
    PRINT SNAKE("CiaoMondo")
    PRINT KEBAB("CiaoMondo")
    PRINT SENTENCE("ciao mondo")
    PRINT DUP(3, "a")
    PRINT REPLACE("a-b-c", "-", "+")


cba
ciao
CIAO
ciao
ciao
ciao
Ciao Mondo
CiaoMondo
ciaoMondo
ciao_mondo
ciao-mondo
Ciao mondo
aaa
a+b+c
cia mnd
00001


## Esempi completi

' 1. Normalizzazione input utente
INPUT "Nome: ", n$
n$ = TRIM$(n$)
IF n$ = "" THEN PRINT "vuoto" : END
n$ = TITLE$(n$)
PRINT "Ciao, " + n$

' 2. Ricerca case-sensitive
DIM s$ = "GMBasic"
IF CONTAINS(s$, "Bas") THEN PRINT "contiene"
IF STARTSWITH(s$, "GM") THEN PRINT "inizia con GM"

' 3. Split/Join pipeline
DIM csv$ = "a,b,c"
DIM parts$() = SPLIT$(csv$, ",")
PARTS = JOIN$(parts$(), "-")


## Dettagli implementativi (per chi estende)

## Vedi anche

<!-- END SURFACE -->
