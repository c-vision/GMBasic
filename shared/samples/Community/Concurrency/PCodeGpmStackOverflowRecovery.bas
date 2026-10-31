' PCodeVM: lo stack operandi ha un limite di sicurezza configurabile, ma l'overflow
' deve restare un errore BASIC catturabile e non deve uccidere il worker GPM.

IMPORT CONCURRENCY

SUB OverflowWorker(Name AS STRING, Results AS CHANNEL OF STRING)
  TRY
    DIM TooDeep AS LIST<INTEGER> = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    SEND "FAIL:OVERFLOW-NOT-RAISED:" + Name TO Results
  CATCH Ex AS ERR
    DIM Recovered AS LIST<INTEGER> = [1, 2, 3]
    IF Recovered.Count = 3 THEN
      SEND "STACK-OVERFLOW-RECOVERED:" + Name TO Results
    ELSE
      SEND "FAIL:STACK-NOT-RESTORED:" + Name TO Results
    END IF
  END TRY
END SUB

SUB Main()
  DIM Results AS CHANNEL OF STRING
  DIM Workers AS WAITGROUP
  DIM Message AS STRING

  GO SUB OverflowWorker("A", Results) IN Workers
  GO SUB OverflowWorker("B", Results) IN Workers

  FOR I = 1 TO 2
    RECEIVE Message FROM Results
    PRINT Message
  NEXT I

  WAIT Workers
  PRINT "PCODE-GPM-STACK-OVERFLOW-DONE"
END SUB
