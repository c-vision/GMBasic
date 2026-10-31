' PCodeVM: il cleanup FINALLY può parcheggiare su rete (TCP RECV dopo il
' disconnect) e il controllo deve continuare correttamente — lo stesso vincolo
' già verificato per SLEEP nel FINALLY sospendibile vale per gli adapter async.
' Con GMBASIC_GOMAXPROCS=1, RECV nel FINALLY parcheggia l'unico worker e la
' goroutine riprende quando il client chiude (EOF → 0 byte).

IMPORT NET
IMPORT CONCURRENCY

SUB NetServer(WG AS WAITGROUP)
  DIM srv AS LONG
  DIM cli AS LONG
  DIM data$
  DIM bytes AS LONG
  TCP OPEN 18779, srv
  IF srv <= 0 THEN
    PRINT "FAIL:NET-OPEN"
    DONE WG
    RETURN
  END IF
  TRY
    TCP ACCEPT srv, cli, 5000
    IF cli <= 0 THEN
      PRINT "FAIL:NET-ACCEPT"
    ELSE
      TCP RECV cli, data$, bytes, 5000
      IF data$ = "PING" THEN
        PRINT "SERVER-RECV-OK"
      ELSE
        PRINT "FAIL:NET-RECV="; data$
      END IF
    END IF
  FINALLY
    IF cli > 0 THEN
      ' Cleanup che parcheggia: dopo il disconnect del client, RECV torna 0 byte.
      TCP RECV cli, data$, bytes, 1000
      IF bytes = 0 THEN
        PRINT "FINALLY-NET-RECV-OK"
      ELSE
        PRINT "FAIL:FINALLY-NET-BYTES="; bytes
      END IF
      TCP CLOSE cli
    END IF
    TCP CLOSE srv
  END TRY
  DONE WG
END SUB

SUB NetClient(WG AS WAITGROUP)
  SLEEP 100
  DIM cli AS LONG
  TCP CONNECT "127.0.0.1", 18779, cli, 5000
  IF cli <= 0 THEN
    PRINT "FAIL:NET-CONNECT"
  ELSE
    TCP SEND cli, "PING"
    TCP CLOSE cli
  END IF
  DONE WG
END SUB

SUB Main()
  DIM WG AS WAITGROUP
  GO SUB NetServer(WG) IN WG
  GO SUB NetClient(WG) IN WG
  WAIT WG
  PRINT "PCODE-GPM-NET-FINALLY-DONE"
END SUB
