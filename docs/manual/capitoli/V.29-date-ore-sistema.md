# V.29 — Date, ore e sistema

## Cosa ottieni da questo capitolo

Date come numeri seriali, formattazione, fusi orari, e le informazioni sul sistema su cui gira il
programma: memoria, percorsi, argomenti, variabili. Sono le funzioni che sembrano banali e che
producono i bug più difficili da riprodurre.

## 1. La data è un numero

```basic
=== DATETIME.DATE ===
Current date (serial):  46328
Current date (text):   02/11/2026
=== OK ===
```

Eseguito (`shared/samples/Datetime/DateTimeDate.bas`, backend C99Native). Il numero `46328` è il
**giorno seriale**: un intero che conta i giorni da un'epoca fissa. Serve perché sommare giorni
diventa sommare interi: `serial + 30` è "fra trenta giorni", senza casi speciali per i mesi.

La data in chiaro è `02/11/2026` nel formato locale (`gg/mm/aaaa`). Le due rappresentazioni sono
la stessa cosa: una per calcolare, una per mostrare.

## 2. Formattare e interpretare

| Operazione | Nota |
|---|---|
| data seriale ↔ testo | il formato dipende dal locale |
| `CTOD` / `DTOC` | le funzioni Clipper, con `SET EPOCH` per gli anni a due cifre |
| componenti | anno, mese, giorno, giorno della settimana |
| differenza | sottrazione fra seriali = giorni |

`SET EPOCH` è la trappola storica: `01/01/30` è il 1930 o il 2030? La risposta dipende da quella
impostazione, e un programma che non la fissa cambia comportamento fra due macchine.

## 3. Ore e istanti

Un **istante** non è una data: ha ora, minuti, secondi e spesso millisecondi. `TICKS` dà i
millisecondi trascorsi, `NOW`/`TIME` l'ora corrente. Per misurare una durata si usa `TICKS`
(sottrazione), mai l'ora del sistema — che può cambiare per un aggiustamento di orologio.

## 4. Il sistema: memoria e limiti

```
Memoria di sistema (SYSTEM.SYS)
==============================
RAM totale          : 131072 MB
Disponibile+reclam. : 92873 MB
Usata               :  38198.125  MB
mem_check: ok
Esempio: un modello che richiede 99 GB NON entra con  90.6971435546875  GB disponibili.
Test completato.
```

Eseguito (`shared/samples/System/SystemMemoryInfo.bas`). Tre informazioni: RAM totale, memoria
disponibile (inclusa quella reclamabile), memoria usata. Il sample fa poi la domanda che conta per
i capitoli V.32-V.35: **ci sta?** Un modello da 99 GB su 90 GB disponibili non entra, e scoprirlo
dopo il caricamento significa un processo ucciso dal sistema.

Guida: `System-SYS-Memoria-Guida-Utente`.

## 5. Argomenti, percorsi, variabili

| Funzione | Cosa dà | Sample |
|---|---|---|
| argomenti della riga di comando | il vettore degli argomenti | `System/SystemCommandArgs.bas` |
| percorsi e cartelle | cartella del programma, dati, cache | `System/SettingsPaths.bas` |
| variabili esistenti | lettura dell'ambiente | `System/SystemMiscellaneous.bas` |
| costanti di sistema | nomi canonici delle piattaforme | `System/SystemConstants.bas` |
| layout di una struct | dimensione e offset dei campi | `System/SystemSizeOfUdtLayout.bas` |

I **percorsi** meritano una nota: `~/.gmbasic/` è la radice dei dati utente (cache dei pacchetti,
librerie native), e il programma non deve mai scrivere nella cartella di installazione. Le regole
sono in `Paths-Config-User-Guide` e nel capitolo VI.38.

## 6. Basso livello

`System/SystemLowLevel.bas` e `SystemLowLevelCanon.bas` coprono le chiamate che scendono sotto il
linguaggio (identificatori di processo, segnali, ambiente). Sono potenti e non portabili: ogni uso
va giustificato, e ogni differenza fra piattaforme va documentata.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| date a due cifre che cambiano secolo | `SET EPOCH` non impostato | fissalo all'avvio |
| differenza di giorni sbagliata di uno | ora inclusa nel calcolo | lavora sui seriali, non sugli istanti |
| durata negativa | orologio aggiustato durante la misura | usa `TICKS` monotono |
| file scritto in una cartella di sistema | percorso fisso nel codice | usa le funzioni dei percorsi |
| memoria "sufficiente" e processo ucciso | disponibile ≠ allocabile | lascia margine, controlla prima |

## 8. Dove andare adesso

