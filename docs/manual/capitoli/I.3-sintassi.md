# I.3 — Sintassi e struttura del programma

## Cosa ottieni da questo capitolo

Le regole che rendono un programma compilabile: dove sta il codice, come si dichiara una
variabile, come si distingue un comando da una funzione, come si scrive un ciclo e come si
leggono i messaggi di errore del compilatore quando si sbaglia una di queste regole.

## 1. Anatomia di un file

Un file `.bas` contiene, in quest'ordine:

1. **righe di commento** (`'` oppure `REM`), che il compilatore ignora;
2. **codice di livello modulo**: dichiarazioni, assegnazioni, chiamate, eseguito prima di `Main`;
3. **definizioni** di `SUB` e `FUNCTION`, nell'ordine che preferisci.

Il codice di livello modulo è una particolarità da conoscere subito: non è ignorato, e le sue
variabili sono visibili dentro le procedure.

```basic
' shared/samples/Modules/ModuleLevelInit.bas (estratto)
DIM BASE_PATH AS STRING
DIM COUNTER AS INTEGER
BASE_PATH = "/tmp/gmbasic-module"
COUNTER = 41
PRINT "init modulo: BASE_PATH="; BASE_PATH; " COUNTER="; COUNTER

SUB Main()
    PRINT "in SUB Main: BASE_PATH="; BASE_PATH
    PRINT "in SUB Main: COUNTER+1="; COUNTER + 1
END SUB
```

Output reale, eseguito sul backend C99Native:

```sh
bash tools/sh/gmb_c99native_run.sh ../shared/samples/Modules/ModuleLevelInit.bas mlv ""
```

```
init modulo: BASE_PATH=/tmp/gmbasic-module COUNTER= 41
in SUB Main: BASE_PATH=/tmp/gmbasic-module
in SUB Main: COUNTER+1= 42
module_ok
```

Un programma **senza** `SUB Main` esegue il solo livello modulo, come in `HelloNative.bas` del
capitolo I.1: è la forma più corta possibile.

## 2. Righe, commenti, continuazioni

| Elemento | Forma | Note |
|---|---|---|
| commento | `' testo` oppure `REM testo` | fino a fine riga |
| continuazione | `_` a fine riga | la riga logica prosegue |
| separatore | `:` | due istruzioni sulla stessa riga |
| etichetta | numero o nome di label | facoltativa, per `GOTO`/`GOSUB` |

Il linguaggio è **case-insensitive**: `Print`, `PRINT` e `print` sono lo stesso comando. La
convenzione del libro è maiuscolo per keyword e comandi, minuscolo per i nomi che scegli tu.

## 3. Comandi e funzioni: la regola che non si deroga

Questa è la distinzione più importante del linguaggio, e sbagliarla produce errori che sembrano
di altra natura.

| | **Comando** | **Funzione** |
|---|---|---|
| Cosa fa | compie un'azione | restituisce un valore |
| Parentesi | **mai** | **sempre** |
| Dove sta | riga autonoma | dentro un'espressione |
| Esempio | `DBF USE "work/clienti.dbf" ALIAS C` | `n = DBF.LASTREC()` |

Da qui due conseguenze pratiche:

- un comando **non ha forma qualificata**: `DBF.USE(...)` non esiste;
- una funzione **non si usa come istruzione**: `DBF.LASTREC()` da solo non è una riga valida.

Il namespace (`DBF`, `ISAM`, `AI`, `TORCH`, `TRAIN`, …) è la prima parola di un comando e il
prefisso di una funzione. È lo stesso dominio visto dai due lati: il comando scrive, la funzione
legge. `AI SET ACTIVE PROVIDER %CHAT, nome` imposta, `AI.ACTIVE(%CHAT)` legge.

Alcuni comandi sono **polimorfici**: lo stesso nome (`CLOSE`, `GET`) ha più registrazioni e il
compilatore sceglie quella giusta in base al contesto risolto, non alla grafia. Non serve
nessuna sintassi speciale: si scrive il comando e il parser fa il resto.

## 4. Variabili, tipi e suffissi

I tipi di base:

