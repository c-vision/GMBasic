' PCodeVM: il cleanup FINALLY può parcheggiare su UDP (RECV che va in timeout).
' Stesso vincolo già verificato per TCP: i park nel FINALLY devono completare.

IMPORT NET
IMPORT CONCURRENCY

SUB UdpServer(wg AS WAITGROUP)
  DIM h AS LONG
  DIM data$
  DIM ip$
  DIM remotePort AS LONG
  UDP OPEN 9091, h
  IF h <= 0 THEN
    PRINT "FAIL:UDP-OPEN"
    DONE wg
    RETURN
  END IF
  TRY
    UDP RECV h, data$, ip$, remotePort, 5000
    IF data$ = "PING" THEN
      PRINT "UDP-RECV-OK"
    ELSE
      PRINT "FAIL:UDP-RECV="; data$
    END IF
  FINALLY
    ' Cleanup che parcheggia: nessun altro datagramma → timeout → riprende.
    UDP RECV h, data$, ip$, remotePort, 300
    IF data$ = "" THEN
      PRINT "FINALLY-UDP-TIMEOUT-OK"
    ELSE
      PRINT "FAIL:FINALLY-UDP-DATA="; data$
    END IF
    UDP CLOSE h
  END TRY
  DONE wg
END SUB

SUB UdpClient(wg AS WAITGROUP)
  SLEEP 100
  DIM h AS LONG
  UDP OPEN 0, h
  IF h <= 0 THEN
    PRINT "FAIL:UDP-CLIENT-OPEN"
    DONE wg
    RETURN
  END IF
  UDP SEND h, "127.0.0.1", 9091, "PING"
  UDP CLOSE h
  DONE wg
END SUB

SUB Main()
  DIM wg AS WAITGROUP
  GO SUB UdpServer(wg) IN wg
  GO SUB UdpClient(wg) IN wg
  WAIT wg
  PRINT "PCODE-GPM-UDP-FINALLY-DONE"
END SUB