- **VI.38** — configurazione e percorsi in dettaglio.
- **IV.22** — scrivere file nella cartella giusta.
- **V.35** — decidere se un modello entra in memoria.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### System-Guida-Utente

*Fonte: `System-Guida-Utente`*


## 1. Runtime e ambiente

IMPORT SYSTEM

PRINT SYSTEM.NUMCPU()              ' numero di CPU
PRINT SYSTEM.NUMGOROUTINE()        ' goroutine attive
PRINT SYSTEM.GOMAXPROCS()          ' processori GPM (0 = legge, >0 = imposta)
PRINT SYSTEM.SYS("SETTINGSPATH")   ' variabile/percorso di sistema
PRINT SYSTEM.SYS("MEMAVAILABLEMB") ' RAM disponibile+reclamabile in MB (API native del SO)
PRINT SYSTEM.SYS("MEMTOTALMB")     ' RAM totale in MB
PRINT SYSTEM.COMMANDCOUNT()        ' numero di argomenti da riga di comando
PRINT SYSTEM.COMMAND(1)            ' primo argomento (1-based; fuori range → "")


## 2. Utilità scalari

PRINT SYSTEM.CHOOSE(2, "a", "b", "c")  ' seleziona per indice (1-based)

### `SYSTEM.CHOOSE` — selezione per indice




- **Indice 1-based**: `CHOOSE(1, …)` è il **primo** valore.
- Indice **fuori intervallo** → `NIL`.
- Il **tipo** del risultato segue il valore scelto (numerico o stringa), quindi si può usare
  direttamente in un'espressione.
- La forma variadica è quella idiomatica:




⚠️ **Nota implementativa (non serve all'utente, ma spiega la parità)**: il compilatore può
impacchettare i valori variadici in contenitori annidati (catena fino a ~10 livelli quando ci sono
istruzioni precedenti). La backend VM/C gestisce la catena con un *flatten* (fix `7fd90098f`); il
MiniTest `shared/minitests/system/choose.bas` copre **entrambi** i casi (semplice e con contesto
precedente) per evitare regressioni.
PRINT SYSTEM.IIF(1 > 0, "sì", "no")    ' if inline
PRINT SYSTEM.EMPTY(x)                  ' -1 se vuoto
PRINT SYSTEM.SIZEOF(x)                 ' dimensione in byte del TIPO STATICO di x → §SIZEOF
PRINT SYSTEM.TYPENAME(x)               ' nome del tipo
PRINT SYSTEM.VALTYPE(x)                ' tipo del valore
s$ = SYSTEM.CTYPE(x, "STRING")         ' coercizione: "STRING"→testo, "INTEGER"/"LONG"→intero,
                                       ' "DOUBLE"/"SINGLE"/"FLOAT"→double, "BOOLEAN"/"BIT"→-1/0


### `SYSTEM.SIZEOF` — dimensione in byte del tipo statico

SYSTEM.SIZEOF(arg)        ' forma qualificata
SIZEOF(arg)               ' forma legacy (keyword del linguaggio)

| # | Forma dell'argomento | Risultato |
|---|---|---|
| 1 | variabile **dichiarata** (`DIM`, parametro, campo) | dimensione del suo **tipo statico** |
| 2 | **nome di `TYPE`** | layout dello UDT (`PACKED TYPE` = somma campi; ordinario = allineamento naturale) |
| 2 | **nome di tipo scalare** (keyword) | `BIT/BOOL/BOOLEAN/BYTE/SBYTE` = 1 · `INTEGER/SHORT/INT/UINT/UINTEGER/USHORT` = 2 · `SINGLE` = 4 · `LONG/ULONG/QUAD/UQUAD/DOUBLE/DATETIME/TIMESPAN` = 8 · `CURRENCY` = 16 · `STRING` (senza `* n`) = **-1** |
| 3 | **letterale numerico** | 8 (il `DOUBLE` di default del linguaggio) |
| 4 | variabile **implicita** (senza `DIM`) | 8 se numerica; **-1** se dinamica: stringa, array, collezione, `CLASS`, oggetto. Il suffisso `$` **dichiara** una stringa (→ -1); i suffissi `% ! #` **non** cambiano il tipo statico (→ 8) |
| 5 | qualsiasi **altra forma** | **-1**: espressione aritmetica (`n + 1`), chiamata (`LEN("ciao")`), accesso a membro o indice, letterale di stringa/booleano, e il **nome nudo di una funzione/costante built-in** (`DATE`, `TIME`, `NOW`, `PI`, `LEN`, `VAL`, `FREEFILE`, `EMPTY`, …) |

IMPORT SYSTEM
TYPE T : a AS INTEGER : b AS DOUBLE : END TYPE
DIM i AS INTEGER        ' SIZEOF(i)   = 2
DIM s AS STRING * 10    ' SIZEOF(s)   = 11
DIM dyn AS STRING       ' SIZEOF(dyn) = -1  (dinamica)
PRINT SIZEOF(i), SIZEOF(s), SIZEOF(dyn), SIZEOF(T), SIZEOF(INTEGER), SIZEOF(42)


### `SYSTEM.COMMAND` / `SYSTEM.COMMANDCOUNT` — argomenti da riga di comando

n  = SYSTEM.COMMANDCOUNT()     ' numero di argomenti utente (escluso il programma)
a$ = SYSTEM.COMMAND(1)         ' primo argomento utente


### `ENV$` / `ENV.HAS` — ambiente di processo (sola lettura)

Il namespace `ENV` espone una lettura **esplicita** dell'ambiente del processo al programma
BASIC. È una API di linguaggio: il valore viene letto quando la funzione viene valutata e
non viene memorizzato dal runtime. Il motore continua a usare il **registro opzioni**
(`system.settings.json`, `gmb_opts`, `AI SET OPTION`) per i propri parametri; `ENV$` non è
un canale di configurazione del motore.

Richiede `IMPORT ENV`.

#### Firme

| Forma | Arità | Parametro | Tipo parametro | Ritorno |
|---|---:|---|---|---|
| `ENV$(name)` | 1 | `name` — nome della variabile d'ambiente | `STRING` | `STRING` |
| `ENV.HAS(name)` | 1 | `name` — nome della variabile d'ambiente | `STRING` | `-1` / `0` |

Le due funzioni sono **pure** rispetto allo stato BASIC: non modificano variabili, aree o
configurazione. Non esiste `ENV SET`: l'ambiente si modifica dal processo chiamante (shell,
`export`, service manager) prima dell'avvio.

