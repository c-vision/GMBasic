' FILE: SQL/SqlExec.bas
' =================================================================================================
' SQL EXEC — esecuzione diretta di statement SQL su database SQLite
' =================================================================================================
IMPORT SQL

SUB Main()
    CLS
    PRINT "=== SQL EXEC ==="

    ' Apri database in memoria ('store', non 'db': DB e' una keyword del linguaggio)
    SQL OPEN ":memory:" ALIAS store

    ' Crea tabella ed inserisci dati
    SQL EXEC store, "CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, name TEXT, price REAL)"
    SQL EXEC store, "INSERT INTO items (name, price) VALUES ('prodotto1', 19.99)"
    SQL EXEC store, "INSERT INTO items (name, price) VALUES ('prodotto2', 29.99)"

    ' Query di verifica (result set: FETCH avanza, rs("col") legge la riga corrente)
    DIM rs AS SQLRESULTSET
    SQL QUERY store, rs, "SELECT id, name, price FROM items ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "id="; rs("id"); " name="; rs("name"); " price="; rs("price")
    LOOP
    SQL CLOSE RESULTSET rs

    SQL CLOSE store
    PRINT "=== SQL EXEC OK ==="
END SUB