| Famiglia | Tipi |
|---|---|
| numerica intera | `BYTE`, `SBYTE`, `INTEGER`, `UINTEGER`, `LONG`, `ULONG`, `QUAD`, `UQUAD` |
| numerica reale | `SINGLE`, `DOUBLE`, `CURRENCY` |
| testo | `STRING` |
| logica | `BOOLEAN` |
| data e ora | `DATE` |
| contenitori | array, `LIST`, `DICT`, UDT, classi |

`DIM` dichiara. Con più variabili sulla stessa riga esistono due forme di `AS`:

```basic
DIM i AS INTEGER, r AS STRING     ' AS per variabile: ognuna ha il suo tipo
DIM a, b AS STRING                ' AS condiviso: vale per tutte quelle senza tipo proprio
DIM x$, y AS INTEGER              ' i suffissi convivono con AS
```

Verificato sul backend C99Native con `shared/minitests/constructs/dim_multivar_shared_as.bas`,
che stampa `shared_ok`:

```basic
DIM a, b AS STRING
a = "x"
b = "y"
DIM n, m AS INTEGER
n = 3
m = 4
DIM c, d AS DOUBLE
c = 1.5
d = 2.5
DIM ok AS INTEGER = 1
IF a <> "x" OR b <> "y" THEN ok = 0
IF n <> 3 OR m <> 4 THEN ok = 0
IF c <> 1.5 OR d <> 2.5 THEN ok = 0
IF ok = 1 THEN PRINT "shared_ok" ELSE PRINT "shared_FAIL"
```

### Il suffisso di tipo è un'annotazione, non parte del nome

`S` e `S$` sono **la stessa variabile**. Il suffisso (`$ % & ! #`) dichiara una famiglia di tipo,
e il compilatore rifiuta le scritture incoerenti:

```basic
DIM s AS STRING
s$ = "ciao"        ' OK: stessa famiglia

DIM u AS STRING
u% = 5             ' ERRORE: 'u' è STRING, 'u%' implica INTEGER
```

Il messaggio è esplicito e vale la pena leggerlo per intero, perché dice esattamente dove sta il
problema:

```
La variabile 'X' e' di tipo <famiglia>, ma il nome 'X$' ha il suffisso di tipo <famiglia>:
sono la STESSA variabile (il suffisso non fa parte del nome), quindi la scrittura e' ambigua.
Usare un nome distinto o togliere il suffisso.
```

In **lettura** il suffisso è decorativo: `PRINT s$` su una variabile numerica stampa il numero,
perché vince il tipo della variabile. L'errore scatta solo in scrittura.

## 5. Controllo di flusso

```basic
IF condizione THEN
    ...
ELSEIF altra THEN
    ...
ELSE
    ...
END IF

FOR i = 1 TO 10 STEP 2
    ...
NEXT i

WHILE condizione
    ...
WEND

DO
    ...
LOOP UNTIL condizione

SELECT CASE valore
    CASE 1, 2
        ...
    CASE ELSE
        ...
END SELECT
```

`FOR` accetta `EXIT FOR` e `CONTINUE FOR`. `ON ERROR GOTO` / `RESUME` gestiscono gli errori di
runtime (capitolo III.14).

### Incremento e decremento

`++` e `--` funzionano sia come istruzione sia dentro un'espressione, e la posizione cambia il
valore restituito:

| Forma | Effetto | Valore dell'espressione |
|---|---|---|
| `++x` (prefisso) | incrementa di 1 | il **nuovo** valore |
| `x++` (postfisso) | incrementa di 1 | il **vecchio** valore |
| `--x` / `x--` | come sopra, sottraendo 1 | idem |

Funzionano su variabili locali, campi di classe (nudi o `SELF.`), elementi di array 1-D e 2-D e
parametri. L'esempio completo è `shared/samples/ControlFlow/IncrementDecrement.bas`, eseguito
sul backend C99Native; questo è l'output reale:

```
i= 7
i= 6
old= 6  nuovo= 8  i= 8
aggiorna= 11
pre= 12  valore= 12
post= 12  valore= 13
media= 8.5
n= 6  tot= 21
cont0= 1  cont4= 1  griglia= 9
catturato= 1  cont2= 2
```

