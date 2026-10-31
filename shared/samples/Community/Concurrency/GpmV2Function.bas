' FILE: Concurrency/GpmV2Function.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): FUNCTION con RETURN di valore sotto la GPM v2
' =================================================================================================
'
' Una FUNCTION may-park (SLEEP nel corpo) chiamata da Main: il RETURN scrive il valore nella
' destinazione decisa alla CALL (Local). Parità: funzione Go con defer/canali.
'
' Output atteso:
'   X-42
'   MAIN-DONE

FUNCTION DoubleIt(N AS LONG) AS LONG
  SLEEP 1
  RETURN N * 2
END FUNCTION

SUB Main()
  DIM X AS LONG
  X = DoubleIt(21)
  PRINT "X-"; X
  PRINT "MAIN-DONE"
END SUB
