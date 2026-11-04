# II.5 — Tipi, variabili e scope

## Cosa ottieni da questo capitolo

Come si dichiara un valore, quali tipi esistono, quando una variabile è visibile e quando no, e
perché lo stesso nome può valere due cose diverse in due punti del programma.

## 1. Dichiarare

```basic
DIM x AS INTEGER
DIM s AS STRING
DIM totale AS DOUBLE = 0
CONST MAX_RIGHE = 100
```

- `DIM` dichiara con tipo esplicito: è la forma da usare.
- `CONST` dichiara una costante: non è riassegnabile.
- Senza dichiarazione, una variabile nasce al primo uso, nello scope corrente. Funziona, ma il
  tipo lo decide il valore: è la prima causa di risultati inattesi nelle espressioni miste.

## 2. I tipi

| Famiglia | Tipi | Note |
|---|---|---|
| interi | `BYTE`, `INTEGER`, `LONG` | `LONG` è quello da usare per contatori e indici ampi |
| reali | `SINGLE`, `DOUBLE` | `DOUBLE` per calcoli e confronti con tolleranza |
| valuta | `CURRENCY` | importi: evita gli errori di arrotondamento del binario |
| booleano | `BIT` | vale **`-1`** (vero) o **`0`** (falso), non `1` |
| testo | `STRING` | lunghezza variabile |
| testo fisso | `STRING * N` | tronca/padda a `N` caratteri: è il tipo dei campi DBF |
| assenza | `NIL` | "nessun valore", distinto da `0` e da `""` |
| nullable | `INTEGER?`, `DOUBLE?`, `STRING?` | un tipo che può contenere `NIL` |

Due conseguenze pratiche:

- **`BIT` non è `1`.** `IF` funziona con entrambi, ma `PRINT` di un confronto stampa `-1`. Chi
  confronta `= 1` ottiene sempre falso.
- **`STRING * N` non è una stringa normale.** Scrivere `"AB"` in un campo di 10 caratteri lascia
  `LEN` = 10, con il padding di spazi: è voluto, perché è il formato dei campi a lunghezza fissa.

## 3. Nullable: quando `0` non basta

Un `INTEGER?` distingue "zero" da "nessun valore":

```basic
DIM n AS INTEGER?
IF n IS NIL THEN PRINT "nessun valore"
n = 42
IF n HAS VALUE THEN PRINT n.VALUE        ' 42
n = NIL
```

| Forma | Significato |
|---|---|
| `n HAS VALUE` | vero se contiene un valore |
| `n IS NIL` | vero se è vuoto |
| `n.VALUE` | il valore contenuto |

È il tipo giusto per i campi facoltativi di un record e per i parametri che possono essere
omessi: `0` come "assente" è un errore che prima o poi arriva in produzione.

## 4. Scope: la visibilità è lessicale

Una variabile è visibile nel blocco in cui è dichiarata e nei blocchi figli. Ogni struttura di
controllo (`IF`, `FOR`, `DO`, `SELECT`, `TRY`) apre un blocco.

| Dichiarazione | Visibilità |
|---|---|
| `DIM` dentro `SUB`/`FUNCTION` | solo quella procedura |
| `LOCAL` | il blocco corrente, con **shadowing** dei nomi esterni |
| `GLOBAL` | lo scope radice del programma: visibile ovunque |
| `DIM` a livello di modulo | il modulo, e con `PUBLIC` gli altri moduli |
| `STATIC` | la procedura, ma **conserva il valore** fra una chiamata e l'altra |

**Shadowing.** `LOCAL G_Globale = 999` dentro una procedura crea una variabile nuova: da lì in
poi quel nome indica la locale, e la globale resta intatta.

**Ciclo di vita.** Una variabile locale a un blocco muore all'uscita dal blocco. È la ragione per
cui il contatore di un `FOR` non è affidabile dopo il `NEXT` (§II.7).

## 5. Passaggio dei parametri

| Modo | Sintassi | Effetto |
|---|---|---|
| per valore | `SUB F(x AS INTEGER)` | la procedura riceve una copia |
| per riferimento | `SUB F(BYREF x AS INTEGER)` | la procedura modifica la variabile del chiamante |

Un array passato a una procedura porta con sé la propria dimensione: `LEN(a)` dentro la `SUB`
restituisce il numero di elementi, non serve passarlo a parte.

