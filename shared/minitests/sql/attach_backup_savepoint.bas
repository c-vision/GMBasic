' SQL ATTACH/DETACH, SQL BACKUP ... TO, SAVEPOINT / ROLLBACK TO SAVEPOINT / RELEASE SAVEPOINT,
' parametri posizionali '?' di SQL EXEC/QUERY (parametri nelle QUERY inclusi).
' Prima C99Native li rifiutava ("statement non supportato") o ignorava i parametri nelle QUERY.
' EXPECT: gt2=g/ 2.5|aux=aux|bk= 2|tx=keep|n= 1
IMPORT SQL
SUB Main()
  ON ERROR RESUME NEXT
  MKDIR "work"
  ON ERROR GOTO 0
  ' nomi univoci per esecuzione: le tre engine condividono il processo e il pool SQLite tiene aperti i file
  DIM u AS STRING = TRIM(STR(INT(RND() * 100000000)))
  DIM pMain AS STRING = "work/ab_main_" + u + ".db"
  DIM pAux AS STRING = "work/ab_aux_" + u + ".db"
  DIM pBk AS STRING = "work/ab_bk_" + u + ".db"
  ' RND e' deterministico nei backend C: ripulisce gli eventuali residui di run precedenti con lo stesso nome
  ON ERROR RESUME NEXT
  IF IO.FILEEXISTS(pMain) THEN KILL pMain
  IF IO.FILEEXISTS(pAux) THEN KILL pAux
  IF IO.FILEEXISTS(pBk) THEN KILL pBk
  ON ERROR GOTO 0
  DIM rs AS SQLRESULTSET

  SQL OPEN pAux ALIAS AuxF
  SQL EXEC AuxF, "CREATE TABLE a (label TEXT)"
  SQL EXEC AuxF, "INSERT INTO a (label) VALUES (?)", "aux"
  SQL CLOSE AuxF

  SQL OPEN pMain ALIAS MainDb
  SQL EXEC MainDb, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL)"
  SQL EXEC MainDb, "INSERT INTO items (name, price) VALUES (?, ?)", "w", 1.5
  SQL EXEC MainDb, "INSERT INTO items (name, price) VALUES (?, ?)", "g", 2.5

  SQL QUERY MainDb, rs, "SELECT name, price FROM items WHERE price > ?", 2
  IF SQL.FETCH(rs) THEN PRINT "gt2="; rs("name"); "/"; rs("price")
  SQL CLOSE RESULTSET rs

  SQL ATTACH MainDb, pAux AS aux
  SQL QUERY MainDb, rs, "SELECT label FROM aux.a"
  IF SQL.FETCH(rs) THEN PRINT "aux="; rs("label")
  SQL CLOSE RESULTSET rs
  SQL DETACH MainDb, "aux"

  SQL BACKUP MainDb TO pBk
  SQL OPEN pBk ALIAS AppBk
  SQL QUERY AppBk, rs, "SELECT COUNT(*) AS n FROM items"
  IF SQL.FETCH(rs) THEN PRINT "bk="; rs("n")
  SQL CLOSE RESULTSET rs
  SQL CLOSE AppBk

  SQL BEGIN TRANSACTION MainDb
  SQL SAVEPOINT MainDb, "sp1"
  SQL EXEC MainDb, "INSERT INTO items (name, price) VALUES ('keep', 1.00)"
  SQL SAVEPOINT MainDb, "sp2"
  SQL EXEC MainDb, "INSERT INTO items (name, price) VALUES ('drop', 2.00)"
  SQL ROLLBACK TO SAVEPOINT MainDb, "sp2"
  SQL RELEASE SAVEPOINT MainDb, "sp1"
  SQL COMMIT MainDb
  SQL QUERY MainDb, rs, "SELECT name FROM items WHERE id > 2"
  IF SQL.FETCH(rs) THEN PRINT "tx="; rs("name")
  SQL CLOSE RESULTSET rs
  SQL QUERY MainDb, rs, "SELECT COUNT(*) AS n FROM items WHERE id > 2"
  IF SQL.FETCH(rs) THEN PRINT "n="; rs("n")
  SQL CLOSE RESULTSET rs
  SQL CLOSE MainDb
  ON ERROR RESUME NEXT
  KILL pMain : KILL pAux : KILL pBk
  ON ERROR GOTO 0
END SUB
