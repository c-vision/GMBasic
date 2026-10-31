' FILE: Concurrency/GpmV2ClosureCapture.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): closure con CATTURA sotto la GPM v2
' =================================================================================================
'
' Una FUNCTION locale cattura la variabile 'fattore' dello scope padre (Main): la modifica al
' genitore si riflette nella closure (parità Go, cattura by-reference).
'
' Output atteso:
'   M-50
'   M-100
'   MAIN-DONE

SUB Main()
  DIM fattore AS LONG = 10
  FUNCTION Moltiplica(x AS LONG) AS LONG
    SLEEP 1
    RETURN x * fattore
  END FUNCTION
  DIM R AS LONG
  R = Moltiplica(5)
  PRINT "M-"; R
  fattore = 20
  R = Moltiplica(5)
  PRINT "M-"; R
  PRINT "MAIN-DONE"
END SUB