## 6. Scope dinamico: `DYNAMIC` (compatibilità Clipper)

`DYNAMIC` cambia la regola: la variabile non appartiene al blocco in cui è scritta, ma alla
**catena di chiamate a runtime**.

```basic
SUB Profondo()
    PRINT messaggio            ' vede la DYNAMIC di Main
    messaggio = "modificato"   ' modifica quella di Main
END SUB

SUB Intermedio()
    DYNAMIC messaggio = "locale"   ' shadowing: nuova DYNAMIC, solo qui
    messaggio = "cambiata"
END SUB

SUB Main()
    DYNAMIC messaggio = "originale"
    Intermedio()
    PRINT messaggio            ' original o ripristinato
END SUB
```

| Regola | Effetto |
|---|---|
| cascata | una procedura chiamata vede e modifica la `DYNAMIC` del chiamante, senza riceverla come parametro |
| shadowing | dichiarare `DYNAMIC x` in una sottoprocedura nasconde quella esterna fino al ritorno |
| ripristino | all'uscita la `DYNAMIC` locale è distrutta e torna visibile quella esterna |

Serve per portare codice Clipper/xBase che conta su questa visibilità. Nel codice nuovo si
preferiscono parametri espliciti: una variabile che dipende dalla catena di chiamate è difficile
da seguire e da testare.

## 7. Esempio eseguito

`shared/samples/Scopes/ScopesGlobal.bas`, backend C99Native:

```
=== Test GLOBAL Variables ===

SuperGlobal (GLOBAL, scope radice): 999
NamespaceVar (DIM module-level, namespace): 111

Dentro SUB: SuperGlobal = 999
Dentro SUB: NamespaceVar = 111

Dopo incremento in SUB: SuperGlobal = 1000
Dopo incremento in SUB: NamespaceVar = 112

Dentro SUB con LET: SuperGlobal = 777
Dopo SUB con LET: SuperGlobal = 777
Dopo SUB con LET: NamespaceVar = 112
```

Le tre lezioni, tutte visibili nei numeri:

1. **`GLOBAL` e `DIM` di modulo si vedono dalla `SUB`**: dentro la procedura i valori sono 999 e
   111, gli stessi del livello radice.
2. **La modifica dalla `SUB` è persistente**: dopo l'incremento la globale vale 1000 e la
   variabile di modulo 112. Non è una copia.
3. **`LET` non crea una locale**: `LET SuperGlobal = 777` scrive la globale (777 resta visibile
   dopo il ritorno), mentre la variabile di modulo resta a 112 perché non è stata toccata.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| un `IF` su un confronto non entra mai | si è scritto `= 1` invece di `<> 0` | il vero è `-1` |
| una variabile "non esiste" dopo un blocco | è locale al blocco, ed è morta all'uscita | dichiarala fuori dal blocco |
| modifiche perse dopo una `SUB` | parametro passato per valore | aggiungi `BYREF` |
| `LEN` di un campo dà 10 invece di 2 | è un campo `STRING * 10` | è il padding previsto; usa `TRIM` per leggere |
| `n.VALUE` su `NIL` | il valore non c'è | controlla prima con `HAS VALUE` |
| due variabili con lo stesso nome si confondono | shadowing voluto o accidentale | rinomina la locale, o qualifica con `SELF` nelle classi |

## 9. Dove andare adesso

- **II.7** — i blocchi che aprono e chiudono gli scope.
- **II.8** — campi e metodi: lo scope dentro una classe.
- **II.9** — array, collezioni e stringhe: tipi composti.
- **II.10** — `DYNAMIC` nell'ecosistema Clipper/xBase.













