# IV.22 — File, I/O e documenti

## Cosa ottieni da questo capitolo

Scrivere e rileggere dati su disco: testo sequenziale, record a lunghezza fissa, binario, e i
documenti (JSON, Markdown, conversione). Le due forme di apertura — `USING` e la classica
`OPEN ... AS #n` — con la differenza che conta: chi chiude il file.

## 1. Le due forme di apertura

```basic
USING F = OPEN(path, OUTPUT)
    WRITE #F, 101, "Mario, Rossi", 123.45, TRUE
END USING

OPEN path FOR INPUT AS #1
LINE INPUT #1, riga2
CLOSE #1
```

`USING ... END USING` chiude **anche in caso di errore**: è la forma da preferire. La forma
classica con numero di file resta per il codice Clipper e per quando il numero di canale è
significativo (es. `#1` come "il file principale").

## 2. Scrivere: `WRITE#` contro `PRINT#`

| Comando | Produce |
|---|---|
| `WRITE #n, ...` | CSV canonico: stringhe fra virgolette, booleani `TRUE`/`FALSE` |
| `PRINT #n, ...` | testo **raw**, esattamente come apparirebbe a video |

La differenza si vede alla rilettura: `WRITE#` è rileggibile a campi, `PRINT#` conserva la forma
umana ma non è garantito che si rilegga uguale. Per dati destinati a un altro programma usa
`WRITE#`; per un log o un file da leggere a occhio, `PRINT#`.

## 3. Leggere: `INPUT#` e `LINE INPUT#`

```basic
USING F = OPEN(path, INPUT)
    DIM id AS INTEGER
    DIM nome AS STRING
    DIM saldo AS DOUBLE
    DIM attivo AS INTEGER
    INPUT #F, id, nome, saldo, attivo
    PRINT "   id="; id; " nome="; nome; " saldo="; saldo; " attivo="; attivo
END USING
```

`INPUT#` legge **campi** con conversione per tipo: `attivo` esce `-1`/`0` perché è un intero
booleano. `LINE INPUT#` legge la **riga intera**, senza dividere: è la scelta giusta per un CSV
in cui un campo contiene una virgola.

Eseguito (`shared/samples/Files/SequentialFilesFull.bas`, backend C99Native):

```
1) WRITE# con USING (OUTPUT)
2) APPEND con PRINT#
3) INPUT# multi-campo
   id= 101  nome=Mario, Rossi saldo= 123.45  attivo=-1
   id= 102  nome=Luigi Verdi saldo=-50  attivo= 0
4) LINE INPUT# + EOF
   riga=riga raw in append
   eof=-1
   lof= 78
5) OPEN classico + CLOSE
   prima=101,"Mario, Rossi",123.45,TRUE
SequentialFilesFull: completato.
```

Tre cose da notare: il campo `"Mario, Rossi"` torna con la virgola **dentro** il valore (le
virgolette del CSV hanno fatto il loro lavoro); `eof=-1` è vero e `lof=78` è la lunghezza in byte;
la forma classica rilegge la prima riga come `101,"Mario, Rossi",123.45,TRUE` — cioè il CSV
canonico di `WRITE#`, non il testo raw.

## 4. Accesso diretto e binario

- `OPEN ... FOR RANDOM` con record a lunghezza fissa: `PUT#`/`GET#` per numero di record;
  il sample `Files/FilesIO.bas` apre con record da 46 byte e legge il terzo;
- `IO.SEEK(n)` dice dove sei, `SEEK(n, pos)` ti porta dove vuoi (1-based);
- per il binario si scrive un **array** di byte con `PUT#` e si rilegge con `GET#`.

Eseguito (`Files/FilesIO.bas`):

```
Aperto 'random.dat' con record di lunghezza 46
Letto record #3 -> ID: 303  (Atteso 303)
Scrivo un array di 11 byte nel file binario...
Posizione dopo scrittura (IO.SEEK(1)): 11 (Atteso 11)
```

Il record a lunghezza fissa è il modo più economico di avere accesso casuale senza un database:
se sai la lunghezza, il record `n` comincia a `(n-1) * lunghezza`.

## 5. File e sistema

| Operazione | Comando/funzione |
|---|---|
| cancellare | `KILL path` |
| esistenza, dimensione, data | funzioni `FILE.*` |
| elenco di cartella | `DIR`/`FILES` |
| memory-mapped | `Files/FilesMemoryMap.bas` |
| lock fra processi | `Files/FilesLocking.bas` |
| watcher di cartella | `Files/FilesSystemWatcher.bas` |

Il locking è la parte da non improvvisare: due processi che scrivono lo stesso file senza lock
producono un file corrotto, non un errore.

## 6. Documenti

- **JSON** — `IMPORT JSON`, oggetti e array nativi; guida `Json-Guida-Utente`;
- **Markdown/DOM** — `Documents/MarkdownAstDemo.bas` produce un AST, `Documents/StaticSiteFromMarkdown.bas`
  genera un sito statico;
- **conversione** — `Documents/ConvertBasics.bas` e `ConvertCommand.bas` per i formati;
- **GREP** — ricerca testuale su file e cartelle, guida `GREP-Guida-Utente`;
- **BSP** — il formato binario strutturato per i documenti grandi.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il file resta aperto | eccezione prima di `CLOSE` | usa `USING … END USING` |
| `INPUT#` legge valori sbagliati | file scritto con `PRINT#` | scrivi con `WRITE#` |
| il campo con la virgola si spezza | `INPUT#` invece di `LINE INPUT#` | leggi la riga intera e dividi tu |
| `eof` vero subito | file aperto in `OUTPUT` (lo tronca) | riapri in `INPUT` |
| record letto dalla posizione sbagliata | `SEEK` 0-based per abitudine | `SEEK` è 1-based |

## 8. Dove andare adesso

- **IV.23** — quando i record diventano un database.
- **IV.24** — gli stessi dati via rete.
- **III.15** — testare un I/O senza dipendere dal disco dell'utente.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### IO-Guida-Utente

*Fonte: `IO-Guida-Utente`*


## 1. File a handle

IMPORT IO

h = IO.FOPEN("dati.txt")        ' apre in lettura → handle (long)
s = IO.FREAD(h)                 ' legge una riga (o `IO.FREAD(h, n)` → n byte)
IO.FCLOSE(h)                    ' chiude l'handle

c = IO.FCREATE("nuovo.txt")     ' crea/tronca → handle
IO.FWRITE(c, "contenuto")       ' scrive
IO.FCLOSE(c)

IO.FSEEK(h, 0)                  ' posiziona l'handle
PRINT IO.LOF(h)                 ' lunghezza del file aperto
PRINT IO.FERROR()               ' codice errore I/O (0 = ok)
PRINT IO.EOF(h)                 ' -1 se a fine file
PRINT IO.FREEFILE()             ' prossimo handle libero
PRINT IO.FINPUT(10, h)          ' legge 10 caratteri dall'handle


## 1bis. File sequenziali stile Clipper (`OPEN`/`USING`/`PRINT #`/`INPUT #`)

### `OPEN` — apertura

OPEN "dati.txt" FOR INPUT  AS #1            ' classico: INPUT/OUTPUT/APPEND/RANDOM/BINARY
OPEN "dati.txt" FOR OUTPUT AS #2
OPEN "dati.txt" FOR APPEND AS #3
CLOSE #1                                     ' chiude il file numero 1 (o CLOSE senza argomenti = tutti)

n = OPEN("dati.txt", INPUT)                  ' forma funzionale: ritorna il NUMERO di file

