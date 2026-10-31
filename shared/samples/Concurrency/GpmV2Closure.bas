' FILE: Concurrency/GpmV2Closure.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): procedura LOCALE sotto la GPM v2
' =================================================================================================
'
' Una SUB locale (senza catture) may-park dichiarata dentro Main: viene compilata come StepV2
' e la call la risolve per SymbolId. (Le closure con CATTURA richiedono il closure context —
' follow-up G4.)
'
' Output atteso:
'   IN 5
'   MAIN-DONE

SUB Main()
  SUB Interno(N AS LONG)
    SLEEP 1
    PRINT "IN"; N
  END SUB
  Interno(5)
  PRINT "MAIN-DONE"
END SUB
