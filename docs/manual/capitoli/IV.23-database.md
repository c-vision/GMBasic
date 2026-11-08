# IV.23 — Database

## Cosa ottieni da questo capitolo

La superficie **completa** dei motori dati di GMBasic: DBF, ISAM, SPEED, RDD, SQL, DuckDB,
MongoDB, LEDGER. Per ognuno: come si apre, quali comandi esistono, quali funzioni si interrogano,
dove sono le trappole. Il capitolo è lungo perché il materiale è lungo: fingere che cinque righe
bastino è il modo di far perdere una settimana a chi legge.

## 1. La mappa: cinque motori, un solo modello

| Motore | Cos'è | File | Indice | Comando tipico |
|---|---|---|---|---|
| **DBF** | tabelle xBase, compatibili Clipper | `.dbf` + `.gms` (schema) | `.ntx` | `DBF USE "x.dbf" ALIAS A` |
| **ISAM** | record-oriented con B+Tree | `.isdb` | interno (primario + secondari) | `ISAM OPEN "x.isdb" OF Tipo ALIAS X INDEXED BY ID` |
| **SPEED** | NoSQL documentale con SQL-like | directory/container | `INDEXED BY` + indici secondari | `SPEED USE "Clienti" FROM Db OF Cliente ALIAS C INDEXED BY ID` |
| **RDD** | strato che astrae gli altri | dipende dal driver | dipende | `RDD USE "x.dbf" VIA "DBF" ALIAS A` |
| **SQL** | database esterno | server/file | del motore | `SQL OPEN …` / `SQL QUERY …` |

Fuori da questi cinque ci sono i satelliti: **DuckDB** (analytics su file, via `SQL`),
**MongoDB** (documenti), **LEDGER** (partita doppia in sola aggiunta).

Il modello comune è quello di sempre — **area** (alias), **cursore**, **record corrente**,
**indice attivo** — e vale su tutti e quattro i backend (IL, PCodeVM, VM/C, C99Native) tramite il
runtime C condiviso. Imparato su DBF, si riusa su ISAM e SPEED.

## 2. DBF — creazione e mappatura dei tipi

```basic
TYPE Cliente
    ID AS LONG
    Nome AS STRING * 40
    Saldo AS CURRENCY
    Nato AS DATE
    Attivo AS BIT
END TYPE

DBF CREATE "clienti.dbf" FROM TYPE Cliente
```

La mappatura è fissa e va conosciuta, perché determina cosa si legge e cosa si confronta:

| Campo BASIC | Tipo DBF | Lunghezza |
|---|---|---|
| `STRING * n` | `C` | `n` |
| `LONG`, `INTEGER` | `N` | — |
| `DOUBLE`, `SINGLE` | `N` | — |
| `CURRENCY`, `MONEY` | **`Y`** | 16 |
| `BIT`, `BOOL` | `L` | 1 |
| `DATE` | `D` | 8 |

I nomi campo sono **troncati a 10 caratteri** dal formato DBF. Il linguaggio è tollerante:
`Descrizione` trova il campo `DESCRIZION`. Non è un permesso a ignorare il limite: due campi che
condividono i primi 10 caratteri sono un errore che scopri tardi.

## 3. DBF — dati

```basic
DBF USE "clienti.dbf" ALIAS C

DBF APPEND BLANK                                   ' record vuoto
DBF APPEND RECORD NuovoCliente                     ' da un UDT
DBF REPLACE Nome WITH "Mario", Saldo WITH 100.50   ' multi-campo, record corrente
DBF GET RECORD Letto                               ' record corrente nell'UDT
DBF GET Letto                                      ' forma late-binding equivalente
```

| Comando | Parametri | Note |
|---|---|---|
| `DBF APPEND BLANK` | — | aggiunge un record vuoto e ci si posiziona |
| `DBF APPEND RECORD <udt>` | UDT dello schema | valori tipizzati, nessuna conversione manuale |
| `DBF REPLACE <campo> WITH <v>[, …]` | coppie, anche via macro | sul **record corrente** |
| `DBF GET RECORD <var>` / `DBF GET <var>` | variabile UDT | lega la variabile al record corrente |

**Campi nudi (late-binding Clipper).** Un campo scritto nudo in un'espressione viene risolto a
runtime dallo schema e **tipizzato come il campo**:

| Tipo DBF | Valore | `VALTYPE` | `TYPENAME` |
|---|---|---|---|
| `N`, `F`, `Y` (currency) | CURRENCY (numerico) | `"N"` | `"CURRENCY"` |
| `L` | BOOL | `"L"` | `"BIT"` |
| `C`, `M`, … | stringa | `"C"` | `"STRING"` |

Conseguenze verificate sui quattro backend: `PRINT Amount` → `" 100.00"` (spazio-segno e
decimali), `STR(Amount)` → `" 100"` (numero, **senza** i decimali del currency). Per i decimali
serve `CSTR` o `PRINT` diretto. Un campo `Y` letto come stringa è un difetto storico dei backend
C, oggi chiuso: se lo vedi, sei su una build vecchia.

I campi **`BIT`** valgono `-1` (vero) / `0` (falso) in contesto numerico; `REPLACE campo WITH
TRUE`/`FALSE`/`-1`/`0` e `APPEND RECORD` accettano entrambe le forme.

## 4. DBF — stato, ricerca, filtri

```basic
DBF.FOUND()      ' ultimo SEEK/LOCATE ha trovato
DBF.EOF()        ' oltre l'ultimo record
DBF.BOF()        ' prima del primo
DBF.DELETED()    ' record corrente marcato cancellato
DBF.USED()       ' esiste un'area DBF attiva
DBF.SELECTED()   ' numero d'area 1-based corrente (0 = nessuna)
DBF.SELECTED("C")' numero d'area dell'alias, 0 se chiusa
DBF.SELECTED(2)  ' 2 se l'area 2 e' aperta, altrimenti 0
DBF.ALIAS()      ' alias dell'area corrente
DBF.RECNO()      ' numero del record corrente
DBF.LASTREC()    ' numero di record
DBF.RECSIZE()    ' dimensione del record in byte
```

```basic
DBF SET ORDER TO 1                 ' indice 1-based
DBF SET ORDER TO "idx_nome"        ' anche senza estensione
DBF SET ORDER TO 0                 ' nessun ordine = scansione fisica
DBF SEEK "Mario"                   ' controllante
DBF SEEK "Mario" SOFT              ' posizione "vicina", FOUND resta FALSE
DBF LOCATE FOR Nome = "Mario"
DBF SET FILTER TO Citta = "Roma"
DBF SET FILTER TO FN(rec) => rec.Attivo = TRUE
DBF SET FILTER TO ""               ' rimuove
```

`SEEK` è **controllante**: se la chiave non c'è, il cursore va a fine area e `FOUND()` è falso.
`SOFT` lascia il cursore vicino ma `FOUND()` resta falso. `SET ORDER TO 0` disattiva l'ordine e
`FOUND()` torna coerente con la scansione fisica.

`SET DELETED OFF` è il **default** (i cancellati restano visibili), come in Clipper; `ON` li
nasconde a `SKIP`/`GO TOP`/`EOF`.

## 5. DBF — indici su espressione

```basic
DBF INDEX ON UPPER(Citta) + STR(ID, 5) TO "idx_citta_id"
DBF INDEX ON Saldo TO "idx_saldo"      ' chiave numerica
```

`INDEX ON` accetta un'**espressione**, non solo un campo: `STR`, `UPPER`/`LOWER`, `TRIM`,
concatenazioni, e il suffisso `#<id>` per forzare l'ordine numerico invece del lessicografico.
Senza `#`, `"10"` viene prima di `"9"`.

## 6. DBF — operazioni file-level con `FOR`/`WHILE`

Questa è la parte che distingue DBF da un array: le operazioni agiscono su **tutto il file**, con
una condizione valutata per record.

| Comando | Cosa fa | Su cosa filtra |
|---|---|---|
| `DBF COPY TO <file> [FOR\|WHILE <cond>]` | copia struttura e record | area corrente |
| `DBF APPEND FROM <file> [FOR\|WHILE <cond>]` | aggiunge record | file **sorgente** |
| `DBF SORT TO <file> ON <campi> [/D] [FOR\|WHILE]` | nuovo DBF ordinato | area corrente |
| `DBF TOTAL ON <chiave> TO <file> FIELDS <campi> [FOR\|WHILE]` | raggruppa e somma | area corrente |
| `DBF LIST [FIELDS …] [FOR\|WHILE] [TO PRINT\|TO FILE <f>]` | elenca | area corrente |
| `DBF UPDATE ON <chiave> FROM <alias> REPLACE …` | join-update | area corrente |
| `DBF DELETE [FOR\|WHILE]` / `DBF RECALL [FOR\|WHILE]` | marca / ripristina | area corrente |
| `DBF JOIN WITH <alias> TO <file> FOR FN(cli,mov) => …` | join annidato | due aree |

Semantica uniforme sui quattro backend: `FOR` si applica a ogni record che soddisfa; `WHILE` si
**ferma al primo falso**. Per `DELETE`/`RECALL` il ciclo parte da TOP.

```basic
DBF COPY TO "attivi.dbf" FOR Attivo = 1
DBF SORT TO "ordinati.dbf" ON Citta, Nome /D FOR Saldo > 0
DBF TOTAL ON Citta TO "totali.dbf" FIELDS Saldo FOR Attivo = 1
DBF LIST FIELDS Nome, Saldo FOR Saldo > 100 TO FILE "estratto.txt"
```

`TOTAL` **richiede un ordine attivo** sulla chiave: senza, raggruppa a caso.

### Join: due forme, due sintassi

```basic
' 1) join con lambda (nessuno spazio intorno al punto)
DBF JOIN WITH Movimenti TO "report.dbf" -
    FOR FN(cli, mov) => Clienti.ID = Movimenti.Rif -
    FIELDS Nome, Importo

' 2) join-update per chiave
DBF UPDATE ON ID FROM Movimenti -
    REPLACE Nome WITH Movimenti.Nome, Saldo WITH Saldo + Movimenti.Saldo
```

Nella lambda `FN(cli, mov)` i nomi `cli`/`mov` sono **marker**: dentro si leggono i campi per
nome dal record corrente, e i riferimenti qualificati vanno scritti **senza spazi** (`Clienti.ID`).
In `UPDATE ON`, i nomi nudi sono della **destinazione**, i qualificati (`Movimenti.Nome`) del
**sorgente**; i record senza corrispondenza non vengono toccati.

## 7. DBF — manutenzione e trasporto

`DBF DELETE` · `DBF RECALL` · `DBF PACK` (rimuove fisicamente) · `DBF ZAP` (svuota) ·
`DBF REINDEX` · `DBF COPY STRUCTURE` · `DBF EXPORT` / `DBF IMPORT` (scambio) ·
`DBF VALIDATE` (integrità) · `DBF CLOSE` (area corrente) · `DBF CLOSE <alias|area>` (solo
quella indicata) · `DBF CLOSE DATABASES` (tutte).

Un record cancellato **non** libera spazio finché non fai `PACK`: è il motivo per cui un DBF
"cancellato" resta della stessa dimensione.

## 8. ISAM — apertura, schema, indici

```basic
TYPE Cliente
    ID AS LONG
    Nome AS STRING * 40
    Citta AS STRING * 30
    Saldo AS CURRENCY
END TYPE

ISAM OPEN "clienti.isdb" OF Cliente ALIAS C INDEXED BY ID, Citta, Nome
```

| Parametro | Significato |
|---|---|
| `<path>` | file `.isdb`, creato se manca |
| `OF <Tipo>` | schema UDT (nessun valore: è un tipo) |
| `ALIAS <alias>` | nome dell'area |
| `INDEXED BY c1[, c2, …]` | **tutti** i campi chiave: il primo è la primaria, gli altri secondari |

`ISAM USE` è sinonimo di `ISAM OPEN`. L'ordine dei campi nello schema è quello del `TYPE`.

```basic
ISAM SELECT C            ' commuta area
ISAM SELECT -1           ' area corrente (no-op)
ISAM CLOSE C
```

Indici a runtime:

```basic
ISAM CREATE INDEX "by_nome" ON C KEY (Nome) LEN 40
ISAM DROP INDEX "by_nome" ON C
ISAM SET ORDER TO "by_nome" ON C
ISAM SET ORDER TO "by_nome" ON -1        ' -1 = area corrente
```

`CREATE INDEX` accetta anche una **CURRENCY** o un'espressione fra virgolette.

## 9. ISAM — dati, navigazione, ricerca

```basic
ISAM APPEND C, NuovoCliente        ' con alias esplicito
ISAM APPEND NuovoCliente           ' area corrente
ISAM PUT C, Modificato             ' aggiorna il record corrente
ISAM GET C, Letto                  ' legge il record corrente
ISAM GET -1, Letto                 ' area corrente
```

```basic
ISAM GO C, TOP
ISAM GO C, BOTTOM
ISAM GO C, 5                       ' quinto record
ISAM SKIP C, 1                     ' avanti di uno
ISAM SKIP C, -1                    ' indietro di uno
ISAM SEEK C, 3                     ' chiave numerica
ISAM SEEK -1, "Laura Neri" USING "Nome"
```

`USING` sceglie l'indice secondario; senza, vale l'indice attivo (`SET ORDER`, default primario).
L'esito si legge con `ISAM.FOUND(alias)`.

## 10. ISAM — filtri, relazioni, lock

```basic
ISAM SET FILTER TO Citta = "Roma" AND Saldo > 100 ON C
ISAM SET FILTER TO FN(rec) => rec.Attivo = TRUE ON -1
ISAM SET FILTER TO "" ON C                      ' rimuove
```

La condizione è **testo ri-parsato per record** dal motore, fonte unica per i quattro backend.
La forma lambda evita di citare i nomi campo in una stringa.

```basic
ISAM SET RELATION ON Ordini TO ID_Cliente INTO Righe
ISAM SET RELATION ON -1                          ' rimuove
```

La relazione riposiziona il **figlio** quando si muove il **padre**. Il campo del figlio è
risolto per euristica: nome chiave, `ID_<padre>`, singolarizzazione (`ID_Cliente`/`ID_Client`).
Se l'euristica sbaglia, la relazione "non funziona" senza errore — è la prima cosa da verificare.

```basic
ISAM LOCK C, RECORD      ' bloccante
ISAM UNLOCK C, RECORD
ok = ISAM.RLOCK(C)       ' non bloccante: TRUE se acquisito
ok = ISAM.FLOCK(C)       ' lock di file
```

## 11. ISAM — le nove funzioni di area

| Funzione | Ritorno | Significato |
|---|---|---|
| `ISAM.RECNO(alias)` | LONG | numero del record fisico corrente |
| `ISAM.EOF(alias)` | BIT | `TRUE` oltre l'ultimo |
| `ISAM.BOF(alias)` | BIT | `TRUE` prima del primo |
| `ISAM.FOUND(alias)` | BIT | ultimo `SEEK` riuscito |
| `ISAM.LASTREC(alias)` | LONG | numero di record |
| `ISAM.RECSIZE(alias)` | LONG | dimensione del record in byte |
| `ISAM.SELECTED(alias)` | LONG | numero d'area (1-based), 0 se chiusa |
| `ISAM.INFO(alias)` | STRING (JSON) | header e schema |
| `ISAM.INTEGRITY(alias)` | STRING (JSON) | validazione del file |

Tutte accettano l'alias o `-1`/nessun argomento (area attiva).

## 12. ISAM — manutenzione, copia, conversione

`ISAM DELETE <alias>` · `ISAM RECALL <alias>` · `ISAM PACK <alias>` · `ISAM REINDEX <alias>` ·
`ISAM COPY TO <file> ON <alias> [FOR <cond>]` · `ISAM SORT TO <file>` · `ISAM EXPORT` /
`ISAM IMPORT` · `ISAM VALIDATE` · `ISAM BACKUP`.

```basic
ISAM CONVERT DBF "clienti.dbf" TO ISAM "clienti.isdb" TYPE Cliente KEY "ID" LEN 4
```

La conversione è il ponte fra i due mondi: porta struttura e dati, e ti dà un file con chiave
primaria dichiarata.

## 13. SPEED — il motore documentale

SPEED è un database **NoSQL documentale** con tabella tipizzata e query SQL-like. Prima si apre il
database fisico, poi la tabella:

```basic
IMPORT SPEED

SPEED OPEN "MioDatabase" ALIAS MioDb
SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Clienti INDEXED BY ID
```

| Comando | Parametri |
|---|---|
| `SPEED OPEN <dir> [ALIAS <a>]` | database fisico (directory o container) |
| `SPEED USE <tab> FROM <db> OF <Tipo> [ALIAS a] [INDEXED BY campo]` | tabella tipizzata |
| `SPEED CLOSE <alias>` / `SPEED CLOSE DATABASES` | chiude area / tutto |

Doppia sintassi ammessa: `SPEED PUT Clienti, c` (legacy) e `SPEED.PUT(Clienti, c)` (moderna).

### Scrittura: `PUT` è un upsert

```basic
DIM c AS Cliente
c.ID = 1 : c.Nome = "Anna" : c.Eta = 28
SPEED PUT Clienti, c        ' inserisce o aggiorna per la chiave INDEXED BY
```

`PUT` **non distingue** insert da update: la chiave decide. È comodo, ed è la ragione per cui una
chiave sbagliata sovrascrive un record esistente senza avvisare.

Inserimento massivo:

```basic
DIM batch AS LIST OF Riga
batch = NEW LIST OF Riga {}
DIM r AS Riga
r.ID = 1 : r.Nome = "Anna" : batch.Add(r)
r.ID = 2 : r.Nome = "Bruno" : batch.Add(r)
n = SPEED.SETMANY(Tabella, batch)      ' → 2, una sola chiamata
```

### Lettura e navigazione

```basic
DIM r AS Cliente
SPEED GET Clienti, 1 INTO r        ' per chiave
SPEED GET r                        ' record corrente
PRINT r.Nome

SPEED GO Clienti, TOP
DO WHILE NOT SPEED.EOF(Clienti)
    SPEED GET Clienti, r
    PRINT r.Nome
    SPEED SKIP Clienti
LOOP
```

Funzioni: `SPEED.EOF`, `BOF`, `FOUND`, `RECNO`, `LASTREC`, `SELECTED`, `DBFILTER` (filtro attivo in
sola lettura).

### Filtri: sempre la forma con stringa

```basic
SPEED SET FILTER TO "ID > 1" ON T
SPEED GO T, TOP
SPEED SET FILTER TO "" ON T
```

La forma senza virgolette (`SET FILTER TO Name > 'A'`) viene letta come **commento** dal lexer:
`'A'` apre un commento fuori dal contesto di espressione. È una trappola del linguaggio, non del
motore.

### Query

```basic
SPEED SELECT Nome, Citta FROM Clienti WHERE Eta > 25 INTO res$()

SPEED SELECT * FROM Clienti INTO res() OF Cliente

SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto -
    INTO ris() OF RisultatoJoin -
    FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente -
    GROUP BY Clienti.Nome

SPEED SELECT Regione, SUM(Importo) AS Totale -
    INTO ris() OF Riga FROM Vendite -
    GROUP BY Regione HAVING SUM(Importo) > 1000 ORDER BY Totale DESC
```