#### Semantica di `ENV$(name)`

- Restituisce il valore corrente della variabile `name` come stringa.
- Se `name` è vuoto (`""`), la variabile non esiste o non ha valore, restituisce `""`.
- Il valore è restituito **senza trasformazioni**: non viene trimato, non viene interpretato
  come numero e non viene normalizzato.
- Il nome è passato al sistema operativo così com'è: su piattaforme POSIX i nomi sono
  **case-sensitive** (`PATH` ≠ `path`); su Windows sono case-insensitive. Il programma non
  deve assumere una policy uniforme.
- Una variabile presente ma vuota (`NAME=`) è indistinguibile da una variabile assente per
  `ENV$` (entrambe restituiscono `""`); per distinguerle usare `ENV.HAS`, che per una
  variabile presente con valore vuoto restituisce comunque `-1`.

#### Semantica di `ENV.HAS(name)`

- Restituisce `-1` (vero BASIC) se la variabile è **definita** nel processo, `0` (falso) se
  non lo è.
- `name = ""` → `0`.
- È la forma consigliata nei test di presenza e nelle configurazioni con fallback:
  ```basic
  IF ENV.HAS("APP_MODE") THEN
      mode = ENV$("APP_MODE")
  ELSE
      mode = "default"
  END IF
  ```
- Il risultato segue la convenzione booleana del linguaggio: `-1`/`0`, utilizzabile
  direttamente in `IF`, `AND`, `OR`, `NOT`.

#### Parametri, arità e diagnostica

- Entrambe le funzioni richiedono **esattamente un argomento**. Chiamate con zero o due
  argomenti sono **rifiutate in compilazione** su tutti i backend:
  `ENV$()` / `ENV$("A","B")` non producono un programma eseguibile.
- L'argomento è tipizzato `STRING`; un valore non stringa viene convertito secondo le regole
  di chiamata dei builtin (un numero diventa la sua rappresentazione testuale). Per un nome
  dinamico costruire la stringa esplicitamente (`n = "APP_" + suffix`).
- Nomi con caratteri speciali (`=`, spazi, `-`) sono passati al sistema operativo; la
  validità dipende dalla piattaforma. Nomi non validi si comportano come assenti.

#### Esempi

```basic
IMPORT ENV

' Presenza e valore
PRINT ENV.HAS("PATH")          ' -1 sui processi normali
PRINT ENV$("PATH")             ' valore corrente (stringa, anche molto lunga)
PRINT "["; ENV$("NON_ESISTE"); "]"   ' []

' Nome vuoto
PRINT ENV.HAS("")              '  0
PRINT "["; ENV$(""); "]"       ' []

' Nome da variabile
DIM n AS STRING
n = "HOME"
PRINT ENV.HAS(n); " "; LEN(ENV$(n))
```