<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **CONVERSIONS.BIN(x1)** — `PRINT "  Binario (BIN):     "; CONVERSIONS.BIN(n)` (shared/samples/Math/MathExtended.bas:54)
- **CONVERSIONS.CBYTE(x1)** — `MMAPPOKE handleMMap, i, CBYTE(i)` (shared/samples/Files/FilesMemoryMap.bas:94)
- **CONVERSIONS.CCUR(x1)** — `140 PRINT "CCUR(987.654321) =", CCUR(987.654321)` (shared/samples/Types/TypesConversions.bas:64)
- **CONVERSIONS.CDBL(x1)** — `130 PRINT "CDBL(S) =", CDBL(S)` (shared/samples/Types/TypesConversions.bas:63)
- **CONVERSIONS.CINT(x1)** — `500 PRINT "CINT(123.7) ="; CINT(123.7); "(Atteso 124, arrotondamento)"` (shared/samples/Types/TypesNumerics.bas:120)
- **CONVERSIONS.CLNG(x1)** — `190 PRINT "CLNG(987654.3) =", CLNG(987654.3); "(Atteso: 987654, arrotondamento standard)"` (shared/samples/Types/TypesConversions.bas:69)
- **CONVERSIONS.CSNG(x1)** — `120 PRINT "CSNG(1.23456789) =", CSNG(1.23456789)` (shared/samples/Types/TypesConversions.bas:62)
- **CONVERSIONS.CSTR(x1)** — `PRINT "  SPEED GET (Legacy): Letto ID " + CONVERSIONS.CSTR(clienteRec.ID)` (shared/samples/Databases/DbSpeedFullTests.bas:140)
- **CONVERSIONS.HEX(x1)** — `PRINT "  Esadecimale (HEX): "; CONVERSIONS.HEX(n)` (shared/samples/Math/MathExtended.bas:52)
- **CONVERSIONS.OCT(x1)** — `PRINT "  Ottale (OCT):      "; CONVERSIONS.OCT(n)` (shared/samples/Math/MathExtended.bas:53)
- **CONVERSIONS.VAL(x1)** — `imp = VAL(RDD.GETFIELD(1, 3))` (shared/samples/Databases/CsvFinancialReport.bas:70)
- **CONVERT.BATCH(x1)** — `PRINT "batchlen="; LEN(CONVERT.BATCH("work/fc_in", "md", 0))` (shared/samples/Documents/ConvertFullCoverage.bas:60)
- **CONVERT.CLOSE(x1)** — `PRINT "close="; CONVERT.CLOSE(h)` (shared/samples/Documents/ConvertFullCoverage.bas:42)
- **CONVERT.CODEBLOCKS(x1)** — `PRINT "codeblocks="; UBOUND(CONVERT.CODEBLOCKS(d)) + 1` (shared/samples/Documents/ConvertFullCoverage.bas:22)
- **CONVERT.EXTRACT(x1)** — `PRINT "extract="; LEN(CONVERT.EXTRACT("../Fixtures/sample.pdf"))` (shared/samples/Documents/ConvertFullCoverage.bas:45)
- **CONVERT.EXTRACTENGINE(x1)** — `PRINT "extractengine="; CONVERT.EXTRACTENGINE("../Fixtures/sample.pdf")` (shared/samples/Documents/ConvertFullCoverage.bas:46)
- **CONVERT.FORMAT(x1)** — `PRINT "format="; CONVERT.FORMAT(d)` (shared/samples/Documents/ConvertFullCoverage.bas:27)
- **CONVERT.FORMATS()** — `PRINT "formats="; CONVERT.FORMATS()` (shared/samples/Documents/ConvertFullCoverage.bas:30)
- **CONVERT.HEADINGS(x1)** — `hsT = CONVERT.HEADINGS(d, 1)` (shared/samples/Documents/StaticSiteFromMarkdown.bas:68)
- **CONVERT.HTML(x1)** — `PRINT "htmllen="; LEN(CONVERT.HTML(h))` (shared/samples/Documents/ConvertFullCoverage.bas:37)
- **CONVERT.IMAGES(x1, x2)** — `PRINT "images="; CONVERT.IMAGES(h, 0, "work")` (shared/samples/Documents/ConvertFullCoverage.bas:40)
- **CONVERT.LINKS(x1)** — `PRINT "links="; UBOUND(CONVERT.LINKS(d)) + 1` (shared/samples/Documents/ConvertFullCoverage.bas:19)
- **CONVERT.MARKDOWN(x1)** — `PRINT "mdlen="; LEN(CONVERT.MARKDOWN(h))` (shared/samples/Documents/ConvertFullCoverage.bas:36)
- **CONVERT.MDIMAGES(x1)** — `PRINT "mdimages="; UBOUND(CONVERT.MDIMAGES(d)) + 1` (shared/samples/Documents/ConvertFullCoverage.bas:20)
- **CONVERT.METADATA(x1)** — `PRINT "metalen="; LEN(CONVERT.METADATA(h))` (shared/samples/Documents/ConvertFullCoverage.bas:38)
- **CONVERT.OPEN(x1)** — `DIM h = CONVERT.OPEN("../Fixtures/sample.pdf")` (shared/samples/Documents/ConvertFullCoverage.bas:33)
- **CONVERT.PAGECOUNT(x1)** — `PRINT "pagecount="; CONVERT.PAGECOUNT(h)` (shared/samples/Documents/ConvertFullCoverage.bas:34)
- **CONVERT.PARSE(x1)** — `d = CONVERT.PARSE(text$)` (shared/samples/Documents/StaticSiteFromMarkdown.bas:66)
- **CONVERT.QUERY(x1, x2)** — `PRINT "query="; UBOUND(CONVERT.QUERY(d, "heading[level=2]")) + 1` (shared/samples/Documents/ConvertFullCoverage.bas:23)
- **CONVERT.RENDER(x1, x2, x3)** — `PRINT "render="; CONVERT.RENDER(h, 0, 100, "work/sample.png")` (shared/samples/Documents/ConvertFullCoverage.bas:41)
- **CONVERT.RENDERHTML(x1)** — `html$ = CONVERT.RENDERHTML(d)` (shared/samples/Documents/StaticSiteFromMarkdown.bas:73)
- **CONVERT.RENDERTEXT(x1)** — `PRINT "rendertext="; CONVERT.RENDERTEXT(d)` (shared/samples/Documents/ConvertFullCoverage.bas:26)
- **CONVERT.SEARCH(x1, x2)** — `PRINT "search="; LEN(CONVERT.SEARCH(h, "the"))` (shared/samples/Documents/ConvertFullCoverage.bas:39)
- **CONVERT.STRIP(x1)** — `PRINT "strip="; CONVERT.STRIP(md$)` (shared/samples/Documents/ConvertFullCoverage.bas:29)
- **CONVERT.TABLES(x1)** — `PRINT "tables="; UBOUND(CONVERT.TABLES(d)) + 1` (shared/samples/Documents/ConvertFullCoverage.bas:21)
- **CONVERT.TEXT(x1)** — `PRINT "textlen="; LEN(CONVERT.TEXT(h))` (shared/samples/Documents/ConvertFullCoverage.bas:35)
- **CONVERT.TOHTML(x1)** — `mdResult = CONVERT.TOHTML(fx + "data.xlsx")` (shared/samples/Documents/ConvertBasics.bas:16)
- **CONVERT.TOMD(x1)** — `mdResult = CONVERT.TOMD(fx + "report.docx")` (shared/samples/Documents/ConvertBasics.bas:12)
- **CONVERT.TOTEXT(x1)** — `text = CONVERT.ToText(path)` (shared/samples/Browser/BrowserD4.bas:26)
- **CONVERT.TREE(x1)** — `PRINT "tree="; CONVERT.TREE(d)` (shared/samples/Documents/ConvertFullCoverage.bas:24)
- **CONVERT.VALIDATE(x1)** — `PRINT "validate="; CONVERT.VALIDATE(md$)` (shared/samples/Documents/ConvertFullCoverage.bas:28)
- **CONVERT.WEB(x1)** — `PRINT "web="; CONVERT.WEB("http://127.0.0.1:1/x")` (shared/minitests/convert/convert_web.bas:8)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Variables-Scope-Spec

