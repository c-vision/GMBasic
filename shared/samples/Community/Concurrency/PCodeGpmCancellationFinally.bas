' PCodeVM: la cancellazione deve staccare un task parcheggiato e drenare FINALLY.

SUB LateChild()
  PRINT "FAIL:LATE-CHILD-RAN"
END SUB

SUB Main()
  TRY
    PRINT "CANCEL-BODY-BEGIN"
    SLEEP 10000
    PRINT "FAIL:CANCEL-BODY-RESUMED"
  FINALLY
    PRINT "CANCEL-FINALLY-BEGIN"
    ' Un figlio creato durante l'unwind appartiene alla nursery gia' cancellata.
    GO SUB LateChild()
    SLEEP 1
    PRINT "CANCEL-FINALLY-END"
  END TRY

  PRINT "FAIL:CANCEL-AFTER-TRY"
END SUB