#### Disponibilità e verifica

Disponibile con semantica identica su **IL, PCodeVM, VM/C, C99Native**.

- MiniTest: `shared/minitests/system/env_read.bas`
  (`EXPECT: present=-1|missing= 0|emptyhas= 0|emptyval=[]|lenvar=-1`), che copre nome
  letterale, nome in variabile, variabile assente, nome vuoto e lunghezza.
- Sample completo: `shared/samples/System/EnvDemo.bas` (loop su più nomi, fallback di
  configurazione, `ENV$`/`ENV.HAS`, nome vuoto).
- Parità verificata con `tools/sh/parity4` sulle quattro gambe e con il gate MiniTest
  xUnit/C99Native.

Per gli **argomenti di riga di comando** il canone resta `SYSTEM.COMMANDCOUNT()` /
`SYSTEM.COMMAND(i)` (1-based, `""` fuori range). Nei runner `--define NOME[=val]` seleziona i
simboli compile-time (`#IFDEF`) senza ricompilare il sorgente a mano.

### `SYSTEM.TYPENAME` / `SYSTEM.VALTYPE` — nome e categoria di un valore

### `SYSTEM.UDTPACKED` / `SYSTEM.UDTALIGNED` — contatori di campi di un TYPE

PACKED TYPE Header : magic AS INTEGER : version AS INTEGER : flags AS BIT : END TYPE
TYPE HeaderN       : magic AS INTEGER : version AS INTEGER : flags AS BIT : END TYPE
DIM h AS Header
DIM hn AS HeaderN
PRINT SIZEOF(h)            ' 5  (PACKED: somma dei campi, nessun padding)
PRINT SIZEOF(hn)           ' 6  (TYPE ordinario: allineamento naturale dei campi)
PRINT SYSTEM.UDTPACKED(hn) ' 3  numero di campi (identico a packed e non packed)
PRINT SYSTEM.UDTALIGNED(hn)' 1


### `SYSTEM.CTYPE` — coercizione di tipo

## 3. Token di identità (`VARPTR`/`STRPTR`)

PRINT SYSTEM.VARPTR(i)     ' token di identita della variabile (mai 0)
PRINT SYSTEM.STRPTR(s)     ' token di identita della stringa (mai 0)


### Sintassi e parametri
| Funzione | Parametro | Tipo | Descrizione |
|---|---|---|---|
| `SYSTEM.VARPTR(x)` | `x` | qualsiasi variabile | token opaco di identità del valore; **1 solo argomento** (0 o 2+ = errore) |
| `SYSTEM.STRPTR(s)` | `s` | `STRING` | token opaco di identità dell'oggetto stringa; **1 solo argomento** |

### Semantica (canone unico 2026-09-30, identico sui 4 backend)

### Esempi

IMPORT SYSTEM
DIM i AS INTEGER = 123
DIM s AS STRING = "Hello"
IF SYSTEM.VARPTR(i) = 0 THEN PRINT "mai qui"
IF SYSTEM.STRPTR(s) <> SYSTEM.STRPTR(s) THEN PRINT "mai qui"
DIM t1 = SYSTEM.VARPTR(i)
' ... altro codice che non tocca i ...
IF SYSTEM.VARPTR(i) <> t1 THEN PRINT "mai qui (stesso oggetto vivo)"


### Verifica e parità

## 4. Processi esterni

PRINT SYSTEM.SHELLOUT("ls -la")   ' esegue e ritorna l'output
PRINT SYSTEM.SHELLCODE("ls")      ' codice di uscita


RUN "echo comando esterno"        ' chain-load via shell (SET SHELL ON richiesto)


## 5. Interazione

SYSTEM.ALERT("Messaggio")
IF SYSTEM.CONFIRM("Procedere?") <> 0 THEN PRINT "ok"


## 6. Orologio di sistema (`TI`/`STRTI`)

DIM t1 = SYSTEM.TI()
SLEEP 100
DIM t2 = SYSTEM.TI()
PRINT (t2 - t1)        ' ~100 (millisecondi)
PRINT SYSTEM.STRTI()   ' "HH:MM:SS" (8 caratteri, es. "10:21:19")


### Sintassi e parametri
| Funzione | Parametri | Ritorno | Descrizione |
|---|---|---|---|
| `SYSTEM.TI()` | **nessuno** (1+ = errore) | `LONG` intero | tick di sistema in **millisecondi** |
| `SYSTEM.STRTI()` | **nessuno** | `STRING` di 8 caratteri | ora corrente nel formato **`HH:MM:SS`** |

### Semantica (canone unico 2026-09-30, identico sui 4 backend)