*Fonte: `Variables-Scope-Spec`*

| Funzionalità / Comportamento       | GLOBAL        | STATIC    | DIM                                | LOCAL              | LET                          | Implicita                     | FOR I=... |
|------------------------------------|---------------|-----------|------------------------------------|--------------------|------------------------------|-------------------------------|-----------|
| Azione Principale                  | Dichiarazione | Dich.     | Dich.                              | Dich.              | Assegnazione                 | Assegnazione                  | Dich.     |
| Scope di Creazione                 | Radice        | Procedura | Contestuale (Namespace/Proc/Blocco)| Procedura o Blocco | Scope Corrente (Proc/Blocco) | Scope Corrente (se non trova) | Blocco    |
| Mantiene valore tra chiamate?      | Sì            | Sì        | No                                 | No                 | No                           | No                            | No        |
| Cerca e modifica variabile esterna?| No            | No        | No                                 | No                 | No                           | Sì                            | No        |
| Crea "ombra" (shadowing)?          | No            | No        | Sì                                 | Sì                 | Sì                           | No                            | Sì        |
|------------------------------------|---------------|-----------|------------------------------------|--------------------|------------------------------|-------------------------------|-----------|

### DirectCast-Guida-Utente

*Fonte: `DirectCast-Guida-Utente`*