| Clausola | Note |
|---|---|
| proiezione | `campo`, `Alias.campo`, `expr AS alias`, `*` |
| aggregati | `COUNT`, `SUM`, `AVG`, `MIN`, `MAX` |
| `INTO … OF Tipo` | copia **posizionale** nei campi dell'UDT |
| `JOIN` | richiede un **indice secondario** sul campo di join della tabella destra |
| `GROUP BY` / `HAVING` / `ORDER BY` | come in SQL |

Il `JOIN` senza indice funziona ma degrada a nested-loop, e i backend possono divergere: crea
l'indice **sempre**.

### Indici, viste, relazioni

```basic
SPEED CREATE INDEX "CittaIdx" ON Clienti KEY (Citta)
SPEED DROP INDEX "CittaIdx" ON Clienti

SPEED SET RELATION ON Clienti TO ID INTO Ordini
SPEED SET RELATION ON Clienti                       ' rimuove

SPEED CREATE VIEW ClientiAttivi OF Cliente AS SELECT * INTO ris() OF Cliente FROM Clienti WHERE Clienti.Attivo = TRUE
SPEED REFRESH VIEW ClientiAttivi
SPEED DROP VIEW ClientiAttivi
```

`KEY (campo)` richiede le **parentesi**: la forma `CREATE INDEX ON field TO file$` non è valida
(era documentata per errore).

### Transazioni, replica, utilità

| Funzione/comando | Effetto |
|---|---|
| `SPEED.BEGIN` / `COMMIT` / `ROLLBACK` | transazione |
| `SPEED.INFO(alias)` | JSON con indici e conteggio |
| `SPEED.REPAIR(alias)` | ripara il file |
| `SPEED.SETREPLICATION(alias, path$)` | log shipping |
| `SPEED.APPLYREPLICATION(log$)` | applica il log |

## 14. RDD — un solo codice per cinque motori

RDD astrae DBF, ISAM, SPEED, SQL, MEMORY (e ARRAY, TEXT, CSV, JSON) dietro comandi identici.

```basic
RDD SET DEFAULT TO "DBF"
RDD USE "clienti.dbf" ALIAS C
RDD USE "clienti.isdb" OF Cliente VIA "ISAM" ALIAS C2
RDD SELECT 1
RDD CLOSE 1
```

| Funzione | Ritorno | Note |
|---|---|---|
| `RDD.GETFIELD(area, col)` | valore | `col` = ordinale **1-based** o nome; legge dal buffer se attivo |
| `RDD.VALTYPE(area)` | STRING | nome del driver (`"DBF"`, `"ARRAY"`, `"SQL"`, …) |
| `RDD.RECNO`/`LASTREC`/`EOF`/`BOF`/`FOUND` | — | come DBF/ISAM |
| `RDD.OLDVAL(area, campo)` | valore/null | valore prima delle modifiche |
| `RDD.GETFLDSTATE(area, campo)` | `0`/`1`/`2` | immutato / modificato / cancellato |
| `RDD.BUFFER(area, mode)` | `-1`/`0` | attiva (2 pessimistico, 3 ottimistico) o disattiva |
| `RDD.TABLEUPDATE(area)` | `-1`/`0` | applica con **una** scrittura e rilascia il lock |
| `RDD.TABLEREVERT(area)` | `-1`/`0` | scarta il buffer (il motore non è mai stato toccato) |
| `RDD.SETDELETED(area, flag)` | — | nasconde/mostra i cancellati |
| `RDD.FLOAT(expr)` | DOUBLE | forza il tipo |

### Il row-buffer, in pratica

```basic
RDD USE "anagrafe.dbf" ALIAS A
ok = RDD.BUFFER(1, 2)              ' pessimistico
RDD SET FIELD 1, 3, 99000          ' deviato nel buffer, non sul disco
v = RDD.GETFIELD(1, 3)             ' 99000 (dal buffer)
old = RDD.OLDVAL(1, "STIPENDIO")   ' valore originale
st = RDD.GETFLDSTATE(1, 3)         ' 1 = modificato
IF RDD.TABLEUPDATE(1) < 0 THEN PRINT "applicato"
```

Il buffer è **dell'area, non del thread**: `BUFFER(area, 0/1)` con modifiche pendenti le scarta.
`TABLEUPDATE` scrive il record **una volta sola** — è il motivo per cui esiste.

### Lock, durabilità, replica

```basic
IF RDD.RLOCK(1) THEN
    RDD REPLACE 1, "Nome" WITH "Mario"
    RDD UNLOCK 1
END IF

RDD.SETDURABILITY(1, "full")       ' -1 = successo
RDD.FLUSHDURABILITY(1)             ' forza il flush
RDD.BEGINGROUPCOMMIT(1)            ' molte scritture, un fsync
RDD.ENDGROUPCOMMIT(1)
RDD.SETREPLICATION(1, "repl.log")  ' ritorna il path risolto
RDD.APPLYREPLICATION("repl.log")
```

### Scope uniforme

`ALL`, `FOR cond`, `WHILE cond`, `NEXT n`, `RECORD n`, `REST`, `CURRENT`. Valgono per
`RDD.DELETE`, `RDD.RECALL`, `RDD.REPLACE`, che ritornano il **conteggio** dei record toccati.

```basic
RDD DELETE FOR Saldo = 0
RDD RECALL ALL
RDD REPLACE "Prezzo" WITH "100" FOR Categoria = "A"
```

Altri comandi: `RDD COPY TO <file> [VIA driver]`, `RDD SORT TO <file> FIELDS …`,
`RDD TOTAL ON <key>`, `RDD CREATE INDEX area, name$, key$`, `RDD SET ORDER TO area, n`,
`RDD SET REPROCESS TO n`, `RDD SET LOCK TIMEOUT TO n`, `RDD ON EVENT`/`OFF EVENT`,
`RDD.LOOKUP(retField$, searchVal$, searchField$)`.

## 15. SQL esterno

```basic
SQL OPEN "…" AS db
SQL QUERY db "SELECT id, name, price FROM prodotti" INTO rs
DO WHILE SQL.FETCH(rs)
    PRINT "id="; SQL.COL(rs, "id"); " name="; SQL.COL(rs, "name")
LOOP
SQL CLOSE rs
SQL CLOSE db
```

Eseguito (`shared/samples/SQL/SqlExec.bas`, backend C99Native):

```
=== SQL EXEC ===
id= 1  name=prodotto1 price= 19.99
id= 2  name=prodotto2 price= 29.99
=== SQL EXEC OK ===
```

| Comando/funzione | Uso |
|---|---|
| `SQL OPEN … AS db` | apre la connessione |
| `SQL QUERY db "<sql>" INTO rs` | esegue una SELECT |
| `SQL EXEC db "<sql>"` | esegue uno statement (INSERT/UPDATE/DDL) |
| `SQL.FETCH(rs)` | avanza di una riga |
| `SQL.COL(rs, n\|"nome")` | legge una colonna |
| `SQL.CHANGES(db)` | righe toccate dall'ultimo statement |
| `SQL.LASTINSERTROWID(db)` | id generato |
| `SQL.CLOSE` | chiude resultset e connessione |
| `SQL.FROMCSV(path, tabella, db)` / `SQL.TOCSV(rs, path)` | scambio CSV |
| `SQL.DFSCHEMA(rs)` | schema del resultset, per il DataFrame |

`SQLRESULTSET` è il tipo del cursore; `Sql/SqlDataFrameG10.bas` mostra il passaggio da resultset a
struttura tabellare per l'analisi.

## 16. DuckDB, MongoDB, LEDGER

| Motore | Apertura | Nota |
|---|---|---|
| **DuckDB** | via `SQL`, file `.duckdb` | colonnare: analytics su file, `GROUP BY` veloce |
| **MongoDB** | `MONGO OPEN …` | documenti, non tabelle: la chiave è `_id` |
| **LEDGER** | `LEDGER OPEN …` | partita doppia, **sola aggiunta**: non si modifica una scrittura |

LEDGER merita una riga in più, perché il modello è diverso da tutti gli altri:

```basic
LEDGER OPEN "contabilita.ldb" ALIAS L
LEDGER ADD L, "2026-11-02", "Cassa", "Ricavi", 1000
b = LEDGER.BALANCE(L, "Cassa")
v = LEDGER.VARIANCE(L)
```

Funzioni: `LEDGER.BALANCE`, `LEDGER.BUDGET`, `LEDGER.VARIANCE` (scostamento dal budget),
`LEDGER.COUNT`, `LEDGER.PLOT` (serie per grafico), `LEDGER.VALIDATE` (quadratura),
`LEDGER.REGISTER`, `LEDGER.IMPORT` (da CSV), `LEDGER.CLOSE`.
Le scritture non si correggono: si aggiunge la **rettifica**, e resta la storia. È l'unico motore
in cui "correggere" è un errore concettuale.

## 17. Esecuzioni reali

**ISAM, ciclo completo** (`Databases/DbIsamNavAndModify.bas`):

```
Aggiunti 3 record.

--- 1. Scansione Sequenziale (SKIP +1) ---
  ID:001      Cognome:Rossi
  ID:002      Cognome:Verdi
  ID:003      Cognome:Bianchi

--- 2. Modifica Record (PUT) ---
  Nuova eta di Verdi:        26

--- 3. Scansione Inversa (SKIP -1) ---
  ID:003      Cognome:Bianchi
  ID:002      Cognome:Verdi
  ID:001      Cognome:Rossi

--- 4. Cancellazione e PACK ---
Numero record dopo PACK: 3
```

Le quattro cose che un motore deve saper fare, in un solo output: scorrere in avanti, modificare
il record corrente, scorrere all'indietro (cioè il cursore è davvero bidirezionale), e compattare.

**DBF, inserimento massivo e group-commit** (`DbDbfAppendMany.bas`, `DbDbfGroupCommit.bas`):

```
APPENDMANY inserted  2
APPENDFROM total  3
```
```
-1
-1
-1
LASTREC= 3
```

`-1` è "successo" (convenzione BASIC): `BEGINGROUPCOMMIT`, il `REPLACE` e `ENDGROUPCOMMIT`
ritornano tutti `-1`. `APPENDMANY` è l'inserimento in blocco, `APPENDFROM` quello da un altro DBF.

**SPEED/ISAM, aree multiple** (`DbIsamSpeedSelectFilter.bas`):

```
--- ISAM SELECT: due aree commutate ---
  Nord TOP: N1
  Sud TOP: S1
```

**Un difetto, registrato e non corretto** (`Databases/CustomerOrdersReport.bas`):

```
2. apertura aree
   clienti:  0
3. join clienti-ordini (DBF SELECT)
   C1:  0  ordini, totale  0
   C2:  0  ordini, totale  0
   C3:  0  ordini, totale  0
```

Il report trova **zero clienti** dopo aver generato tre clienti e cinque ordini, e il join produce
zero per tutti: il sample chiude con `OK` mentre i numeri sono vuoti. È un difetto aperto — sta
nel campione o nell'apertura delle aree, non nel libro. Non usare questo sample come modello
finché non è chiuso, e diffida di un `OK` che non controlla i numeri.

## 18. Scegliere il motore

| Se devi… | Usa |
|---|---|
| leggere dati Clipper esistenti | **DBF** |
| record singoli per chiave, migliaia di record | **ISAM** |
| report con join e aggregati, senza server | **SPEED** o **ISAM** + `SPEED SELECT` |
| cambiare motore senza riscrivere il codice | **RDD** |
| più processi, transazioni vere, tipi e `NULL` | **SQL** esterno |
| analisi su file colonnare | **DuckDB** |
| documenti con struttura variabile | **MongoDB** |
| registrazioni contabili immutabili | **LEDGER** |

Percorsi di conversione: `ISAM CONVERT DBF … TO ISAM …`, `DBF EXPORT`/`ISAM IMPORT`,
`SPEED`/`SQL` via CSV (`SQL.FROMCSV`).

## 19. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `DBF USE("x.dbf")` non compila | parentesi su un **comando** | `DBF USE "x.dbf" ALIAS A` |
| `SEEK` non trova una chiave esistente | nessun ordine attivo | `SET ORDER TO` / `INDEXED BY` |
| `TOTAL ON` raggruppa male | manca l'ordine sulla chiave | imposta l'indice prima |
| due campi "uguali" | nomi troncati a 10 caratteri | rinomina uno dei due |
| `"10"` prima di `"9"` | chiave testuale | `STR(n)` o suffisso `#id` |
| `CREATE INDEX ON campo TO file` | sintassi inesistente | `SPEED CREATE INDEX "n" ON T KEY (campo)` |
| filtro SPEED ignorato | condizione senza virgolette | `SET FILTER TO "ID > 1" ON T` |
| join SPEED lentissimo o divergente | manca l'indice a destra | `CREATE INDEX` sul campo di join |
| `PUT` sovrascrive un record | `PUT` è upsert | controlla la chiave prima |
| relazione ISAM che non muove il figlio | euristica sul nome del campo | dichiara il campo nel modo atteso |
| modifiche perse allo spegnimento | niente flush/commit | `FLUSHDURABILITY`, `COMMIT`, group-commit |
| file DBF grande dopo le cancellazioni | manca `PACK` | `DBF PACK` |
| due programmi si bloccano | lock non gestito | `RLOCK`/`FLOCK` con timeout |

## 20. Dove andare adesso

- **II.10** — compatibilità Clipper/xBase: il perché di questa sintassi.
- **IV.22** — il livello sotto: file, record, CSV.
- **IV.24** — il database raggiunto via rete.
- **V.34** — gli stessi dati come vettori, per un modello.















## HDF5: dataset numerici multidimensionali

HDF5 non è un motore di database e non sostituisce DBF/ISAM/SPEED: è un **formato di scambio**
per array multidimensionali, con gruppi, dataset tipizzati e attributi. In GMBasic si usa come
archivio di risultati numerici (serie di barre, matrici di rendimenti, griglie di parametri) che
poi vengono riletti da BASIC o da altri strumenti.

Il modello è gerarchico: un file contiene **gruppi** (anche annidati) e **dataset**; ogni
oggetto può portare **attributi** scalari (stringa o numero). HDF5 non crea i gruppi intermedi:
per scrivere `barre/ohlc` il gruppo `barre` deve esistere, altrimenti la scrittura fallisce con
`-1`. I dataset si dichiarano con tipo (`F64` o `I64`) e un rango di 1–8 dimensioni; la
compressione è **deflate + shuffle** (`H5WRITEF64C`, livello 0–9).

Due dettagli contano quando si legge: le funzioni tipizzate (`H5READF64`, `H5READI64`)
rifiutano il tipo sbagliato, mentre `H5READDBL` converte **qualsiasi** dataset numerico in
double ed è la scelta giusta per dati di provenienza eterogenea; e `out()` va dimensionato
prima con `H5SIZE(file, nome)`, perché una lettura non può sapere quanto spazio ha il vettore
del chiamante. L'enumerazione (`H5COUNT`/`H5NAME`/`H5KIND`) usa `H5Ovisit3` con indice per
nome: l'ordine può cambiare fra build diverse di HDF5, quindi non è una base stabile per la
logica del programma.

Il pacchetto include **HDF5 2.2.0** (l'ultima versione) con le sue dipendenze (`libsz.2` e
`libaec.0`, per szip) e le rende relocabili (`@rpath` + `@loader_path`). Un dettaglio non ovvio: la dylib non si
chiama `libhdf5.dylib`. Il runtime GMBasic pubblica in `~/.gmbasic/libs` una libhdf5 1.14.6 con
install-name `@rpath/libhdf5.dylib` e la VM/C la precarica; poiché dyld deduplica le immagini per
install-name, con il nome standard la VM/C avrebbe riusato la 1.14.6 mentre gli altri backend
avrebbero caricato la 2.2.0 — `H5VERSION` divergente (verificato). Battezzando la copia
`@rpath/libhdf5.320.dylib` (il soname della serie 2.x) e sfruttando il binding a due livelli di
Mach-O, le chiamate dello shim sono legate alla nostra copia: la versione riportata è **2.2.0 su
IL, PCode, VM/C e C99 nativo**.

Il pacchetto disattiva i diagnostici testuali che HDF5 scriverebbe su console a ogni errore
(`H5Eset_auto2` sullo stack di default, eseguito al caricamento della dylib): l'esito di ogni
chiamata si legge dal solo codice di ritorno — `0` per l'esito positivo, un valore negativo per
gli errori, con la stessa semantica su tutti e quattro i backend. In questo modo un programma
che sonda l'esistenza di un dataset (`H5SIZE` / `H5READDBL` su un nome assente) non sporca
l'output, cosa che rende i test di parità confrontabili riga per riga.

Esempio completo: `packages/hdf5/1.0.0/samples/Hdf5Demo.bas` (barre OHLC, volumi interi,
rendimenti compressi, gruppo di metadati, attributi, elenco degli oggetti e rilettura tipizzata
e type-agnostica). MiniTest: `minitests/hdf5ffi/{h5_roundtrip,h5_errors}.bas`.







<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### HDF5-Guida-Utente

*Fonte: `HDF5-Guida-Utente`*


REQUIRE "hdf5"
IMPORT HDF5


## 1. Modello

## 2. Convenzioni FFI

## 3. Funzioni (19)
| Funzione | Parametri | Ritorno | Note |
|---|---|---|---|
| `H5CREATE` | path STRING | LONG | Crea (o tronca) un file HDF5. Ritorna l'handle, 0 su errore. |
| `H5OPEN` | path STRING | LONG | Apre un file HDF5 esistente in lettura/scrittura. Ritorna l'handle, 0 su errore. |
| `H5CLOSE` | h LONG | LONG | Chiude il file e libera l'handle. 0 = ok. |
| `H5WRITEF64` | h LONG, name STRING, rank LONG, dims() LONG, data() DOUBLE | LONG | dataset F64: rank 1..8, dims() = rank valori, data() = prodotto(dims) valori. |
| `H5WRITEI64` | h LONG, name STRING, rank LONG, dims() LONG, data() LONG | LONG | dataset I64 (interi a 64 bit). |
| `H5WRITEF64C` | h LONG, name STRING, rank LONG, dims() LONG, data() DOUBLE, compress LONG | LONG | dataset F64 con compressione deflate+shuffle (compress 0..9, 0 = nessuna). |
| `H5MAKEGROUP` | h LONG, path STRING | LONG | crea un gruppo (anche annidato: "serie/2026"). |
| `H5WRITEATTRSTR` | h LONG, obj STRING, attr STRING, val STRING | LONG | attributo stringa su un oggetto (dataset o gruppo). |
| `H5WRITEATTRF64` | h LONG, obj STRING, attr STRING, val DOUBLE | LONG | attributo numerico (double) su un oggetto. |
| `H5COUNT` | h LONG | LONG | numero di oggetti (dataset + gruppi) del file, in ordine di creazione. |
| `H5NAME` | h LONG, k LONG | STRING | nome completo dell'oggetto k-esimo. |
| `H5KIND` | h LONG, k LONG | LONG | tipo dell'oggetto k-esimo: 0 = dataset, 1 = gruppo, 2 = altro. |
| `H5SIZE` | h LONG, name STRING | LONG | dimensione (numero di elementi) del dataset: serve a dimensionare out(). |
| `H5READF64` | h LONG, name STRING, out() DOUBLE, dims() LONG, nelem() LONG | LONG | legge un dataset F64 in out() (>= H5SIZE); dims() = rank dimensioni, nelem(0) = elementi. |
| `H5READI64` | h LONG, name STRING, out() LONG, dims() LONG, nelem() LONG | LONG | legge un dataset I64 in out(). |
| `H5READDBL` | h LONG, name STRING, out() DOUBLE, dims() LONG, nelem() LONG | LONG | legge QUALSIASI dataset numerico convertendolo in double. |
| `H5READSTR` | h LONG, name STRING | STRING | primo elemento di un dataset di stringhe. |
| `H5READATTRSTR` | h LONG, obj STRING, attr STRING | STRING | attributo stringa. |
| `H5VERSION` |  | STRING | versione della libreria HDF5 sottostante. |