| parametro | significato |
|---|---|
| `path` | percorso del file (stringa, relativo alla CWD o assoluto) |
| `modo` | `INPUT`/`READ`/`R` = lettura (file deve esistere); `OUTPUT`/`WRITE`/`W` = crea/tronca; `APPEND`/`A` = aggiunge in coda (crea se manca); `RANDOM`, `BINARY` = accesso non sequenziale |
| `#n` | numero di file (intero ≥ 1); nella forma funzionale il numero è il valore di ritorno |
| `ACCESS` | (opzionale) `READ`, `WRITE`, `READ WRITE` |
| `LOCK` | (opzionale) `SHARED`, `READ`, `WRITE`, `READ WRITE` |

### `USING` — chiusura automatica

USING F = OPEN("dati.txt", OUTPUT)
    PRINT #F, "ciao"
END USING            ' F viene chiuso automaticamente (anche in caso di errore)


### Scrittura: `PRINT #` e `WRITE #`

PRINT #F, "testo raw"          ' scrive il testo così com'è + newline
WRITE #F, 101, "Mario", 123.45, TRUE


### Lettura: `INPUT #` e `LINE INPUT #`

INPUT #F, id, nome$, saldo, attivo     ' legge UNA riga, divide nei campi CSV
LINE INPUT #F, riga$                    ' legge la RIGA INTERA (nessuno split)


### Funzioni di stato
| funzione | parametro | risultato |
|---|---|---|
| `EOF(n)` | numero di file | `-1` se a fine file, `0` altrimenti (considera il buffer di lettura) |
| `LOF(n)` | numero di file | lunghezza in byte del file aperto |
| `SEEK(n, pos)` | numero di file, posizione **1-based** | posiziona il cursore |
| `FREEFILE()` | — | prossimo numero di file libero |

### Esempio completo

USING F = OPEN("dati.txt", OUTPUT)
    WRITE #F, 1, "Mario, Rossi", 100.5, TRUE
END USING
USING F = OPEN("dati.txt", APPEND)
    PRINT #F, "nota libera"
END USING
USING F = OPEN("dati.txt", INPUT)
    DIM id AS INTEGER, nome AS STRING, tot AS DOUBLE, ok AS INTEGER
    INPUT #F, id, nome, tot, ok
    PRINT id; " "; nome; " "; tot; " "; ok
    DIM nota AS STRING
    LINE INPUT #F, nota
    PRINT nota
    PRINT "fine="; EOF(F)
END USING


## 1ter. File RANDOM a record: `PUT #` / `GET #` su un UDT

TYPE DatiCliente
  ID AS LONG
  Nome AS STRING * 20
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE

DIM c AS DatiCliente, d AS DatiCliente
c.ID = 303 : c.Nome = "Paolo Gialli" : c.Saldo = 1500.75 : c.Attivo = TRUE
OPEN "clienti.dat" FOR RANDOM AS #2 LEN = 46   ' LEN = byte di un record (default 128)
PUT #2, 3, c      ' scrive il record n. 3 (offset (3-1)*LEN)
GET #2, 3, d      ' legge il record n. 3 nei campi di d
PUT #2, , c       ' record omesso = posizione corrente (accesso sequenziale)
CLOSE #2

| Tipo del campo | Byte | Contenuto |
|---|---|---|
| `BIT`, `BYTE`, `SBYTE` | 1 | `BIT`: 0/1 su disco, riletto come 0/-1 |
| `INTEGER`, `UINTEGER` | 2 | intero con/senza segno |
| `SINGLE` | 4 | IEEE 754 |
| `LONG`, `QUAD`, `ULONG`, `UQUAD` | 8 | int64 |
| `DOUBLE` | 8 | IEEE 754 |
| `CURRENCY` | 16 | int64 = valore × 10000 (arrotondato) + 8 byte a zero |
| `STRING * n` | n + 1 | testo UTF-8 troncato/riempito a n con spazi, poi un terminatore NUL |

## 2. Percorsi e file system

PRINT IO.CWD()                       ' directory corrente
PRINT IO.FILEEXISTS("a.txt")         ' -1/0
PRINT IO.DIREXISTS("cartella")       ' -1/0
PRINT IO.NORMALIZEPATH("a/../b")     ' percorso normalizzato
PRINT IO.PARENTDIR("a/b/c")          ' directory padre
PRINT IO.FOLDERSIZE("cartella")      ' dimensione totale (byte)
PRINT IO.RESOLVEFILEPATH("a.txt")    ' percorso risolto/assoluto
PRINT IO.APPPATH()                   ' cartella dell'applicazione
PRINT IO.BINPATH()                   ' cartella dei binari
PRINT IO.PROJECTFOLDER()             ' cartella del progetto
PRINT IO.TEMPFILE()                  ' nome file temporaneo
PRINT IO.TEMPDIR()                   ' directory temporanea
PRINT IO.GETFOLDERPATH("documents")  ' cartella di sistema (opz. maxDepth)


KILL "vecchio.txt"    ' cancella un file (comando legacy, niente parentesi)
ERASE "vecchio.txt"   ' alias di KILL, stessa semantica su tutti i backend


