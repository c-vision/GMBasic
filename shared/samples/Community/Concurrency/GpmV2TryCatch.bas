' FILE: Concurrency/GpmV2TryCatch.bas
' =================================================================================================
'       G3 (piano ILBackend-GPM-Stackless-Coroutines): TRY/CATCH sotto la GPM v2
' =================================================================================================
'
' Un'eccezione nel corpo del TRY (ERROR) viene catturata dal blocco CIL protetto → transizione
' Throw → il driver fa l'unwind FINALLY-first (qui assente) e salta al CATCH.
'
' Output atteso:
'   CATCH 5
'   DOPO-CATCH
'   MAIN-DONE

SUB Main()
  TRY
    ERROR 5
  CATCH
    SLEEP 5
    PRINT "CATCH 5"
  END TRY
  PRINT "DOPO-CATCH"
  PRINT "MAIN-DONE"
END SUB