## Sintassi

DIRECTCAST(espressione, Tipo)

| Parametro | Descrizione |
|---|---|
| `espressione` | qualsiasi espressione: stringa, numero, valore boxed/dinamico (`ANY`, risultato di funzione dinamica) |
| `Tipo` | keyword di tipo target (vedi tabella) |

## Semantica per tipo target
| Tipo target | Comportamento |
|---|---|
| `LONG` / `INTEGER` / `DOUBLE` / `SINGLE` / `CURRENCY` | **coercizione numerica**: una stringa è convertita (`"3.5"` → `3.5`, stessa semantica di `VAL`); un numero resta sé stesso; un valore boxed è estratto |
| `STRING` | identità se l'espressione è già stringa; estrae il testo da un valore boxed |
| `LIST` / `DICT` / `HASHSET` / `QUEUE` / `STACK` | estrae la collezione da un valore boxed (dynamic) |
| `REGEX` / `STRINGBUILDER` | estrazione dell'oggetto (sulle backend C: handle) |

## Esempi

DIM n AS LONG
DIM d AS DOUBLE
DIM s AS STRING

n = DIRECTCAST("42", LONG)        ' 42
d = DIRECTCAST("3.5", DOUBLE)     ' 3.5
s = DIRECTCAST("abc", STRING)     ' "abc"

IF ABS(DIRECTCAST("3.5", DOUBLE) - 3.5) < 0.0001 THEN PRINT "double ok"
IF DIRECTCAST("42", LONG) = 42 THEN PRINT "long ok"


## Disponibilità e parità (4/4)

## Riferimenti

### NullConditionalAccess-Guida-Utente

*Fonte: `NullConditionalAccess-Guida-Utente`*


## Panoramica

## Sintassi

TYPE Contatto
    Nome AS STRING * 32
    Eta  AS INTEGER
END TYPE

DIM c AS Contatto?      ' tipo NULLABLE — DIM ... AS <Tipo>?
c = NIL
PRINT c?.Nome            ' "" — base NIL, nessun errore
PRINT c?.Eta              ' "" — idem, anche per un campo numerico

c = NEW Contatto()
c.Nome = "Mario" : c.Eta = 30
PRINT c?.Nome            ' Mario
PRINT c?.Eta              '  30


DIM c AS Contatto?
c = NIL
PRINT c.Nome    ' ERRORE — dereferenzia NIL. Usare c?.Nome per l'accesso sicuro.


## Parità Backend
| Backend | `c?.campo` su NIL → `""` | `c?.campo` su istanza reale | Parità |
|---------|:-:|:-:|:-:|
| IL | ✓ | ✓ | 5/5 |
| PCodeVM | ✓ | ✓ | 5/5 |
| VM/C | ✓ | ✓ | 5/5 |
| C99Native | ✓ (fix 2026-09-21) | ✓ (fix 2026-09-21) | 5/5 |

## Storia (bug risolti)

## MiniTest

tools/sh/test_minitests_parity.sh constructs/expr_nullconditional            # IL/PCode/VM-C (xUnit)
# [storico] gate C99Native rimosso il 2026-10-25 — oggi: tools/sh/test_minitests_c99native
GMB_MINI_ONLY="constructs/expr_nullconditional" tools/sh/test_minitests_c99native   # PASS (fix 2026-09-21)


## Sample

## Riferimenti

### TYPEINFO-Riflessione-Guida-Utente

*Fonte: `TYPEINFO-Riflessione-Guida-Utente`*


