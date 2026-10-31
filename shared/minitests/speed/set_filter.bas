' MiniTest SPEED SET FILTER — filtro numerico su navigazione
' EXPECT: top: Beta
' NOTA PARITÀ: PASSA su IL, PCode E VM/C (vedi (solved) - VMC-SPEED-SET-FILTER.md —
' 3 bug distinti: ChunkA1Serializer StringExpr.ToString() con virgolette,
' speed_filter_pass logica invertita, locationKind TOP stale 498 vs 479).
' La forma con apice singolo nel BASIC source (Name > 'A') è un commento (limite Lexer).
IMPORT IO
IMPORT SPEED

TYPE SRow
  ID AS LONG
  Name AS STRING * 32
END TYPE

SUB Main()
  DIM rec AS SRow
  ON ERROR RESUME NEXT
  RMDIR "work/sm_sf_num", TRUE
  ON ERROR GOTO 0
  MKDIR "work"
  SPEED OPEN "work/sm_sf_num" ALIAS SmDb
  SPEED USE "Items" FROM SmDb OF SRow ALIAS T INDEXED BY ID, Name

  rec.ID = 1 : rec.Name = "Alpha" : SPEED PUT T, rec
  rec.ID = 2 : rec.Name = "Beta"  : SPEED PUT T, rec
  rec.ID = 3 : rec.Name = "Gamma" : SPEED PUT T, rec

  SPEED SET FILTER TO "ID > 1" ON T
  SPEED GO T, TOP
  SPEED GET T, rec
  PRINT "top: " + rec.Name
  ' EXPECT: top: Beta
END SUB