### Esempi

IMPORT SYSTEM
IMPORT CONCURRENCY
DIM t1 = SYSTEM.TI()
SLEEP 100
DIM t2 = SYSTEM.TI()
IF t2 < t1 THEN PRINT "mai qui (monotono)"
IF (t2 - t1) < 50 THEN PRINT "mai qui (unita ms)"
IF LEN(SYSTEM.STRTI()) <> 8 THEN PRINT "mai qui"


### Verifica e parità

## Vedi anche

### DataOra-Guida-Utente

*Fonte: `DataOra-Guida-Utente`*

| Forma | Tipo | Esempio | Significato |
|---|---|---|---|
| `TIME()` | numero | `46302.9034027778` | seriale data+ora (la parte frazionaria è l'ora del giorno) |
| `DATE()` | numero | `46302` | seriale del solo giorno |
| `STRTIME()`, `TIME$`, `TI$` | stringa | `21:40:54` | ora, formato `HH:MM:SS` |
| `STRDATE()`, `DATE$` | stringa | `07/10/2026` | data, formato `dd/MM/yyyy` |
| `TI()` | numero | `86038726` | **contatore** in millisecondi: ha senso solo la differenza fra due letture |

## Esempio

SUB Main()
    PRINT TIME(); " "; DATE()          ' 46302.9034027778  46302
    PRINT STRTIME(); " "; STRDATE()    ' 21:40:54  07/10/2026
    PRINT TIME$; " "; DATE$            ' stesse stringhe, senza parentesi

    DIM t0 AS LONG = TI()
    SLEEP 100
    PRINT "ms trascorsi: "; TI() - t0
END SUB


## Note

### Datetime-Guida-Utente

*Fonte: `Datetime-Guida-Utente`*


## 1. Costruzione di una data

### `DATETIME.DATEVALUE("YYYY-MM-DD") → data`

### `DATETIME.DATESERIAL(year, month, day) → data`

d = DATETIME.DATESERIAL(1999, 12, 31)
PRINT DATETIME.YEAR(d)   ' 1999
PRINT DATETIME.MONTH(d)  ' 12
PRINT DATETIME.DAY(d)    ' 31


## 2. Lettura delle componenti

d = DATETIME.DATEVALUE("1999-12-31")
PRINT DATETIME.YEAR(d)    ' 1999
PRINT DATETIME.MONTH(d)   ' 12
PRINT DATETIME.DAY(d)     ' 31


## 3. Calcoli

### `DATETIME.DATEADD(interval$, amount, data) → data`

d0 = DATETIME.DATEVALUE("1999-12-31")
r  = DATETIME.DATEADD("D", 5, d0)
PRINT DATETIME.YEAR(r)   ' 2000
PRINT DATETIME.MONTH(r)  ' 1
PRINT DATETIME.DAY(r)    ' 5


### `DATETIME.DATEDIFF(interval$, dataInizio, dataFine) → numero`

PRINT DATETIME.DATEDIFF("D", DATETIME.DATEVALUE("2000-01-10"), DATETIME.DATEVALUE("2000-01-20"))  ' 10
PRINT DATETIME.DATEDIFF("M", DATETIME.DATEVALUE("2000-01-15"), DATETIME.DATEVALUE("2000-03-15"))  ' 2
PRINT DATETIME.DATEDIFF("yyyy", DATETIME.DATEVALUE("1998-06-01"), DATETIME.DATEVALUE("2001-06-01"))  ' 3


### `DATETIME.EOMONTH(data [, mesi]) → data`

d = DATETIME.EOMONTH(DATETIME.DATEVALUE("1999-12-15"))
PRINT DATETIME.YEAR(d)   ' 1999
PRINT DATETIME.MONTH(d)  ' 12
PRINT DATETIME.DAY(d)    ' 31

' con lo spostamento: fine del mese successivo
e = DATETIME.EOMONTH(DATETIME.DATEVALUE("2026-01-15"), 1)
PRINT DATETIME.YEAR(e); DATETIME.MONTH(e); DATETIME.DAY(e)   ' 2026  2  28

| parametro | tipo | significato |
|---|---|---|
| `data` | data/numero (OADate) | data di partenza |
| `mesi` | INTEGER, **opzionale** (default 0) | mesi da aggiungere prima di calcolare la fine mese |

## 4. Ora

### `DATETIME.TIMESERIAL(hour, minute, second) → tempo`

d = DATETIME.TIMESERIAL(23, 45, 30)
PRINT DATETIME.HOUR(d)    ' 23
PRINT DATETIME.MINUTE(d)  ' 45
PRINT DATETIME.SECOND(d)  ' 30


### `DATETIME.DATEADD` su ore/minuti/secondi

t = DATETIME.TIMESERIAL(23, 45, 30)
PRINT DATETIME.HOUR(DATETIME.DATEADD("s", 30, t))    ' 23  (+30s → 23:46:00)
PRINT DATETIME.MINUTE(DATETIME.DATEADD("s", 30, t))  ' 46
PRINT DATETIME.SECOND(DATETIME.DATEADD("s", 30, t))  ' 0
PRINT DATETIME.HOUR(DATETIME.DATEADD("n", 15, t))    ' 0   (+15min → 00:00:30)
PRINT DATETIME.MINUTE(DATETIME.DATEADD("n", 15, t))  ' 0
PRINT DATETIME.SECOND(DATETIME.DATEADD("n", 15, t))  ' 30
PRINT DATETIME.HOUR(DATETIME.DATEADD("h", 1, t))     ' 0   (+1h → 00:45:30)
PRINT DATETIME.MINUTE(DATETIME.DATEADD("h", 1, t))   ' 45
PRINT DATETIME.SECOND(DATETIME.DATEADD("h", 1, t))   ' 30


### Ora corrente

PRINT DATETIME.SECONDS()    ' secondi dalla mezzanotte (0..86399)
PRINT DATETIME.STRTIME()    ' ora corrente "HH:MM:SS"
PRINT DATETIME.TI()         ' timestamp (secondi dall'epoca, con frazione)