Le righe che contano: `old = i++` prende il valore vecchio e poi incrementa; `nuovo = ++i`
incrementa e restituisce il nuovo; `pre` e `post` mostrano la stessa differenza sui campi di una
classe; `catturato = cont(2)++` cattura `1` e lascia `2` nell'array.

## 6. Array, matrici, liste

| Cosa | Sintassi | Nota |
|---|---|---|
| array 1-D inferito | `DIM a() = {1, 2, 3}` | dimensione dal letterale |
| array 1-D dimensionato | `DIM a(5) = {1,2,3}` | dimensione esplicita |
| matrice 2-D dichiarata | `M = {{1,2,3},{4,5,6}}` | graffe o quadre |
| matrice implicita | `A = {{10,20},{30,40}}` | solo graffe, dimensione inferita |
| lista | `A = [1, 2, 3]` | fuori da un `DIM` di array, `[...]` è una lista |

Il letterale di matrice deve essere **rettangolare**: righe di lunghezza diversa sono un errore.
Gli elementi si leggono e si scrivono sia con le tonde sia con le quadre: `A(1, 0)` e `A[1, 0]`.
I limiti si chiedono con `UBOUND(A)` e `UBOUND(M, 2)`; `LBOUND` dà il primo indice.

Attenzione alla trappola: `A = [[1,2],[3,4]]` su una variabile **non dichiarata** è una
**lista di liste** (accesso `A[i][j]`), non una matrice. Per avere una matrice implicita servono
le graffe. Dentro un `DIM` che dichiara un array, invece, quadre e graffe sono equivalenti.

## 7. Simboli di questo capitolo

| Simbolo | Forma | Note |
|---|---|---|
| `DIM` | comando | dichiarazione; `AS` per variabile o condiviso |
| `SUB` / `FUNCTION` | dichiarazione | procedure; la seconda restituisce un valore |
| `IF` / `ELSEIF` / `ELSE` / `END IF` | blocchi | `THEN` obbligatorio |
| `FOR` / `NEXT` | blocco | `STEP`, `EXIT FOR`, `CONTINUE FOR` |
| `WHILE` / `WEND`, `DO` / `LOOP` | blocchi | condizioni in testa o in coda |
| `SELECT CASE` / `END SELECT` | blocco | `CASE` multiplo con la virgola |
| `UBOUND`, `LBOUND` | funzioni | limiti di array e matrici |
| `++`, `--` | operatori | prefisso e postfisso, su l-value |

## 8. Errori frequenti

| Messaggio o sintomo | Causa | Correzione |
|---|---|---|
| `La variabile 'X' e' di tipo …, ma il nome 'X$' ha il suffisso …` | suffisso di famiglia diversa dalla variabile | togliere il suffisso o usare un nome distinto |
| `Expected …` su una riga con `DBF.USE(...)` | forma qualificata di un comando | `DBF USE ...`, senza parentesi |
| matrice con righe di lunghezza diversa | letterale non rettangolare | allineare le righe |
| `A[i][j]` su una matrice dichiarata | accesso da lista di liste | usare `A(i, j)` o `A[i, j]` |
| variabile vuota o `0` dentro `Main` | aspettativa sbagliata sul livello modulo | il livello modulo gira **prima**, non dopo |
| `i` non incrementato da `x = i++` | postfisso restituisce il vecchio valore | usare `++i` se serve il nuovo |

## 9. Dove andare adesso

- **II.4** — grammatica e lessico: come il compilatore legge davvero queste righe.
- **II.5** — tipi, variabili e scope in dettaglio.
- **II.7** — controllo di flusso, con tutti i casi di `SELECT CASE` e dei cicli.
- **II.9** — array, collezioni e stringhe.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Sintassi-Base-Guida-Utente

*Fonte: `Sintassi-Base-Guida-Utente`*


## Struttura

## Istruzioni e blocchi

## Nomi

## Comandi vs funzioni (regola chiave)

## Tipi

## Operatori di incremento/decremento (`++` / `--`)

## DIM: più variabili nella stessa riga

