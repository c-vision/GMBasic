' ============================================================================
'  duckdb_demo.bas — uso COMPLESSO di DuckDB da GM-BASIC via package + FFI.
'  Eseguibile sulle 5 gambe: tools/parity5 samples/includes/duckdb_demo.bas
' ============================================================================
'
'  REQUIRE "duckdb"   +   IMPORT DUCKDB WITH ALIAS   (perche' servono)
'  ---------------------------------------------------------------------------
'  REQUIRE attiva un PACCHETTO del package manager di GM-BASIC. Per "duckdb":
'    - risolve/installa il pacchetto (se manca lo scarica dal registry in ~/.gmb/packages);
'    - include automaticamente l'include SQL/FFI del pacchetto (duckdb.inc), che dichiara
'      le funzioni native DUCKDB_* (DECLARE ... LIB "gmbduckdb");
'    - fornisce la libreria nativa (libgmbduckdb, shim self-contained su DuckDB) e i
'      flag di link (-I/-L/-l) risolti dal manifest (ffi.headers/ffi.libs).
'  Senza REQUIRE bisognerebbe scrivere a mano le DECLARE e linkare la libreria.
' ---------------------------------------------------------------------------

REQUIRE "duckdb"
IMPORT DUCKDB WITH ALIAS     ' namespace: consente DUCKDB.<Membro>(...)

' ----------------------------------------------------------------------------
'  1) Apertura database in-memory ("": in-memory; altrimenti path di un file .duckdb)
'     DUCKDB.OPEN -> handle LONG; 0 = errore.
' ----------------------------------------------------------------------------
DIM h AS LONG
h = DUCKDB.OPEN("")
IF h = 0 THEN
  PRINT "ERRORE: apertura DuckDB fallita"
  END
END IF

' ----------------------------------------------------------------------------
'  2) DDL: schema della tabella di barre OHLCV.
'     DUCKDB.EXEC -> 0 ok, <>0 errore.
' ----------------------------------------------------------------------------
DIM rc AS LONG
rc = DUCKDB.EXEC(h, "CREATE TABLE bars(ts BIGINT, sid INTEGER, o DOUBLE, h DOUBLE, l DOUBLE, c DOUBLE, v DOUBLE)")

' ----------------------------------------------------------------------------
'  3) INGEST BULK via Appender (molto piu' veloce di INSERT ripetuti).
'     3 simboli x 3000 barre sintetiche. OHLCV passati come CSV "o,h,l,c,v".
' ----------------------------------------------------------------------------
DIM app AS LONG
app = DUCKDB.APPENDER(h, "bars")

DIM i AS LONG
DIM ts AS LONG
DIM px AS DOUBLE
DIM lowpx AS DOUBLE
DIM highpx AS DOUBLE
DIM ohlcv AS STRING

FOR i = 1 TO 3000
  ts = 1700000000000 + i * 60000
  px = 100 + (i MOD 50)
  highpx = px + 0.5
  lowpx = px - 0.5
  ohlcv = LTRIM(STR(px)) + "," + LTRIM(STR(highpx)) + "," + LTRIM(STR(lowpx)) + "," + LTRIM(STR(px)) + ",1000"
  rc = DUCKDB.APPEND(app, ts, (i MOD 3), ohlcv)
NEXT i
rc = DUCKDB.FLUSH(app)
rc = DUCKDB.CLOSEAPPENDER(app)

' ----------------------------------------------------------------------------
'  4) Query scalare (intero): conteggio barre.
'     DUCKDB.QUERY(h, sql, BYREF out) -> out riceve la prima cella come LONG.
' ----------------------------------------------------------------------------
DIM n AS LONG
IF DUCKDB.QUERY(h, "SELECT count(*) FROM bars", n) = 0 THEN
  PRINT "barre totali = "; n
END IF

' ----------------------------------------------------------------------------
'  5) Aggregazione (OLAP): candele 5 minuti per simbolo, materializzate in tabella.
' ----------------------------------------------------------------------------
rc = DUCKDB.EXEC(h, "CREATE TABLE candles AS SELECT sid, ts//300000*300000 AS bucket, arg_min(o,ts) AS o, max(h) AS h, min(l) AS l, arg_max(c,ts) AS c, sum(v) AS v FROM bars GROUP BY sid, bucket")

IF DUCKDB.QUERY(h, "SELECT count(*) FROM candles", n) = 0 THEN
  PRINT "candele 5m = "; n
END IF

' ----------------------------------------------------------------------------
'  6) Range query su un simbolo/finestra.
' ----------------------------------------------------------------------------
IF DUCKDB.QUERY(h, "SELECT count(*) FROM bars WHERE sid=1 AND ts BETWEEN 1700000000000 AND 1700060000000", n) = 0 THEN
  PRINT "range sid=1 = "; n
END IF

' ----------------------------------------------------------------------------
'  7) Lettura di RIGHE (non solo scalari): DUCKDB.QUERYJSON ritorna le righe come JSON.
'     Utile per elaborare un set di risultati nel linguaggio (loop su una tabella).
' ----------------------------------------------------------------------------
DIM righe AS STRING
righe = DUCKDB.QUERYJSON(h, "SELECT sid, bucket, c FROM candles ORDER BY sid, bucket")
PRINT "candele (righe JSON) = "; righe

' ----------------------------------------------------------------------------
'  7b) Export CSV (built-in, niente estensioni).
' ----------------------------------------------------------------------------
rc = DUCKDB.EXEC(h, "COPY candles TO '/tmp/duckdb_demo_candles.csv' (HEADER)")

' ----------------------------------------------------------------------------
'  8) Chiusura.
' ----------------------------------------------------------------------------
rc = DUCKDB.CLOSE(h)
PRINT "duckdb_demo: completato"
