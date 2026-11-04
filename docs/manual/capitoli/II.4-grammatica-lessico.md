# II.4 — Grammatica e lessico

## Cosa ottieni da questo capitolo

Le regole che valgono prima della semantica: come si separano le istruzioni, come si scrivono i
commenti, quali caratteri formano un nome, come si compongono le stringhe, e cosa fa il
preprocessore prima che il programma esista. Sono le regole che spiegano gli errori di sintassi
più comuni.

## 1. Una riga, un'istruzione

Il confine naturale di un'istruzione è la fine della riga:

```basic
DIM totale AS DOUBLE
totale = 0
PRINT "pronto"
```

Il due punti `:` separa più istruzioni sulla **stessa** riga:

```basic
DIM a AS INTEGER : a = 5 : PRINT a
```

Una riga lunga si spezza con l'underscore `_` a fine riga, e la riga successiva continua
l'istruzione. La continuazione si può anche scrivere mettendo i due punti **all'inizio** della
riga seguente:

```basic
PRINT "Valore="; _
   42
: PRINT "continua"
```

> **Non confondere `_` con la continuazione di stringa.** Dentro una stringa raw (vedi §5)
> l'underscore è testo, non continuazione: è una delle prove del sample di §6.

## 2. Commenti

| Forma | Ambito | Note |
|---|---|---|
| `' testo` | fino a fine riga | forma idiomatica BASIC |
| `REM testo` | fino a fine riga | forma storica, ancora valida |
| `// testo` | fino a fine riga | stile C, accettato dal lessico |
| `/* … */` | più righe | stile C, si chiude esplicitamente |

