' FILE: Concurrency/GpmV2ByRef.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): BYREF sotto la GPM v2
' =================================================================================================
'
' Una SUB may-park con parametro BYREF: la modifica nel callee si riflette nel chiamante
' (RefToVar). Il park (SLEEP) è attraversato con la cella byref intatta.
'
' Output atteso:
'   N-12
'   MAIN-DONE

SUB IncRef(BYREF X AS LONG)
  X = X + 1
  SLEEP 1
  X = X + 1
END SUB

SUB Main()
  DIM N AS LONG = 10
  IncRef(N)
  PRINT "N-"; N
  PRINT "MAIN-DONE"
END SUB