### 3.1 Parametri in dettaglio
| Parametro | Tipo | Significato |
|---|---|---|
| `h` | `LONG` | handle del file aperto (`H5CREATE`/`H5OPEN`). `0` = errore: tutte le funzioni che lo ricevono falliscono con `-1` (tranne `H5CLOSE(0)` che è un no-op e torna `0`). |
| `path` | `STRING` | percorso del file (`H5CREATE`/`H5OPEN`) o nome del gruppo (`H5MAKEGROUP`). Nei nomi HDF5 la `/` separa i livelli. |
| `name` | `STRING` | nome completo dell'oggetto nel file, es. `"barre/ohlc"`. Deve esistere già alla lettura. |
| `rank` | `LONG` | numero di dimensioni del dataset, `1..8`. `dims()` deve avere almeno `rank` elementi. |
| `dims()` | `LONG` | dimensioni del dataset: `dims(0)` è la più esterna. Il numero di valori dati è `dims(0)*...*dims(rank-1)`. |
| `data()` / `out()` | `DOUBLE` o `LONG` | nel write contiene i valori da scrivere; nel read è il buffer di uscita, dimensionato **almeno** a `H5SIZE(h, name)` elementi. Il tipo dell'array BASIC deve corrispondere al dataset (`H5READF64`/`H5READDBL` → `DOUBLE`, `H5READI64` → `LONG`). |
| `nelem()` | `LONG` | array di **un** elemento: in uscita riceve il numero di elementi effettivamente letti (utile per verificare che il buffer fosse sufficiente). |
| `compress` | `LONG` | livello **deflate** `0..9` (`0` = nessuna compressione); la compressione è sempre abbinata a *shuffle*. |
| `obj` | `STRING` | nome dell'oggetto su cui si scrive/legge l'attributo (dataset o gruppo). |
| `attr` | `STRING` | nome dell'attributo, es. `"unit"`, `"symbol"`. |
| `val` | `STRING` / `DOUBLE` | valore scalare dell'attributo. |
| `k` | `LONG` | indice `0..H5COUNT(h)-1` nell'enumerazione degli oggetti. Fuori range: `H5NAME` → stringa vuota, `H5KIND` → `2` (altro). |

## 4. Errori
| Codice | Significato | Esempio |
|---|---|---|
| `0` | esito positivo | `H5WRITEF64`, `H5READF64`, `H5CLOSE` |
| `-1` | handle o argomento non valido, dataset assente, **tipo inatteso** | `H5READF64(h, "conti", ...)` su un dataset I64; `H5SIZE(h, "inesistente")` |
| handle `0` | `H5CREATE`/`H5OPEN` non riusciti | percorso non scrivibile o file assente in `H5OPEN` |

## 5. Esempio

REQUIRE "hdf5"
IMPORT HDF5

fh = HDF5.H5CREATE("serie.h5")
DIM d(1) AS LONG : d(0) = 2 : d(1) = 3
DIM v(5) AS DOUBLE
FOR i = 0 TO 5 : v(i) = i + 0.5 : NEXT i
ok = HDF5.H5WRITEF64(fh, "serie", 2, d, v)      ' 0 = ok
CALL HDF5.H5CLOSE(fh)

fh = HDF5.H5OPEN("serie.h5")
PRINT HDF5.H5SIZE(fh, "serie")                   ' 6
DIM out(5) AS DOUBLE
DIM dims(7) AS LONG
DIM n(0) AS LONG
ok = HDF5.H5READF64(fh, "serie", out, dims, n)   ' 0 = ok


## 6. Protocollo §4.1 — esito
| Funzioni | MiniTest | IL | PCode | VM/C | C99Native |
|---|---|---|---|---|---|
| 19 | `hdf5ffi/h5_roundtrip.bas`, `hdf5ffi/h5_errors.bas` | ✓ | ✓ | ✓ | ✓ |

## 7. Riferimenti

## 8. Limiti

### DBF-Guida-Utente

*Fonte: `DBF-Guida-Utente`*


## 1. Apertura e creazione

### `DBF CREATE <path> FROM TYPE <Tipo>`

### `DBF USE <path> [ALIAS <alias>] [AS <Tipo>]`

### `DBF SELECT <alias> | <area>` — comando
| Parametro | Tipo | Significato |
|---|---|---|
| `<alias>` | identificatore nudo o stringa | alias registrato con `DBF USE … ALIAS`, case-insensitive |
| `<area>` | numero/espressione numerica | numero d'area 1-based (1 = prima area aperta) |

DBF USE "clienti.dbf" ALIAS Clienti     ' area 1
DBF USE "ordini.dbf"  ALIAS Ordini      ' area 2 (corrente)
DBF SELECT Clienti                      ' comando: corrente = Clienti (area 1)
PRINT DBF.ALIAS(); DBF.SELECTED()       ' Clienti 1
DBF SELECT 2                            ' forma numerica: corrente = Ordini (area 2)


### `DBF.SELECTED([alias|area])` — funzione
| Parametro | Tipo | Significato |
|---|---|---|
| *(nessuno)* | — | area **corrente** |
| `<alias>` | stringa o identificatore nudo | area dell'alias (un identificatore nudo è **sempre** il nome dell'alias, anche se esiste una variabile omonima) |
| `<area>` | numero/espressione numerica | restituisce il numero se quell'area è aperta, altrimenti `0` |

DBF USE "clienti.dbf" ALIAS Clienti
DBF USE "ordini.dbf"  ALIAS Ordini
PRINT DBF.SELECTED()            ' 2  (area corrente = ultima aperta)
PRINT DBF.SELECTED("Clienti")   ' 1
PRINT DBF.SELECTED(Clienti)     ' 1  (identificatore nudo = alias)
PRINT DBF.SELECTED(9)           ' 0  (area non aperta)
DBF SELECT Clienti
PRINT DBF.SELECTED()            ' 1


### `DBF CLOSE [<alias>|<area>[, …]]` · `DBF CLOSE DATABASES|ALL`

DBF USE "clienti.dbf" ALIAS Clienti     ' area 1
DBF USE "ordini.dbf"  ALIAS Ordini      ' area 2
DBF SELECT Clienti
DBF CLOSE Ordini                        ' chiude SOLO Ordini (area 2)
PRINT DBF.SELECTED()                    ' 1 (Clienti resta corrente)
DBF CLOSE                               ' chiude l'area corrente (Clienti)
PRINT DBF.SELECTED()                    ' 0


## 2. Dati: APPEND / REPLACE / GET

### `DBF APPEND BLANK` · `DBF APPEND RECORD <dict>`

### `DBF REPLACE <campo1> WITH <v1>[, <campo2> WITH <v2>, …]`

### `DBF GET RECORD <variabile>` · `DBF.RECNO()` · `DBF.LASTREC()`

## 3. Stato e filtri

## 4. Manutenzione / I/O

## 5. Conversioni e condizioni

### Campo nudo come valore (late-binding Clipper)
| Tipo DBF | Valore risultante | `VALTYPE` | `TYPENAME` |
|---|---|---|---|
| `N` (numerico), `F` (float), **`Y` (currency)** | CURRENCY (numerico + scala del testo) | `"N"` | `"CURRENCY"` |
| `L` (logico) | BOOL | `"L"` | `"BIT"` |
| `C`/`M`/… | STRINGA | `"C"` | `"STRING"` |

### `STR$(n)` — numerico → stringa

## 6. SEEK, ordine e indici su espressione (§4.1)

### `DBF SEEK <chiave> [SOFT]`

### `DBF INDEX ON <espressione> TO <file>`

## 7. Operazioni file-level con `FOR`/`WHILE`

## 8. `SET DELETED` e campi `BIT`

## 9. `DBF GET <variabile>` (late-binding)

## 7bis. Clausole `FOR`/`WHILE` — riferimento completo per comando (§4.1)

### `DBF DELETE [FOR|WHILE <cond>]` · `DBF RECALL [FOR|WHILE <cond>]`

### `DBF COPY TO <file> [FOR|WHILE <cond>]`

### `DBF APPEND FROM <file> [FOR|WHILE <cond>]`

### `DBF SORT TO <file> ON <campo1>[, <campo2>…] [/D] [FOR|WHILE <cond>]`

### `DBF TOTAL ON <chiave> TO <file> FIELDS <campo1>[, <campo2>…] [FOR|WHILE <cond>]`

### `DBF LIST [FIELDS <c1>[, <c2>…]] [FOR|WHILE <cond>] [TO PRINT | TO [FILE] <file>]`

### `DBF JOIN WITH <alias> TO <file> FOR FN(cli, mov) => <cond> [FIELDS <c1>, <c2>, …]`

### `DBF SET FILTER TO <expr>` · `… TO FN(rec) => <expr>` · `… TO ""`

### `DBF INDEX ON <expr> TO <file>` · `DBF SET ORDER TO <n>|<nome>|<nome>.ntx>|0`

### `DBF CLOSE DATABASES` · `DBF GO TOP`/`GO BOTTOM` · campo senza record corrente

## Protocollo §4.1 — esito 4 backend (MiniTest per comando)
| Superficie | MiniTest | IL | PCode | VM/C | C99 | C99Native |
|---|---|---|---|---|---|---|
| `DELETE FOR` + `SET DELETED` | `mt41_delete_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `RECALL FOR` (lambda) | `mt41_recall_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `INDEX ON` chiave composta + `SET ORDER "<nome>"` | `mt41_index_composite.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `TOTAL ON … FIELDS … FOR` | `mt41_total_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `SORT … /D FOR` | `mt41_sort_desc_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `COPY TO … FOR` | `mt41_copy_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `APPEND FROM … FOR` | `mt41_append_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `JOIN WITH … FOR FN(cli,mov)` | `mt41_join_lambda.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `SET FILTER TO FN(rec) =>` | `mt41_set_filter_lambda.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `CLOSE DATABASES` (multi-area) | `mt41_close_all.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `SEEK` controllante/SOFT | `dbf_seek_order.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `INDEX ON` espressione/`STR`/`#id` | `dbf_index_expr.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `COPY`/`APPEND`/`SORT`/`TOTAL … FOR` | `dbf_file_ops_for.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| campo `BIT` numerico `-1`/`0` | `dbf_bit_replace.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `DBF GET <var>` late-binding | `dbf_get_var.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `DBF SELECT` (comando) + `DBF.SELECTED()` (area 1-based) | `dbf_selected_area.bas` | ✓ | ✓ | ✓ | ✓ | ✓ |

## Riferimenti

### Database-Guida-Utente

*Fonte: `Database-Guida-Utente`*


## 1. Regola di sintassi (vale per tutti i namespace)
| | COMANDO | FUNZIONE |
|---|---|---|
| parentesi | **MAI** | **sempre** `NAMESPACE.FUNZIONE(a, b)` |
| scopo | esegue un'azione | ritorna un valore (si usa in un'espressione) |

DBF USE "clienti.dbf" ALIAS C          ' comando (legacy a spazio, senza parentesi)
n = DBF.LASTREC()                      ' funzione (parentesi, ritorna un valore)


## 2. DBF — tabelle DBF native (30 funzioni)
| funzione | ritorna |
|---|---|
| `DBF.RECNO()` | numero del record corrente (1-based) |
| `DBF.LASTREC()` | numero di record |
| `DBF.EOF()` / `DBF.BOF()` | `-1` (TRUE) / `0` (FALSE) |
| `DBF.FOUND()` | esito dell'ultima ricerca |
| `DBF.USED()` | c'è un'area DBF attiva |
| `DBF.SELECTED([alias\|area])` / `DBF.ALIAS()` | numero d'area 1-based (0 = nessuna) / alias attivo |
| `DBF.FIELDNAME(n)` / `DBF.RECSIZE()` | nome dell'n-esimo campo / dimensione record |
| `DBF.DELETED()` | record corrente marcato deleted |
| `DBF.DBFILTER()` | filtro attivo come stringa |
| `DBF.INDEXORD()` / `DBF.INDEXKEY()` | ordine/indice attivo e sua chiave |
| `DBF.UPDATE()` *(vedi `LUPDATE`)* | aggiorna il campo data/ora |
| `DBF.LOOKUP(campoRisultato, valore, campoRicerca [, area])` | valore cercando su un altro campo |
| `DBF.NETERR()` | ultimo errore di rete (replicazione) |

### 2bis. Replicazione: perché i sample usano argomenti NON validi

ISAM.SETREPLICATION(X, g_work + "/tcp_repl.isdb.repl")
PRINT ISAM.REPLICATIONSHIP("127.0.0.1", 0)     ' 0: porta fuori range, nessun connect
PRINT ISAM.REPLICATIONRECV(0)                  ' 0: porta fuori range, nessun accept


## 3. ISAM — indici ISAM (24 funzioni)

### `ISAM.INFO([alias])` — ispezione del file ISAM

### `LEDGER EDIT` — modifica di una transazione

LEDGER EDIT <handle> ID <id> WITH <data>, "<descrizione>" { "<conto>": <importo>, … } INTO <var>


### `LEDGER DELETE` — annulla una transazione

LEDGER DELETE <handle> ID <id>


## 4. IO — file e percorsi (29 funzioni)
| gruppo | funzioni |
|---|---|
| file | `IO.FOPEN`, `IO.FCREATE`, `IO.FCLOSE`, `IO.FREAD`, `IO.FWRITE`, `IO.FINPUT`, `IO.FSEEK`, `IO.SEEK`, `IO.EOF`, `IO.LOF`, `IO.FERROR`, `IO.FREEFILE` |
| percorsi | `IO.CWD`, `IO.DIR`, `IO.DIREXISTS`, `IO.FILEEXISTS`, `IO.PARENTDIR`, `IO.NORMALIZEPATH`, `IO.RESOLVEFILEPATH`, `IO.DIREXISTS`, `IO.FOLDER()`, `IO.FOLDERSIZE`, `IO.GETFOLDERPATH`, `IO.PROJECTFOLDER`, `IO.APPPATH`, `IO.BINPATH`, `IO.TEMPDIR`, `IO.TEMPFILE` |
| righe | `IO.READBYTES`, `IO.READLINES`, `IO.READFILE`, `IO.COUNTLINES` |

## 5. RDD — driver sostituibili (22 funzioni)

## 6. LEDGER — libro mastro (17 funzioni)
| funzione | ritorna |
|---|---|
| `LEDGER.OPEN(path)` | handle del libro (imposta il libro corrente) |
| `LEDGER.ADD(h, data, desc, postings)` | id della transazione |
| `LEDGER.EDIT(h, id, data, desc, postings:dict)` | id aggiornato — **solo forma statement** (vedi §9) |
| `LEDGER.COUNT(h)` / `LASTREC(h)` | numero di transazioni |
| `LEDGER.ACCOUNTS(h, root)` / `BALANCE(h, root)` / `REGISTER(h, query)` | **liste** (usare `LEN(...)`) |
| `LEDGER.GET(h, id)` | transazione come oggetto |
| `LEDGER.IMPORT(h, csv, mapping)` | numero di righe importate (0 se il mapping è invalido) |
| `LEDGER.PLOT(h, query, tipo)` | numero di punti (instabile: asserire `> 0`) |
| `LEDGER.VALIDATE(h)` / `VARIANCE(...)` | controllo/varianza |

## 7. Pattern ricorrente: la **area attiva**

RDD USE "clienti.csv" VIA "CSV" ALIAS C
DBF GO TOP                 ' navega l'area RDD (nessun DBF aperto)
PRINT DBF.RECNO(), DBF.EOF()


## 8. Come verificare (4 backend)

# MiniTest mirato: IL + PCode + VM/C con oracolo EXPECT
tools/sh/test_minitests_parity.sh <sottostringa>       # es. rdd_fields

# quinta backend: lexer/parser/emitter C + runtime C, eseguito
tools/sh/test_minitests_c99native                    # filtro: GMB_MINI_ONLY=<nome>

# UN sample su tutte le backend (IL, PCode, VM/C, C99Native) con diff normalizzato
tools/sh/parity4 shared/samples/Databases/RDD_Demo.bas
tools/sh/parity4 <file.bas> --legs il,pcode,vmc      # sottoinsieme di backend, più veloce

# tutti i sample di una directory in UN processo (IL/PCode in-process, VM/C per file)
tools/sh/parity4_sweep shared/samples/Databases rdd

# copertura: MiniTest e SAMPLE, per namespace, con la regola stretta
python3 tools/basic/minitest_census.bas --domain database
python3 tools/basic/minitest_census.bas --ns DBF --source samples    # elenca cosa manca nei sample


