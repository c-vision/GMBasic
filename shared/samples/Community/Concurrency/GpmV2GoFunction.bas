' FILE: Concurrency/GpmV2GoFunction.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): GO FUNCTION ... INTO (future) sotto la GPM v2
' =================================================================================================
'
' GO FUNCTION spawna una goroutine che calcola DoubleIt(21); il future risolve col valore di
' ritorno quando il task completa (RETURN → Resolve). La lettura della variabile dereferenzia
' il future (implicit-await lite; l'await vero è G5).
'
' Output atteso:
'   X-42
'   MAIN-DONE

FUNCTION DoubleIt(N AS LONG) AS LONG
  SLEEP 5
  RETURN N * 2
END FUNCTION

SUB Main()
  DIM F AS LONG
  GO FUNCTION DoubleIt(21) INTO F
  SLEEP 30
  PRINT "X-"; F
  PRINT "MAIN-DONE"
END SUB
