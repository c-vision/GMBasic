' MiniTest: SPEED SELECT ... INTO — motore di query (JOIN, GROUP BY, aggregato SUM).
' Struttura allineata al sample DbSpeedFullTests.bas (tipi e dati identici).
' 3 clienti (101,102,103) + 3 ordini (101:10.50, 101:5.25, 102:1.00).
' JOIN Clienti.ID = Ordini.ID_Cliente, GROUP BY Clienti.Nome → 2 gruppi.
' EXPECT: rows= 2
IMPORT IO
IMPORT SPEED

TYPE Cliente
  ID AS LONG
  Nome AS STRING * 50
  Citta AS STRING * 30
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE

TYPE Ordine
  ID_Cliente AS LONG
  ID_Ordine AS LONG
  Descrizione AS STRING * 50
  Importo AS CURRENCY
END TYPE

TYPE RisultatoJoin
  NomeCliente AS STRING * 50
  TotaleImporto AS CURRENCY
END TYPE

SUB Main()
  ON ERROR RESUME NEXT
  RMDIR "bin/sel_db", TRUE
  MKDIR "bin"
  ON ERROR GOTO 0
  SPEED OPEN "bin/sel_db" ALIAS SelDb
  SPEED USE "Clienti" FROM SelDb OF Cliente ALIAS Clienti INDEXED BY ID
  SPEED USE "Ordini" FROM SelDb OF Ordine ALIAS Ordini INDEXED BY ID_Ordine
  DIM c AS Cliente
  c.ID = 101 : c.Nome = "Mario Rossi" : c.Citta = "Roma" : c.Saldo = 150.75 : c.Attivo = TRUE : SPEED PUT Clienti, c
  c.ID = 102 : c.Nome = "Giulia Verdi" : c.Citta = "Milano" : c.Saldo = 2300.50 : c.Attivo = TRUE : SPEED PUT Clienti, c
  c.ID = 103 : c.Nome = "Andrea Bianchi" : c.Citta = "Torino" : c.Saldo = -120.00 : c.Attivo = FALSE : SPEED PUT Clienti, c
  DIM o AS Ordine
  o.ID_Cliente = 101 : o.ID_Ordine = 1 : o.Descrizione = "Penna" : o.Importo = 10.50 : SPEED PUT Ordini, o
  o.ID_Cliente = 101 : o.ID_Ordine = 2 : o.Descrizione = "Quaderno" : o.Importo = 5.25 : SPEED PUT Ordini, o
  o.ID_Cliente = 102 : o.ID_Ordine = 3 : o.Descrizione = "Gomma" : o.Importo = 1.00 : SPEED PUT Ordini, o
  DIM ris() AS RisultatoJoin
  SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris() OF RisultatoJoin FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente GROUP BY Clienti.Nome
  PRINT "rows=" + CONVERSIONS.CSTR(UBOUND(ris) + 1)
END SUB
