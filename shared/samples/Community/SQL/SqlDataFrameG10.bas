' FILE: SQL/SqlDataFrameG10.bas
' =================================================================================================
' G10 — DataFrame SQL: DFSCHEMA · COL · TOCSV · FROMCSV (CSV → tabella → SELECT → colonna → CSV)
' =================================================================================================
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    KILL "work/g10_prodotti.csv"

    SQL OPEN ":memory:" ALIAS st
    SQL EXEC st, "CREATE TABLE prodotti(id INTEGER, nome TEXT, categoria TEXT, prezzo INTEGER)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(1,'penna','ufficio',150)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(2,'quaderno','ufficio',250)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(3,'mouse','informatica',990)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(4,'tastiera','informatica',1200)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(5,'gomma','ufficio',75)"
    SQL EXEC st, "INSERT INTO prodotti VALUES(6,'monitor','informatica',4000)"

    DIM rs AS SQLRESULTSET
    SQL QUERY st, rs, "SELECT id, nome, categoria, prezzo FROM prodotti ORDER BY id"
    PRINT "=== SCHEMA ==="
    PRINT SQL.DFSCHEMA(rs)
    PRINT "=== COLONNA nome ==="
    DIM nomi() = SQL.COL(rs, "nome")
    PRINT nomi(0)
    PRINT nomi(1)
    PRINT nomi(2)
    PRINT nomi(3)
    PRINT nomi(4)
    PRINT nomi(5)
    PRINT "=== TOCSV ==="
    PRINT "rc="; SQL.TOCSV(rs, "work/g10_prodotti.csv")
    SQL CLOSE RESULTSET rs

    PRINT "=== FROMCSV + GROUP BY ==="
    PRINT "rc="; SQL.FROMCSV("st", "prodotti2", "work/g10_prodotti.csv")
    DIM rs2 AS SQLRESULTSET
    SQL QUERY st, rs2, "SELECT categoria, COUNT(*) AS n, SUM(prezzo) AS tot FROM prodotti2 GROUP BY categoria ORDER BY categoria"
    DO WHILE SQL.FETCH(rs2)
        PRINT rs2("categoria"); ": n="; rs2("n"); " tot="; rs2("tot")
    LOOP
    SQL CLOSE RESULTSET rs2
    SQL CLOSE st
    KILL "work/g10_prodotti.csv"
    PRINT "=== OK ==="
END SUB
