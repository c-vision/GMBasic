' PCodeVM: GOEXIT deve terminare l'intera goroutine attraversando i FINALLY di
' tutte le activation frame. Il FINALLY puo' parcheggiare senza perdere l'unwind.

SUB DeepExit()
  TRY
    PRINT "GOEXIT-BODY"
    GO EXIT
    PRINT "FAIL:GOEXIT-CONTINUED"
  FINALLY
    PRINT "GOEXIT-INNER-FINALLY-BEGIN"
    SLEEP 1
    PRINT "GOEXIT-INNER-FINALLY-END"
  END TRY
END SUB

SUB Worker()
  TRY
    DeepExit()
    PRINT "FAIL:GOEXIT-RETURNED"
  FINALLY
    PRINT "GOEXIT-OUTER-FINALLY-BEGIN"
    SLEEP 1
    PRINT "GOEXIT-OUTER-FINALLY-END"
  END TRY

  PRINT "FAIL:GOEXIT-AFTER-TRY"
END SUB

SUB Main()
  DIM workers AS WAITGROUP
  GO SUB Worker() IN workers
  WAIT workers
  PRINT "PCODE-GPM-GOEXIT-FINALLY-DONE"
END SUB