DIM i AS INTEGER, r AS STRING     ' AS per-variabile
DIM a, b AS STRING                ' AS CONDIVISO: vale per TUTTE le variabili della lista
DIM x$, y AS INTEGER              ' combinabile con i suffissi di tipo ($ = STRING)


## Suffissi di tipo (`$ % & ! #`): annotazione, NON parte del nome

DIM s AS STRING
s$ = "ciao"        ' OK: stesso tipo (STRING)
PRINT s            ' ciao

DIM n AS INTEGER
n% = 42            ' OK: INTEGER, stessa famiglia numerica
PRINT n            ' 42

DIM x AS LONG
x% = 7             ' OK: `%` è INTEGER, ma la famiglia è ancora numerica


DIM s AS SINGLE
s$ = "abc"         ' ERRORE: 's' è numerica, 's$' implica STRING
DIM u AS STRING
u% = 5             ' ERRORE: 'u' è STRING, 'u%' implica INTEGER
v = 1
v$ = "x"           ' ERRORE: 'v' è DOUBLE (implicito), 'v$' implica STRING


## Array e matrici — dichiarazione e letterali

### FUNCTION-Main-ExitCode-Guida-Utente

*Fonte: `FUNCTION-Main-ExitCode-Guida-Utente`*


## 1. Sintassi

FUNCTION Main() AS INTEGER
  ' ...
  RETURN 7
END FUNCTION


SUB Main()
  ' ...
END SUB


## 2. Parametri
| Forma | Exit code del processo |
|---|---|
| `SUB Main()` | `0` |
| `FUNCTION Main() AS INTEGER ... RETURN n` | `n` |
| `END FUNCTION` senza `RETURN` | `0` |

## 3. Semantica

## 4. Esempi

### Successo

FUNCTION Main() AS INTEGER
  PRINT "ok"
  RETURN 0
END FUNCTION


### Codice d'errore applicativo

FUNCTION Main() AS INTEGER
  DIM den AS INTEGER
  den = 0
  IF den = 0 THEN RETURN 2
  PRINT 10 / den
  RETURN 0
END FUNCTION


./programma
echo $?        # 2


### Uscita anticipata

FUNCTION Main() AS INTEGER
  DIM i AS INTEGER
  FOR i = 1 TO 10
    IF i = 5 THEN RETURN 42
    PRINT i
  NEXT i
  RETURN 0
END FUNCTION


### Con `DOEVENTS`

FUNCTION Main() AS INTEGER
  SCREEN 12
  DOEVENTS
  SCREEN 0
  RETURN 7
END FUNCTION


## 5. Verifica di parità

tools/sh/check_exitcode_parity.sh
# PASS IL (7)
# PASS PCode (7)
# PASS VM/C (7)
# PASS C99Native (7)
# EXITCODE PARITY OK


## 6. Errori comuni
| Errore | Correzione |
|---|---|
| `FUNCTION Main(x AS INTEGER)` | `Main` non accetta parametri |
| `RETURN "errore"` in `Main AS INTEGER` | usare un intero; le stringhe non sono exit code |
| aspettarsi `300` dalla shell | su POSIX si osserva `300 mod 256 = 44` |
| dimenticare `RETURN` | vale `0` |

### Modulo-Livello-TopLevel-Guida-Utente

*Fonte: `Modulo-Livello-TopLevel-Guida-Utente`*


## Esempio

' Livello modulo: eseguito prima di SUB Main.
DIM BASE_PATH AS STRING
DIM COUNTER AS INTEGER
BASE_PATH = "/tmp/gmbasic-module"
COUNTER = 41
PRINT "init modulo: COUNTER="; COUNTER

SUB Main()
    PRINT "in SUB Main: BASE_PATH="; BASE_PATH
    PRINT "in SUB Main: COUNTER+1="; COUNTER + 1
END SUB


init modulo: COUNTER= 41
in SUB Main: BASE_PATH= /tmp/gmbasic-module
in SUB Main: COUNTER+1= 42


## Regole

## Nota storica (bug corretto)

## Test ed esempi

<!-- END SURFACE -->