### Conversioni e utilità

d = DATETIME.DATEVALUE("2024-01-15")
PRINT DATETIME.CDOW(d)                 ' nome del giorno ("Monday")
PRINT DATETIME.CMONTH(d)               ' nome del mese ("January")
PRINT DATETIME.DTOS(d)                 ' "YYYYMMDD"
PRINT DATETIME.STOD("20240115")        ' data seriale da "YYYYMMDD"
PRINT DATETIME.FIRSTDAYOFMONTH(d)      ' primo giorno del mese
PRINT DATETIME.ISOWEEKOFYEAR(d)        ' numero di settimana ISO
PRINT DATETIME.UNIXTIMESTAMP(d)        ' secondi epoch da una data
PRINT DATETIME.DATEFROMUNIX(1705276800)' data da secondi epoch
PRINT DATETIME.NOW()                   ' data+ora correnti (seriale)
PRINT DATETIME.STRTI()                 ' ora corrente come stringa "HH:MM:SS"


### Stampa di un valore DATA/ORA (`PRINT`)

PRINT "adesso: "; NOW()                      ' adesso: 01/11/2026 18:40:53
DIM d AS DATETIME
d = TODAY()
PRINT "oggi:   "; d                          ' oggi:   01/11/2026 00:00:00


#### `FORMAT` su un valore DATA/ORA: la maschera decide il tipo di resa
| maschera | cosa produce | esempio |
|---|---|---|
| contiene `#` o `0` (maschera **numerica**) | il **seriale OADate** formattato | `FORMAT(d, "0.000000")` → `46024.000000` |
| contiene token di data/ora (`y M d H h m n s t`) | data/ora **leggibile** | `FORMAT(d, "dd/MM/yyyy")` → `02/01/2026` |
| nessuno dei due | formato numerico .NET | `FORMAT(d, "F2")` |

DIM d AS DATE
d = DATETIME.DATESERIAL(2026, 1, 2)
PRINT FORMAT(d, "0.000000")    ' 46024.000000   (seriale)
PRINT FORMAT(d, "dd/MM/yyyy")  ' 02/01/2026     (data)
PRINT FORMAT(d, "0.00%")       ' 4602400.00%    (numerica, percentuale)

DIM t AS TIME
t = DATETIME.TIMESERIAL(3, 4, 5)
PRINT FORMAT(t, "0.000000")    ' 0.127836       (TIME = frazione di giornata)
PRINT FORMAT(t, "HH:mm:ss")    ' 03:04:05


## 5. Predicati (booleani -1/0)

PRINT DATETIME.ISLEAPYEAR(DATETIME.DATEVALUE("2000-02-15"))  ' -1
PRINT DATETIME.ISLEAPYEAR(DATETIME.DATEVALUE("1999-12-31"))  ' 0
PRINT ISDATE("2024-01-15")  ' -1   (solo formato ISO; "15/01/2024" → 0)
PRINT ISDATE("2024-13-01")  ' 0    (mese invalido)
PRINT ISDATE("xyz")         ' 0
PRINT ISNUMERIC("123")      ' -1
PRINT ISNUMERIC("1e3")      ' -1   (esponente)
PRINT ISNUMERIC("0x1F")     ' -1   (esadecimale)
PRINT ISNUMERIC("abc")      ' 0
PRINT ISNUMERIC("")         ' 0