## 2bis. Comandi `CHDIR` e `KILL`
| comando | parametri | effetto |
|---|---|---|
| `CHDIR "path"` | percorso della **nuova directory** di lavoro | cambia la CWD (canone C# `Directory.SetCurrentDirectory`) |
| `KILL "file"` | percorso del **file** | cancella il file, se esiste (canone C# `File.Delete`) |

## 3. Lettura file

b = IO.READBYTES("a.bin")        ' lista di byte (0..255)
l = IO.READLINES("a.txt")        ' lista di righe
s = IO.READFILE("a.txt")         ' intero file di testo come STRINGA
PRINT IO.COUNTLINES("a.txt")     ' numero di righe


## 4. Memory-map

mm = IO.MEMORYMAP("a.bin")       ' mappa il file in memoria (estensione minima 4096)
PRINT IO.MMAPPEEK(mm, 0)         ' byte alla posizione 0
PRINT IO.MMAPREADBYTES(mm, 0, 4) ' 4 byte dalla posizione 0

MMAPPOKE mm, 0, 42               ' scrive un byte
MMAPWRITEBYTES mm, 0, buf()      ' scrive un blocco (BYTE())
IO.CLOSE mm                      ' chiude la mappa


## 5. Verifiche sui quattro backend

## Vedi anche

### BSP-Guide

*Fonte: `BSP-Guide`*


## Cos'è

## Sintassi

### Tre costrutti
| Sintassi | Significato |
|---|---|
| `<% codice %>` | Blocco BASIC inline |
| `<%= expr %>` | Output espressione (equivale a `PRINT expr;`) |
| `<%@ DIRETTIVA %>` | Direttiva di pagina |

### Esempio minimo

<%@ IMPORT HTTP %>
<%
  DIM title = "Home Page"
%>
<!DOCTYPE html>
<html>
<body>
  <h1><%= title %></h1>
  <p>Ora: <%= TIME %></p>
</body>
</html>


### Direttive disponibili
| Direttiva | Esempio |
|---|---|
| `<%@ IMPORT NS %>` | `<%@ IMPORT HTTP %>` |
| `<%@ LAYOUT "file.bsp" %>` | Layout padre (Razor-style); alias `MASTER` |
| `<%@ CACHE 60 %>` | Cache HTTP per N secondi |
| `<%@ AUTHOR "Nome" %>` | Metadati |
| `<%@ ROUTE "/path" %>` | Auto-registra route |
| `<%@ MODEL Tipo %>` | Dichiara `MODEL` (per `@Model.Prop`) |
| `<%@ AUTHORIZE ruolo %>` | Proteggi la pagina: 401 se non autenticato, 403 se il ruolo non combacia |

### Variabili built-in
| Variabile | Tipo | Contenuto |
|---|---|---|
| `REQ` | `HTTPREQUEST` | Richiesta corrente (Method, Path, Headers, Body, IP) |
| `RES` | `HTTPRESPONSE` | Risposta da costruire (Send, SendJSON, SetCookie, Redirect, StatusCode) |
| `SESSION` | `JSON` | Dati sessione persistenti tra richieste |
| `FORM` | `JSON` | Dati form (POST) |
| `QUERY` | `JSON` | Query string params |
| `PAGE` | `JSON` | Metadati pagina (Title, etc.) |

## Output e sicurezza (anti-XSS) — IMPORTANTE
| Sintassi | Cosa genera | Esempio |
|---|---|---|
| `<%= nomeUtente %>` | `PRINT STRINGS.HTMLENCODE(nomeUtente);` — **escapato** | testo innocuo |
| `<%= htmlFidato \| raw %>` | `PRINT htmlFidato;` — output **grezzo** | HTML che hai generato tu |
| `@Model.Titolo` | escaped di default | testo dal model |
| `@Html.Encode(Model.Descrizione)` | encode esplicito | sempre sicuro |
| `@Html.Raw(htmlFidato)` | raw esplicito | solo per HTML di cui ti fidi |
| `@section` / `@RenderSection` | restano **grezzi** (HTML statico autoredatto) | layout/sezioni |

<p>Ciao, <%= utente.Nome %></p>          ' escaped → "&lt;script&gt;" non esegue
<p><%= notaHTMLDaAdmin | raw %></p>       ' SOLO se è HTML generato/approvato da te
<h1>@Html.Encode(Model.Titolo)</h1>       ' encode esplicito (equivalente al default)


## Sessione (teoria e pratica)

' 1) stile classico BSP:
SESSION("user") = "Gaetano"
DIM user = SESSION("user")

' 2) stile "richiesta" (stessa identica sessione):
REQ("Session")("user") = "Gaetano"
DIM user = REQ("Session")("user")


IMPORT HTTP
DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
cfg.Sessions = TRUE          ' sessioni attive anche sulle route HTTP
cfg.SessionFile = "work/sessions.db"   ' persistenza su disco ("" = in-memory)
HTTP CONFIG cfg


<%@ AUTHORIZE admin %>
<html><body><h1>Pannello admin</h1>
<p>Utente: <%= SESSION("user") %></p>
</body></html>


### Comandi server-side

## Pipeline

Scrittura                Sviluppo                       Runtime
─────────                ────────                       ───────

index.bsp   →   BSP ./pages/  →  P-Code (cache)  →  GET /index.bsp → serve da cache
  (HTML+BASIC)   (REPL/CLI)      793 opcode          (HTTP dispatch)


### Step 1: Scrittura

### Step 2: Pre-compilazione

' Da REPL:
BSP ./pages/

' Da CLI:
gmb -bsp ./pages/


### Step 3: Server HTTP

IMPORT HTTP

' Registra route API
HTTP ROUTE "GET", "/api/info", InfoHandler()

' Avvia server (auto-registra BSP se ./pages/ esiste)
HTTP SERVER 8080


HTTP BSP "/blog/*.bsp", "./blog"
HTTP SERVER 8080


### Step 4: Hot reload

## Layout system

<!DOCTYPE html>
<html>
<body>
  <header>Intestazione</header>
  <main><% RENDERBODY() %></main>
  <footer>Piè di pagina</footer>
</body>
</html>


<%@ LAYOUT "MainLayout.bsp" %>
<h1>Contenuto della pagina</h1>
<p>Questo va dentro RENDERBODY()</p>


## Componenti

<% RENDERCOMPONENT "ProductCard" WITH item %>


## Comandi REPL
| Comando | Descrizione |
|---|---|
| `BSP <dir>` | Pre-compila tutti i .bsp in `<dir>` |
| `BSP` | Pre-compila `./pages/` (default) |

## CLI

gmb -bsp ./pages/           # Pre-compila
gmb file.bas -n             # Compila IL nativo (AOT/NativeAOT, senza runtime .NET a carico)
gmb file.bas -n -full       # ⚠️ legacy (D3): self-contained con runtime .NET, NON più lo standard


## File del progetto
| File | Ruolo |
|---|---|
| `GMBasicSharp/common/CommonLib/Http/BspCompiler.cs` | Parser `.bsp` → sorgente BASIC |
| `GMBasicSharp/common/CommonLib/Http/BspRuntime.cs` | Cache compilazione (ConcurrentDictionary) |
| `GMBasicSharp/common/CommonLib/Http/BspSessionManager.cs` | Sessioni cookie-based |
| `GMBasicSharp/REPL/ReplEngine.cs` | Comando REPL `BSP <dir>` |
| `GMBasicSharp/REPL/Program.cs` | CLI `-bsp <dir>` |
| `GMBasic/native/gmb_vm_c/` | HTTP dispatch `.bsp` (VM/C) |
| `GMBasic/native/gmb_vm_c/gmb_vm_core.c` | HTTP dispatch `.bsp` (VM/C) |
| `GMBasicSharp/common/IlBackendRuntime/PCode/BspDispatch.cs` | Serve le pagine su IL/PCode (seed REQ/RES/SESSION, write-back) |
| `GMBasicSharp/common/Compiler/Compiler.SubOpCode.cs` | Opcode `HTTP_BSP` |

## Architettura

┌──────────────────────────────────────────────────────────┐
│                    HTTP SERVER (VM/C)                     │
│                                                          │
│  Richiesta GET /index.bsp                                │
│       ↓                                                  │
│  TryServeBspPage(request, bspDir)                        │
│       │                                                  │
│       ├─ Cache hit? → VM/C Run(chunk A1) → output HTML   │
│       └─ Cache miss? → 503 (pre-compila prima)           │
│                                                          │
│  Pipeline di compilazione (BSP REPL/CLI):                │
│  .bsp → BspCompiler.Compile() → BASIC source             │
│       → Orchestrator.CompileFromString() → BasicProgram   │
│       → Compiler.Compile() → CompiledChunk (P-Code/A1)    │
│       → BspRuntime.CacheStore()                          │
└──────────────────────────────────────────────────────────┘


## Security aggiornata (2026-09-02)

### CONVERT-User-Manual

*Fonte: `CONVERT-User-Manual`*


## 1. Installazione e configurazione

### 1.1 Libreria PDF (PDFOxide)
| Sistema operativo / architettura | Cartella | File |
|---|---|---|
| macOS Apple Silicon (arm64) | `~/.gmbasic/libs/osx-arm64/` | `libpdf_oxide.dylib` |
| macOS Intel (x86_64) | `~/.gmbasic/libs/osx-x64/` | `libpdf_oxide.dylib` |
| Linux x86-64 | `~/.gmbasic/libs/linux-x64/` | `libpdf_oxide.so` |
| Linux arm64 | `~/.gmbasic/libs/linux-arm64/` | `libpdf_oxide.so` |
| Windows x64 | `~/.gmbasic/libs/win-x64/` | `pdf_oxide.dll` |

### 1.2 OCR (pagine scansionate)

' Registra un modello OCR locale (es. PaddleOCR-VL-1.6 GGUF) e lo attiva
AI REGISTER "local-ocr", %OCR, "PaddleOCR-VL-1.6-GGUF"
AI SET ACTIVE %OCR, "local-ocr"

' Verifica che l'OCR funzioni (il prompt è libero)
PRINT AI.OCR("immagine.png")


## 2. Formati supportati

txt  csv  md  html  rtf  docx  epub  odt  xlsx  pptx  pdf  url


## 3. Conversione di formato

### `CONVERT.TOMD(path)` → stringa

IMPORT CONVERT

md$ = CONVERT.TOMD("report.pdf")      ' PDF → markdown (PDFOxide)
PRINT LEN(md$)                        ' lunghezza del markdown
PRINT MID(md$, 1, 80)                 ' prime 80 righe? No: primi 80 caratteri


### `CONVERT.TOHTML(path)` → stringa

### `CONVERT.TOTEXT(path)` → stringa

### `CONVERT.BATCH(dir, fmt, rec)` → stringa

tutto$ = CONVERT.BATCH("C:/documenti", "md", 1)
PRINT "Convertiti " + STR(LEN(tutto$)) + " caratteri di markdown"


### `CONVERT.WEB(url)` → stringa

contenuto$ = CONVERT.WEB("https://example.com")
PRINT MID(contenuto$, 1, 100)


### `CONVERT.FORMATS()` → stringa

PRINT CONVERT.FORMATS()
' → txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url


### Comando legacy: `CONVERT "file" INTO variabile`

CONVERT "report.docx" INTO testo$
PRINT LEN(testo$)


## 4. PDF con handle: lettura a pagina

### `CONVERT.OPEN(src)` → handle (numero)

### `CONVERT.CLOSE(h)` → numero

### `CONVERT.PAGECOUNT(h)` → numero

### `CONVERT.TEXT(h, page)` → stringa

### `CONVERT.MARKDOWN(h, page)` → stringa

### `CONVERT.HTML(h, page)` → stringa

### `CONVERT.METADATA(h)` → stringa

### `CONVERT.SEARCH(h, pattern)` → stringa

### `CONVERT.IMAGES(h, page, dir)` → numero

### `CONVERT.RENDER(h, page, dpi, out)` → numero

IMPORT CONVERT

h = CONVERT.OPEN("manuale.pdf")
IF h = 0 THEN
    PRINT "Apertura fallita"
    END
END IF

PRINT "Pagine: " + STR(CONVERT.PAGECOUNT(h))
PRINT "Markdown completo: " + STR(LEN(CONVERT.MARKDOWN(h, -1)))
PRINT "Metadati: " + CONVERT.METADATA(h)

r = CONVERT.SEARCH(h, "installazione")
IF LEN(r) > 0 THEN
    PRINT "Trovato: " + r
END IF


## 5. Estrazione automatica e OCR

### `CONVERT.EXTRACT(src)` → stringa

testo$ = CONVERT.EXTRACT("scansione.pdf")   ' testo + OCR dove serve
PRINT testo$


### `CONVERT.EXTRACTENGINE(src)` → stringa

PRINT CONVERT.EXTRACTENGINE("misto.pdf")   ' → PDFOXIDE | OCR | HYBRID


### Comando: `CONVERT EXTRACT "src" TO "dest"`

CONVERT EXTRACT "report.pdf" TO "report.md"
PRINT "Estratto in report.md"


### Comando: `CONVERT RENDER "src" TO "out.png"`

CONVERT RENDER "frontespizio.pdf" TO "frontespizio.png"
PRINT "Immagine salvata"


## 6. Markdown come struttura (AST)

### `CONVERT.PARSE(text)` → dict (documento)

d = CONVERT.PARSE("# Titolo" + CHR(10) + "testo con **grassetto**")


### `CONVERT.HEADINGS(doc, level)` → lista di dict

### `CONVERT.LINKS(doc)` → lista di dict

### `CONVERT.MDIMAGES(doc)` → lista di dict

### `CONVERT.TABLES(doc)` → lista di dict

### `CONVERT.CODEBLOCKS(doc)` → lista di dict

### `CONVERT.TREE(doc)` → stringa

#0 heading level=1
#1 paragraph text="Testo con bold e link."
#2 heading level=2
#3 list items=2
#4 codeblock lang=basic len=7


### `CONVERT.RENDERHTML(doc)` → stringa

### `CONVERT.RENDERTEXT(doc)` → stringa

### `CONVERT.VALIDATE(text)` → stringa

### `CONVERT.QUERY(doc, selector)` → lista di dict

### `CONVERT.STRIP(text)` → stringa

### `CONVERT.FORMAT(doc)` → stringa

### Come si leggono i campi dei dict

hs = CONVERT.HEADINGS(d, 0)
PRINT "Titoli: " + STR(UBOUND(hs) + 1)

FOR EACH h IN hs
    PRINT "#" + STR(h.level) + " " + h.text
NEXT

FOR EACH l IN CONVERT.LINKS(d)
    PRINT l.text + " → " + l.url
NEXT


### Esempio completo (verificato): indice di un documento

IMPORT CONVERT

SUB Main
    NL = CHR(10)
    testo$ = "# Introduzione" + NL + NL + _
             "Testo di esempio con [riferimento](https://example.com)." + NL + NL + _
             "## Sezione 1" + NL + NL + "## Sezione 2"

    d = CONVERT.PARSE(testo$)

    PRINT "=== Sommario ==="
    FOR EACH h IN CONVERT.HEADINGS(d, 0)
        PRINT STR(h.level) + ". " + h.text
    NEXT

    PRINT "=== HTML ==="


## 7. Riferimento rapido
| Funzione | Ritorna | Esempio |
|---|---|---|
| `CONVERT.TOMD(path)` | stringa (markdown) | `md$ = CONVERT.TOMD("f.pdf")` |
| `CONVERT.TOHTML(path)` | stringa (html) | `h$ = CONVERT.TOHTML("f.docx")` |
| `CONVERT.TOTEXT(path)` | stringa (testo) | `t$ = CONVERT.TOTEXT("f.rtf")` |
| `CONVERT.BATCH(dir, fmt, rec)` | stringa | `CONVERT.BATCH("d", "md", 1)` |
| `CONVERT.WEB(url)` | stringa | `CONVERT.WEB("https://x")` |
| `CONVERT.FORMATS()` | stringa | `CONVERT.FORMATS()` |
| `CONVERT.OPEN(src)` | numero (handle) | `h = CONVERT.OPEN("f.pdf")` |
| `CONVERT.CLOSE(h)` | numero | `CONVERT.CLOSE(h)` |
| `CONVERT.PAGECOUNT(h)` | numero | `CONVERT.PAGECOUNT(h)` |
| `CONVERT.TEXT(h, page)` | stringa | `CONVERT.TEXT(h, 0)` |
| `CONVERT.MARKDOWN(h, page)` | stringa | `CONVERT.MARKDOWN(h, -1)` |
| `CONVERT.HTML(h, page)` | stringa | `CONVERT.HTML(h, 0)` |
| `CONVERT.METADATA(h)` | stringa | `CONVERT.METADATA(h)` |
| `CONVERT.SEARCH(h, pattern)` | stringa | `CONVERT.SEARCH(h, "testo")` |
| `CONVERT.IMAGES(h, page, dir)` | numero | `CONVERT.IMAGES(h, -1, "img")` |
| `CONVERT.RENDER(h, page, dpi, out)` | numero | `CONVERT.RENDER(h, 0, 150, "p.png")` |
| `CONVERT.EXTRACT(src)` | stringa | `CONVERT.EXTRACT("f.pdf")` |
| `CONVERT.EXTRACTENGINE(src)` | stringa | `CONVERT.EXTRACTENGINE("f.pdf")` |
| `CONVERT.PARSE(text)` | dict (documento) | `d = CONVERT.PARSE(md$)` |
| `CONVERT.HEADINGS(doc, level)` | lista dict | `CONVERT.HEADINGS(d, 1)` |
| `CONVERT.LINKS(doc)` | lista dict | `CONVERT.LINKS(d)` |
| `CONVERT.MDIMAGES(doc)` | lista dict | `CONVERT.MDIMAGES(d)` |
| `CONVERT.TABLES(doc)` | lista dict | `CONVERT.TABLES(d)` |
| `CONVERT.CODEBLOCKS(doc)` | lista dict | `CONVERT.CODEBLOCKS(d)` |
| `CONVERT.TREE(doc)` | stringa | `CONVERT.TREE(d)` |
| `CONVERT.RENDERHTML(doc)` | stringa | `CONVERT.RENDERHTML(d)` |
| `CONVERT.RENDERTEXT(doc)` | stringa | `CONVERT.RENDERTEXT(d)` |
| `CONVERT.VALIDATE(text)` | stringa (`""` = ok) | `CONVERT.VALIDATE(md$)` |
| `CONVERT.QUERY(doc, sel)` | lista dict | `CONVERT.QUERY(d, "heading[level=2]")` |
| `CONVERT.STRIP(text)` | stringa | `CONVERT.STRIP(md$)` |
| `CONVERT.FORMAT(doc)` | stringa | `CONVERT.FORMAT(d)` |

## 8. Errori e limiti noti
| Situazione | Cosa succede |
|---|---|
| `CONVERT.OPEN` fallisce (file inesistente/corrotto) | restituisce `0` |
| Libreria PDFOxide non trovata | PDF letto con PdfPig (più lento); `RENDER`/`IMAGES` non disponibili |
| Nessun provider OCR attivo | `EXTRACT` su pagine scansionate lascia `> [OCR REQUIRED — page N]` |
| `CONVERT.SEARCH` senza match | stringa vuota |
| OCR disponibile solo sui backend C# (IL/PCodeVM) | su VM/C/C99 `EXTRACT` fa solo text-layer |
| Formato file non riconosciuto | il contenuto è trattato come testo grezzo |

## 9. Esempi pronti in `shared/samples/Documents/`
| File | Cosa dimostra |
|---|---|
| `ConvertFullCoverage.bas` | **tutte le funzioni CONVERT** (markdown + PDF + conversioni), deterministico |
| `DocumentBatchExtract.bas` | pipeline batch PDF → Markdown su file |
| `MarkdownAstDemo.bas` | tutte le funzioni del Markdown AST |
| `StaticSiteFromMarkdown.bas` | generare un sito statico (Markdown → HTML) |
| `ReadmeKnowledgeBase.bas` | estrarre indice e link da un README |

## 10. Parità backend (2026-09-12)

## 11. Riverifica 2026-09-16 — blocco VALIDATO DEFINITIVO

### Convert-Setup-User-Manual

*Fonte: `Convert-Setup-User-Manual`*


## 1. Prima di tutto: preparare la macchina (`gmb setup`)

gmb setup ALL            (CLI)
SETUP ALL                (REPL)


gmb setup LIBS           librerie native per il tuo OS (SDL, motore PDF, OCR, SSL, ...)
gmb setup TORCH          librerie ML (libtorch) — solo macOS
gmb setup BROWSER        Chromium per il plugin browser
gmb setup STATUS         stato della cache di tutti i componenti
gmb setup RM <comp>      elimina la cache di un componente


## 2. Convertire un documento in Markdown/HTML/testo
| Funzione | Cosa fa |
|---|---|
| `CONVERT.TOMD(path)` | → **Markdown** |
| `CONVERT.TOHTML(path)` | → **HTML** |
| `CONVERT.TOTEXT(path)` | → **testo puro** |
| `CONVERT.BATCH(dir, fmt, rec)` | converte tutti i file di una cartella (ricorsivo opzionale) |
| `CONVERT.WEB(url)` | pagina web → Markdown |
| `CONVERT.FORMATS()` | elenco dei formati supportati |

IMPORT CONVERT

md$ = CONVERT.TOMD("report.docx")     ' → markdown
PRINT LEN(md$)

' Tutti i .pdf di una cartella, ricorsivo, in Markdown:
tutto$ = CONVERT.BATCH("documenti", "md", 1)
PRINT MID(tutto$, 1, 80)


CONVERT "report.docx"                 → markdown a schermo
CONVERT "data.xlsx" -o table.html     → salva come HTML
BATCH "documenti/" -r                 → tutti i file → markdown, ricorsivo
WEB2MD "https://example.com"          → pagina web → markdown
CONVFMT                               → elenco formati


## 3. Leggere un PDF pagina per pagina (handle)
| Funzione | Cosa fa |
|---|---|
| `CONVERT.OPEN(src)` | apre il PDF → **handle** (numero); `0` = fallito |
| `CONVERT.CLOSE(h)` | chiude e libera |
| `CONVERT.PAGECOUNT(h)` | numero di pagine |
| `CONVERT.TEXT(h, page)` | testo puro della pagina |
| `CONVERT.MARKDOWN(h, page)` | markdown della pagina (`-1` = tutto il documento) |
| `CONVERT.HTML(h, page)` | HTML della pagina |
| `CONVERT.METADATA(h)` | metadati (`pages=N; version=M.m`) |
| `CONVERT.SEARCH(h, pattern)` | cerca testo → `p<numero>|<match>` per pagina |
| `CONVERT.IMAGES(h, page, dir)` | estrae le immagini in una cartella (conta) |
| `CONVERT.RENDER(h, page, dpi, out)` | pagina → PNG |

IMPORT CONVERT

h = CONVERT.OPEN("manuale.pdf")
IF h = 0 THEN PRINT "Apertura fallita": END

PRINT "Pagine: " + STR(CONVERT.PAGECOUNT(h))
PRINT "Markdown completo: " + STR(LEN(CONVERT.MARKDOWN(h, -1)))
PRINT "Metadati: " + CONVERT.METADATA(h)

r = CONVERT.SEARCH(h, "installazione")
IF LEN(r) > 0 THEN PRINT "Trovato: " + r

CONVERT.RENDER(h, 0, 150, "prima_pagina.png")   ' PNG 612x792 a 150 dpi
CONVERT.CLOSE(h)


## 4. Estrazione automatica e OCR (pagine scansionate)

testo$ = CONVERT.EXTRACT("scansione.pdf")   ' testo + OCR dove serve
PRINT testo$


CONVERT EXTRACT "scansione.pdf" TO "estratto.md"   ' estrae e salva su file
CONVERT RENDER  "frontespizio.pdf" TO "front.png"  ' renderizza la pagina 0 a PNG


## 5. Markdown come struttura (AST)

IMPORT CONVERT

d = CONVERT.PARSE("# Titolo" + CHR(10) + "testo con **bold**")

' Sommario (titoli con livello)
FOR EACH h IN CONVERT.HEADINGS(d, 0)
    PRINT STR(h.level) + ". " + h.text
NEXT

' Link
FOR EACH l IN CONVERT.LINKS(d)
    PRINT l.text + " → " + l.url
NEXT

' HTML e testo
PRINT CONVERT.RENDERHTML(d)

| Funzione | Cosa fa |
|---|---|
| `CONVERT.PARSE(text)` | testo → **documento** (oggetto) |
| `CONVERT.HEADINGS(doc, level)` | titoli (`level<=0` = tutti) |
| `CONVERT.LINKS(doc)` | link `{text, url}` |
| `CONVERT.MDIMAGES(doc)` / `TABLES` / `CODEBLOCKS` | immagini / tabelle / blocchi codice |
| `CONVERT.TREE(doc)` | dump leggibile dei blocchi |
| `CONVERT.QUERY(doc, "heading[level=2]")` | selettore di blocchi |
| `CONVERT.RENDERHTML(doc)` / `RENDERTEXT(doc)` | HTML / testo puro |
| `CONVERT.VALIDATE(text)` | `""` se valido, altrimenti l'errore |
| `CONVERT.STRIP(text)` | Markdown → testo puro |
| `CONVERT.FORMAT(doc)` | AST → Markdown canonico |

## 6. Comandi REPL necessari (riepilogo)
| Comando REPL | Cosa fa |
|---|---|
| `SETUP [LIBS\|TORCH\|BROWSER\|ALL]` | scarica i componenti runtime per il tuo OS |
| `SETUP STATUS` / `SETUP RM <comp>` | stato cache / elimina cache |
| `CONVERT "file" [-o out]` | converte un documento a schermo o su file |
| `CONVERT EXTRACT "src" TO "dest"` | estrae un documento (con OCR) su file |
| `CONVERT RENDER "src" TO "out.png"` | renderizza la pagina 0 a PNG |
| `BATCH "dir" [-r] [-o ext]` | conversione batch |
| `WEB2MD "url"` | pagina web → Markdown |
| `CONVFMT` | elenco formati |

## 7. Riferimento rapido
| Funzione | Ritorna | Esempio |
|---|---|---|
| `CONVERT.TOMD(path)` | stringa | `md$ = CONVERT.TOMD("f.pdf")` |
| `CONVERT.TOHTML(path)` | stringa | `h$ = CONVERT.TOHTML("f.docx")` |
| `CONVERT.TOTEXT(path)` | stringa | `t$ = CONVERT.TOTEXT("f.rtf")` |
| `CONVERT.BATCH(dir, fmt, rec)` | stringa | `CONVERT.BATCH("d", "md", 1)` |
| `CONVERT.WEB(url)` | stringa | `CONVERT.WEB("https://x")` |
| `CONVERT.FORMATS()` | stringa | `CONVERT.FORMATS()` |
| `CONVERT.OPEN(src)` | handle | `h = CONVERT.OPEN("f.pdf")` |
| `CONVERT.CLOSE(h)` | numero | `CONVERT.CLOSE(h)` |
| `CONVERT.PAGECOUNT(h)` | numero | `CONVERT.PAGECOUNT(h)` |
| `CONVERT.TEXT(h, page)` | stringa | `CONVERT.TEXT(h, 0)` |
| `CONVERT.MARKDOWN(h, page)` | stringa | `CONVERT.MARKDOWN(h, -1)` |
| `CONVERT.HTML(h, page)` | stringa | `CONVERT.HTML(h, 0)` |
| `CONVERT.METADATA(h)` | stringa | `CONVERT.METADATA(h)` |
| `CONVERT.SEARCH(h, pattern)` | stringa | `CONVERT.SEARCH(h, "testo")` |
| `CONVERT.IMAGES(h, page, dir)` | numero | `CONVERT.IMAGES(h, -1, "img")` |
| `CONVERT.RENDER(h, page, dpi, out)` | numero | `CONVERT.RENDER(h, 0, 150, "p.png")` |
| `CONVERT.EXTRACT(src)` | stringa | `CONVERT.EXTRACT("f.pdf")` |
| `CONVERT.EXTRACTENGINE(src)` | stringa | `CONVERT.EXTRACTENGINE("f.pdf")` |
| `CONVERT.PARSE(text)` | documento | `d = CONVERT.PARSE(md$)` |
| `CONVERT.HEADINGS(doc, level)` | lista | `CONVERT.HEADINGS(d, 1)` |
| `CONVERT.LINKS(doc)` | lista | `CONVERT.LINKS(d)` |
| `CONVERT.MDIMAGES/TABLES/CODEBLOCKS(doc)` | lista | `CONVERT.TABLES(d)` |
| `CONVERT.TREE(doc)` | stringa | `CONVERT.TREE(d)` |
| `CONVERT.QUERY(doc, sel)` | lista | `CONVERT.QUERY(d, "heading[level=2]")` |
| `CONVERT.RENDERHTML(doc)` / `RENDERTEXT(doc)` | stringa | `CONVERT.RENDERHTML(d)` |
| `CONVERT.VALIDATE(text)` | stringa (`""`=ok) | `CONVERT.VALIDATE(md$)` |
| `CONVERT.STRIP(text)` | stringa | `CONVERT.STRIP(md$)` |
| `CONVERT.FORMAT(doc)` | stringa | `CONVERT.FORMAT(d)` |

## 8. Errori e limiti noti
| Situazione | Cosa succede |
|---|---|
| `gmb setup LIBS` non eseguito | le funzioni documenti non trovano le librerie → errori di caricamento |
| `CONVERT.OPEN` su file inesistente | restituisce `0` |
| PDF interamente scansionato e nessun OCR | pagine segnalate con `> [OCR REQUIRED — page N]` |
| `CONVERT.SEARCH` senza match | stringa vuota |
| OCR C# (PaddleOCR) | richiede un modello configurato (`AI SET ACTIVE %OCR`); il C (Tesseract) non ne richiede |

## 9. Esempi pronti in `shared/samples/Documents/`
| File | Cosa dimostra |
|---|---|
| `DocumentBatchExtract.bas` | pipeline batch PDF → Markdown su file |
| `MarkdownAstDemo.bas` | tutte le funzioni del Markdown AST |
| `StaticSiteFromMarkdown.bas` | generare un sito statico (Markdown → HTML) |
| `ReadmeKnowledgeBase.bas` | estrarre indice e link da un README |

### DOM-Guida-Utente

*Fonte: `DOM-Guida-Utente`*


REQUIRE "dom"
IMPORT DOM WITH ALIAS


## API
| Membro | Firma | Cosa |
|---|---|---|
| `AVAILABLE` | `() AS LONG` | 1 se la lib è caricata |
| `PARSE` | `(html AS STRING) AS LONG` | parse **HTML** (tollerante) → handle DOC (0 = err) |
| `PARSEXML` | `(xml AS STRING) AS LONG` | parse **XML** (stretto) → handle DOC |
| `FREE` | `(d AS LONG) AS LONG` | libera un DOC |
| `SELECT` | `(d AS LONG, xpath AS STRING) AS LONG` | valuta un **XPath** → handle NODESET (0 = nessun match/err) |
| `NUMNODES` | `(ns AS LONG) AS LONG` | numero di nodi di un NODESET |
| `NODENAME` | `(ns AS LONG, i AS LONG) AS STRING` | nome del tag del nodo i |
| `NODETEXT` | `(ns AS LONG, i AS LONG) AS STRING` | testo del nodo i |
| `NODEATTR` | `(ns AS LONG, i AS LONG, name AS STRING) AS STRING` | attributo `name` del nodo i |
| `OUTERHTML` | `(ns AS LONG, i AS LONG) AS STRING` | serializzazione del nodo i |
| `FREENODESET` | `(ns AS LONG) AS LONG` | libera un NODESET |
| `TEXT` | `(d AS LONG, xpath AS STRING) AS STRING` | scorciatoia: testo del PRIMO match |
| `ATTR` | `(d AS LONG, xpath AS STRING, name AS STRING) AS STRING` | scorciatoia: attributo del PRIMO match |
| `LASTERROR` | `(d AS LONG) AS STRING` | ultimo errore del DOC |

## Esempio

REQUIRE "dom"
IMPORT DOM WITH ALIAS

d = DOM.PARSE("<h1 id=""t"">Ciao</h1><a href=""https://x"">X</a>")
PRINT DOM.TEXT(d, "//h1")            ' -> Ciao
PRINT DOM.ATTR(d, "//h1", "id")      ' -> t
ns = DOM.SELECT(d, "//a")
PRINT DOM.COUNT(ns); " "; DOM.NODEATTR(ns, 0, "href")   ' -> 1 https://x
DOM.FREENODESET(ns)
DOM.FREE(d)

' XML
x = DOM.PARSEXML("<root><qty>2</qty></root>")
PRINT DOM.TEXT(x, "//qty")           ' -> 2
DOM.FREE(x)


## Disponibilità

## Riferimenti

### Document-Processing

*Fonte: `Document-Processing`*


## 1. Architettura

BASIC source
  └─ CONVERT.* (funzioni) / CONVERT <verbo> ... (comandi)
       ├─ IL / PCodeVM   → NativeDocPdf.cs (P/Invoke libpdf_oxide) + MdSupport.cs (AST)
       ├─ VM/C           → gmb_doc_c/gmb_doc_pdf.c + gmb_runtime_c/gmb_md.c
       └─ C99 (transpiler) → gmb_doc_* / gmb_md_* (stessa lib, compilata nel binario)


## 2. Librerie native (`~/.gmbasic/libs/<os>/`)
| RID | File |
|---|---|
| `osx-arm64` / `osx-x64` | `libpdf_oxide.dylib` |
| `linux-x64` / `linux-arm64` | `libpdf_oxide.so` |
| `win-x64` | `pdf_oxide.dll` |

## 3. Funzioni `CONVERT` (handle-based, PDF)

## 4. OCR fallback (scanned PDF)

## 5. Markdown AST in `CONVERT`

d = CONVERT.PARSE(text)                 ' dict {type, blocks:[...]}
h = CONVERT.HEADINGS(d[, level])        ' lista {level, text}  (level<=0 = tutte)
l = CONVERT.LINKS(d)                    ' lista {text, url}
CONVERT.MDIMAGES / TABLES / CODEBLOCKS  ' liste di dict
t = CONVERT.TREE(d)                     ' dump leggibile dei blocchi
q = CONVERT.QUERY(d, "heading[level=2]")
CONVERT.RENDERHTML(d) · CONVERT.RENDERTEXT(d)
e = CONVERT.VALIDATE(text)              ' "" se ok, altrimenti l'errore (fence aperte)
CONVERT.STRIP(text)                     ' markdown → testo puro
CONVERT.FORMAT(d|text)                  ' AST → markdown canonico


## 6. Conversione di formato

## 7. Esempi (`shared/samples/Documents/`)
| Sample | Cosa fa | Backend verificati |
|---|---|---|
| `DocumentBatchExtract.bas` | pipeline batch PDF→Markdown in `work/` | IL, VM/C |
| `MarkdownAstDemo.bas` | demo di tutte le funzioni MD AST | IL, PCodeVM, VM/C, C99 |
| `StaticSiteFromMarkdown.bas` | wiki/static-site: .md → `work/site/*.html` + index | IL, PCodeVM, VM/C (C99 SKIP: gap file-handle) |
| `ReadmeKnowledgeBase.bas` | parser README → knowledge base `work/kb.md` | IL, PCodeVM, VM/C, C99 |

## 8. Verifica parità

# content-presence C (gmb_doc_c) vs IL (ConversionLib), 9 formati
bash tools/sh/convert_parity.sh

# MD AST identico su IL/VM/C/C99 (md_vm: H0:2 H1:1, md_all: LINKS:2 IMGS:0 TBLS:1 CBS:1 QRY:1)
dotnet run --project REPL -p:GmbEdition=Commercial --no-build -- --managed  <file>.bas
dotnet run --project REPL -p:GmbEdition=Commercial --no-build --              <file>.bas  # VM/C
GMB_MINI_ONLY="<dominio>/<file>.bas" tools/sh/test_minitests_c99native


## 9. Limiti noti / divertaenze

### GREP-Guida-Utente

*Fonte: `GREP-Guida-Utente`*


## 1. Funzioni BASIC (namespace `GREP`)
| funzione | parametri | ritorno | descrizione |
|---|---|---|---|
| `GREP.MATCHES(pattern$, path$)` | `pattern$` regex, `path$` file o cartella | **ARRAY di STRING** | una voce per hit: `"file:riga:colonna:testo"` |
| `GREP.COUNT(pattern$, path$)`   | idem | **LONG** | numero di hit |
| `GREP.FILES(pattern$, path$)`   | idem | **ARRAY di STRING** | nomi dei file che contengono il pattern (distinti, ordine di apparizione) |

### Parametri (dettaglio)

### Semantica dei campi

### Esempio

IMPORT GREP

SUB Main()
    DIM hits() AS STRING
    hits = GREP.MATCHES("needle", "Work/input")      ' ARRAY
    PRINT "Hit: "; UBOUND(hits) + 1
    FOR i = 0 TO UBOUND(hits)
        PRINT "  "; hits[i]                          ' "file:riga:col:testo"
    NEXT

    DIM files() AS STRING
    files = GREP.FILES("needle", "Work/input")        ' ARRAY (file distinti)
    FOR i = 0 TO UBOUND(files)
        PRINT "  file: "; files[i]
    NEXT



### Polimorfismo / console-aware

### Formati trasparenti (nessuna opzione)

## 2. Comando REPL (`grep` / `rg`)

grep [-R] [-i] [-n] [-w] [-v] [-l] [-c] [-o] [-F] [-.]
     [-m N] [-A N] [-B N] [-C N]
     [--type EXT] [--include GLOB] [--exclude GLOB] [--depth N]
     PATTERN [PATH]
rg   ...   (alias)     # PATH default = cartella corrente del progetto

| opzione | significato |
|---|---|
| `-R`/`-r` | ricorsivo |
| `-i` | ignore case |
| `-n` | numeri di riga |
| `-w` | whole word |
| `-v` | invert (righe NON corrispondenti) |
| `-l` | solo i nomi dei file con match |
| `-c` | solo il conteggio per file |
| `-o` | solo la parte corrispondente |
| `-F` | pattern letterale (non regex) |
| `-.` | includi file nascosti |
| `-m N` | max N hit per file |
| `-A N` / `-B N` / `-C N` | N righe di contesto dopo / prima / attorno (contesto stampato `file-riga-testo`, gruppi separati da `--`) |
| `--type EXT` | solo file con quell'estensione (es. `c`, `csv`) |
| `--include` / `--exclude GLOB` | filtri glob |
| `--depth N` | profondità massima di ricorsione |

## 3. Comando CLI headless (`gmb grep` / `gmb rg`)

gmb grep [stesse opzioni] PATTERN [PATH]
gmb rg   ...   (alias)     # PATH default = cartella corrente


gmb grep -rn --type c TODO src
gmb rg -in -C2 needle .


## 4. Note implementative

## 5. Riferimenti

## Test e sample (§4.1)

### Json-Guida-Utente

*Fonte: `Json-Guida-Utente`*


## 1. Parsing: `JSON.PARSE(source$) → oggetto`

IMPORT JSON

DIM q$ = CHR(34)
DIM src$ = "{" + q$ + "name" + q$ + ": " + q$ + "Ada" + q$ + ", " + q$ + "age" + q$ + ": 36}"
DIM obj = JSON.PARSE(src$)


## 2. Lettura di un campo: `JSON.EXTRACT(obj, path$) → stringa`

PRINT JSON.EXTRACT(obj, "name")        ' campo piatto:  Ada
PRINT JSON.EXTRACT(obj, "user.name")   ' chiave annidata
PRINT JSON.EXTRACT(obj, "tags[0]")     ' indice array (segmento finale)


## 3. Serializzazione: `JSON.TOJSON(value) → stringa`

DIM list = {1, 2, 3}
PRINT JSON.TOJSON(list)     ' [1,2,3]


## 4. File e settings

## 5. Verifiche sui quattro backend
| Funzione | MiniTest | IL | PCodeVM | VM-C | C99Native |
|---|---|---|---|---|---|
| `JSON.PARSE` | `json/parse.bas`, `json/nested.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.EXTRACT` | `json/parse.bas`, `json/extract_path.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.TOJSON`/`STRINGIFY` | `json/tojson.bas`, `json/tojson_dict.bas`, `json/stringify.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.TOOBJECT` | `json/toobject.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.LOAD` | `json/load.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.READVALUE` | `json/readvalue.bas` (entrambe le arità) | ✅ | ✅ | ✅ | ✅ |
| `JSON.READSECTION` | `json/readsection.bas` (INI), `json/readsection_json.bas` (JSON, entrambe le arità) | ✅ | ✅ | ✅ | ✅ |
| Metodi DICT | `collections/set_get_grafe.bas`, `setkeyvalue.bas`, `minmaxpair.bas`, `dict_values.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON SAVE`/`WRITEVALUE`/`WRITESECTION` (comandi) | `json/commands.bas` | ✅ | ✅ | ✅ | ✅ |
| varianti comandi (sezione, aggiornamento, sostituzione, lista) | `json/commands_variants.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON.KEYS` | `json/keys.bas` | ✅ | ✅ | ✅ | ✅ |
| `JSON OPEN` (comando, handle) | — | ✅ | ✅ | ✅ | ⛔ gap |

## Vedi anche

<!-- END SURFACE -->




<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **IO.APPPATH()** — `PRINT "AppPath: "; LEN(IO.AppPath()) > 0` (shared/samples/Files/FilesFullCoverage.bas:50)
- **IO.BINPATH()** — `PRINT "BinPath: "; LEN(IO.BinPath()) > 0` (shared/samples/Files/FilesFullCoverage.bas:53)
- **IO.COUNTLINES(x1)** — `PRINT "CountLines: "; IO.CountLines(seq)` (shared/samples/Files/FilesFullCoverage.bas:68)
- **IO.CWD()** — `registryRoot = IO.NORMALIZEPATH(IO.CWD() + "/" + registryRoot)` (shared/samples/PackageRegistry/PackageServer.bas:84)
- **IO.DIR(x1)** — `entries = IO.DIR(registryRoot + "/")` (shared/samples/PackageRegistry/PackageServer.bas:157)
- **IO.DIREXISTS(x1)** — `IF IO.DirExists(GMBWORK$) = 0 THEN MKDIR GMBWORK$` (shared/samples/Wsp/Derive.bas:12)
- **IO.EOF(x1)** — `230 WHILE NOT IO.EOF(1)` (shared/samples/Files/FilesIO.bas:80)
- **IO.FCLOSE(x1)** — `IO.FCLOSE(h)` (shared/samples/Crypto/VerificaDownload.bas:25)
- **IO.FCREATE(x1)** — `h = IO.FCREATE("work/pacchetto.zip.txt")` (shared/samples/Crypto/VerificaDownload.bas:23)
- **IO.FERROR()** — `PRINT "nessun errore: "; (IO.FERROR() = 0)` (shared/samples/Files/ArchivioDocumenti.bas:51)
- **IO.FINPUT(x1, x2)** — `PRINT "FINPUT(0) len: "; LEN(IO.FINPUT(1, 0))` (shared/samples/System/SystemFullCoverage.bas:20)
- **IO.FOPEN(x1)** — `DIM r = IO.FOPEN(tmp)` (shared/samples/Math/MathAtnVectorAngle.bas:96)
- **IO.FREAD(x1)** — `PRINT "riletto         = "; IO.FREAD(r)` (shared/samples/Math/MathAtnVectorAngle.bas:97)
- **IO.FSEEK(x1, x2)** — `IO.FSeek(h, 0)` (shared/samples/Files/FilesFullCoverage.bas:37)
- **IO.FWRITE(x1, x2)** — `IO.FWRITE(h, contenuto)` (shared/samples/Crypto/VerificaDownload.bas:24)
- **IO.FILEEXISTS(x1)** — `IF IO.FILEEXISTS("work/thread.db") THEN KILL "work/thread.db"` (shared/samples/Databases/SqlTimeBucketOhlcv.bas:10)
- **IO.FOLDERSIZE(x1)** — `PRINT "FolderSize: "; IO.FolderSize(".") >= 0` (shared/samples/Files/FilesFullCoverage.bas:29)
- **IO.FREEFILE()** — `760 PRINT "Prossimo handle libero (IO.FREEFILE):"; IO.FREEFILE()` (shared/samples/Files/FilesIO.bas:132)
- **IO.GETTEMPDIR()** — `DIM dir = IO.GETTEMPDIR()` (shared/samples/Math/MathAtnVectorAngle.bas:89)
- **IO.GETTEMPFILE()** — `DIM tmp = IO.GETTEMPFILE()` (shared/samples/Math/MathAtnVectorAngle.bas:90)
- **IO.GETFOLDERPATH(x1)** — `PRINT "GetFolderPath: "; IO.DirExists(IO.GetFolderPath("."))` (shared/samples/Files/FilesFullCoverage.bas:27)
- **IO.LOF(x1)** — `720 PRINT "IO.LOF('"; FILE_SEQ$; "') ="; IO.LOF(1); "bytes"` (shared/samples/Files/FilesIO.bas:126)
- **IO.NORMALIZEPATH(x1)** — `registryRoot = IO.NORMALIZEPATH(IO.CWD() + "/" + registryRoot)` (shared/samples/PackageRegistry/PackageServer.bas:84)
- **IO.PARENTDIR(x1)** — `PRINT "ParentDir: "; IO.DirExists(IO.ParentDir(seq))` (shared/samples/Files/FilesFullCoverage.bas:54)
- **IO.PROJECTFOLDER()** — `PRINT "app="; (LEN(IO.APPPATH()) > 0); "|bin="; (LEN(IO.BINPATH()) > 0); "|proj="; (LEN(IO.PROJECTFO…` (shared/minitests/io/io_all.bas:56)
- **IO.READBYTES(x1)** — `DIM bytes() = IO.ReadBytes(seq)` (shared/samples/Files/FilesFullCoverage.bas:60)
- **IO.READFILE(x1)** — `ripristinato = IO.READFILE("work/backup_out/report.txt")` (shared/samples/compression/BackupReport.bas:41)
- **IO.READLINES(x1)** — `DIM lines = IO.READLINES("/tmp/gmb_compression_sample_out/gmb_compression_sample.txt")` (shared/samples/compression/CompressionRoundTrip.bas:33)
- **IO.RESOLVEFILEPATH(x1)** — `PRINT "ResolveFilePath: "; IO.FileExists(IO.ResolveFilePath(seq))` (shared/samples/Files/FilesFullCoverage.bas:55)
- **IO.SEEK(x1)** — `560 PRINT "Posizione dopo scrittura (IO.SEEK(1)):"; IO.SEEK(1); "(Atteso 11)"` (shared/samples/Files/FilesIO.bas:112)
- **IO.TEMPDIR()** — `PRINT "TempDir: "; LEN(IO.TempDir()) > 0` (shared/samples/Files/FilesFullCoverage.bas:52)
- **IO.TEMPFILE()** — `PRINT "TempFile: "; LEN(IO.TempFile()) > 0` (shared/samples/Files/FilesFullCoverage.bas:51)
- **IO.WRITEBYTES(x1, x2)** — `PRINT "writebytes="; IO.WRITEBYTES("/tmp/gmb_mt_io_all_wb.bin", wb)` (shared/minitests/io/io_all.bas:83)
- **JSON.EXTRACT(x1, x2)** — `250 PRINT "Integrity valid: "; JSON.EXTRACT(checkJson, "valid")` (shared/samples/Databases/QueryPhaseD.bas:39)
- **JSON.KEYS(x1)** — `FOR EACH k IN JSON.KEYS(gPackages)` (shared/samples/PackageRegistry/PackageServer.bas:344)
- **JSON.LOAD(x1)** — `380 PRINT "--- 6. Caricamento da file con JSON.Load ---"` (shared/samples/Types/TypesJson.bas:96)
- **JSON.PARSE(x1)** — `230 ' INTEGRITY ritorna il TESTO JSON: si tiene in una stringa e si converte con JSON.PARSE` (shared/samples/Databases/QueryPhaseD.bas:34)
- **JSON.READSECTION(x1)** — `DIM keys AS JSON = JSON.ReadSection(settingsPath$, "paths")` (shared/samples/System/SettingsPaths.bas:49)
- **JSON.READVALUE(x1)** — `PRINT "configDir   = "; JSON.ReadValue(settingsPath$, "paths", "configDir")` (shared/samples/System/SettingsPaths.bas:36)
- **JSON.STRINGIFY(x1)** — `1120 PRINT "Registro Legacy (Spese:Libri): " + JSON.Stringify(tuttiConti$)` (shared/samples/Databases/DbLedgerFullTests.bas:159)
- **JSON.TOJSON(x1)** — `DIM UdtAsJson AS JSON = JSON.ToJson(MyUdt)` (shared/samples/Json/JsonFullTests.bas:124)
- **JSON.TOOBJECT(x1, x2)** — `DIM NewUdt AS ConfigData = JSON.ToObject(ConfigData, UdtAsJson)` (shared/samples/Json/JsonFullTests.bas:127)
<!-- END DOMAIN TABLE -->