## Funzioni
| Funzione | Ritorna | Semantica |
|---|---|---|
| `TYPEINFO(v)` o `TYPEINFO.TYPEINFO(v)` | STRING | nome del tipo del valore: `"NIL"`, `"STRING"`, `"BIT"`, `"DOUBLE"`, `"LONG"`, `"LIST"`, `"DICT"`, il nome del TYPE per un UDT (`"Person"`). I numerici sono `"DOUBLE"` (in GMBasic i numerici sono double). Una variabile nullable vale `"NIL"` quando non ha valore. |
| `TYPEINFO.HAS(v)` | BIT (`1`/`0`) | `1` se il valore non è NIL, `0` se NIL (anche per una variabile nullable rilasciata). |
| `METHODINFO(tipo [, membro])` | STRING (JSON) | metodi del tipo registrato, o `{"membro":"tipo"}` se il membro è indicato; `{}` se il tipo non è registrato (il runtime C non ha il registro C#). |
| `PROPERTYINFO(tipo [, membro])` | STRING (JSON) | come `METHODINFO`, per le proprietà/campi. |

IMPORT TYPEINFO

TYPE Person
  Name AS STRING * 32
  Age AS INTEGER
END TYPE

SUB Main()
  DIM p AS Person
  DIM n AS INTEGER?
  n = 7
  PRINT TYPEINFO(42)              ' DOUBLE
  PRINT TYPEINFO("hello")         ' STRING
  PRINT TYPEINFO(p)               ' Person
  PRINT TYPEINFO(n)               ' DOUBLE
  PRINT TYPEINFO.HAS(n)           '  1


## Tipi nullable (`DIM x AS <scalare>?`)
| Sintassi | Significato |
|---|---|
| `DIM x AS INTEGER?` / `DOUBLE?` / `STRING?` | dichiarazione (parte da `NIL`) |
| `DIM x AS INTEGER? = 100` | dichiarazione con valore iniziale |
| `x = <valore>` | assegna un valore |
| `x = NIL` | rilascia il valore |
| `x HAS VALUE` | vero se ha un valore |
| `x IS NIL` | vero se è `NIL` |
| `x.VALUE` | il valore sottostante (numerico o stringa) |

DIM n AS INTEGER?
IF n IS NIL THEN PRINT "nessun valore"
n = 42
IF n HAS VALUE THEN PRINT n.VALUE          ' 42
n = NIL
IF NOT (n HAS VALUE) THEN PRINT "rilasciato"


## Introspezione di layout: `SIZEOF` / `UDTPACKED` / `UDTALIGNED`
| funzione | ritorna | esempio |
|---|---|---|
| `SYSTEM.UDTPACKED(v)` | **numero di campi** del TYPE di `v` (0 se il tipo non è risolvibile) | `Points` con 2 campi → `2` |
| `SYSTEM.UDTALIGNED(v)` | **1** (costante: i TYPE non-PACKED sono allineati) | → `1` |
| `SIZEOF(x)` | **byte del tipo STATICO** di `x`; **-1** se non esiste un layout statico | vedi sotto |

TYPE UnpackedRecord
    id AS LONG
    flag AS BIT
    v AS DOUBLE
END TYPE

PACKED TYPE PackedRecord
    id AS LONG
    flag AS BIT
    v AS DOUBLE
END TYPE

DIM r AS UnpackedRecord
PRINT SIZEOF(UnpackedRecord)    ' 24  allineamento naturale, taglia multipla di 8
PRINT SYSTEM.UDTPACKED(r)      ' 3   numero di campi
PRINT SYSTEM.UDTALIGNED(r)     ' 1

| classe di `SIZEOF(arg)` | risultato |
|---|---|
| variabile dichiarata (`DIM`, parametro, campo) | dimensione del tipo statico |
| nome di `TYPE` / nome di tipo scalare | layout UDT / larghezza (keyword `STRING` senza `* n` → -1) |
| letterale numerico | 8 (il `DOUBLE` di default) |
| variabile implicita | 8 se numerica, -1 se dinamica (stringa/array/collezione/`CLASS`); suffisso `$` → -1, `% ! #` restano 8 |
| ogni altra forma: espressione aritmetica, chiamata, membro, indice, **nome nudo di funzione/costante built-in** (`DATE`, `TIME`, `NOW`, `PI`, `LEN`, `VAL`, `FREEFILE`, …) | **-1** |

## Copertura e verifica (R1–R4)

## Riferimenti

### Types-NamedTuple-Guida-Utente

*Fonte: `Types-NamedTuple-Guida-Utente`*


## Panoramica

## Sintassi

### Creazione

DIM t = (x: 10, y: 20)           ' interamente nominato
DIM t = (10, 20)                  ' interamente anonimo (come prima)
DIM t = (10, y: 20, 30)          ' mix


### Accesso per nome

DIM t = (x: 10, y: 20, z: 30)
PRINT t.x      ' 10 — il Linker risolve a t(0)
PRINT t.y      ' 20 — il Linker risolve a t(1)
PRINT t.z      ' 30 — il Linker risolve a t(2)


### Accesso per indice (resta valido)

PRINT t(0)     ' 10
PRINT t(1)     ' 20
PRINT t(2)     ' 30


### Destrutturazione

DIM t = (x: 10, y: 20, z: 30)
DIM (a, b, c) = t     ' a=10, b=20, c=30
PRINT a               ' 10


### Swap

DIM (px, py) = (1, 2)
(px, py) = (py, px)   ' px=2, py=1


### Ritorno multiplo da FUNCTION

FUNCTION GetBounds() AS (lo AS DOUBLE, hi AS DOUBLE)
    RETURN (lo: 3, hi: 99)
END FUNCTION

DIM result = GetBounds()
PRINT result.lo    ' 3
PRINT result(0)    ' 3 (stessa cosa)


## Come funziona (compilazione)

## Parità Backend
| Backend | Tuple nominati | Accesso `.x` | Destrutturazione | Swap | Stringhe | Nested | Parità |
|---------|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| IL | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | 5/5 |
| PCodeVM | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | 5/5 |
| VM/C | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | 5/5 |
| C99Native | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | 5/5 |

## Forme estese (risolte)

## MiniTest

# Esegui i minitests delle tuple nominate
GMB_MINI_ONLY=named_tuple dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests

# Esegui un singolo minitest
GMB_MINI_ONLY=named_tuple_field_access dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests


## Sample

### Formattazione in interpolazione/concatenazione
| Tipo | Resa | Esempio |
|---|---|---|
| Numero | G15, cultura invariante (punto decimale) | `$"{245/6}"` → `40.8333333333333` |
| Intero | senza decimali | `$"{10}"` → `10` |
| Tupla/collezione | `[e0, e1, ...]` | `$"{pair}"` → `[10, 20]` |
| Stringa | così com'è | `$"{name}"` → `Alice` |

## Riferimenti

### Variables-Scope-Guida-Utente

*Fonte: `Variables-Scope-Guida-Utente`*


## Dichiarazione

## Scope

## Scope dinamico (`DYNAMIC`)

SUB Profondo()
    PRINT messaggio            ' vede la DYNAMIC di Main (cascata)
    messaggio = "modificato"   ' modifica QUELLA di Main
END SUB

SUB Intermedio()
    PRINT messaggio            ' "originale"
    DYNAMIC messaggio = "locale"   ' shadowing: nuova DYNAMIC per questa procedura
    messaggio = "cambiata"
    PRINT messaggio            ' "cambiata" (sulla locale)
END SUB                         ' uscita: la DYNAMIC locale è distrutta

SUB Main()
    DYNAMIC messaggio = "originale"
    Intermedio()
    PRINT messaggio            ' "originale" (ripristino automatico)

| Regola | Effetto |
|---|---|
| **Cascata** | una procedura chiamata vede e modifica la `DYNAMIC` del chiamante, senza passarla come parametro |
| **Shadowing** | dichiarare `DYNAMIC x` in una sottoprocedura nasconde quella del chiamante finché non ritorna |
| **Ripristino** | all'uscita dalla procedura la sua `DYNAMIC` è distrutta e torna visibile quella esterna |

## Convenzioni

## Tipi nullable (`DIM x AS <scalare>?`)
| Sintassi | Significato |
|---|---|
| `DIM n AS INTEGER?` / `DOUBLE?` / `STRING?` | dichiarazione (parte da `NIL`) |
| `DIM n AS INTEGER? = 100` | dichiarazione con valore iniziale |
| `n = 42` / `n = NIL` | assegna un valore / rilascia |
| `n HAS VALUE` | vero se ha un valore |
| `n IS NIL` | vero se è `NIL` |
| `n.VALUE` | il valore sottostante (numerico o stringa) |

DIM n AS INTEGER?
IF n IS NIL THEN PRINT "nessun valore"
n = 42
IF n HAS VALUE THEN PRINT n.VALUE          ' 42
n = NIL
IF NOT (n HAS VALUE) THEN PRINT "rilasciato"


## Variabili omonime di funzioni builtin

<!-- END SURFACE -->