## 6. Verifiche sui backend (2026-09-07)

## Vedi anche

### System-SYS-Memoria-Guida-Utente

*Fonte: `System-SYS-Memoria-Guida-Utente`*


## Proprietà
| Proprietà | Significato |
|---|---|
| `OS` | `MACOS` / `LINUX` / `WINDOWS` |
| `ARCH` | `ARM64` / `X64` / `X86` |
| `UI` | `STANDARD` |
| `VERSION` | `GMBASICAI` |
| `RUNTIME` | runtime del backend (`NATIVE-C`, `.NET …`) |
| `CPUCOUNT` | numero di CPU |
| `MACHINE` | nome host |
| `USER` | utente |
| `CWD` | directory corrente |
| `SETTINGSPATH` | percorso file settings |
| `MEMAVAILABLE` / `MEMFREE` / `MEMRECLAIMABLE` | **byte disponibili+reclamabili** (senza swap) |
| `MEMTOTAL` | byte di RAM totale |
| `MEMUSED` | byte usati (totale − disponibile) |
| `MEMAVAILABLEMB` / `MEMTOTALMB` | valori in **MB** |

## Attendibilità della memoria

## Esempio

IMPORT SYSTEM WITH ALIAS
SUB Main()
    PRINT "RAM totale          : "; SYSTEM.SYS("MEMTOTALMB"); " MB"
    PRINT "Disponibile+reclam. : "; SYSTEM.SYS("MEMAVAILABLEMB"); " MB"
    PRINT "Usata               : "; VAL(SYSTEM.SYS("MEMUSED")) / 1048576; " MB"
END SUB


## Uso pratico: decidere se un modello entra in memoria

DIM disp AS DOUBLE
disp = VAL(SYSTEM.SYS("MEMAVAILABLE"))          ' byte reclamabili
IF disp < 99 * 1073741824 THEN                   ' es. modello da 99 GB
    PRINT "Memoria insufficiente: il modello non entra."
END IF


## Test ed esempi

<!-- END SURFACE -->