Un commento non è codice e non viene preprocessato: una direttiva scritta dentro un commento
multiriga **non** ha effetto (è la prova del caso 16 dell'esempio in §6).

## 3. Nomi e suffissi

- I nomi sono **case-insensitive**: `Totale`, `totale` e `TOTALE` sono la stessa variabile.
- Il nome può portare un **suffisso di tipo**: `$` stringa, `%` intero, `!` singola precisione,
  `#` doppia precisione. Il suffisso fa parte della dichiarazione implicita, non del nome.
- Le parole chiave del linguaggio non sono utilizzabili come nomi.

La forma consigliata resta `DIM nome AS TIPO`: è esplicita e non dipende dal suffisso.

## 4. Numeri di riga: si possono scrivere, non servono

I programmi storici hanno i numeri di riga, e il lessico li accetta ancora:

```basic
10 REM programma storico
20 PRINT "ciao"
30 END
```

I numeri **non** sono etichette utilizzabili da `GOTO`: servono solo a far leggere il sorgente
ai programmi scritti per il BASIC classico. Il codice nuovo non li usa, e le etichette per
`GOTO`/`GOSUB` sono una sintassi separata (`nome:`).

> **Attenzione a mescolarli.** Un numero di riga seguito da un'istruzione su più righe fisiche
> (continuazione `_`) funziona, ma il numero appartiene alla **riga logica**: la numerazione
> delle righe fisiche successive non esiste.

## 5. Stringhe

| Forma | Sintassi | Contenuto |
|---|---|---|
| normale | `"ciao"` | testo; l'apice doppio si raddoppia: `"un ""apice"" interno"` |
| interpolata | `$"totale: {tot}"` | valutata a runtime, sostituisce `{espressione}` |
| raw, una riga | `` `testo` `` | nessuna interpretazione: apici doppi e `#` sono testo |
| raw, più righe | `` ` `` … `` ` `` | conserva gli a capo |

La stringa raw esiste per i casi in cui il testo contiene i delimitatori del linguaggio — JSON,
frammenti SQL, espressioni regolari. Con le stringhe normali servirebbe un raddoppio per ogni
apice; con la raw no.

## 6. Il preprocessore

Prima del parsing, le direttive decidono quali righe entrano nel programma:

| Direttiva | Effetto |
|---|---|
| `#DEF SIMBOLO` | definisce un simbolo |
| `#UNDEF SIMBOLO` | lo rimuove |
| `#IFDEF SIMBOLO` | include il blocco se il simbolo è definito |
| `#IFUNDEF SIMBOLO` | include il blocco se **non** è definito |
| `#ELSE` | ramo alternativo |
| `#ENDIF` | chiude il blocco |

I blocchi si annidano, e le direttive non hanno effetto dentro le stringhe raw né dentro i
commenti: il preprocessore distingue il testo dal codice.

## 7. Esempio eseguito

`shared/samples/Lexer/LexerRawStrings.bas` mette insieme tutti i casi. Eseguito sul backend
C99Native, stampa:

```
Inizio
Modalità Debug Attiva (Livello 1)
Annidato: Debug e Production sono attivi.
Questo non dovrebbe mai apparire
Valore= 200.06
Test Colon Continuation
FATTO
OK
Multi-line 1 Fatto
`Questo viene stampato in produzione`
`Questo è per il debug`
Modalità Legacy NON definita. Corretto.
```

Tre cose si leggono direttamente:

1. **Il blocco escluso non sparisce dal sorgente, sparisce dal programma.** `Questo non dovrebbe
   mai apparire` viene stampato davvero: sta nel ramo `#ELSE` di un `#IFDEF PRODUCTION` annidato,
   e `PRODUCTION` è definito. È l'esempio di come una direttiva possa cambiare il comportamento
   senza toccare una sola riga di codice.
2. **La continuazione funziona su più righe fisiche**: `Valore=` è seguito da `200.06`, che è
   `42/... + 700` calcolato attraverso due continuazioni.
3. **La direttiva dentro la stringa raw resta testo.** Le due righe con i backtick stampano
   anche il contenuto che sembra una direttiva: il preprocessore non l'ha eseguita.

E la verifica negativa, il caso più utile:

```
ERRORE: Il simbolo 'SHOULD_NOT_EXIST' è stato definito illegalmente!
OK: Il simbolo 'SHOULD_NOT_EXIST' non è stato definito.
```

La prima riga **non** compare nell'output: il `#DEF` scritto dentro un commento `/* … */` non ha
definito nulla, quindi il ramo `#IFDEF SHOULD_NOT_EXIST` è stato escluso e si è stampato solo il
messaggio del ramo `#ELSE`. Un preprocessore che leggesse il testo senza distinguere i commenti
avrebbe stampato la riga sbagliata.

## 8. Comandi e funzioni: la regola che nasce dal lessico

La distinzione è strutturale, non stilistica:

```basic
DBF USE "clienti.dbf" ALIAS C     ' comando: nessuna parentesi
n = DBF.LASTREC()                 ' funzione: parentesi sempre
```

- Un **comando** esegue e non restituisce niente: si scrive con le parole chiave separate da
  spazio, e **non** ammette la forma `DBF.USE(...)`.
- Una **funzione** restituisce un valore e si usa dentro un'espressione: **sempre** con le
  parentesi.

Un nome non può fare l'uno e l'altro secondo il contesto: quando serve sia scrivere sia leggere,
esistono due nomi distinti (comando `AI SET ACTIVE`, funzione `AI.ACTIVE`). Il lessico di un
programma che rispetta la regola si legge senza ambiguità.

## 9. Errori frequenti

| Messaggio o sintomo | Causa | Correzione |
|---|---|---|
| errore di sintassi alla riga successiva | manca `_` di continuazione | aggiungi `_` o chiudi l'istruzione |
| percentuale o cancelletto "spariti" dal nome | il suffisso è stato interpretato come tipo | scrivi `DIM nome AS TIPO` |
| `Expected newline after ...` | due istruzioni sulla stessa riga senza `:` | inserisci `:` o vai a capo |
| una direttiva sembra ignorata | è dentro una stringa raw o un commento | sposta la direttiva fuori dal testo |
| `CASE ...: corpo` rifiutato | il corpo di un `CASE` va su riga successiva | vedi II.7 |

## 10. Dove andare adesso

- **II.5** — tipi, variabili e scope: cosa significano i nomi che il lessico ha riconosciuto.
- **II.7** — controllo di flusso: `IF`, `SELECT CASE`, cicli.
- **II.11** — moduli e preprocessore usato su più file.
- **III.12** — come queste forme diventano bytecode e codice C.














<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **PRINT** — `PRINT "=== Composability Demo ==="` (shared/samples/Connectors/ConnectorComposability.bas)
- **CONNECTOR** — `AI CONNECTOR LOAD "gmail"` (shared/samples/Connectors/ConnectorComposability.bas)
- **LOAD** — `AI CONNECTOR LOAD "gmail"` (shared/samples/Connectors/ConnectorComposability.bas)
- **WRITE** — `DIM drive$ AS STRING = AI.CONNECTORCALL("google-drive", "write", "{""path"":""/contracts/hotel.pdf""…` (shared/samples/Connectors/ConnectorComposability.bas)
- **SET** — `AI CONNECTOR SECRET SET "google-sheets", "api_key", "demo"` (shared/samples/Connectors/ConnectorT1_SheetsNotionLinear.bas)
- **HTTP** — `PRINT "=== T1 OK (F2 HTTP generico, 200 se token reale) ==="` (shared/samples/Connectors/ConnectorT1_SheetsNotionLinear.bas)
- **SEND** — `AI CONNECTOR POLICY SET "gmail", "send", "approve"` (shared/samples/Connectors/AgentConnectorGmailAuto.bas)
- **GET** — `AI CONNECTOR MARKETPLACE GET "gmail"` (shared/samples/Connectors/ConnectorFullCoverage.bas)
- **JSON** — `AI CONNECTOR REGISTER "petstore", "openapi", "https://petstore.swagger.io/v2/swagger.json"` (shared/samples/Connectors/ConnectorOpenApiPetstore.bas)
- **VECTOR** — `DIM ai$ AS STRING = AI.CONNECTORSEARCH("vector")` (shared/samples/Connectors/ConnectorMarketplace.bas)
- **OPEN** — `DIM iss$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic"",""state"":""open""}…` (shared/samples/Connectors/AgentConnectorComposeAuto.bas)
- **CLS** — `CLS` (shared/samples/UI/UiType.bas)
- **INPUT** — `DIM result = UI.TYPE("input[type=text]", "testo di esempio")` (shared/samples/UI/UiType.bas)
- **KEYGEN** — `DIM keys = CR.KEYGEN("ECDSA", 256)` (shared/samples/Crypto/CryptoAsymmetric.bas)
- **KEYCLOSE** — `KEYCLOSE pvtHandle` (shared/samples/Crypto/CryptoFunctions.bas)
- **CLOSE** — `CLOSE #1` (shared/samples/Crypto/CryptoStrHashFile.bas)
- **MKDIR** — `IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work` (shared/samples/Crypto/CryptoUncoveredCoverage.bas)
- **KILL** — `5 KILL g_work + "/sql_minimal.db" : ON ERROR RESUME NEXT` (shared/samples/Databases/SqlMinimalCrud.bas)
- **FETCH** — `70 DO WHILE SQL.FETCH(rs)` (shared/samples/Databases/SqlMinimalCrud.bas)
- **RMDIR** — `110 IF IO.DIREXISTS(cWork) THEN RMDIR cWork, TRUE` (shared/samples/Databases/DbfMemoCrud.bas)
- **COPY** — `150 COPY cGolden83 TO g_work + "/dbase_83.dbf"` (shared/samples/Databases/DbfMemoCrud.bas)
- **GO** — `190 DBF GO TOP` (shared/samples/Databases/DbfMemoCrud.bas)
- **PUT** — `360 PRINT "dBase IV memo put="; MEMO` (shared/samples/Databases/DbfMemoCrud.bas)
- **SEEK** — `ISAM SEEK X, "T2"` (shared/samples/Databases/ISAM_Minimal_Test.bas)
- **DONE** — `PRINT "DONE"` (shared/samples/Databases/ISAM_Minimal_Test.bas)
- **LOCK** — `PRINT "2. Lock e sicurezza"` (shared/samples/Databases/RDD_SQL_CRUD.bas)
- **UNLOCK** — `RDD UNLOCK 1` (shared/samples/Databases/RDD_SQL_CRUD.bas)
- **CHDIR** — `CHDIR cPath` (shared/samples/Databases/DbSpeedFullTests.bas)
- **VIEW** — `// SPEED CREATE VIEW (Legacy) - Crea una vista persistente` (shared/samples/Databases/DbSpeedFullTests.bas)
- **CHANNEL** — `SUB Worker(ID AS INTEGER, InC AS CHANNEL OF INTEGER, OutC AS CHANNEL OF INTEGER, WG AS WAITGROUP)` (shared/samples/Databases/bench_concurrency.bas)
- **RECEIVE** — `RECEIVE V FROM InC` (shared/samples/Databases/bench_concurrency.bas)
- **WAIT** — `WAIT WG` (shared/samples/Databases/bench_concurrency.bas)
- **ZIP** — `SQL EXEC S, "INSERT INTO ev VALUES (9,'[" + q + "z" + q + "]','[42]','{" + q + "city" + q + ":" + q …` (shared/samples/Databases/SqlWritableSources.bas)
- **CONVERT** — `ISAM CONVERT DBF g_work + "/dbf_source.dbf" TO ISAM g_work + "/isam_from_dbf.isdb" TYPE DbfTestType …` (shared/samples/Databases/DbIsamFullTests.bas)
- **ONCE** — `AI RAG INDEX TEXT "sdb", "articles", "a1", "Keyword alpha appears once. Semantic topic: database ind…` (shared/samples/Databases/VectorSearchJson.bas)
- **MESSAGE** — `PRINT $"Message: '{msg}' has {LEN(msg)} chars"` (shared/samples/Strings/StringsInterpolation.bas)
- **CLEAR** — `225 PRINT "--- 8. TEST Clear ---"` (shared/samples/Strings/StringsStringBuilder.bas)
- **SETOPTION** — `AI SET OPTION "execplan", plan` (shared/samples/AiLLM/ExecPlanRuntime.bas)
- **WINDOW** — `TRAIN DISTILL BUILD jsonl, { "model": model, "out": ds, "window": 16, "holdout": 0.2, "max": 40 }` (shared/samples/AiLLM/DistillPipeline.bas)
- **CIRCLE** — `DESCRIBE = "Circle(r=" + STR(SELF.Raggio) + ")"` (shared/samples/Types/TypesImplementsInterface.bas)
- **RANDOMIZE** — `1120 RANDOMIZE 12345` (shared/samples/Types/TypesConversions.bas)
- **POINT** — `DIM point = (x: 100, y: 200, z: 300)` (shared/samples/Types/TypesNamedTuple.bas)
- **SCREEN** — `SCREEN 12, 320, 200, 32` (shared/samples/Types/OperatorsDoEventsDemo.bas)
- **LINE** — `LINE (i * 10, 60)-(i * 10, 140), 10` (shared/samples/Types/OperatorsDoEventsDemo.bas)
- **PSET** — `IF (i XOR 1) <> 0 THEN PSET (i * 10, 40), 7` (shared/samples/Types/OperatorsDoEventsDemo.bas)
- **DOEVENTS** — `DOEVENTS` (shared/samples/Types/OperatorsDoEventsDemo.bas)
- **MATRIX** — `DIM m = MATRIX.CREATE(2, 2)` (shared/minitests/matrix/matrix_all.bas)
- **AEVAL** — `a = EVAL("localInt + 5")` (shared/samples/Clipper/ClipperMacroLocalVar.bas)
- **MOUSE** — `DBF REPLACE Region WITH "Nord", Product WITH "Mouse", Amount WITH 10.00` (shared/samples/Clipper/ClipperAggregate.bas)
- **MOVE** — `ON WINDOW MOVE GOSUB FinestraSpostata` (shared/samples/Clipper/ClipperEvents.bas)
- **WAITKEY** — `WAITKEY` (shared/samples/Clipper/ClipperEvents.bas)
- **COLOR** — `WINDOW Main PRINT "color     = " + LTRIM(STR(n))` (shared/samples/Clipper/GraphicsControlsDemo.bas)
- **SLEEP** — `SLEEP 1000` (shared/samples/Clipper/GraphicsControlsDemo.bas)
- **AADD** — `PRINT "--- 1. Test AADD (Aggiunta) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **AINS** — `PRINT "--- 2. Test AINS / ADEL (Inserimento/Cancellazione) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **ADEL** — `PRINT "--- 2. Test AINS / ADEL (Inserimento/Cancellazione) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **AFILL** — `PRINT "--- 3. Test AFILL (Riempimento) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **ACOPY** — `PRINT "--- 4. Test ACOPY (Copia) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **ASORT** — `PRINT "--- 6. Test ASORT (Ordinamento) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **ASHUFFLE** — `PRINT "--- 14. Test AREVERSE e ASHUFFLE (Ordinamento inverso e casuale) ---"` (shared/samples/Clipper/ClipperArrays.bas)
- **CAPTION** — `@ 4, 10 GET ok TYPE "CheckBox" CAPTION "Accetto i termini"` (shared/samples/Clipper/ClipperGetValidControls.bas)
- **SAVESCREEN** — `SAVE SCREEN "snap1"` (shared/samples/Clipper/SSel.bas)
- **WATCH** — `WINDOW DEFINE Watch FROM 4, 1 TO 13, 23 TITLE "Watchlist"` (shared/samples/Clipper/TradingPlatform.bas)
- **CHART** — `WINDOW DEFINE Chart FROM 4, 24 TO 19, 56 TITLE "BTCUSDT - 1m"` (shared/samples/Clipper/TradingPlatform.bas)
- **CLIP** — `IMPORT CLIP` (shared/samples/Clipper/WindowEditable.bas)
- **RESTORESCREEN** — `RESTORE SCREEN "snap1"` (shared/samples/Clipper/WindowEditable.bas)
- **CPRINT** — `AEVAL Clienti, { |c| PRINT "  -> Cliente: "; c.Nome; ", Fatturato: "; c.Fatturato }` (shared/samples/Clipper/ClipperCodeBlocks.bas)
- **FONT** — `FONT "font.ttf", 12` (shared/samples/Clipper/WindowPixelColorTest.bas)
- **PIXEL** — `WINDOW DEFINE Win FROM 140, 170 TO 340, 470 TITLE "TEST" PIXEL` (shared/samples/Clipper/WindowPixelColorTest.bas)
- **WEB2MD** — `WEB2MD url INTO md` (shared/samples/Web/WebToMarkdown.bas)
- **SPRITE** — `h = MEDIA.IMGREAD("sprite.png")` (shared/samples/include/media/MediaPaletteDemo.bas)
- **SHUTDOWN** — `PRINT "[SERVER] Eseguo SHUTDOWN parziale (lato invio)."` (shared/samples/Network/NetTcpAdvanced.bas)
- **COND** — `PRINT "cond     ="; LINALG.COND(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas)
- **MUTEX** — `IMPORT MUTEX` (shared/samples/Math/MathAtnVectorAngle.bas)
- **FREAD** — `PRINT "riletto         = "; IO.FREAD(r)` (shared/samples/Math/MathAtnVectorAngle.bas)
- **SYNC** — `HTTP ROUTE "POST", "/api/packages/{name}/sync", HandleApiSync()` (shared/samples/PackageRegistry/PackageServer.bas)
- **SHELL** — `SET SHELL ON` (shared/samples/PackageRegistry/PackageServer.bas)
- **UNZIP** — `COMPRESSION.UNZIP(tmpZip, staging)` (shared/samples/PackageRegistry/PackageServer.bas)
- **INKEY** — `WHILE INKEY() = ""` (shared/samples/Mouse/MouseTests.bas)
- **LOCATE** — `LOCATE 7, 1` (shared/samples/Mouse/MouseTests.bas)
- **CURSOR** — `MOUSE CURSOR 2` (shared/samples/Mouse/MouseTests.bas)
- **SENDKEY** — `SENDKEY CHR(27)` (shared/samples/Controls/CodeAreaDemo.bas)
- **PAINT** — `SUB Paint(g AS ANY)` (shared/samples/Controls/UserControlGfxSugarDemo.bas)
- **DRAW** — `CONTROL DRAW g, 0                  ' clear` (shared/samples/Controls/UserControlBaseDemo.bas)
- **DEFER** — `DEFER MUTEX UNLOCK mu` (shared/samples/Concurrency/PCodeGpmMutexDefer.bas)
- **VIEWPORT** — `REM verifica viewport` (shared/samples/Browser/BrowserE10.bas)
- **SPRITEDEF** — `SPRITEDEF START 0` (shared/samples/GameDev/SpritedefDemo.bas)
- **TILEMAP** — `TILEMAP LOAD texId, tileW, tileH, tilesPerRow, mapW, mapH, tiles` (shared/samples/GameDev/TilemapDemo.bas)
- **PLAY** — `PLAY LOOP "T100 O2 L8 C G C G E G E G" ON "bass"` (shared/samples/Audio/MusicPoliphonicDemo.bas)
- **SOUND** — `SOUND STOP ALL` (shared/samples/Audio/MusicPoliphonicDemo.bas)
- **PRESET** — `PRINT "  preset="; presets(i); " -> User-Agent ricevuto dal server: "; jp.userAgent` (shared/samples/Http/HttpFetchAdvanced.bas)
- **IMAGEGET** — `PRINT "   SCREEN 320,200 — LINE/CIRCLE/PAINT — IMAGEGET/IMAGEPUT"` (shared/samples/Ai/WebAgentCanvasImageDemo.bas)
- **IMAGEPUT** — `PRINT "   SCREEN 320,200 — LINE/CIRCLE/PAINT — IMAGEGET/IMAGEPUT"` (shared/samples/Ai/WebAgentCanvasImageDemo.bas)
- **PALETTE** — `PRINT "   JS: CanvasReplay({op:""putimage"",...}, canvas) → palette + AND/OR/XOR"` (shared/samples/Ai/WebAgentCanvasImageDemo.bas)
- **RENAME** — `PRINT "Costanti eventi: CREATE="; %FILE_CREATED; " DELETE="; %FILE_DELETED; " CHANGE="; %FILE_CHANGE…` (shared/samples/Files/FilesSystemWatcher.bas)
- **UNWATCH** — `PRINT "--- 4. Test Disattivazione UNWATCH ---"` (shared/samples/Files/FilesSystemWatcher.bas)
- **LINE INPUT** — `PRINT "4) LINE INPUT# + EOF"` (shared/samples/Files/SequentialFilesFull.bas)
- **MEMORYMAP** — `REM FILE: Files/MemoryMap.bas` (shared/samples/Files/FilesMemoryMap.bas)
- **MMAPPOKE** — `PRINT "--- 3. Test MEMORYMAP.MMAPPOKE (Scrittura singoli byte) ---"` (shared/samples/Files/FilesMemoryMap.bas)
- **MMAPWRITEBYTES** — `PRINT "--- 5. Test MEMORYMAP.MMAPWRITEBYTES (Scrittura blocco di byte) ---"` (shared/samples/Files/FilesMemoryMap.bas)
- **ERASE** — `PRINT "-- KILL / ERASE --"` (shared/samples/Files/FilesFullCoverage.bas)
- **XCOPY** — `PRINT "-- IO.XCOPY --"` (shared/samples/Files/IoUncoveredCoverage.bas)
- **SCREENSET** — `130 SCREENSET , 1` (shared/samples/Sprites/SpritesAndPages.bas)
- **CONVBAT** — `CONVBAT "work/fc_in", "md", 0 INTO cbatch$` (shared/samples/Documents/ConvertFullCoverage.bas)
- **CCLS** — `CCLS : CPRINT "Caricamento texture 'sprite.png'..."` (shared/samples/graphics/GraphicsDrawTextures.bas)
- **POLYGON** — `ShowIntroScreen("Test di POLYGON", "Disegno di un poligono pieno")` (shared/samples/graphics/GraphicsDrawTextures.bas)
- **CLOCATE** — `370 CLOCATE 20, 1` (shared/samples/graphics/GraphicsFonts.bas)
- **PCOPY** — `20 ' Testa PSET/PRESET, PAINT, PCOPY, SCREENSET, SAVE/RESTORE SCREEN.` (shared/samples/graphics/GraphicsMultipleScreens.bas)
- **BSAVE** — `PRINT "GRAPHICS - BSAVE/BLOAD scoperte dagli esempi"` (shared/samples/graphics/GraphicsUncoveredCoverage.bas)
- **BLOAD** — `PRINT "GRAPHICS - BSAVE/BLOAD scoperte dagli esempi"` (shared/samples/graphics/GraphicsUncoveredCoverage.bas)
- **IMAGEPUTS** — `SCREENSET 0, 0 ' attiva=visibile=0: il SYNC+IMAGEPUTS disegnano sulla pagina che il Present mostra (…` (shared/samples/graphics/GraphicsPutsVsTextures.bas)
- **TEXTMODE** — `PRINT "TEXTMODE "; modes(i)` (shared/samples/graphics/ConsoleModeSelect.bas)
- **CSAVESCREEN** — `PRINT "-- CSAVESCREEN / CRESTORESCREEN --"` (shared/samples/graphics/GraphicsConsoleModes.bas)
- **CRESTORESCREEN** — `PRINT "-- CSAVESCREEN / CRESTORESCREEN --"` (shared/samples/graphics/GraphicsConsoleModes.bas)
- **CPALETTE** — `PRINT "-- CPALETTE --"` (shared/samples/graphics/GraphicsConsoleModes.bas)
- **SCROLLMODE** — `SCROLLMODE "BUFFERED"` (shared/samples/Console/ConsoleSaveRestoreScreen.bas)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Grammar

*Fonte: `Grammar`*


## Panoramica

## Lessico

### Keyword principali (~280)

### Operatori
| Operatore | Descrizione |
|-----------|-------------|
| `+` `*` `-` `/` `\` `^` | Aritmetici |
| `MOD` | Modulo |
| `=` `<>` `<` `>` `<=` `>=` | Confronto |
| `AND` `OR` `NOT` `XOR` `IMP` `EQV` | Logici |
| `??` | Null-coalescing |
| `?` `:` | Ternary |
| `++` `--` | Incremento/decremento |
| `<<` `>>` | Shift bitwise |
| `?.` `?()` | Null-conditional |
| `...` | ParamArray / spread |
| `&` | Concatenazione stringhe |

### Letterali

## Sistema di Tipi
| Tipo | Descrizione |
|------|-------------|
| `BYTE` / `SBYTE` | 8-bit intero |
| `SHORT` / `USHORT` | 16-bit intero |
| `INTEGER` / `UINTEGER` | 32-bit intero |
| `LONG` / `ULONG` | 64-bit intero |
| `QUAD` / `UQUAD` | 128-bit intero |
| `SINGLE` / `DOUBLE` | Float 32 / 64 bit |
| `CURRENCY` | Decimale finanziario |
| `STRING` | Unicode variabile |
| `BIT` / `BOOL` | Booleano |
| `DATE` / `TIME` / `DATETIME` / `TIMESPAN` | Temporali |
| `ANY` | Variant dinamico |
| `VOID` | Nessun valore |
| `ERR` | Oggetto errore |

### Tipi complessi

## Controllo Flusso

IF...THEN...ELSEIF...ELSE...END IF
condition ? trueVal : falseVal        ' Ternary inline
SELECT CASE value / CASE / CASE ELSE / END SELECT
FOR i = start TO end [STEP step] / NEXT i
FOR EACH item IN collection / NEXT
DO WHILE...LOOP / DO...LOOP UUNTIL
WHILE...WEND
ON ERROR GOTO / RESUME [NEXT|0|label]


## Procedure

### SUB

SUB DrawSprite(x AS INTEGER, y AS INTEGER, id AS LONG)
END SUB


### FUNCTION

FUNCTION Add(a AS INTEGER, b AS INTEGER) AS INTEGER
    FUNCTION = a + b
END FUNCTION


### Lambda (inline)

DIM f = FUNCTION(x) x * 2
DIM g = SUB(x) PRINT x


### Generator (YIELD)

FUNCTION Fibonacci(n AS INTEGER) AS IENUMERABLE OF LONG
    DIM a = 0, b = 1
    FOR i = 1 TO n
        YIELD a
        DIM t = a + b : a = b : b = t
    NEXT
END FUNCTION


### Property

CLASS Person
    PRIVATE _name AS STRING
    PROPERTY Name AS STRING
        GET: RETURN _name
        SET: _name = VALUE
    END PROPERTY
END CLASS


## Classi e OOP

CLASS Shape
    VIRTUAL FUNCTION Area() AS DOUBLE
        RETURN 0.0
    END FUNCTION
END CLASS

CLASS Circle INHERITS Shape
    PUBLIC Radius AS DOUBLE
    VIRTUAL FUNCTION Area() AS DOUBLE
        RETURN 3.14159 * Radius * Radius
    END FUNCTION
END CLASS

CLASS Rectangle INHERITS Shape
    PUBLIC Width AS DOUBLE, Height AS DOUBLE
    OVERRIDE FUNCTION Area() AS DOUBLE


## Gestione Errori

TRY
    DIM result = 1 \ 0
CATCH ex AS DivideByZeroException
    PRINT "Errore: " + ex.Message
FINALLY
    PRINT "Pulizia"
END TRY

' Rethrow
THROW New ArgumentException("Parametro non valido")


## Array e Collezioni

' Dichiarazione
DIM arr(10) AS INTEGER
DIM mat(5, 5) AS DOUBLE
DIM list AS NEW LIST OF STRING

' Literal
DIM nums = {1, 2, 3, 4, 5}
DIM dict = {"key": "value"}
DIM matrix = {{1, 2}, {3, 4}}

' Comprehension
DIM evens = FROM x IN 1 TO 100 WHERE x MOD 2 = 0 SELECT x
Dim squares = (FROM x IN 1 TO 10 SELECT x * x)


### ASORT — ordinamento con comparatore

TYPE Prodotto
    Nome AS STRING * 20
    Prezzo AS CURRENCY
END TYPE

DIM P(2) AS Prodotto
P(0).Nome = "Penna" : P(0).Prezzo = 1.5
P(1).Nome = "Monitor" : P(1).Prezzo = 250
P(2).Nome = "Tastiera" : P(2).Prezzo = 90

' Predicato: TRUE = a prima di b -> ordine DECRESCENTE per Prezzo
ASORT P, { |a, b| RETURN a.Prezzo > b.Prezzo }

' Predicato crescente
ASORT P, { |a, b| RETURN a.Prezzo < b.Prezzo }



## Puntatori e FFI

DECLARE FUNCTION GetTickCount LIB "kernel32.dll () AS LONG
DECLARE SUB memcpy LIB "msvcrt.dll" ALIAS "memcpy" (BYVAL dest AS ANY, BYVAL src AS ANY, BYVAL n AS LONG)


## Null-Coalescing

' Operatore ?? — restituisce il destro se il sinistro è Nothing
DIM result = possiblyNull ?? defaultValue
DIM chained = a ?? b ?? c ?? "fallback"

' Confronto con IIF (meno efficiente, valuta entrambi)
DIM result = IIF(possiblyNull ISNOT Nothing, possiblyNull, defaultValue)


## Stringhe

' Standard
DIM s = "Hello World"

' Raw (tripla quotazione)
DIM raw = """Questa è una
stringa su più righe"""

' Backtick raw
DIM tick = `Nessun escape \n qui`

' Concatenazione
DIM full = "Hello" + " " + "World"
DIM full2 = "Hello" & " " & "World"

' Funzioni
UPPER$(s) / LOWER$(s)


## Feature in implementazione
| Feature | Stato | Esempio |
|---------|-------|---------|
| String Interpolation | ✅ Sprint 1 | `shared/samples/Strings/StringsInterpolation.bas` |
| Tuple | ✅ Sprint 1 | `shared/samples/Types/TypesTuple.bas` |
| Pattern Matching | ✅ Sprint 1 | `shared/samples/Branches/BranchesMatch.bas` |
| Async/Await | 🔄 Sprint 2 | `shared/samples/Concurrency/ConcurrencyAsync.bas` |
| SWITCH expression | ✅ 2026-06-30 | `shared/samples/Branches/BranchesSwitchExpr.bas` — `SWITCH(v) { CASE 1: "a", CASE 2: "b", ELSE: "?" }` |
| Nullable types (`INTEGER?`) | ✅ Sprint 3 | NullableTypes-Implementation |
| Generics tipizzati (`LIST OF T`) | ✅ Sprint 3 | GenericsImplementation |
| Type inference end-to-end | ✅ Sprint 3 | GenericsImplementation |
| Attributes (`<Serializable>`) | ✅ Sprint 4 | AttributesImplementation |
| LINQ Query Syntax | ✅ Sprint 5 | LinqQuerySyntax |
| StringBuilder | ✅ Sprint 5 | built-in |
| Regex | ✅ Sprint 5 | built-in |

## Note di Design

## Limiti noti

### Direttiva-Sintassi-AGENTS

*Fonte: `Direttiva-Sintassi-AGENTS`*


## 1. Principio fondamentale: comando ≠ funzione
| | **COMANDO** | **FUNZIONE** |
|---|---|---|
| Scopo | esegue un'azione, non ritorna un valore | ritorna un valore usato in un'espressione |
| Parentesi | **MAI** | **sempre** (`f(x, y)`) |
| Uso | riga di istruzione autonoma | dentro un'espressione (assegnazione, PRINT, condizione) |
| Esempio | `DBF USE "file" ALIAS D` | `n = DBF.LASTREC()` |

## 2. Le due forme sintattiche

### 2.0 Comando — SOLO sintassi legacy (spazio, senza parentesi)

NAMESPACE VERBO [oggetto] [clausole...]


DBF USE "Work/dbase_83.dbf" EXCLUSIVE ALIAS D83
ISAM OPEN "Work/test_min.isdb" OF R ALIAS X INDEXED BY ID
AI SET ACTIVE PROVIDER %CHAT, nome
AI RAG INDEX TEXT "db", "docs", "id", "testo"
WINDOW DEFINE Main FROM 1, 1 TO 50, 20 TITLE "Finestra"


### 2.1 Funzione — `NAMESPACE.FUNZIONE(argomenti)`

[risultato$ =] NAMESPACE.FUNZIONE(arg1, arg2, ...)


n = DBF.LASTREC()
attivo$ = AI.ACTIVE(%CHAT)
d$ = JSON.PARSE(...)


## 3. Coerenza interna

## 4. Checklist per chi implementa un nuovo builtin

## 5. Sanzioni implicite

## 6. `NAMESPACE` e `IMPORT` — modularità

### 6.1 `NAMESPACE` — definizione di un dominio

NAMESPACE <Nome>


### 6.2 `IMPORT` — direttiva di modulo, non comando
| Forma | Sintassi | Significato |
|---|---|---|
| Import completo | `IMPORT GRAPHICS` | importa tutti i simboli PUBLIC |
| Import con alias | `IMPORT GRAPHICS AS SPRITES` | importa tutto sotto l'alias `SPRITES` |
| Import selettivo | `IMPORT GRAPHICS (x, y, z)` | importa solo `x`, `y`, `z` |
| Import selettivo con alias | `IMPORT GRAPHICS (x AS col, y AS row)` | importa `x` come `col`, `y` come `row` |
| Import selettivo + alias namespace | `IMPORT GRAPHICS (x, y) AS G` | importa `x`,`y` sotto l'alias `G` |
| Wildcard | `IMPORT GRAPHICS.*` | equivalente all'import completo |
| Legacy alias | `IMPORT GRAPHICS WITH ALIAS` | equivalente a `AS GRAPHICS` |

### 6.3 Namespace virtuale

IMPORT GRAPHICS AS SPRITES      ' namespace virtuale: SPRITES punta a GRAPHICS
IMPORT GRAPHICS (SPRITE, SYNC) AS SPRITES   ' variante selettiva (solo gli sprite)


### ImplicitLineNumbers

*Fonte: `ImplicitLineNumbers`*


## Problema

### Sample affetti

## Soluzione

### 1. Chiavi sintetiche nel Parser (`GMBasicSharp/common/Parser/Parser.Statements.cs`)

const int SyntheticLineBase = 1_000_000;
int syntheticLineCounter = 0;
// ...
int synthKey = SyntheticLineBase + syntheticLineCounter++;
statementsByLine[synthKey] = lineStmts;


### 2. ExecutionOrder (`GMBasicSharp/common/Ast/BasicModule.cs`)

module.ExecutionOrder = [..executionOrder];


### 3. Executor: HandleEndOfCodeStream (`Executor.ExecutionSlice.cs`) — **storico (D1)**

var execOrder = frame.OwningModule.ExecutionOrder;
int currentIdx = execOrder.IndexOf(processor.CurrentLineNumber);
int nextIdx = currentIdx >= 0 ? currentIdx + 1 : 0;
if (nextIdx < execOrder.Length) {
    processor.CurrentLineNumber = execOrder[nextIdx];
    processor.InstructionPointer = 0;
    ExecuteFrame(frame);
}


### 4. Entry Point (`Executor.Startup.cs`) — **storico (D1)**

### 5. Linker: tutti i punti `new BasicModule(...)` preservano `ExecutionOrder`

## Impatto

## Stato

### Line-Number-Spec

*Fonte: `Line-Number-Spec`*


## Stato: Completo (2026-07-13)

## Riferimenti

## 1. Panoramica

## 2. Regole

### Regola 1 (Entry Point): Priorità di `SUB Main`

### Regola 2 (Scoping): I numeri di riga sono globali

### Regola 3 (Cross-module): Namespace e line numbers sono ortogonali

### Regola 4 (END NAMESPACE): non esiste

## 3. Pattern di utilizzo

### 3a. Programma a soli numeri di riga (Legacy)

10 CLS
20 PRINT "Hello World"
30 FOR I = 1 TO 5
40   PRINT I
50 NEXT I
60 END


### 3b. Programma moderno (SUB Main)

SUB Main()
    CLS
    PRINT "Hello"
END SUB


### 3c. Programma ibrido

SUB Main()
    CLS
    GOSUB 100
END SUB

100 REM subroutine legacy
110 PRINT "chiamata da GOSUB"
120 RETURN


## 4. Dettagli implementativi (runtime C# legacy — D1)

### 4a. Parser — controllo line number dentro NAMESPACE

if (_currentNamespace != null)
    throw CreateError(Peek(),
        $"Line numbers are not allowed inside NAMESPACE '{_currentNamespace}'.",
        ErrorCode.SyntaxError);


### 4b. Executor — _hasExplicitMain

private bool _hasExplicitMain;


_hasExplicitMain = mainProc != null;


bool shouldBlockLineAdvancement = _hasExplicitMain && processor.CallStackDepth <= 1;
if (!shouldBlockLineAdvancement)
{
    // Avanza alla prossima linea numerata
}


### 4c. Known issues (preesistenti, ora visibili)
| Bug | Campioni colpiti | Causa |
|-----|-----------------|-------|
| `NEXT senza FOR` | Tutti i programmi con FOR loop a livello globale | Il linker non trasforma FOR/NEXT in EnterLoopStmt/ForCheckAndJumpStmt per codice line-numbered |
| `Symbol not found` | Programmi con variabili a scope globale | Scoping runtime non allineato tra frame Procedure e scope globale |
| `Call stack empty` | Programmi con WITH/GOTO/strutture non-loop | Strutture di controllo non procedure-bound non gestite dal global-frame runner |
| `FOR senza NEXT` | FOR annidati o NEXT multiplo | Stessa causa del NEXT senza FOR |

## Stato implementazione (2026-07-13)
| Modifica | File | Stato |
|----------|------|-------|
| Parser error per line numbers in NAMESPACE | `Parser.Statements.cs` | ✅ Completo |
| `_hasExplicitMain` flag | `Executor.cs` + `Executor.Startup.cs` | ✅ Completo |
| HandleEndOfCodeStream fix | `Executor.ExecutionSlice.cs` | ✅ Completo |
| PerformStackUnwinding preserva outer loop | `Gpm.Jumps.cs` | ✅ Completo |
| EXIT FOR nested loop — corretto | `Gpm.Jumps.cs` (ExitStmt dispatcher) | ✅ Completo |

## 5. Applicabilità nel repo unificato

## 6. Cronologia fix (runtime C# legacy)

### 2026-07-14 — Types 26/26 completato

### 2026-07-13 — Collections 8/8 completato
| Fix | File |
|-----|------|
| Dict literal quotes (parser) | `Parser.Expressions.Literals.cs` |
| ListWrapper/TreeWrapper parentheses | `Gpm.Assigns.cs` |
| Tree/AVL/RedBlack bracket access | `Gpm.Expressions.cs`, `Gpm.Expressions.Arrays.cs` |
| LBOUND/UBOUND collections | `Gpm.Arrays.Functions.Sync.cs` |
| ViewPrint/Palette dispatcher | `Executor.ExecutionSlice.cs` |
| List void method return | `Gpm.Expressions.Collections.cs` |

### 2026-07-13 — Loop runtime completato
| Fix | File | Commit |
|-----|------|--------|
| HandleEndOfCodeStream (avanzamento condizionale) | `Executor.ExecutionSlice.cs` | `1dadd6a` |
| `_hasExplicitMain` flag | `Executor.cs`, `Executor.Startup.cs` | `1dadd6a` |
| PerformStackUnwinding preserva outer loop | `Gpm.Jumps.cs` | `1dadd6a` |
| Parser error line numbers in NAMESPACE | `Parser.Statements.cs` | `1dadd6a` |

### Polimorfismo-Guida-Utente

*Fonte: `Polimorfismo-Guida-Utente`*


## 1. Cosa significa "polimorfismo" in GMBasic
| Asse | Domanda | Risoluzione |
|---|---|---|
| **A. Polimorfismo semantico** (a runtime) | *Dove* agisce il comando? | dipende dallo **stato corrente** (finestra attiva / `SCREEN` / console) |
| **B. Polimorfismo di namespace** (statico) | *In quale dominio* è definito il comando? | dipende dagli `IMPORT` attivi e dal nome del verbo |

## 2. Asse A — Polimorfismo semantico (bersaglio a runtime)
| Comando | Bersaglio grafico | Bersaglio console |
|---|---|---|
| `CLS` | cancella la pagina/finestra grafica | pulisce la console |
| `COLOR` | imposta i colori della pagina grafica | imposta fg/bg della console |
| `LOCATE` | posiziona il cursore grafico | posiziona il cursore console |
| `PRINT` | disegna testo sulla pagina grafica | scrive sulla console |
| `PALETTE` | imposta un colore della palette grafica | imposta un colore della palette console |
| `SAVESCREEN` / `RESTORESCREEN` | snapshot della pagina grafica | snapshot della console |

### 2.1 Regola di scelta del bersaglio (priorità)

### 2.2 Varianti di **forzatura console** (prefisso `C`)
| Polimorfico | Variante forzata console |
|---|---|
| `CLS` | `CCLS` |
| `LOCATE` | `CLOCATE` |
| `PRINT` | `CPRINT` |
| `PALETTE` | `CPALETTE` |
| `SAVESCREEN` | `CSAVESCREEN` |
| `RESTORESCREEN` | `CRESTORESCREEN` |

SCREEN 1            ' attiva la pagina grafica
CLS                 ' pulisce la PAGINA GRAFICA (polimorfico)
PRINT "in grafica"  ' disegna sulla PAGINA GRAFICA
CCLS                ' pulisce SOLO la CONSOLE (forzato)
CPRINT "x"          ' scrive SOLO sulla CONSOLE (forzato)


## 3. Asse B — Polimorfismo di namespace (statico)
| Comando | Namespace |
|---|---|
| `CLS`, `COLOR`, `LOCATE`, `PRINT` | `CONSOLE` e `GRAPHICS` |
| `INPUT`, `LINE INPUT`, `GET` | `IO` e `CONSOLE` |

## 4. Relazione con `SCREEN` e `TEXTMODE` (spesso confusi)

TEXTMODE %C64     ' console in modalita' C64 (font/palette/PETSCII)
PRINT "hello"     ' → console (nessuna grafica attiva)
SCREEN 1          ' ora la grafica e' attiva
PRINT "ciao"      ' → PAGINA GRAFICA (polimorfismo)
CCLS              ' → CONSOLE (forzato)
SCREEN 0          ' chiude la grafica; si torna alla console
TEXTMODE %SDL     ' riporta la console alla modalita' SDL


## 5. Per chi implementa un nuovo comando polimorfico

## 6. Errori comuni
| Sintomo | Causa / Soluzione |
|---|---|
| `CONSOLE.CLS` / `GRAPHICS.CLS` "Token inatteso" | I comandi **non** hanno forma qualificata: usa la forma nuda. |
| `PRINT` non scrive sulla console con la grafica aperta | Comportamento polimorfico: con `SCREEN > 0` va alla grafica. Usa `CPRINT` per forzare la console. |
| `CCLS`/`CPRINT` non disponibili | Sono attive di default (namespace `GRAPHICS` attivo di default, come `SCREEN`); se il namespace fosse disattivato, serve `IMPORT GRAPHICS`. |

<!-- END SURFACE -->