## 9. Lacune MISURATE (non nascoste)
| # | voce | stato |
|---|---|---|
| 1 | Copertura (census): MiniTest DBF/ISAM/IO/RDD 100%, LEDGER 16/17; **sample** DBF 46/46, ISAM 41/41, IO 29/29, RDD 22/22, LEDGER 16/17 | misurata con `tools/basic/minitest_census.bas --domain database` |
| 2 | `LEDGER.EDIT(...)` come **funzione**: rifiutata per progetto da IL e PCode («usa la forma statement») → il census la conta mancante, ma la superficie canonica è lo statement `LEDGER EDIT … INTO` | artefatto documentato (`Todo/(open) - MiniTest-Oracolo…` §17quinquies) |
| 3 | **Row-buffer RDD**: il runtime C non ha un buffer a righe → `OLDVAL`/`GETFLDSTATE` di un record in buffer restano vuoti/0 e `TABLEUPDATE`/`TABLEREVERT` sono nominali. Il sample `RDD_Buffer_Demo` diffa **solo** su quelle righe | **aperto** → `Todo/(solved) - RDD-Row-Buffer-Motore-C.md` |
| 4 | **Data in variabile implicita**: `d = RDD.TODAY()` → IL coercizza a seriale (`46276`), PCode conserva l'oggetto data (formattata). Con variabile **dichiarata** i due lati concordano (misurato con probe) | **aperto** → `Bugs/(open) - Data-Implicita-Coercizione-IL-vs-PCode.md` |
| 5 | **`RDD ON/OFF EVENT`**: richiede handler BASIC dispatchati come task GPM (§0ter); nel motore C è un **errore esplicito** (non un no-op silenzioso) | aperto (feature, non bug) |
| 6 | **VM/C — replicazione DBF**: `SETREPLICATION`/`APPLYREPLICATION`/`FOLLOWREPLICATION` divergono (`Replication log: .` e `-1/-1` invece del path e dei conteggi). Il sample gira (rc=0) ma IL/PCode ≠ VM/C | **aperto** (2026-09-11); non corretto perché la VM/C era in lavorazione da altra AI |
| 7 | ~~Segfault delle liste LEDGER su VM/C~~ | **CHIUSO** `b69586191` — era un **cast incrociato** (oggetto del parser JSON letto come oggetto del VM); verificato 5/5 run |
| 8 | ~~`LEN(LEDGER.BALANCE(h))` → 0 su IL e 2 su VM/C~~ | **CHIUSO** (`b69586191`: stesso difetto del punto 7) |
| 9 | ~~Campi impliciti sempre vuoti su VM/C (`PRINT Nome`)~~ | **CHIUSO** `d7a501e38` (`OP_LOAD_FIELD` senza base → campo dell'area attiva) |
| 10 | ~~`ISAM GET` non popolava l'UDT; `ISAM GO TOP` andava a fondo; `RDD SET FIELD` su area ISAM~~ | **CHIUSO** `22dfd6c76` |

## 10. Note per chi estende il dominio (lezioni misurate)

### 2ter. JOIN WITH (nested-loop join)

DBF USE "clienti.dbf" ALIAS C
DBF USE "ordini.dbf" ALIAS O
DBF SELECT O
DBF JOIN WITH C TO "join.dbf" FOR O.IDCLI = C.ID
DBF USE "join.dbf" ALIAS J
PRINT DBF.LASTREC()


DBF JOIN WITH C TO "empty.dbf" FOR O.ID = "NEVER"   ' 0 match


### 2quater. DBF SELECT (commutazione area attiva)

DBF USE "ordini.dbf" ALIAS O
DBF USE "clienti.dbf" ALIAS C   ' current = C
DBF SELECT O                     ' current = O (primary del JOIN)
DBF JOIN WITH C TO "join.dbf" FOR O.IDCLI = C.ID


### Dbf-UpdateOn-Guida-Utente

*Fonte: `Dbf-UpdateOn-Guida-Utente`*


## Sintassi

DBF UPDATE ON <campoChiave> FROM <aliasSorgente> REPLACE <campo> WITH <espressione> [, <campo2> WITH <espressione2> …]


## Parametri
| Parametro | Significato |
|---|---|
| `<campoChiave>` | Nome del campo chiave, **presente in entrambe** le aree (destinazione e sorgente). Il confronto è testuale, case-insensitive, con trim degli spazi (i numerici DBF sono right-aligned). |
| `<aliasSorgente>` | Alias dell'area sorgente, come registrato da `DBF USE … ALIAS <alias>`. Deve essere già aperta. |
| `<campo>` | Campo della **destinazione** (area corrente) da scrivere. |
| `<espressione>` | Espressione valutata per ogni record abbinato. |

## Semantica (Clipper)

## Esempio

DBF USE "clienti.dbf" ALIAS Clienti
DBF USE "movimenti.dbf" ALIAS Movimenti

DBF UPDATE ON ID FROM Movimenti REPLACE Nome WITH Movimenti.Nome, Saldo WITH Saldo + Movimenti.Saldo


## Disponibilità per backend
| Backend | Stato |
|---|---|
| **IL** | ✅ (IlRuntime.DbfUpdate → UpdateOn) |
| **PCodeVM** | ✅ (SysCall.DBF_UPDATE implementato) |
| **C99Native (C)** | ✅ (emette `gmb_db_dbf_update_on`) |
| **VM/C** | ✅ `gmb_sys_GMB_SYS_DBF_UPDATE` → `gmb_db_dbf_update_on` (verificato con `tools/sh/parity4`: `vmc == il`) |

## Riferimenti

### DuckDB-Guida-Utente

*Fonte: `DuckDB-Guida-Utente`*


## 1. Attivazione

REQUIRE "duckdb"              ' package manager: risolve/installa il pacchetto e ne include duckdb.inc
IMPORT DUCKDB WITH ALIAS      ' porta il namespace in scope -> chiamate DUCKDB.<Membro>(...)


## 2. Convenzioni FFI

## 3. Membri del namespace DUCKDB

### DUCKDB.AVAILABLE() AS LONG

### DUCKDB.OPEN(path AS STRING) AS LONG

DIM h AS LONG
h = DUCKDB.OPEN("")            ' in-memory
' h = DUCKDB.OPEN("/tmp/mio.duckdb")
IF h = 0 THEN PRINT "open fallito"


### DUCKDB.CLOSE(h AS LONG) AS LONG

### DUCKDB.EXEC(h AS LONG, sql AS STRING) AS LONG

DUCKDB.EXEC(h, "CREATE TABLE bars(ts BIGINT, sid INTEGER, o DOUBLE, h DOUBLE, l DOUBLE, c DOUBLE, v DOUBLE)")
DUCKDB.EXEC(h, "COPY bars TO '/tmp/bars.parquet' (FORMAT PARQUET)")   ' se l'estensione Parquet è disponibile


### DUCKDB.QUERY(h AS LONG, sql AS STRING, BYREF out AS LONG) AS LONG

DIM n AS LONG
IF DUCKDB.QUERY(h, "SELECT count(*) FROM bars", n) = 0 THEN PRINT "righe="; n


### DUCKDB.APPENDER(h AS LONG, tbl AS STRING) AS LONG

### DUCKDB.APPEND(app AS LONG, ts AS LONG, sid AS LONG, ohlcv AS STRING) AS LONG

### DUCKDB.FLUSH(app AS LONG) AS LONG

### DUCKDB.CLOSEAPPENDER(app AS LONG) AS LONG

### DUCKDB.LASTERR(h AS LONG, buf AS STRING) AS LONG

## 4. Esempio minimo

REQUIRE "duckdb"
IMPORT DUCKDB WITH ALIAS

DIM h AS LONG, app AS LONG, n AS LONG
h = DUCKDB.OPEN("")
DUCKDB.EXEC(h, "CREATE TABLE t(ts BIGINT, sid INTEGER, o DOUBLE, h DOUBLE, l DOUBLE, c DOUBLE, v DOUBLE)")
app = DUCKDB.APPENDER(h, "t")
DUCKDB.APPEND(app, 1000, 1, "1,2,0,1,10")
DUCKDB.FLUSH(app)
DUCKDB.CLOSEAPPENDER(app)
IF DUCKDB.QUERY(h, "SELECT count(*) FROM t", n) = 0 THEN PRINT "count="; n   ' count= 1
DUCKDB.CLOSE(h)


## 5. Esempio complesso e test

## 6. Note

### ISAM-Guida-Utente

*Fonte: `ISAM-Guida-Utente`*


## 1. Apertura e schema

### `ISAM OPEN <path> OF <Tipo> ALIAS <alias> [INDEXED BY <c1>[, <c2>, …]]`

### `ISAM SELECT <alias>` / `ISAM SELECT -1`

### `ISAM CLOSE <alias>`

## 2. Dati: APPEND / PUT / GET

### `ISAM APPEND <alias>, <record>`  ·  `ISAM APPEND <record>`

### `ISAM PUT <alias>, <record>`  ·  `ISAM PUT <record>`

### `ISAM GET <alias>, <record>`  ·  `ISAM GET -1, <record>`

## 3. Navigazione e ricerca

### `ISAM GO <alias>, TOP|BOTTOM`  ·  `ISAM GO <alias>, <n>`

### `ISAM SKIP <alias>, <n>`

### `ISAM SEEK <alias>, <chiave> [USING "<indice>"]`

## 4. Indici e filtro

### `ISAM SET ORDER TO "<indice>" ON <alias>|-1`

### `ISAM CREATE INDEX "<nome>" ON <alias>|-1 KEY (<campo>) LEN <n>`

### `ISAM DROP INDEX "<nome>" ON <alias>|-1`

### `ISAM SET FILTER TO <condizione> ON <alias>|-1`

## 5. Relazioni master-detail

### `ISAM SET RELATION ON <padre> TO <campo_chiave> INTO <figlio>`

## 6. Locking

### `ISAM LOCK <alias>[, RECORD]` · `ISAM UNLOCK <alias>[, RECORD]`

## 7. Manutenzione / I/O massivo

## 8. Funzioni (`ISAM.<FUNZIONE>(alias)`)
| Funzione | Ritorno | Note |
|---|---|---|
| `ISAM.RECNO(alias)` | LONG | numero record fisico corrente |
| `ISAM.EOF(alias)` | BIT | `TRUE` (-1) a fine area |
| `ISAM.BOF(alias)` | BIT | `TRUE` (-1) a inizio area |
| `ISAM.FOUND(alias)` | BIT | `TRUE` (-1) se l'ultimo SEEK ha trovato |
| `ISAM.LASTREC(alias)` | LONG | numero di record |
| `ISAM.RECSIZE(alias)` | LONG | dimensione record in byte |
| `ISAM.SELECTED(alias)` | LONG | numero d'area (1-based), 0 se non aperta |
| `ISAM.INFO(alias)` | STRING(JSON) | header + schema |
| `ISAM.INTEGRITY(alias)` | STRING(JSON) | validazione |

## 9. Conversioni: `CONVERSIONS.CSTR(x)` / `STR$(x)`

## 10. Truthiness nelle condizioni (`IF` / `DO WHILE`)

## 11. Disponibilità per backend
| Comando/Funzione | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| `ISAM OPEN/SELECT/CLOSE` | ✓ | ✓ | ✓ | ✓ |
| `APPEND/PUT/GET` | ✓ | ✓ | ✓ | ✓ |
| `GO/SKIP/SEEK [USING]` | ✓ | ✓ | ✓ | ✓ |
| `SET ORDER` (alias/indice) | ✓ | ✓ | ✓ | ✓ |
| `SET FILTER` (expr/lambda) | ✓ | ✓ | ✓ | ✓ |
| `CREATE/DROP INDEX` | ✓ | ✓ | ✓ | ✓ |
| `SET RELATION` | ✓ | ✓ | ✓ | ✓ |
| `RECNO/EOF/BOF/FOUND/LASTREC/RECSIZE/SELECTED` | ✓ | ✓ | ✓ | ✓ |
| `RLOCK/FLOCK/LOCK/UNLOCK` | ✓ | ✓ | ✓ | ✓ |
| `CONVERT` | ✓ | ✓ | ✓ | ✓ |

## Riferimenti

### LEDGER-Guide

*Fonte: `LEDGER-Guide`*


## 1. Apertura e chiusura

### `LEDGER.OPEN file$ AS h%` / `LEDGER OPEN file$ AS h%`

IMPORT LEDGER
h% = LEDGER.OPEN("contabilita.dat")
' Legacy:
LEDGER OPEN "contabilita.dat" AS h%


### `LEDGER.CLOSE h%` / `LEDGER CLOSE h%`

LEDGER CLOSE h%


### `LEDGER.SAVE h%`

LEDGER SAVE h%


## 2. Scrittura — ADD

### `LEDGER.ADD h% WITH date, desc$ { postings } [INTO id%]`

IMPORT LEDGER
h% = LEDGER.OPEN("contabilita.dat")
d! = DATETIME.TODAY()

LEDGER ADD h% WITH d!, "Salario Gennaio" {
  "Attivita:Banca"  2000
  "Ricavi:Stipendio" -2000
} INTO txId%

LEDGER ADD h% WITH d!, "Acquisto Libri" {
  "Spese:Libri"  50
  "Attivita:Banca" -50
} INTO txId2%

' Con tag:
LEDGER ADD h% WITH d!, "Utenze;Tag:bollette" {


## 3. Lettura — GET / REGISTER / BALANCE / ACCOUNTS

### `LEDGER.GET h% ID id% INTO json$`

DIM j$ AS STRING
LEDGER GET h% ID txId% INTO j$
PRINT j$ ' {"id":1,"date":"2024-01-01","desc":"...","postings":[...]}


### `LEDGER.REGISTER h% WHERE query$ INTO arr$()`

DIM reg$() AS STRING
LEDGER REGISTER h% WHERE "account:Spese:Libri" INTO reg$()
FOR i% = 0 TO UBOUND(reg$())
  PRINT reg$(i%)
NEXT


### `LEDGER.BALANCE h% [WHERE query$] INTO json$`

DIM bal$ AS STRING
LEDGER BALANCE h% WHERE "account:Attivita:Banca" INTO bal$
PRINT bal$ ' {"Attivita:Banca": 1950, ...}

LEDGER BALANCE h% INTO bal$ ' tutti i conti


### `LEDGER.ACCOUNTS h% [prefix$] INTO arr$()`

DIM all$() AS STRING
LEDGER ACCOUNTS h% INTO all$()

DIM spese$() AS STRING
LEDGER ACCOUNTS h% "Spese" INTO spese$()


## 4. Modifica e cancellazione

### `LEDGER.EDIT h% ID id% WITH date, desc$ { postings } [INTO newId%]`

LEDGER EDIT h% ID txId2% WITH DATETIME.DATEADD("D", 6, d!), "Acquisto Libri Aggiornato" {
  "Spese:Libri" 55
  "Attivita:Banca" -55
}


### `LEDGER.DELETE h% ID id%`

LEDGER DELETE h% ID txId%


### `LEDGER.VALIDATE h% INTO ok%`

DIM ok% AS LONG
LEDGER VALIDATE h% INTO ok%
PRINT ok% ' -1 = valido


## 5. Navigazione cursore (pattern DBF/ISAM)

LEDGER GO h% TOP
LEDGER SKIP h% 5
PRINT LEDGER.RECNO(h%)


## 6. Query avanzate

### `LEDGER.VARIANCE h% WHERE query$ INTO json$`

### `LEDGER.PLOT h% WHERE query$ INTO json$`

DIM var$ AS STRING
LEDGER VARIANCE h% WHERE "account:Spese:*" INTO var$


## 7. Esempi completi

### Esempio 1 — Ciclo di vita

IMPORT LEDGER
h% = LEDGER.OPEN("contabilita.dat")
d! = DATETIME.TODAY()

LEDGER ADD h% WITH d!, "Salario" {
  "Attivita:Banca" 3000
  "Ricavi:Stipendio" -3000
} INTO id1%

LEDGER ADD h% WITH d!, "Spesa" {
  "Spese:Cibo" 40
  "Attivita:Banca" -40
}

DIM bal$ AS STRING
LEDGER BALANCE h% INTO bal$


### Esempio 2 — Registro e validazione

IMPORT LEDGER
h% = LEDGER.OPEN("contabilita.dat")
LEDGER REGISTER h% WHERE "account:Spese:*" INTO reg$()
PRINT "Movimenti spese: " + STR$(UBOUND(reg$())+1)

DIM ok% AS LONG
LEDGER VALIDATE h% INTO ok%
PRINT "Valido: " + STR$(ok%)


### Esempio 3 — Edit e delete

IMPORT LEDGER
h% = LEDGER.OPEN("contabilita.dat")
LEDGER EDIT h% ID id% WITH d!, "Correzione" {
  "Spese:Libri" 60
  "Attivita:Banca" -60
}
LEDGER DELETE h% ID id%


## 8. Backend e note

## 9. Conteggi, cursore, import, varianza

PRINT LEDGER.COUNT(h%)        ' numero transazioni (LONG)
PRINT LEDGER.LASTREC(h%)      ' ultimo record (LONG)
PRINT LEDGER.RECNO(h%)        ' record corrente (LONG)
PRINT LEDGER.BOF(h%)          ' TRUE(-1) se cursore prima del primo record
PRINT LEDGER.EOF(h%)          ' TRUE(-1) se cursore oltre l'ultimo record
PRINT LEDGER.FOUND(h%)        ' TRUE(-1) se l'ultima ricerca ha trovato un record
DIM j = LEDGER.GET(h%, id%)   ' transazione per id (JSON/array)
DIM v = LEDGER.VARIANCE(h%, "Attivita:Cassa", "2026-01")   ' varianza per conto/periodo (JSON)
PRINT LEDGER.IMPORT(h%, "mov.csv", "date,desc,amount")     ' import CSV → numero righe


## Riferimenti

### MongoDB-Guida-Utente

*Fonte: `MongoDB-Guida-Utente`*


REQUIRE "mongo"
IMPORT MONGO WITH ALIAS        ' consente MONGO.<Membro>(...)


## Membro → parametri → ritorno
| Membro | Parametri | Ritorno | Cosa fa |
|---|---|---|---|
| `MONGO.AVAILABLE()` | — | LONG | 1 se la lib è presente e utilizzabile. |
| `MONGO.CONNECT(uri$)` | `uri$` = stringa di connessione (es. `"mongodb://localhost:27017"`) | LONG | Apre un client; ritorna un **handle** (>0) o 0. |
| `MONGO.CLOSE(h)` | `h` = handle client | LONG | Chiude il client. 1 ok. |
| `MONGO.PING(h)` | `h` | LONG | Verifica la connessione (`{ping:1}`). 1 ok, 0 err. |
| `MONGO.INSERT(h, db$, coll$, doc$)` | `doc$` = documento **JSON** | LONG | Inserisce un documento. 1 ok. |
| `MONGO.COUNT(h, db$, coll$, filter$)` | `filter$` = filtro JSON (`""` = tutti) | LONG | Conta i documenti; **-1** su errore. |
| `MONGO.FIND(h, db$, coll$, filter$)` | `filter$` | STRING | **Legge**: ritorna un **array JSON** dei documenti (`""` = tutti). |
| `MONGO.REMOVE(h, db$, coll$, filter$)` | `filter$` | LONG | Rimuove i documenti che soddisfano il filtro. 1 ok. |
| `MONGO.MODIFY(h, db$, coll$, filter$, update$)` | `update$` = update JSON (es. `{ "$set": { ... } }`) | LONG | Aggiorna i documenti. 1 ok. |
| `MONGO.LASTERROR(h)` | `h` | STRING | Ultimo errore del client (`""` se nessuno). |

## Esempio

REQUIRE "mongo"
IMPORT MONGO WITH ALIAS

DIM h AS LONG
h = MONGO.CONNECT("mongodb://localhost:27017")
IF h = 0 THEN PRINT "server non disponibile" : END

PRINT MONGO.INSERT(h, "testdb", "people", "{ ""name"": ""Ada"", ""age"": 36 }")
PRINT MONGO.COUNT(h, "testdb", "people", "")                  ' 1
PRINT MONGO.FIND(h, "testdb", "people", "{ ""name"": ""Ada"" }")   ' [{ ... }]
PRINT MONGO.MODIFY(h, "testdb", "people", "{ ""name"": ""Ada"" }", "{ ""$set"": { ""age"": 37 } }")
PRINT MONGO.REMOVE(h, "testdb", "people", "{ ""age"": 37 }")
MONGO.CLOSE(h)


## Disponibilità sui 4 backend

## Prerequisiti / note

## Riferimenti

### RDD-Guide

*Fonte: `RDD-Guide`*


## 1. Concetti

## 2. Apertura e selezione

### `RDD.USE file$ [VIA driver$] [ALIAS alias$]`

### `RDD.USE table$ FROM db OF TypeName` (per ISAM/SPEED tipizzati)

IMPORT RDD
RDD SET DEFAULT TO "DBF"
RDD USE "clienti.dbf" ALIAS C
RDD USE "clienti.isdb" OF Cliente VIA "ISAM" ALIAS C2


### `RDD.SELECT n%|alias$`

RDD SELECT 1
RDD SELECT C


### `RDD.CLOSE [INDEX name$] | [area]`

RDD CLOSE 1
RDD CLOSE INDEX 1, "by_nome"


## 3. Navigazione e stato
| Comando/Funzione | Descrizione |
|---|---|
| `RDD.GO TOP|BOTTOM|n%` | Posiziona |
| `RDD.SKIP [n%]` | Avanza |
| `RDD.EOF(n%)` / `BOF` | Fine/inizio |
| `RDD.FOUND(n%)` | Dopo seek |
| `RDD.RECNO(n%)` / `LASTREC` | Posizione / totale |
| `RDD.SELECTED()` | Alias corrente |
| `RDD.GETFIELD(n%, col%)` | Campo per indice 1-based o nome `col$` (col buffer attivo: legge dal buffer — §5) |
| `RDD.VALTYPE(n%)` | Nome del driver dell'area (`"DBF"`, `"ARRAY"`, `"SQL"`, …) |
| `RDD.TODAY()` / `CTOD`/`DTOC`/`CURVAL` | Date/valuta (`DTOC` → ISO `yyyy-MM-dd` su tutti i backend) |
| `RDD.FLOAT(expr)` | Converte/forza a DOUBLE (es. `RDD.FLOAT(12.5)`) |
| `RDD.SETDELETED(n%, flag)` | Funzione: nasconde (`1`/`-1`) o mostra (`0`) i record cancellati nell'area `n%` |
| `RDD.RECALL(scope, expr)` / `DELETE` / `REPLACE` | Scoped (`"CURRENT"` = record corrente, `"ALL"`, `NEXT n`, …); ritorna il conteggio |
| `RDD.OLDVAL(n%, col%)` / `GETFLDSTATE` / `BUFFER` / `TABLEUPDATE` / `TABLEREVERT` | Stato di modifica e row-buffer |

RDD GO TOP
DO WHILE NOT RDD.EOF(1)
  PRINT RDD.GETFIELD(1, 0)
  RDD SKIP 1
LOOP


## 4. Lock e transazioni

IF RDD.RLOCK(1) THEN
  RDD REPLACE 1, "Nome" WITH "Mario"
  RDD UNLOCK 1
END IF

IF RDD.FLOCK(1) THEN
  ' ... batch ...
  RDD UNLOCK 1
END IF


### Durabilità, group-commit e replicazione
| Comando/Funzione | Ritorno | Descrizione |
|---|---|---|
| `RDD.SETDURABILITY(area, mode$)` | `-1`/`0` | imposta la durabilità (`"full"`, `"normal"`, …); `-1` = successo |
| `RDD.FLUSHDURABILITY(area)` | `-1`/`0` | forza il flush su disco; `-1` = successo |
| `RDD.BEGINGROUPCOMMIT(area)` / `RDD.ENDGROUPCOMMIT(area)` | `-1`/`0` | apre/chiude un group-commit (più scritture, un solo fsync); `-1` = successo |
| `RDD.SETREPLICATION(area, logPath$)` | path | imposta il log di replicazione e ritorna il **path risolto** |
| `RDD.APPLYREPLICATION(logPath$)` | conteggio | applica le voci del log (≥ `0` = successo) |
| `RDD.FOLLOWREPLICATION(logPath$)` | conteggio | segue il log (streaming) |

## 5. Modifica
| Comando | Descrizione |
|---|---|
| `RDD.DELETE [scope]` | Marca cancellati |
| `RDD.RECALL [scope]` | Ripristina |
| `RDD.REPLACE field WITH value [, ...] [scope]` | Sostituisce campi |
| `RDD.PACK` | Rimuove fisicamente |

RDD DELETE FOR Saldo = 0
RDD RECALL ALL
RDD REPLACE "Prezzo" WITH "100" FOR Categoria = "A"


### Table buffering (row-buffer) e stato dei campi
| Funzione | Ritorno | Descrizione |
|---|---|---|
| `RDD.BUFFER(area, mode)` | `-1`/`0` | attiva (mode `2` pessimistico, `3` ottimistico) o disattiva (`0`/`1`) il row-buffer sul record corrente dell'area; allo snapshot segue la deviazione delle scritture |
| `RDD.GETFIELD(area, field)` | valore | campo del record corrente; con buffer attivo legge DAL BUFFER. `field` = ordinale **1-based** (1..n) oppure nome `"NOME"` |
| `RDD.OLDVAL(area, field)` | valore/null | valore dello snapshot (prima delle modifiche); senza buffer attivo → vuoto |
| `RDD.GETFLDSTATE(area, field)` | `0`/`1`/`2` | `0` immutato, `1` modificato, `2` record marcato cancellato; senza buffer → `0` |
| `RDD.TABLEUPDATE(area)` | `-1`/`0` | applica i campi modificati al motore con UNA scrittura del record, azzera gli stati, rilascia il lock pessimistico e disattiva |
| `RDD.TABLEREVERT(area)` | `-1`/`0` | scarta il buffer (il motore non è mai stato toccato) e disattiva |

RDD USE "anagrafe.dbf" ALIAS A
ok = RDD.BUFFER(1, 2)              ' pessimistico: -1
RDD SET FIELD 1, 3, 99000          ' deviato nel buffer
v = RDD.GETFIELD(1, 3)             ' 99000 (dal buffer)
old = RDD.OLDVAL(1, "STIPENDIO")   ' snapshot originale
st = RDD.GETFLDSTATE(1, 3)         ' 1 = modificato
IF RDD.TABLEUPDATE(1) < 0 THEN PRINT "applicato"   ' -1 = successo
' revert senza commit:
RDD.BUFFER(1, 3) : RDD SET FIELD 1, 2, "TEMP" : RDD.TABLEREVERT(1)


## 6. Indici e SET

### `RDD.CREATE INDEX area, name$, key$`

### `RDD.CLOSE INDEX area, name$`

RDD CREATE INDEX 1, "by_nome", "Nome"
RDD CLOSE INDEX 1, "by_nome"


### `RDD.SET ...`
| Comando | Descrizione |
|---|---|
| `RDD.SET DEFAULT TO driver$` | Driver default (`DBF`/`ISAM`/`MEMORY`) |
| `RDD.SET DELETED ON|OFF` | Nasconde cancellati |
| `RDD.SET FIELD area, field, value` | Imposta campo per area |
| `RDD.SET ORDER TO area, n%` | Ordine indice |
| `RDD.SET REPROCESS TO n%` | Retry lock |
| `RDD.SET LOCK TIMEOUT TO n%` | Timeout lock |

RDD SET DEFAULT TO "ISAM"
RDD SET DELETED ON
RDD SET ORDER TO 1, 1


## 7. Eventi

RDD ON EVENT "evento", SUB_handler
RDD OFF EVENT "evento"


## 8. Copia / Ordinamento / Aggregazione
| Comando | Descrizione |
|---|---|
| `RDD.COPY TO file$ [VIA driver$]` | Copia tabella |
| `RDD.SORT TO file$ [FIELDS ...]` | Ordina |
| `RDD.TOTAL ON key$` | Totalizza |

RDD COPY TO "backup.dbf" VIA "DBF"
RDD SORT TO "sorted.dbf" FIELDS "Cognome"
RDD TOTAL ON "Regione"


## 9. Esempi completi

### Esempio 1 — CRUD Memory

IMPORT RDD
RDD SET DEFAULT TO "MEMORY"
RDD USE "mem" ALIAS 1
DIM r AS Cliente : r.Nome = "Anna" : r.Eta = 28
RDD APPEND 1, r

RDD GO TOP
DO WHILE NOT RDD.EOF(1)
  PRINT RDD.GETFIELD(1, 1) ' Nome
  RDD SKIP 1
LOOP
RDD CLOSE 1


### Esempio 2 — DBF via RDD

IMPORT RDD
RDD SET DEFAULT TO "DBF"
RDD USE "clienti.dbf" ALIAS 1
RDD SET FILTER TO "Eta > 25"  ' via DBF driver
PRINT RDD.LASTREC(1)


### Esempio 3 — ISAM via RDD

IMPORT RDD
RDD USE "clienti.isdb" OF Cliente VIA "ISAM" ALIAS 1
RDD CREATE INDEX 1, "by_nome", "Nome"
RDD SET ORDER TO 1, 1
IF RDD.FOUND(1) THEN PRINT "trovato"


### Esempio 4 — SQL via RDD

IMPORT RDD
RDD SET DEFAULT TO "SQL"
RDD USE "Host=localhost;..." ALIAS 1
' Poi SQL.QUERY via RDD


## 10. Backend e note

## Riferimenti

### SPEED-Guide

*Fonte: `SPEED-Guide`*


## 1. Apertura database e tabelle

### `SPEED.OPEN db$ [ALIAS alias$]`

IMPORT SPEED
SPEED OPEN "MioDatabase" ALIAS MioDb
' Moderna:
SPEED.OPEN("MioDatabase", "MioDb")


### `SPEED.USE table$ FROM dbAlias OF TypeName [ALIAS alias$] [INDEXED BY field]`

TYPE Cliente
  ID AS LONG
  Nome AS STRING
  Eta AS LONG
END TYPE

SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Clienti INDEXED BY ID
' Moderna: SPEED.USE("Clienti", MioDb, "Cliente", "Clienti", "ID")


### `SPEED.CLOSE [alias [, ...] | DATABASES]`

SPEED CLOSE Clienti
SPEED CLOSE DATABASES


## 2. Scrittura e lettura

### `SPEED.PUT alias, var` — Inserisce o aggiorna (upsert) per chiave `INDEXED BY`.

DIM c AS Cliente : c.ID = 1 : c.Nome = "Anna" : c.Eta = 28
SPEED PUT Clienti, c


### `SPEED.SETMANY(alias, lista)` — inserimento massivo da `LIST OF <record>`

TYPE Riga
  ID AS LONG
  Nome AS STRING * 32
END TYPE
DIM batch AS LIST OF Riga
batch = NEW LIST OF Riga {}            ' inizializzatore vuoto (vedi Collezioni §2.1)
DIM r AS Riga
r.ID = 1 : r.Nome = "Anna" : batch.Add(r)
r.ID = 2 : r.Nome = "Bruno" : batch.Add(r)
DIM n AS LONG
n = SPEED.SETMANY(Tabella, batch)      ' → 2


### `SPEED.GET alias, key -> var` / `SPEED.GET alias, var`

DIM r AS Cliente
SPEED GET Clienti, 1 INTO r
PRINT r.Nome


### `SPEED.DELETE alias, key` / `SPEED.RECALL`

## 3. Query integrate

### `SPEED.SELECT ... FROM ... [JOIN ...] [WHERE ...] [GROUP BY ...] [HAVING ...] [ORDER BY ...] [INTO ... OF Tipo]`

' Selezione semplice (proiezione di colonne)
SPEED SELECT Nome, Citta FROM Clienti WHERE Eta > 25 INTO res$()

' SELECT * (espande tutti i campi del TYPE di destinazione)
SPEED SELECT * FROM Clienti INTO res() OF Cliente

' Join + aggregazione (GROUP BY) + INTO su UDT
' NB: il JOIN richiede un indice sul campo di join della tabella DESTRA
' (senza indice i backend divergono: crearlo SEMPRE).
SPEED CREATE INDEX "ID_Cliente" ON Ordini KEY (ID_Cliente)
SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto -
  INTO ris() OF RisultatoJoin -
  FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente -
  GROUP BY Clienti.Nome

' Con filtro post-aggregazione e ordinamento


## 4. Navigazione cursore (pattern DBF/ISAM)

SPEED GO Clienti TOP
DO WHILE NOT SPEED.EOF(Clienti)
  DIM r AS Cliente : SPEED GET r
  PRINT r.Nome
  SPEED SKIP Clienti
LOOP

| Funzione | Descrizione |
|---|---|
| `SPEED.EOF(alias)` / `BOF` | Fine/inizio |
| `SPEED.FOUND(alias)` | Dopo `SEEK`/`GET` |
| `SPEED.RECNO(alias)` / `LASTREC` | Posizione / totale |
| `SPEED.SELECTED()` | Alias corrente |
| `SPEED.DBFILTER(alias)` | `STRING` filtro attivo (solo lettura) |

### `SPEED.SETFILTER "condizione" ON alias`

SPEED SET FILTER TO "ID > 1" ON T
SPEED GO T, TOP
SPEED GET T, rec
PRINT rec.Name            ' primo record che passa il filtro


## 5. Indici

### `SPEED CREATE INDEX <nome$> ON <alias> KEY (<campo>)`

### `SPEED DROP INDEX <nome$> ON <alias>`

SPEED CREATE INDEX "CittaIdx" ON Clienti KEY (Citta)
SPEED DROP INDEX "CittaIdx" ON Clienti

| Comando | Parametri | Descrizione |
|---|---|---|
| `SPEED CREATE INDEX` | `nome$` = nome indice (stringa o identificatore); `alias` = tabella; `(campo)` = campo indicizzato (tra parentesi) | Crea un indice secondario |
| `SPEED DROP INDEX` | `nome$` = nome indice; `alias` = tabella | Elimina l'indice |

## 5bis. Viste materializzate e relazioni (2026-09-22)

' Relazione master-detail (navigazione): imposta e rimuove
SPEED SET RELATION ON Clienti TO ID INTO Ordini
SPEED SET RELATION ON Clienti                ' rimuove la relazione

' Vista persistente (tabella di sola lettura da una SELECT)
SPEED CREATE VIEW ClientiAttivi OF Cliente AS SELECT * INTO ris() OF Cliente FROM Clienti WHERE Clienti.Attivo = TRUE
SPEED REFRESH VIEW ClientiAttivi             ' ricalcola i dati della vista
SPEED DROP VIEW ClientiAttivi                ' elimina la definizione

| Comando | Descrizione |
|---|---|
| `SPEED SET RELATION ON master TO chiave INTO detail` | Relazione master-detail per la navigazione |
| `SPEED SET RELATION ON master` | Rimuove la relazione |
| `SPEED CREATE VIEW nome OF Tipo AS SELECT ...` | Vista persistente (sola lettura) |
| `SPEED REFRESH VIEW nome` | Ricalcola la vista dai dati correnti |
| `SPEED DROP VIEW nome` | Elimina la definizione della vista |

## 6. Funzioni di utilità
| Funzione | Descrizione |
|---|---|
| `SPEED.INFO(alias)` | Info tabella (indici, count) → `JSON` |
| `SPEED.REPAIR(alias)` | Ripara |
| `SPEED.SETREPLICATION(alias, path$)` | Replica log shipping |
| `SPEED.BEGIN/COMMIT/ROLLBACK` | Transazioni |

PRINT SPEED.INFO(Clienti)
SPEED REPAIR Clienti


## 7. Esempi completi

### Esempio 1 — CRUD NoSQL

IMPORT SPEED
TYPE Cliente : ID AS LONG : Nome AS STRING : Eta AS LONG : END TYPE

SPEED OPEN "MioDatabase" ALIAS MioDb
SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Clienti INDEXED BY ID

DIM c AS Cliente : c.ID = 1 : c.Nome = "Mario" : c.Eta = 30
SPEED PUT Clienti, c

DIM r AS Cliente
SPEED GET Clienti, 1 INTO r
PRINT r.Nome ' "Mario"

r.Eta = 31
SPEED PUT Clienti, r



### Esempio 2 — Query con join

IMPORT SPEED
SPEED OPEN "MioDatabase" ALIAS MioDb
SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS C INDEXED BY ID
SPEED USE "Ordini" FROM MioDb OF Ordine ALIAS O INDEXED BY ID_Ordine

SPEED SELECT * FROM C JOIN O ON C.ID = O.ClienteID WHERE O.Totale > 100 INTO res$()
FOR i% = 0 TO UBOUND(res$())
  PRINT res$(i%)
NEXT


### Esempio 3 — Replica e transazioni

IMPORT SPEED
SPEED OPEN "Primary" ALIAS P
PRINT SPEED.SETREPLICATION(P, ".")
SPEED PUT P, rec
PRINT SPEED.APPLYREPLICATION(S, "repl.log")


## 8. Backend e note

## Riferimenti

### SQL-Guide

*Fonte: `SQL-Guide`*


## 1. Apertura e chiusura

### `SQL.OPEN conn$ [ALIAS alias$]` / `SQL OPEN conn$ [ALIAS alias$]`

IMPORT SQL
SQL OPEN "Data Source=app.db" ALIAS AppDb
SQL OPEN "Host=localhost;Port=5432;Database=mydb;Username=user" ALIAS Pg
' Senza ALIAS: usa alias default "SQL"
SQL OPEN "Data Source=:memory:"


### `SQL.CLOSE db$` / `SQL CLOSE RESULTSET rs`

SQL CLOSE AppDb
SQL CLOSE RESULTSET rs


## 2. Esecuzione

### `SQL.EXEC db$, sql$ [, params...]`

SQL EXEC AppDb, "CREATE TABLE t (id LONG, name STRING)"
SQL EXEC AppDb, "INSERT INTO t VALUES (1, 'Anna')"


### `SQL.QUERY db$, sql$ [, params...] INTO rs`

DIM rs AS LONG
rs = SQL.QUERY(AppDb, "SELECT * FROM t WHERE id > ?", 0)


### `SQL.FETCH(rs) → BOOLEAN`

DO WHILE SQL.FETCH(rs)
  PRINT rs("name"), rs("id")
LOOP


### Accesso campi result set

' Per nome o indice (0-based)
PRINT rs("name")
PRINT rs(0)


## 2bis. Funzioni temporali (TIMEBUCKET / FIRST_TS / LAST_TS)

### `TIMEBUCKET(ts, secs)`
| parametro | tipo | obbligatorio | significato |
|---|---|---|---|
| `ts` | intero o reale | sì | timestamp da arrotondare |
| `secs` | intero o reale | sì | ampiezza del bucket in secondi (> 0) |

### `FIRST_TS(x, ts)` / `LAST_TS(x, ts)`
| parametro | tipo | obbligatorio | significato |
|---|---|---|---|
| `x` | qualsiasi | sì | valore da prelevare (tipo preservato: numero, testo, blob) |
| `ts` | intero o reale | sì | chiave temporale su cui confrontare |

IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    SQL OPEN "work/ticks.db" ALIAS TB
    SQL EXEC TB, "CREATE TABLE ticks(ts INTEGER, price INTEGER, vol INTEGER)"
    SQL EXEC TB, "INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),(300,20,50),(360,18,75),(420,22,125),(-60,9,10)"
    DIM rs AS SQLRESULTSET
    SQL QUERY TB, rs, "SELECT TIMEBUCKET(ts,300) AS b, FIRST_TS(price,ts) AS o, MAX(price) AS h, MIN(price) AS l, LAST_TS(price,ts) AS c, SUM(vol) AS v FROM ticks GROUP BY b ORDER BY b"
    DO WHILE SQL.FETCH(rs)
        PRINT "b="; rs("b"); " o="; rs("o"); " h="; rs("h"); " l="; rs("l"); " c="; rs("c"); " v="; rs("v")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE TB
END SUB


## 2ter. Aggregati statistici (VAR_* / STDDEV* / MEDIAN / MODE / QUANTILE / COVAR* / CORR)
| Aggregato | Significato | Ritorno | MiniTest |
|---|---|---|---|
| `VAR_POP(x)` | varianza di popolazione (÷ n) | reale; `NULL` se n = 0 | `shared/minitests/sql/var_pop.bas` |
| `VAR_SAMP(x)` | varianza campionaria (÷ n−1) | reale; `NULL` se n < 2 | `shared/minitests/sql/var_samp.bas` |
| `VARIANCE(x)` | alias di `VAR_SAMP(x)` | reale; `NULL` se n < 2 | `shared/minitests/sql/variance.bas` |
| `STDDEV_POP(x)` | deviazione standard di popolazione | reale; `NULL` se n = 0 | `shared/minitests/sql/stddev_pop.bas` |
| `STDDEV_SAMP(x)` | deviazione standard campionaria | reale; `NULL` se n < 2 | `shared/minitests/sql/stddev_samp.bas` |
| `STDDEV(x)` | alias di `STDDEV_SAMP(x)` | reale; `NULL` se n < 2 | `shared/minitests/sql/stddev.bas` |
| `MEDIAN(x)` | mediana (media dei due centrali se n pari) | reale; `NULL` se n = 0 | `shared/minitests/sql/median.bas` |
| `MODE(x)` | valore più frequente (a parità, il più piccolo) | `NULL` se n = 0 | `shared/minitests/sql/mode.bas` |
| `QUANTILE(x, p)` | quantile con `p ∈ [0,1]`, interpolazione lineare | `NULL` se n = 0 o `p∉[0,1]` | `shared/minitests/sql/quantile.bas` |
| Aggregato | Significato | Ritorno | MiniTest |
|---|---|---|---|
| `COVAR_POP(y, x)` | covarianza di popolazione | `NULL` se n = 0 | `shared/minitests/sql/covar_pop.bas` |
| `COVAR_SAMP(y, x)` | covarianza campionaria | `NULL` se n < 2 | `shared/minitests/sql/covar_samp.bas` |
| `CORR(y, x)` | correlazione di Pearson | `NULL` se n < 2 o varianza nulla | `shared/minitests/sql/corr.bas` |

IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    SQL OPEN "work/stats.db" ALIAS T
    SQL EXEC T, "CREATE TABLE prices(p REAL, v REAL, sector TEXT)"
    SQL EXEC T, "INSERT INTO prices VALUES (2,10,'a'),(4,20,'a'),(4,20,'a'),(4,25,'b'),(5,30,'b'),(5,30,'b'),(7,40,'a'),(9,50,'b')"
    DIM rs AS SQLRESULTSET
    SQL QUERY T, rs, "SELECT ROUND(MEDIAN(p),4) AS median, ROUND(STDDEV_SAMP(p),4) AS sd, ROUND(QUANTILE(p,0.05),4) AS var95, MODE(p) AS mode, ROUND(CORR(p,v),4) AS corr FROM prices"
    IF SQL.FETCH(rs) THEN
        PRINT "median="; rs("median"); " sd="; rs("sd"); " var95="; rs("var95"); " mode="; rs("mode"); " corr="; rs("corr")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE T
END SUB


## 2quater. Scan diretto di file CSV (virtual table `csv`)

CREATE VIRTUAL TABLE <nome> USING csv(name='<file.csv>'[, header=1][, delim=',']);

| parametro | obbligatorio | significato |
|---|---|---|
| `name` (o 1° argomento posizionale) | sì | percorso del file CSV (assoluto o relativo alla CWD del programma). Accetta anche le forme quotate `name='file.csv'` / `name="file.csv"` (le quote esterne vengono rimosse) |
| `header` | no | `1` (default) = prima riga come nomi colonna; `0` = nessuna intestazione, colonne `c0,c1,…`. Accetta `header=0` e `header='0'` |
| `delim` | no | separatore di campo, un solo carattere (default `,`). Accetta `delim=;` e `delim=';'`/`delim=";"` — le quote esterne sono rimosse, quindi `delim=';'` usa davvero `;` |

IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS C
    SQL EXEC C, "CREATE VIRTUAL TABLE prezzi USING csv(name='dati/prezzi.csv')"

    DIM rs AS SQLRESULTSET
    ' lettura + UDF
    SQL QUERY C, rs, "SELECT symbol, ROUND(AVG(price),2) AS medio FROM prezzi WHERE volume > 1000 GROUP BY symbol ORDER BY medio DESC"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("symbol"); " "; rs("medio")
    LOOP
    SQL CLOSE RESULTSET rs

    ' scrittura: INSERT/UPDATE/DELETE riscrivono il file
    SQL EXEC C, "INSERT INTO prezzi VALUES (99,'NEWCO',12.5,500)"


## 2quinquies. Scan diretto di file Parquet (virtual table `parquet`)

CREATE VIRTUAL TABLE <nome> USING parquet(name='<file.parquet>');

| parametro | obbligatorio | significato |
|---|---|---|
| `name` (o 1° argomento posizionale) | sì | percorso del file Parquet (assoluto o relativo alla CWD). Accetta `name='file.parquet'` (quote esterne rimosse) |
| `schema` | no | usato con `CREATE VIRTUAL TABLE`: **crea** il file con lo schema dato. Forma: `schema='col TIPO, col2 TIPO2, …'` |

IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS P
    ' lettura
    SQL EXEC P, "CREATE VIRTUAL TABLE trades USING parquet(name='dati/trades.parquet')"
    ' creazione da schema + scrittura
    SQL EXEC P, "CREATE VIRTUAL TABLE mag USING parquet(name='dati/mag.parquet', schema='id LONG, nome TEXT, prezzo DOUBLE, attivo BOOL, giacenza INT32')"
    SQL EXEC P, "INSERT INTO mag VALUES (1,'vite',0.5,1,1000)"
    SQL EXEC P, "UPDATE mag SET prezzo=0.9 WHERE id=1"
    SQL EXEC P, "DELETE FROM mag WHERE giacenza=0"

    DIM rs AS SQLRESULTSET
    SQL QUERY P, rs, "SELECT symbol, ROUND(AVG(price),2) AS medio FROM trades WHERE volume > 1000 GROUP BY symbol ORDER BY medio DESC"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("symbol"); " "; rs("medio")


## 3. Transazioni

SQL BEGIN TRANSACTION AppDb
SQL EXEC AppDb, "INSERT INTO t VALUES (2, 'Bob')"
IF lErr THEN
  SQL ROLLBACK AppDb
ELSE
  SQL COMMIT AppDb
END IF

' Savepoint
SQL SAVEPOINT AppDb, "sp1"
SQL ROLLBACK TO SAVEPOINT AppDb, "sp1"
SQL RELEASE SAVEPOINT AppDb, "sp1"


## 4. Funzioni di utilità
| Funzione | Descrizione |
|---|---|
| `SQL.LASTINSERTROWID(db$)` | Ultimo ROWID inserito |
| `SQL.CHANGES(db$)` | Righe modificate dall'ultimo EXEC |
| `SQL.ERROR(db$)` | Ultimo errore |
| `SQL.ATTACH db$, file$, alias$` | Allega altro DB |
| `SQL.BACKUP db$ TO file$` | Backup file |

PRINT SQL.LASTINSERTROWID("AppDb")
PRINT SQL.CHANGES("AppDb")
SQL ATTACH AppDb, "other.db", "Other"
SQL BACKUP AppDb TO "backup.db"


## 5. Parametri e tipi

SQL EXEC AppDb, "INSERT INTO t VALUES (?, ?)", id%, name$
SQL QUERY AppDb, "SELECT * FROM t WHERE name = ?", "Anna" INTO rs


## 6. Esempi completi

### Esempio 1 — CRUD SQLite

IMPORT SQL
SQL OPEN "Data Source=:memory:" ALIAS AppDb
SQL EXEC AppDb, "CREATE TABLE utenti (id LONG PRIMARY KEY, nome STRING)"
SQL EXEC AppDb, "INSERT INTO utenti VALUES (1, 'Anna')"
SQL EXEC AppDb, "INSERT INTO utenti VALUES (2, 'Bob')"

DIM rs AS LONG
rs = SQL.QUERY(AppDb, "SELECT * FROM utenti ORDER BY nome")
DO WHILE SQL.FETCH(rs)
  PRINT rs("nome")
LOOP
SQL CLOSE RESULTSET rs
SQL CLOSE AppDb


### Esempio 2 — Transazione con rollback

IMPORT SQL
SQL OPEN "Data Source=app.db" ALIAS AppDb
SQL BEGIN TRANSACTION AppDb
SQL EXEC AppDb, "INSERT INTO t VALUES (1, 'test')"
SQL EXEC AppDb, "INSERT INTO t VALUES (1, 'dup')" ' chiave duplicata -> errore
IF SQL.ERROR(AppDb) <> "" THEN
  SQL ROLLBACK AppDb
  PRINT "rollback: " + SQL.ERROR(AppDb)
ELSE
  SQL COMMIT AppDb
END IF


### Esempio 3 — PostgreSQL

IMPORT SQL
SQL OPEN "Host=localhost;Port=5432;Database=mydb;Username=user;Password=pass" ALIAS Pg
SQL EXEC Pg, "CREATE TABLE IF NOT EXISTS t (id SERIAL PRIMARY KEY, v STRING)"
SQL EXEC Pg, "INSERT INTO t (v) VALUES ('hello')"
PRINT SQL.LASTINSERTROWID("Pg")
SQL CLOSE Pg


### Esempio 4 — Via RDD (driver SQL)

IMPORT RDD
RDD SET DEFAULT TO "SQL"
RDD USE "Host=localhost;..." ALIAS 1
' Poi SQL.QUERY via RDD alias 1


### Esempio 5 — Sorgenti scrivibili CSV + Parquet (flat e annidato)

## 7. Backend e note

### Scritture pesanti e GPM

## G10 — DataFrame (`SQL.DFSCHEMA` / `SQL.COL` / `SQL.TOCSV` / `SQL.FROMCSV`)
| Funzione | Firma | Ritorna |
|---|---|---|
| `SQL.DFSCHEMA(rs)` | `SQLRESULTSET` → STRING | schema JSON: `{"rows":n,"columns":[{"name":..,"type":..}]}` |
| `SQL.COL(rs, col)` | `SQLRESULTSET, STRING` → ARRAY | intera colonna `col` come ARRAY (elementi stringa) |
| `SQL.TOCSV(rs, path)` | `SQLRESULTSET, STRING` → LONG | scrive CSV (header + righe); **0** = ok |
| `SQL.FROMCSV(alias, table, path)` | `STRING, STRING, STRING` → LONG | crea la tabella `table` dal CSV (header = nomi colonne, tutte TEXT); **0** = ok |

### Parametri (uno per uno)

IMPORT SQL
SQL OPEN ":memory:" ALIAS st
SQL EXEC st, "CREATE TABLE t(id INTEGER, name TEXT)"
SQL EXEC st, "INSERT INTO t VALUES(1,'a')"
DIM rs AS SQLRESULTSET
SQL QUERY st, rs, "SELECT id, name FROM t ORDER BY id"
PRINT SQL.DFSCHEMA(rs)                 ' {"rows":2,"columns":[...]}
DIM ids() = SQL.COL(rs, "id")          ' ARRAY colonna
PRINT ids(0); ids(1)
PRINT SQL.TOCSV(rs, "work/out.csv")    ' 0
PRINT SQL.FROMCSV("st", "t2", "work/out.csv")  ' 0
SQL CLOSE RESULTSET rs
SQL CLOSE st


## Riferimenti

### DBF-Guide

*Fonte: `DBF-Guide`*


## 1. Apertura e selezione work area

### `DBF.USE file$ [, EXCLUSIVE|SHARED] [, ALIAS alias$] [, INDEX idx$ [, ...]] [, AS TypeName]`

### `DBF.OPEN file$ [, ...] ALIAS alias$` — sinonimo di `USE` con `ALIAS` obbligatorio

IMPORT DBF
DBF USE "clienti.dbf" ALIAS Clienti EXCLUSIVE NEW INDEX "by_cogn.ntx"
' Moderna qualificata:
DBF.USE("clienti.dbf", FALSE, ["by_cogn.ntx"], "Clienti", "Cliente")


### `DBF.SELECT area` / `DBF.SELECT(n%)`

DBF SELECT Clienti
DBF.SELECT(2)


### `DBF.CLOSE [area [, ...] | DATABASES]`

DBF CLOSE Clienti
DBF CLOSE DATABASES
DBF.CLOSE("Clienti", "Ordini")


## 2. Navigazione

### `DBF.GO TOP|BOTTOM|n%|expr`

DBF GO TOP
DBF GO 15
DBF.GO("TOP")


### `DBF.SKIP [n%]`

DBF SKIP -5
DBF.SKIP(10)


### `DBF.SEEK key$|num`

DBF SET ORDER TO 1
DBF SEEK "Rossi"
IF DBF.FOUND() THEN PRINT DBF.RECNO()


## 3. Lettura dati

### `DBF.GET [var]`

DIM r AS Cliente
DBF GET r
PRINT r.Nome


### `DBF.LIST [FIELDS f1 [, ...]] [FOR cond] [TO FILE file$|TO PRINT]`

DBF LIST FIELDS Nome, Citta FOR Saldo > 0 TO PRINT
DBF.LIST("Nome, Citta", "Saldo > 0")


### Funzioni di stato
| Funzione | Ritorno |
|---|---|
| `DBF.EOF()` / `DBF.BOF()` | `BOOLEAN` fine/inizio file |
| `DBF.FOUND()` | `BOOLEAN` dopo `SEEK` |
| `DBF.RECNO()` / `DBF.RECCOUNT()` / `DBF.LASTREC()` | `LONG` posizione / totale |
| `DBF.SELECTED([alias$])` | `LONG` numero dell'area corrente (1-based; 0 se nessuna) |
| `DBF.ALIAS([alias$])` | `STRING` alias dell'area |
| `DBF.USED([alias$])` | `BOOLEAN` area aperta e utilizzabile |
| `DBF.DELETED()` | `BOOLEAN` record corrente marcato cancellato |
| `DBF.RECSIZE()` | `LONG` dimensione del record in byte |
| `DBF.LUPDATE()` | `STRING` data ultimo update file (`YYYY-MM-DD`) |
| `DBF.DBFILTER([alias$])` | `STRING` filtro attivo (`SET FILTER`) |
| `DBF.FIELDNAME(n [, alias$])` | `STRING` nome dell'n-esimo campo (1-based) |
| `DBF.INDEXKEY(n)` | `STRING` espressione chiave dell'indice n |
| `DBF.INDEXORD()` | `LONG` ordine dell'indice attivo |
| `DBF.DBRELATION([n])` | `STRING` relazione n-esima |
| `DBF.NETERR([area])` | `LONG` ultimo errore di rete/replicazione |

### Lock di record/file (`RLOCK` / `FLOCK`)

IF DBF.RLOCK() THEN PRINT "record bloccato"
IF DBF.FLOCK() THEN PRINT "file bloccato"


### Replicazione (`REPLICATIONSHIP`/`REPLICATIONRECV`)

DBF.REPLICATIONSHIP(area, host$, port)   ' invia i record a un peer
DBF.REPLICATIONRECV(area, port)          ' riceve i record da un peer


DO WHILE NOT DBF.EOF()
  PRINT DBF.RECNO(), DBF.EOF()
  DBF SKIP
LOOP


## 4. Modifica dati

### `DBF.APPEND BLANK` / `DBF.APPEND RECORD var` / `DBF.APPEND FROM file$ [FOR cond]`

DBF APPEND BLANK
DIM c AS Cliente : c.Nome = "Mario" : DBF APPEND RECORD c
DBF APPEND FROM "nuovi.dbf" FOR Anno > 2023


### `DBF.DELETE [FOR cond|ALL|NEXT n|REST]` e `DBF.RECALL`

DBF DELETE FOR Anno < 2000
DBF RECALL ALL


### `DBF.REPLACE field WITH expr [, ...] [FOR cond]`

DBF REPLACE Prezzo WITH Prezzo * 1.1 FOR Scontato
DBF REPLACE NOME WITH "Mario", CITTA WITH "Roma"


### `DBF.PACK` / `DBF.ZAP` / `DBF.REINDEX`

DBF PACK
DBF ZAP
DBF REINDEX


## 5. Indici e ordinamento

### `DBF.INDEX ON expr TO file$`

DBF INDEX ON Cognome TO "by_cogn.ntx"
DBF.INDEXON("STR(Recno())", "by_recno.ntx")


### `DBF.SET ORDER TO n%|tag$`

DBF SET ORDER TO 1
DBF.SETORDER("BY_COGN")


### `DBF.SORT TO file$ ON field [, ...] [/D]`

DBF SORT TO "sorted.dbf" ON Cognome, Nome /D


## 6. Import / Export / Copia

### `DBF.IMPORT alias FROM file$ AS CSV|JSON`

### `DBF.EXPORT [alias] TO file$ AS CSV|JSON`

DBF IMPORT Clienti FROM "data.csv" AS CSV
DBF EXPORT Clienti TO "out.json" AS JSON


### `DBF.COPY TO file$` / `DBF.COPY STRUCTURE TO file$`

DBF COPY TO "backup.dbf"
DBF COPY STRUCTURE TO "template.dbf"


## 7. Comandi relazionali

### `DBF.JOIN WITH alias TO file$ FOR cond`

DBF JOIN WITH Altro TO "joined.dbf" FOR ClienteID = Altro.ID


### `DBF.UPDATE ON key FROM alias REPLACE field WITH expr`

DBF UPDATE ON Codice FROM Altro REPLACE Prezzo WITH Altro.Prezzo


### `DBF.TOTAL ON key TO file$ FIELDS field [, ...]`

DBF TOTAL ON Regione TO "tot.dbf" FIELDS Importo


## 8. Comandi SET
| Comando | Descrizione |
|---|---|
| `DBF.SET FILTER TO cond` | Filtro record (`DBF.FILTER()` legge condizione) |
| `DBF.SET DELETED ON|OFF` | Nasconde/mostra record cancellati |
| `DBF.SET EXACT ON|OFF` | Confronto esatto stringhe |
| `DBF.SET RELATION TO key INTO alias` | Relazione tra tabelle |
| `DBF.SET ORDER TO` | Già descritto |
| `DBF.SET DEFAULT TO path$` | Cartella default per file DBF |
| `DBF.SET PATH TO path$` | Search path |
| `DBF.SET SCHEMA TO ...` | Schema campi |

DBF SET FILTER TO Anno = 2024
DBF SET DELETED ON


## 9. Funzioni di utilità
| Funzione | Descrizione |
|---|---|
| `DBF.LOOKUP(alias$, key$, field$)` | Cerca in altra tabella e ritorna campo |
| `DBF.APPENDMANY(list)` | Append da `LIST OF UDT` (batch) |
| `DBF.SETDURABILITY(mode$)` / `FLUSHDURABILITY()` | `Buffered` vs `Full` + `BEGINGROUPCOMMIT`/`ENDGROUPCOMMIT` |
| `DBF.SETREPLICATION(...)` / `APPLYREPLICATION` / `FOLLOWREPLICATION` | Replica log shipping |

' Batch append da lista UDT
DIM batch AS LIST OF Cliente
' ... popola batch
PRINT DBF.APPENDMANY(batch)

' Durabilità
PRINT DBF.SETDURABILITY("Buffered")
PRINT DBF.FLUSHDURABILITY()


## 10. Transazioni

DBF BEGIN TRANSACTION
DBF APPEND BLANK
DBF REPLACE Nome WITH "Test"
IF lErr THEN DBF ROLLBACK ELSE DBF COMMIT


## 11. Esempi completi

### Esempio 1 — CRUD base

IMPORT DBF
DBF USE "clienti.dbf" ALIAS C EXCLUSIVE
DBF GO TOP
DO WHILE NOT DBF.EOF()
  DIM r AS Cliente : DBF GET r
  PRINT r.Nome, r.Citta
  DBF SKIP
LOOP
DBF CLOSE C


### Esempio 2 — Indice e seek

IMPORT DBF
DBF USE "ordini.dbf" ALIAS Ordini INDEX "by_id.ntx"
DBF SET ORDER TO 1
DBF SEEK "12345"
IF DBF.FOUND() THEN
  DIM o AS Ordine : DBF GET o
  PRINT o.Cliente
END IF


### Esempio 3 — Filtro e replace

IMPORT DBF
DBF USE "sales.dbf" ALIAS S
DBF SET FILTER TO Importo > 1000
DBF GO TOP
DO WHILE NOT DBF.EOF()
  PRINT DBF.RECNO(), S.Importo
  DBF SKIP
LOOP
DBF REPLACE Importo WITH Importo * 1.1 FOR Scontato
DBF SET FILTER TO  '  '  ' reset


### Esempio 4 — Join e total

IMPORT DBF
DBF USE "ordini.dbf" ALIAS Ordini
DBF JOIN WITH Clienti TO "joined.dbf" FOR Ordini.ClienteID = Clienti.ID
DBF USE "joined.dbf" ALIAS J
PRINT DBF.LASTREC() ' record uniti


## 12. Backend e note

## Riferimenti implementativi

### ISAM-Guide

*Fonte: `ISAM-Guide`*


## 1. Apertura e selezione

### `ISAM.OPEN file$ OF TypeName [SHARED|EXCLUSIVE] [ENCODING enc$] [ALIAS alias$] [INDEXED BY field]`

IMPORT ISAM
TYPE Cliente
  Nome AS STRING
  Eta AS LONG
END TYPE

ISAM OPEN "clienti.isdb" OF Cliente ALIAS C INDEXED BY Nome
' Moderna:
ISAM.OPEN("clienti.isdb", "Cliente", "C", "Nome")


### `ISAM.USE file$ OF TypeName ...` — sinonimo di `OPEN` (compatibilità).

### `ISAM.SELECT alias$` / `ISAM.SELECT(n%)`

ISAM SELECT C


### `ISAM.CLOSE [alias [, ...] | DATABASES]`

ISAM CLOSE C
ISAM CLOSE DATABASES


## 2. Navigazione
| Comando | Descrizione |
|---|---|
| `ISAM.GO TOP|BOTTOM|n%` | Posiziona cursore |
| `ISAM.SKIP [n%]` | Avanza/indietreggia |
| `ISAM.SEEK key$, [USING field$]` | Cerca per chiave nell'indice |

ISAM GO TOP
ISAM SKIP -5
ISAM SEEK "Rossi"
IF ISAM.FOUND(C) THEN PRINT ISAM.RECNO(C)


## 3. Lettura / Scrittura

### `ISAM.GET #n, var` / `ISAM.PUT #n, var` (legacy con `#n` opzionale)

### `ISAM.GET var` / `ISAM.PUT var` (moderna, area corrente)

DIM r AS Cliente
ISAM GET C, r      ' legacy con area
ISAM GET r         ' moderna (area corrente)
r.Eta = r.Eta + 1
ISAM PUT r


### `ISAM.APPEND [var]` / `ISAM.DELETE` / `ISAM.RECALL` / `ISAM.PACK` / `ISAM.REINDEX`

DIM n AS Cliente : n.Nome = "Mario" : n.Eta = 30
ISAM APPEND n
ISAM DELETE        ' marca cancellato
ISAM RECALL        ' ripristina
ISAM PACK          ' rimuove fisicamente
ISAM REINDEX


## 4. Indici

### `ISAM.CREATE INDEX ON field TO file$`

### `ISAM.DROP INDEX file$`

ISAM CREATE INDEX ON Nome TO "by_nome.idx"
ISAM DROP INDEX "by_nome.idx"


### `ISAM.SET ORDER TO n%|tag$`

### `ISAM.SET FILTER TO cond$`

ISAM SET ORDER TO 1
ISAM SET FILTER TO Eta > 25
PRINT ISAM.DBFILTER(C) ' filtro attivo


## 5. Funzioni di stato
| Funzione | Ritorno |
|---|---|
| `ISAM.EOF(alias)` / `ISAM.BOF(alias)` | `BOOLEAN` |
| `ISAM.FOUND(alias)` | `BOOLEAN` dopo `SEEK` |
| `ISAM.RECNO(alias)` / `ISAM.LASTREC(alias)` | `LONG` |
| `ISAM.SELECTED([alias$])` | `LONG` numero d'area 1-based |
| `ISAM.USED([alias$])` | `BOOLEAN` area aperta e utilizzabile |
| `ISAM.DELETED(alias)` | `BOOLEAN` record corrente cancellato |
| `ISAM.RECSIZE(alias)` | `LONG` dimensione record |
| `ISAM.LUPDATE(alias)` | `STRING` data ultimo update (`YYYY-MM-DD`) |
| `ISAM.DBFILTER(alias)` | `STRING` filtro |
| `ISAM.DBRELATION([alias])` | `STRING` relazione attiva |
| `ISAM.INDEXORD(alias)` / `ISAM.INDEXKEY(alias)` | `LONG` ordine / `STRING` chiave indice attivo |
| `ISAM.RLOCK(alias)` / `ISAM.FLOCK(alias)` | `BOOLEAN` lock record / file (`-1` = acquisito) |
| `ISAM.APPENDMANY(alias, records)` | `LONG` record inseriti (array di UDT) |

' Inserimento in blocco: array di record (NON il letterale annidato {{..}}).
DIM batch(0) AS Item
DIM r AS Item : r.ID = "R003" : r.Name = "Gamma" : batch(0) = r
PRINT ISAM.APPENDMANY(C, batch)          ' → 1


### Replicazione (`REPLICATIONSHIP` / `REPLICATIONRECV`)

ISAM.REPLICATIONSHIP(area, host$, port)   ' invia i record a un peer (TCP)
ISAM.REPLICATIONRECV(area, port)          ' riceve i record da un peer (TCP)
ISAM.SETREPLICATION(area, log$)           ' imposta il log di replica
ISAM.APPLYREPLICATION(area, log$)         ' applica il log
ISAM.FOLLOWREPLICATION(area, log$)        ' segue il log (tail)


DO WHILE NOT ISAM.EOF(C)
  DIM r AS Cliente : ISAM GET r
  PRINT r.Nome
  ISAM SKIP
LOOP


## 6. Transazioni e lock

ISAM BEGIN TRANSACTION
ISAM APPEND n
IF lErr THEN ISAM ROLLBACK ELSE ISAM COMMIT

ISAM LOCK C
' ... modifica ...
ISAM UNLOCK C


## 7. Import / Export / Conversione
| Comando | Descrizione |
|---|---|
| `ISAM.IMPORT alias FROM file$ AS CSV|JSON` | Importa da CSV/JSON |
| `ISAM.EXPORT alias TO file$ AS CSV|JSON` | Esporta |
| `ISAM.CONVERT alias TO file$` | Converte formato |
| `ISAM.BACKUP TO file$` | Backup |

ISAM IMPORT C FROM "data.csv" AS CSV
ISAM EXPORT C TO "out.json" AS JSON


## 8. JOIN / SORT / TOTAL (come DBF)

ISAM JOIN WITH Altro TO "joined.isdb" FOR C.ID = Altro.ID
ISAM SORT TO "sorted.isdb" ON Nome
ISAM TOTAL ON Regione TO "tot.isdb" FIELDS Importo


## 9. Funzioni di utilità
| Funzione | Descrizione |
|---|---|
| `ISAM.REPAIR(alias)` | Ripara tabella corrotta → `JSON` diagnostico |
| `ISAM.INTEGRITY(alias)` | Verifica integrità → `JSON` |
| `ISAM.INFO(alias)` | Info tabella (indici, count) → `JSON` |
| `ISAM.SETREPLICATION(alias, path$)` / `APPLYREPLICATION` | Replica log shipping |

DIM j AS JSON = ISAM.INTEGRITY(C)
PRINT ISAM.INFO(C)


## 10. Esempi completi

### Esempio 1 — CRUD

IMPORT ISAM
TYPE Cliente : Nome AS STRING : Eta AS LONG : END TYPE
ISAM OPEN "clienti.isdb" OF Cliente ALIAS C INDEXED BY Nome
DIM n AS Cliente : n.Nome = "Anna" : n.Eta = 28
ISAM APPEND n
ISAM GO TOP
DO WHILE NOT ISAM.EOF(C)
  DIM r AS Cliente : ISAM GET r
  PRINT r.Nome, r.Eta
  ISAM SKIP
LOOP
ISAM CLOSE C


### Esempio 2 — Indice e seek

IMPORT ISAM
ISAM OPEN "ordini.isdb" OF Ordine ALIAS O INDEXED BY ID
ISAM CREATE INDEX ON ID TO "by_id.idx"
ISAM SET ORDER TO 1
ISAM SEEK "12345"
IF ISAM.FOUND(O) THEN
  DIM o AS Ordine : ISAM GET o
  PRINT o.Cliente
END IF


### Esempio 3 — Replica

IMPORT ISAM
ISAM OPEN "primary.isdb" OF Log ALIAS P
PRINT ISAM.SETREPLICATION(P, ".")
ISAM APPEND n
PRINT ISAM.APPLYREPLICATION(S, "isam_repl.isdb.repl")


## 11. Backend e note

## Riferimenti

<!-- END SURFACE -->







<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **DBF.ALIAS()** — `PRINT "corrente dopo USE: "; DBF.ALIAS(); " area="; DBF.SELECTED()` (shared/samples/Databases/SelectAreas.bas:42)
- **DBF.APPENDMANY(x1)** — `130 n = DBF.APPENDMANY(Bulk, batch)` (shared/samples/Databases/DbDbfAppendMany.bas:26)
- **DBF.APPENDMANY(x1, x2)** — `130 n = DBF.APPENDMANY(Bulk, batch)` (shared/samples/Databases/DbDbfAppendMany.bas:26)
- **DBF.APPENDMANY(x1, x2)** — `130 n = DBF.APPENDMANY(Bulk, batch)` (shared/samples/Databases/DbDbfAppendMany.bas:26)
- **DBF.APPLYREPLICATION(x1)** — `230 applied = DBF.APPLYREPLICATION(Secondary, g_work + "/dbf_repl.dbf.repl")` (shared/samples/Databases/DbfReplication.bas:38)
- **DBF.APPLYREPLICATION(x1, x2)** — `230 applied = DBF.APPLYREPLICATION(Secondary, g_work + "/dbf_repl.dbf.repl")` (shared/samples/Databases/DbfReplication.bas:38)
- **DBF.BEGINGROUPCOMMIT()** — `PRINT DBF.BEGINGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:19)
- **DBF.BEGINGROUPCOMMIT()** — `PRINT DBF.BEGINGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:19)
- **DBF.BEGINGROUPCOMMIT()** — `PRINT DBF.BEGINGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:19)
- **DBF.BOF()** — `1790 PRINT "  BOF: " + CONVERSIONS.CSTR(DBF.BOF())` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:260)
- **DBF.DBFILTER()** — `PRINT "filtro vuoto: "; (LEN(DBF.DBFILTER("C")) = 0)` (shared/samples/Databases/AnagraficaClienti.bas:36)
- **DBF.DBRELATION()** — `1870 PRINT "  DBRELATION: " + DBF.DBRELATION()` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:268)
- **DBF.DELETED()** — `1430    PRINT "  ID: " + STR(clienteRec.ID) + ", Nome: " + clienteRec.Nome + " (Cancellato: " + CONV…` (shared/samples/Clipper/ClipperDbfFullTests.bas:230)
- **DBF.ENDGROUPCOMMIT()** — `PRINT DBF.ENDGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:23)
- **DBF.ENDGROUPCOMMIT()** — `PRINT DBF.ENDGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:23)
- **DBF.ENDGROUPCOMMIT()** — `PRINT DBF.ENDGROUPCOMMIT()` (shared/samples/Databases/DbDbfGroupCommit.bas:23)
- **DBF.EOF()** — `DO WHILE NOT DBF.EOF()` (shared/samples/Databases/RDD_ReplaceRealData.bas:44)
- **DBF.FIELDNAME(x1)** — `1840 PRINT "  FIELDNAME(1): " + DBF.FIELDNAME(1)` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:265)
- **DBF.FLOCK()** — `1880 PRINT "  RLOCK/FLOCK/NETERR: " + CONVERSIONS.CSTR(DBF.RLOCK()) + "," + CONVERSIONS.CSTR(DBF.FLO…` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:269)
- **DBF.FLUSHDURABILITY()** — `PRINT DBF.FLUSHDURABILITY()` (shared/samples/Databases/DbDbfBuffered.bas:21)
- **DBF.FLUSHDURABILITY()** — `PRINT DBF.FLUSHDURABILITY()` (shared/samples/Databases/DbDbfBuffered.bas:21)
- **DBF.FLUSHDURABILITY()** — `PRINT DBF.FLUSHDURABILITY()` (shared/samples/Databases/DbDbfBuffered.bas:21)
- **DBF.FOLLOWREPLICATION()** — `290 followed = DBF.FOLLOWREPLICATION(g_work + "/dbf_repl.dbf.repl")` (shared/samples/Databases/DbfReplication.bas:45)
- **DBF.FOLLOWREPLICATION(x1)** — `290 followed = DBF.FOLLOWREPLICATION(g_work + "/dbf_repl.dbf.repl")` (shared/samples/Databases/DbfReplication.bas:45)
- **DBF.FOUND()** — `210 PRINT "CDX tag PRIMARY SEEK recno=5 ->"; nRec; " found="; DBF.FOUND()` (shared/samples/Databases/DbfIndexCompound.bas:32)
- **DBF.INDEXKEY()** — `1860 PRINT "  INDEXKEY(0): " + DBF.INDEXKEY(0)` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:267)
- **DBF.INDEXORD()** — `1850 PRINT "  INDEXORD: " + STR(DBF.INDEXORD())` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:266)
- **DBF.LASTREC()** — `180 nBefore = DBF.LASTREC()` (shared/samples/Databases/DbfMemoCrud.bas:32)
- **DBF.LOOKUP(x1, x2, x3, x4)** — `PRINT "citta di Bob: "; TRIM(DBF.LOOKUP("Citta", "Bob", "Nome", "C"))` (shared/samples/Databases/AnagraficaClienti.bas:37)
- **DBF.LUPDATE()** — `1820 PRINT "  LUPDATE: " + CONVERSIONS.CSTR(DBF.LUPDATE())` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:263)
- **DBF.NETERR()** — `1880 PRINT "  RLOCK/FLOCK/NETERR: " + CONVERSIONS.CSTR(DBF.RLOCK()) + "," + CONVERSIONS.CSTR(DBF.FLO…` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:269)
- **DBF.RECNO()** — `240 PRINT "MDX BY_SHAPE first recno="; DBF.RECNO()` (shared/samples/Databases/DbfIndexCompoundWrite.bas:33)
- **DBF.RECSIZE()** — `1830 PRINT "  RECSIZE: " + STR(DBF.RECSIZE())` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:264)
- **DBF.REPLICATIONRECV(x1)** — `330 PRINT "recv porta 0 (fuori range): "; DBF.REPLICATIONRECV(0)` (shared/samples/Databases/DbfReplication.bas:57)
- **DBF.REPLICATIONRECV(x1, x2)** — `330 PRINT "recv porta 0 (fuori range): "; DBF.REPLICATIONRECV(0)` (shared/samples/Databases/DbfReplication.bas:57)
- **DBF.REPLICATIONSHIP(x1, x2)** — `310 PRINT "ship porta 0 (fuori range): "; DBF.REPLICATIONSHIP("127.0.0.1", 0)` (shared/samples/Databases/DbfReplication.bas:55)
- **DBF.REPLICATIONSHIP(x1, x2, x3)** — `310 PRINT "ship porta 0 (fuori range): "; DBF.REPLICATIONSHIP("127.0.0.1", 0)` (shared/samples/Databases/DbfReplication.bas:55)
- **DBF.RLOCK()** — `1880 PRINT "  RLOCK/FLOCK/NETERR: " + CONVERSIONS.CSTR(DBF.RLOCK()) + "," + CONVERSIONS.CSTR(DBF.FLO…` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:269)
- **DBF.SELECTED()** — `PRINT "aree aperte: clienti="; DBF.SELECTED("Clienti"); " ordini="; DBF.SELECTED("Ordini")` (shared/samples/Databases/SelectAreas.bas:41)
- **DBF.SETDURABILITY(x1)** — `PRINT DBF.SETDURABILITY("Full")` (shared/samples/Databases/DbDbfGroupCommit.bas:18)
- **DBF.SETDURABILITY(x1, x2)** — `PRINT DBF.SETDURABILITY("Full")` (shared/samples/Databases/DbDbfGroupCommit.bas:18)
- **DBF.SETDURABILITY(x1, x2)** — `PRINT DBF.SETDURABILITY("Full")` (shared/samples/Databases/DbDbfGroupCommit.bas:18)
- **DBF.SETREPLICATION(x1)** — `110 logPath = DBF.SETREPLICATION(Primary, ".")` (shared/samples/Databases/DbfReplication.bas:25)
- **DBF.SETREPLICATION(x1, x2)** — `110 logPath = DBF.SETREPLICATION(Primary, ".")` (shared/samples/Databases/DbfReplication.bas:25)
- **DBF.SETREPLICATION(x1, x2)** — `110 logPath = DBF.SETREPLICATION(Primary, ".")` (shared/samples/Databases/DbfReplication.bas:25)
- **DBF.USED()** — `1800 PRINT "  USED: " + CONVERSIONS.CSTR(DBF.USED())` (shared/samples/Clipper/ClipperDbfLegacySyntax.bas:261)
- **ISAM.APPENDMANY(x1)** — `PRINT "prodotti inseriti: "; ISAM.APPENDMANY("M", nuovi)` (shared/samples/Databases/MagazzinoIsam.bas:43)
- **ISAM.APPENDMANY(x1, x2)** — `PRINT "prodotti inseriti: "; ISAM.APPENDMANY("M", nuovi)` (shared/samples/Databases/MagazzinoIsam.bas:43)
- **ISAM.APPENDMANY(x1, x2)** — `PRINT "prodotti inseriti: "; ISAM.APPENDMANY("M", nuovi)` (shared/samples/Databases/MagazzinoIsam.bas:43)
- **ISAM.APPLYREPLICATION(x1)** — `270 applied = ISAM.APPLYREPLICATION(Secondary, "isam_repl.isdb.repl")` (shared/samples/Databases/IsamReplication.bas:41)
- **ISAM.APPLYREPLICATION(x1, x2)** — `270 applied = ISAM.APPLYREPLICATION(Secondary, "isam_repl.isdb.repl")` (shared/samples/Databases/IsamReplication.bas:41)
- **ISAM.APPLYREPLICATION(x1, x2)** — `270 applied = ISAM.APPLYREPLICATION(Secondary, "isam_repl.isdb.repl")` (shared/samples/Databases/IsamReplication.bas:41)
- **ISAM.BOF()** — `300 WHILE NOT ISAM.BOF(Clienti)` (shared/samples/Databases/DbIsamNavAndModify.bas:42)
- **ISAM.DBFILTER()** — `PRINT "filtro attivo: ["; ISAM.DBFILTER("M"); "]"` (shared/samples/Databases/MagazzinoIsam.bas:62)
- **ISAM.DBFILTER()** — `PRINT "filtro attivo: ["; ISAM.DBFILTER("M"); "]"` (shared/samples/Databases/MagazzinoIsam.bas:62)
- **ISAM.DBRELATION()** — `PRINT "relation_empty="; (LEN(ISAM.DBRELATION()) = 0)` (shared/minitests/isam/isam_all.bas:41)
- **ISAM.DBRELATION()** — `PRINT "relation_empty="; (LEN(ISAM.DBRELATION()) = 0)` (shared/minitests/isam/isam_all.bas:41)
- **ISAM.DELETED()** — `PRINT "deleted="; ISAM.DELETED()` (shared/minitests/isam/isam_all.bas:42)
- **ISAM.DELETED()** — `PRINT "deleted="; ISAM.DELETED()` (shared/minitests/isam/isam_all.bas:42)
- **ISAM.EOF()** — `180 WHILE NOT ISAM.EOF(Scored)` (shared/samples/Databases/QueryPhaseD.bas:28)
- **ISAM.FLOCK()** — `bResult = ISAM.FLOCK(Clienti)` (shared/samples/Databases/DbIsamFullTests.bas:352)
- **ISAM.FOLLOWREPLICATION()** — `PRINT "follow>=-1="; (ISAM.FOLLOWREPLICATION(lp) >= -1)` (shared/minitests/isam/isam_all.bas:78)
- **ISAM.FOLLOWREPLICATION(x1)** — `PRINT "follow>=-1="; (ISAM.FOLLOWREPLICATION(lp) >= -1)` (shared/minitests/isam/isam_all.bas:78)
- **ISAM.FOUND()** — `IF ISAM.FOUND(X) THEN` (shared/samples/Databases/ISAM_Minimal_Test.bas:47)
- **ISAM.INDEXKEY()** — `PRINT "indexkey_len>=0="; (LEN(ISAM.INDEXKEY()) >= 0)` (shared/minitests/isam/isam_all.bas:38)
- **ISAM.INDEXKEY()** — `PRINT "indexkey_len>=0="; (LEN(ISAM.INDEXKEY()) >= 0)` (shared/minitests/isam/isam_all.bas:38)
- **ISAM.INDEXORD()** — `PRINT "ordine di indice: "; ISAM.INDEXORD()` (shared/samples/Databases/MagazzinoIsam.bas:33)
- **ISAM.INDEXORD()** — `PRINT "ordine di indice: "; ISAM.INDEXORD()` (shared/samples/Databases/MagazzinoIsam.bas:33)
- **ISAM.INFO()** — `240 DIM infoJson AS JSON = ISAM.INFO(Scored)` (shared/samples/Databases/QueryPhaseD.bas:38)
- **ISAM.INFO()** — `240 DIM infoJson AS JSON = ISAM.INFO(Scored)` (shared/samples/Databases/QueryPhaseD.bas:38)
- **ISAM.INTEGRITY()** — `232 DIM checkText AS STRING = ISAM.INTEGRITY(Scored)` (shared/samples/Databases/QueryPhaseD.bas:36)
- **ISAM.INTEGRITY()** — `232 DIM checkText AS STRING = ISAM.INTEGRITY(Scored)` (shared/samples/Databases/QueryPhaseD.bas:36)
- **ISAM.LASTREC()** — `380 PRINT "Numero record dopo PACK:"; ISAM.LASTREC(Clienti)` (shared/samples/Databases/DbIsamNavAndModify.bas:51)
- **ISAM.LOG()** — `lg = ISAM.LOG()` (shared/minitests/isam/isam_all.bas:52)
- **ISAM.LOG()** — `lg = ISAM.LOG()` (shared/minitests/isam/isam_all.bas:52)
- **ISAM.RECNO()** — `PRINT "RECNO() su area attiva: " + CONVERSIONS.CSTR(ISAM.RECNO())` (shared/samples/Databases/DbIsamFullTests.bas:119)
- **ISAM.RECSIZE()** — `PRINT "RECSIZE(Ordini): " + CONVERSIONS.CSTR(ISAM.RECSIZE(Ordini))` (shared/samples/Databases/DbIsamFullTests.bas:123)
- **ISAM.REPLICATIONRECV(x1)** — `PRINT "recv porta 0 (fuori range): "; ISAM.REPLICATIONRECV(0)` (shared/samples/Databases/IsamReplicationTcp.bas:39)
- **ISAM.REPLICATIONRECV(x1, x2)** — `PRINT "recv porta 0 (fuori range): "; ISAM.REPLICATIONRECV(0)` (shared/samples/Databases/IsamReplicationTcp.bas:39)
- **ISAM.REPLICATIONSHIP(x1, x2)** — `PRINT "ship porta 0 (fuori range): "; ISAM.REPLICATIONSHIP("127.0.0.1", 0)` (shared/samples/Databases/IsamReplicationTcp.bas:37)
- **ISAM.REPLICATIONSHIP(x1, x2, x3)** — `PRINT "ship porta 0 (fuori range): "; ISAM.REPLICATIONSHIP("127.0.0.1", 0)` (shared/samples/Databases/IsamReplicationTcp.bas:37)
- **ISAM.RLOCK()** — `bResult = ISAM.RLOCK(Clienti)` (shared/samples/Databases/DbIsamFullTests.bas:342)
- **ISAM.SELECTED()** — `PRINT "Work Area corrente (Clienti): " + CONVERSIONS.CSTR(ISAM.SELECTED(Clienti))` (shared/samples/Databases/DbIsamFullTests.bas:113)
- **ISAM.SETREPLICATION(x1)** — `110 logPath = ISAM.SETREPLICATION(Primary, ".")` (shared/samples/Databases/IsamReplication.bas:23)
- **ISAM.SETREPLICATION(x1, x2)** — `110 logPath = ISAM.SETREPLICATION(Primary, ".")` (shared/samples/Databases/IsamReplication.bas:23)
- **ISAM.SETREPLICATION(x1, x2)** — `110 logPath = ISAM.SETREPLICATION(Primary, ".")` (shared/samples/Databases/IsamReplication.bas:23)
- **ISAM.USED()** — `PRINT "in uso: "; ISAM.USED("M")` (shared/samples/Databases/MagazzinoIsam.bas:32)
- **LEDGER.ACCOUNTS(x1)** — `DIM conti() = LEDGER.ACCOUNTS(h)` (shared/samples/Databases/ContabilitaPasticceria.bas:78)
- **LEDGER.ADD(x1, x2, x3, x4)** — `100 R.Name = "Alpha" : batch.Add(R)` (shared/samples/Databases/DbDbfAppendMany.bas:22)
- **LEDGER.BALANCE(x1)** — `PRINT "balance: "; LEN(LEDGER.BALANCE(h))` (shared/samples/Databases/LedgerEditDemo.bas:33)
- **LEDGER.BOF(x1)** — `PRINT "bof: "; LEDGER.BOF(h)` (shared/minitests/ledger/ledger_cursor_detail.bas:37)
- **LEDGER.COUNT(x1)** — `PRINT "count dopo ADD: "; LEDGER.COUNT(h)` (shared/samples/Databases/LedgerEditDemo.bas:26)
- **LEDGER.EDIT(x1, x2, x3, x4, x5)** — `LEDGER EDIT hLedger ID txId2 WITH DATETIME.DATEADD("D", 6, dDate), "Acquisto Libri Aggiornato" {` (shared/samples/Databases/DbLedgerModernTests.bas:3591)
- **LEDGER.EOF(x1)** — `PRINT "eof: "; LEDGER.EOF(h)` (shared/minitests/ledger/ledger_cursor_detail.bas:38)
- **LEDGER.FOUND(x1)** — `PRINT "found: "; LEDGER.FOUND(h)` (shared/minitests/ledger/ledger_ops.bas:63)
- **LEDGER.GET(x1)** — `PRINT "righe dell'ultima transazione: "; LEN(LEDGER.GET(h, LEDGER.LASTREC(h)))` (shared/samples/Databases/ContabilitaPasticceria.bas:86)
- **LEDGER.IMPORT(x1, x2, x3)** — `PRINT "esito import (0 = ok): "; LEDGER.IMPORT(h, "work/banca.csv", "date,desc,amount")` (shared/samples/Databases/ContabilitaPasticceria.bas:91)
- **LEDGER.LASTREC(x1)** — `PRINT "ultimo numero: "; LEDGER.LASTREC(h)` (shared/samples/Databases/ContabilitaPasticceria.bas:56)
- **LEDGER.OPEN(x1)** — `hLedger = LEDGER.OPEN("contabilita.dat")` (shared/samples/Databases/DbLedgerModernTests.bas:104)
- **LEDGER.PLOT(x1, x2, x3)** — `PRINT "grafico creato: "; (LEDGER.PLOT(h, "", "bar") > 0)` (shared/samples/Databases/ContabilitaPasticceria.bas:97)
- **LEDGER.RECNO(x1)** — `PRINT "recno: "; LEDGER.RECNO(h)` (shared/minitests/ledger/ledger_cursor_detail.bas:36)
- **LEDGER.REGISTER(x1)** — `PRINT "movimenti in registro: "; LEN(LEDGER.REGISTER(h))` (shared/samples/Databases/ContabilitaPasticceria.bas:84)
- **LEDGER.VALIDATE(x1)** — `PRINT "libro bilanciato: "; LEDGER.VALIDATE(h)` (shared/samples/Databases/ContabilitaPasticceria.bas:60)
- **LEDGER.VARIANCE(x1, x2, x3)** — `scostamento = LEDGER.VARIANCE(h, "Spese:Farina", "2026-01")` (shared/samples/Databases/ContabilitaPasticceria.bas:67)
- **RDD.BOF()** — `PRINT "bof: "; RDD.BOF()` (shared/samples/Databases/RddCompleteCoverage.bas:34)
- **RDD.BUFFER(x1, x2)** — `ok = RDD.BUFFER(1, 3)` (shared/samples/Databases/RDD_Memory_Demo.bas:21)
- **RDD.CTOD(x1)** — `DIM d = RDD.CTOD("2026-01-15")` (shared/samples/Databases/RddCompleteCoverage.bas:47)
- **RDD.CURVAL(x1)** — `c = RDD.CURVAL(999.99)` (shared/samples/Databases/RDD_SQL_CRUD.bas:75)
- **RDD.DELETE(x1, x2)** — `PRINT "delete: "; RDD.DELETE("CURRENT", TRUE)` (shared/samples/Databases/RddCompleteCoverage.bas:56)
- **RDD.DTOC(x1)** — `PRINT "ctod->dtoc: ["; RDD.DTOC(d); "]"` (shared/samples/Databases/RddCompleteCoverage.bas:48)
- **RDD.EOF()** — `PRINT "eof: "; RDD.EOF()` (shared/samples/Databases/RddCompleteCoverage.bas:35)
- **RDD.FLOAT(x1)** — `PRINT "float: "; RDD.FLOAT(12.5)` (shared/samples/Databases/RddCompleteCoverage.bas:50)
- **RDD.FLOCK(x1)** — `ok = RDD.FLOCK(1)` (shared/samples/Databases/RDD_SQL_CRUD.bas:35)
- **RDD.FOUND()** — `PRINT "found: "; RDD.FOUND()` (shared/samples/Databases/RddCompleteCoverage.bas:36)
- **RDD.GETFIELD(x1, x2)** — `PRINT "   GETFIELD(1,1)="; RDD.GETFIELD(1, 1)` (shared/samples/Databases/RDD_SQL_CRUD.bas:28)
- **RDD.GETFLDSTATE(x1, x2)** — `PRINT "state-mod: "; RDD.GETFLDSTATE(1, 3)` (shared/samples/Databases/RDD_Buffering_Complete.bas:51)
- **RDD.OLDVAL(x1, x2)** — `PRINT "old-num: "; RDD.OLDVAL(1, 3)` (shared/samples/Databases/RDD_Buffering_Complete.bas:49)
- **RDD.RECALL(x1, x2)** — `PRINT "recall: "; RDD.RECALL("CURRENT", TRUE)` (shared/samples/Databases/RddCompleteCoverage.bas:57)
- **RDD.RECNO()** — `PRINT "recno: "; RDD.RECNO()` (shared/samples/Databases/RddCompleteCoverage.bas:37)
- **RDD.REPLACE(x1, x2, x3, x4)** — `PRINT "replace: "; RDD.REPLACE("CURRENT", "NOME", "X", TRUE)` (shared/samples/Databases/RddCompleteCoverage.bas:55)
- **RDD.RLOCK(x1)** — `ok = RDD.RLOCK(1)` (shared/samples/Databases/RDD_SQL_CRUD.bas:38)
- **RDD.SETDELETED(x1, x2)** — `IF RDD.SETDELETED(1, 0) THEN PRINT "setdeleted: si" ELSE PRINT "setdeleted: no"` (shared/samples/Databases/RddCompleteCoverage.bas:62)
- **RDD.TABLEREVERT(x1)** — `RDD.TABLEREVERT(1)` (shared/samples/Databases/RDD_Memory_Demo.bas:23)
- **RDD.TABLEUPDATE(x1)** — `PRINT "update: "; RDD.TABLEUPDATE(1)` (shared/samples/Databases/RDD_Buffering_Complete.bas:62)
- **RDD.TODAY()** — `d = RDD.TODAY()` (shared/samples/Databases/RDD_SQL_CRUD.bas:73)
- **RDD.VALTYPE(x1)** — `PRINT "valtype: "; RDD.VALTYPE(1)` (shared/samples/Databases/RDD_Buffering_Complete.bas:41)
- **SPEED.BEGINGROUPCOMMIT()** — `IF SPEED.BEGINGROUPCOMMIT() THEN PRINT "begin: si"` (shared/minitests/speed/gaps.bas:23)
- **SPEED.BEGINGROUPCOMMIT()** — `IF SPEED.BEGINGROUPCOMMIT() THEN PRINT "begin: si"` (shared/minitests/speed/gaps.bas:23)
- **SPEED.ENDGROUPCOMMIT()** — `IF SPEED.ENDGROUPCOMMIT() THEN PRINT "end: si"` (shared/minitests/speed/gaps.bas:24)
- **SPEED.ENDGROUPCOMMIT()** — `IF SPEED.ENDGROUPCOMMIT() THEN PRINT "end: si"` (shared/minitests/speed/gaps.bas:24)
- **SPEED.EOF()** — `// SPEED.EOF: vero oltre l'ultimo record` (shared/samples/Databases/DbSpeedFullTests.bas:308)
- **SPEED.FLUSHDURABILITY()** — `IF SPEED.FLUSHDURABILITY() THEN PRINT "flush: si"` (shared/minitests/speed/gaps.bas:22)
- **SPEED.FLUSHDURABILITY()** — `IF SPEED.FLUSHDURABILITY() THEN PRINT "flush: si"` (shared/minitests/speed/gaps.bas:22)
- **SPEED.INFO()** — `IF LEN(SPEED.INFO()) > 0 THEN PRINT "info: si"` (shared/minitests/speed/gaps.bas:25)
- **SPEED.INFO(x1)** — `IF LEN(SPEED.INFO()) > 0 THEN PRINT "info: si"` (shared/minitests/speed/gaps.bas:25)
- **SPEED.SELECTED()** — `PRINT "sel="; SPEED.SELECTED()` (shared/minitests/speed/speed_all.bas:47)
- **SPEED.SELECTED()** — `PRINT "sel="; SPEED.SELECTED()` (shared/minitests/speed/speed_all.bas:47)
- **SPEED.SETDURABILITY(x1)** — `IF UPPER(SPEED.SETDURABILITY("FULL")) = "FULL" THEN PRINT "setdur: si"` (shared/minitests/speed/gaps.bas:21)
- **SPEED.SETDURABILITY(x1, x2)** — `IF UPPER(SPEED.SETDURABILITY("FULL")) = "FULL" THEN PRINT "setdur: si"` (shared/minitests/speed/gaps.bas:21)
- **SPEED.SETMANY(x1)** — `150 n = SPEED.SETMANY(Bulk, batch)` (shared/samples/Databases/DbSpeedSetMany.bas:28)
- **SPEED.SETMANY(x1, x2)** — `150 n = SPEED.SETMANY(Bulk, batch)` (shared/samples/Databases/DbSpeedSetMany.bas:28)
- **SPEED.VALIDATE()** — `PRINT "pre="; SPEED.VALIDATE()` (shared/minitests/speed/validate.bas:23)
- **SPEED.VALIDATE(x1)** — `PRINT "pre="; SPEED.VALIDATE()` (shared/minitests/speed/validate.bas:23)
- **SPEED.VALIDATE(x1)** — `PRINT "pre="; SPEED.VALIDATE()` (shared/minitests/speed/validate.bas:23)
- **SPEED.VALIDATE(x1, x2)** — `PRINT "pre="; SPEED.VALIDATE()` (shared/minitests/speed/validate.bas:23)
- **SQL.CHANGES(x1)** — `330 nChanged = SQL.CHANGES("AppDb")` (shared/samples/Databases/SqlAdvancedFeatures.bas:60)
- **SQL.COL(x1, x2)** — `DIM ids() = SQL.COL(rs, "id")` (shared/samples/Databases/SqlDataframe.bas:20)
- **SQL.DFSCHEMA(x1)** — `PRINT "schema="; SQL.DFSCHEMA(rs)` (shared/samples/Databases/SqlDataframe.bas:17)
- **SQL.EXEC(x1, x2)** — `DIM r1 = SQL.EXEC("C", "CREATE TABLE p (id INT, v INT)")` (shared/minitests/sql/exec_fn_value.bas:10)
- **SQL.FETCH(x1)** — `70 DO WHILE SQL.FETCH(rs)` (shared/samples/Databases/SqlMinimalCrud.bas:15)
- **SQL.FROMCSV(x1, x2, x3)** — `PRINT "fromcsv="; SQL.FROMCSV("D", "prezzi", "../Fixtures/csv_scan.csv")` (shared/samples/Databases/SqlDataframe.bas:11)
- **SQL.LASTINSERTROWID(x1)** — `290 lastId = SQL.LASTINSERTROWID("AppDb")` (shared/samples/Databases/SqlAdvancedFeatures.bas:56)
- **SQL.TOCSV(x1, x2)** — `PRINT "tocsv="; SQL.TOCSV(rs, "work/dataframe.csv")` (shared/samples/Databases/SqlDataframe.bas:28)
<!-- END DOMAIN TABLE -->
