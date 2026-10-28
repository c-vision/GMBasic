' FILE: duckdb.inc — binding FFI DuckDB (namespace DUCKDB) per il pacchetto GMTrading 'duckdb'.
' Uso: REQUIRE "duckdb"   (oppure IMPORT DUCKDB)   ->  chiamate DUCKDB.<Membro>(...)
'
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG oppure tutti DOUBLE). Qui famiglia
' LONG: stringhe come puntatore (AS STRING), handle come LONG (puntatore), output scalare
' via BYREF LONG. OHLCV via CSV "o,h,l,c,v". Nomi membri SENZA underscore (vietato in GMBasic);
' il simbolo C (con underscore) va in ALIAS. La lib e' caricata a runtime dalla cache
' pacchetti (~/.gmb/packages/<pkg>/<ver>/lib/<platform>/).

NAMESPACE DUCKDB

' Disponibilita' (1 = lib buildata con DuckDB, 0 = no-op).
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbduckdb" ALIAS "gmbduckdb_available" () AS LONG
' Apre/crea un database. path "" o ":memory:" = in-memory. Ritorna handle LONG (>0 ok, 0 err).
PUBLIC DECLARE FUNCTION OPEN LIB "gmbduckdb" ALIAS "gmbduckdb_open" (path AS STRING) AS LONG
' Chiude il database. 0 ok.
PUBLIC DECLARE FUNCTION CLOSE LIB "gmbduckdb" ALIAS "gmbduckdb_close_i" (h AS LONG) AS LONG
' Esegue SQL senza risultato (DDL/DML/COPY ... TO 'file'). 0 ok, <>0 err.
PUBLIC DECLARE FUNCTION EXEC LIB "gmbduckdb" ALIAS "gmbduckdb_exec" (h AS LONG, sql AS STRING) AS LONG
' Query scalare: scrive in out (BYREF) la prima cella della prima riga come intero. 0 ok.
PUBLIC DECLARE FUNCTION QUERY LIB "gmbduckdb" ALIAS "gmbduckdb_query_i64" (h AS LONG, sql AS STRING, BYREF out AS LONG) AS LONG
' Legge le RIGHE della query come ARRAY JSON: [{"col":"val",...},...]. "" su errore.
PUBLIC DECLARE FUNCTION QUERYJSON LIB "gmbduckdb" ALIAS "gmbduckdb_query_json" (h AS LONG, sql AS STRING) AS STRING
' Apre un Appender sulla tabella (ingest bulk). Ritorna handle appender LONG. 0 err.
PUBLIC DECLARE FUNCTION APPENDER LIB "gmbduckdb" ALIAS "gmbduckdb_appender_open" (h AS LONG, tbl AS STRING) AS LONG
' Appende una barra OHLCV: ohlcv = "o,h,l,c,v" (es. "1.5,2.5,0.5,2.0,1000"). 0 ok.
PUBLIC DECLARE FUNCTION APPEND LIB "gmbduckdb" ALIAS "gmbduckdb_append_bar_csv" (app AS LONG, ts AS LONG, sid AS LONG, ohlcv AS STRING) AS LONG
' Flush dell'appender (rende visibili le righe). 0 ok.
PUBLIC DECLARE FUNCTION FLUSH LIB "gmbduckdb" ALIAS "gmbduckdb_appender_flush" (app AS LONG) AS LONG
' Chiude l'appender. 0 ok.
PUBLIC DECLARE FUNCTION CLOSEAPPENDER LIB "gmbduckdb" ALIAS "gmbduckdb_appender_close_i" (app AS LONG) AS LONG
' Copia l'ultimo errore nel buffer stringa fornito. 0 ok.
PUBLIC DECLARE FUNCTION LASTERR LIB "gmbduckdb" ALIAS "gmbduckdb_last_error_buf" (h AS LONG, buf AS STRING) AS LONG