<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **DATETIME.CDOW(x1)** — `DATETIME.CDOW(dataRata); "  "; _` (shared/samples/System/ScadenzeRate.bas:45)
- **DATETIME.CMONTH(x1)** — `DATETIME.CMONTH(dataRata); "  "; _` (shared/samples/System/ScadenzeRate.bas:46)
- **DATETIME.CTOD(x1)** — `partenza = DATETIME.CTOD("2026-01-15")` (shared/samples/System/ScadenzeRate.bas:29)
- **DATETIME.DATEADD(x1, x2, x3)** — `dDate = DATETIME.DATEADD("D", 5, dDate)` (shared/samples/Databases/DbLedgerModernTests.bas:127)
- **DATETIME.DATEDIFF(x1, x2, x3)** — `giorni = DATETIME.DATEDIFF("D", partenza, ultima)` (shared/samples/System/ScadenzeRate.bas:62)
- **DATETIME.DATEFROMUNIX(x1)** — `u = DATETIME.DATEFROMUNIX(1584230400)` (shared/minitests/datetime/datetime_all.bas:48)
- **DATETIME.DATESERIAL(x1, x2, x3)** — `d = DATETIME.DATESERIAL(2026, 1, 2) + DATETIME.TIMESERIAL(3, 4, 5)` (shared/minitests/datetime/format_datetime.bas:14)
- **DATETIME.DATEVALUE(x1)** — `settle = DATETIME.DATEVALUE("2024-01-01")` (shared/samples/Quant/QuantLab.bas:121)
- **DATETIME.DAY(x1)** — `PRINT DATETIME.DAY(d)` (shared/minitests/datetime/ymd.bas:5)
- **DATETIME.DAYSINMONTH(x1)** — `PRINT "giorni di febbraio: "; DATETIME.DAYSINMONTH(DATETIME.CTOD("2026-02-01"))` (shared/samples/System/ScadenzeRate.bas:68)
- **DATETIME.DOW(x1)** — `g = DATETIME.DOW(d)` (shared/samples/System/ScadenzeRate.bas:15)
- **DATETIME.DTOC(x1)** — `PRINT "dtoc="; DATETIME.DTOC(DATETIME.DATEVALUE("2026-01-02"))` (shared/minitests/datetime/format_datetime.bas:34)
- **DATETIME.DTOS(x1)** — `PRINT "  Data: " + DATETIME.DTOS(registerResults(i).Date) + ", Desc: " + registerResults(i).Descript…` (shared/samples/Databases/DbLedgerModernTests.bas:206)
- **DATETIME.DATE()** — `PRINT "=== DATETIME.DATE ==="` (shared/samples/Datetime/DateTimeDate.bas:9)
- **DATETIME.EOMONTH(x1)** — `PRINT "fine mese partenza: "; DAY(DATETIME.EOMONTH(partenza))` (shared/samples/System/ScadenzeRate.bas:55)
- **DATETIME.FIRSTDAYOFMONTH(x1)** — `PRINT "inizio mese dopo:   "; DAY(DATETIME.FIRSTDAYOFMONTH(DATETIME.DATEADD("M", 1, partenza)))` (shared/samples/System/ScadenzeRate.bas:56)
- **DATETIME.HOUR(x1)** — `PRINT "TIMESERIAL(23,45,30): H="; DATETIME.HOUR(d); " M="; DATETIME.MINUTE(d); " S="; DATETIME.SECON…` (shared/samples/Datetime/TimeNow.bas:32)
- **DATETIME.ISDATE(x1)** — `PRINT "ISDATE(2024-01-15) = "; DATETIME.ISDATE("2024-01-15")` (shared/samples/graphics/CoordsAndTextGaps.bas:23)
- **DATETIME.ISLEAPYEAR(x1)** — `PRINT "2026 bisestile:     "; DATETIME.ISLEAPYEAR(partenza)` (shared/samples/System/ScadenzeRate.bas:67)
- **DATETIME.ISOWEEKOFYEAR(x1)** — `DATETIME.ISOWEEKOFYEAR(dataRata); "        "; _` (shared/samples/System/ScadenzeRate.bas:47)
- **DATETIME.MINUTE(x1)** — `PRINT "TIMESERIAL(23,45,30): H="; DATETIME.HOUR(d); " M="; DATETIME.MINUTE(d); " S="; DATETIME.SECON…` (shared/samples/Datetime/TimeNow.bas:32)
- **DATETIME.MONTH(x1)** — `PRINT DATETIME.MONTH(d)` (shared/minitests/datetime/ymd.bas:4)
- **DATETIME.NOW()** — `IF DATETIME.NOW() > 0 THEN PRINT "now-ok"` (shared/minitests/datetime/date_time_now.bas:6)
- **DATETIME.SECOND(x1)** — `PRINT "TIMESERIAL(23,45,30): H="; DATETIME.HOUR(d); " M="; DATETIME.MINUTE(d); " S="; DATETIME.SECON…` (shared/samples/Datetime/TimeNow.bas:32)
- **DATETIME.SECONDS()** — `DIM s = DATETIME.SECONDS()` (shared/samples/Datetime/TimeNow.bas:17)
- **DATETIME.STOD(x1)** — `PRINT "stod="; YEAR(DATETIME.STOD("19991231"))` (shared/minitests/datetime/datetime_all.bas:22)
- **DATETIME.STRDATE()** — `PRINT "Current date (text):   "; DATETIME.STRDATE()` (shared/samples/Datetime/DateTimeDate.bas:12)
- **DATETIME.STRTI()** — `IF LEN(DATETIME.STRTI()) > 0 THEN PRINT "strti-ok"` (shared/minitests/datetime/strti.bas:4)
- **DATETIME.STRTIME()** — `PRINT "STRTIME() = "; DATETIME.STRTIME()` (shared/samples/Datetime/TimeNow.bas:22)
- **DATETIME.TI()** — `DIM t = DATETIME.TI()` (shared/samples/Datetime/TimeNow.bas:25)
- **DATETIME.TIMESERIAL(x1, x2, x3)** — `DIM d = DATETIME.TIMESERIAL(23, 45, 30)` (shared/samples/Datetime/TimeNow.bas:30)
- **DATETIME.TODAY()** — `dDate = DATETIME.TODAY()` (shared/samples/Databases/DbLedgerModernTests.bas:356)
- **DATETIME.TIME()** — `IF DATETIME.TIME() >= 0 THEN PRINT "time-ok"` (shared/minitests/datetime/date_time_now.bas:5)
- **DATETIME.UNIXTIMESTAMP(x1)** — `PRINT DATETIME.UNIXTIMESTAMP(d)` (shared/minitests/datetime/unixts.bas:3)
- **DATETIME.WEEKDAY(x1)** — `PRINT DATETIME.WEEKDAY(d)` (shared/minitests/datetime/dow.bas:4)
- **DATETIME.YEAR(x1)** — `PRINT DATETIME.YEAR(d)` (shared/minitests/datetime/ymd.bas:3)
<!-- END DOMAIN TABLE -->
