' MiniTest SPEED — `SPEED CLOSE [alias | DATABASES]`.
' Verifica che CLOSE sia riconosciuto, che chiuda l'area e che un'apertura successiva
' funzioni ancora (nessuna corruzione dello stato dopo la chiusura).
' EXPECT: put: si|closed: si|reopen: si|closedall: si
IMPORT SPEED

TYPE Riga
  ID AS LONG
  Nome AS STRING * 16
END TYPE

SUB Main()
  SPEED OPEN "work/close_db" ALIAS MioDb
  SPEED USE "T" FROM MioDb OF Riga ALIAS Tbl INDEXED BY ID
  DIM r AS Riga
  r.ID = 1 : r.Nome = "Anna"
  SPEED PUT Tbl, r
  PRINT "put: si"

  SPEED CLOSE
  PRINT "closed: si"

  SPEED OPEN "work/close_db" ALIAS MioDb2
  SPEED USE "T" FROM MioDb2 OF Riga ALIAS Tbl2 INDEXED BY ID
  PRINT "reopen: si"

  SPEED CLOSE DATABASES
  PRINT "closedall: si"
END SUB
