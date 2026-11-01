' SQL — Sorgenti scrivibili (virtual table `csv` e `parquet`) + UDF statistiche/temporali.
' Esercita TUTTE le operazioni di scrittura implementate:
'   CSV     : CREATE VIRTUAL TABLE (name/header/delim), INSERT, UPDATE, DELETE, SELECT + UDF
'   PARQUET : CREATE VIRTUAL TABLE (name/schema, tutti i tipi), INSERT, UPDATE, DELETE, DELETE-all
'   PARQUET : colonne ANNIDATE (list/struct/map) lette/scritte via JSON — INSERT/UPDATE/DELETE
' Ogni file è riscritto su disco in modo atomico (tmp+rename): formato RFC4180 / Parquet.
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/ordini.csv") THEN KILL "work/ordini.csv"
    IF IO.FILEEXISTS("work/magazzino.parquet") THEN KILL "work/magazzino.parquet"
    IF IO.FILEEXISTS("work/eventi.parquet") THEN KILL "work/eventi.parquet"
    ON ERROR GOTO 0

    ' ---------------- CSV scrivibile (delim ',' + header) ----------------
    OPEN "work/ordini.csv" FOR OUTPUT AS #1
    PRINT #1, "id,cliente,importo"
    PRINT #1, "1,alice,100"
    PRINT #1, "2,bob,250"
    CLOSE #1
    SQL OPEN ":memory:" ALIAS S
    SQL EXEC S, "CREATE VIRTUAL TABLE ordini USING csv(name='work/ordini.csv', header=1, delim=',')"
    SQL EXEC S, "INSERT INTO ordini VALUES (3,'carol',175)"
    SQL EXEC S, "UPDATE ordini SET importo=260 WHERE cliente='bob'"
    SQL EXEC S, "DELETE FROM ordini WHERE id=1"
    DIM rs AS SQLRESULTSET
    SQL QUERY S, rs, "SELECT id, cliente, importo FROM ordini ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "csv "; rs("id"); " "; rs("cliente"); " "; rs("importo")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL QUERY S, rs, "SELECT COUNT(*) AS n, ROUND(MEDIAN(importo),2) AS med FROM ordini"
    IF SQL.FETCH(rs) THEN
        PRINT "csv n="; rs("n"); " mediana="; rs("med")
    END IF
    SQL CLOSE RESULTSET rs

    ' ---------------- Parquet scrivibile (CREATE da schema, tutti i tipi) ----------------
    SQL EXEC S, "CREATE VIRTUAL TABLE mag USING parquet(name='work/magazzino.parquet', schema='id LONG, nome TEXT, prezzo DOUBLE, sconto FLOAT, attivo BOOL, giacenza INT32')"
    SQL EXEC S, "INSERT INTO mag VALUES (1,'vite',0.5,0.1,1,1000)"
    SQL EXEC S, "INSERT INTO mag VALUES (2,'bullone',1.25,0.2,1,500)"
    SQL EXEC S, "INSERT INTO mag VALUES (3,'dado',0.75,0.15,0,250)"
    SQL EXEC S, "UPDATE mag SET prezzo=0.9 WHERE nome='dado'"
    SQL EXEC S, "DELETE FROM mag WHERE id=2"
    SQL QUERY S, rs, "SELECT id, nome, prezzo, attivo, giacenza FROM mag ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "pq  "; rs("id"); " "; rs("nome"); " "; rs("prezzo"); " "; rs("attivo"); " "; rs("giacenza")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL QUERY S, rs, "SELECT ROUND(VAR_POP(prezzo),6) AS vp, ROUND(STDDEV_POP(prezzo),6) AS sd FROM mag"
    IF SQL.FETCH(rs) THEN
        PRINT "pq  var_pop="; rs("vp"); " stddev_pop="; rs("sd")
    END IF
    SQL CLOSE RESULTSET rs

    ' ---------------- Parquet ANNIDATO scrivibile (list/struct/map via JSON) ----------------
    COPY "../Fixtures/parquet_nested.parquet" TO "work/eventi.parquet"
    DIM q AS STRING
    q = CHR(34)
    SQL EXEC S, "CREATE VIRTUAL TABLE ev USING parquet(name='work/eventi.parquet')"
    SQL EXEC S, "UPDATE ev SET tags='[" + q + "x" + q + "," + q + "y" + q + "]' WHERE id=1"
    SQL EXEC S, "INSERT INTO ev VALUES (9,'[" + q + "z" + q + "]','[42]','{" + q + "city" + q + ":" + q + "naples" + q + "," + q + "zip" + q + ":999}','{" + q + "k" + q + ":" + q + "v" + q + "}')"
    SQL EXEC S, "DELETE FROM ev WHERE id=2"
    SQL QUERY S, rs, "SELECT id, tags, prices, info, attrs FROM ev ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "nest "; rs("id"); " tags="; rs("tags"); " prices="; rs("prices"); " info="; rs("info"); " attrs="; rs("attrs")
    LOOP
    SQL CLOSE RESULTSET rs

    SQL CLOSE S
END SUB
