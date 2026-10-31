' PCodeVM: un errore nel FINALLY sostituisce quello originale senza rieseguire il cleanup.

SUB InnerFailure()
  TRY
    ERROR 701, "original-error"
  FINALLY
    PRINT "ERROR-FINALLY-BEGIN"
    SLEEP 1
    ERROR 702, "cleanup-error"
    PRINT "FAIL:ERROR-FINALLY-CONTINUED"
  END TRY
END SUB

SUB Main()
  TRY
    InnerFailure()
    PRINT "FAIL:INNER-RETURNED"
  CATCH Ex AS ERR
    PRINT "OUTER-CAUGHT:"; Ex.Description
  END TRY
  PRINT "PCODE-GPM-FINALLY-ERROR-DONE"
END SUB
