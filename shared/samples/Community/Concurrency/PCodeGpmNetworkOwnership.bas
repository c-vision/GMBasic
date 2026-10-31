' PCodeVM: stress OWNERSHIP della rete — il worker parcheggia e riprende su
' disconnect e timeout ripetuti in LOOP (non un singolo caso). Ogni ciclo:
'  1. TCP ACCEPT con timeout (park generazionale, no thread bloccato);
'  2. TCP RECV finché il client chiude (EOF) o scade il timeout;
'  3. disconnect remoto -> RECV torna 0 byte, riprende ESATTAMENTE una volta.
' Con GOMAXPROCS=1 tutto il carico sta su un solo worker: se un wake fosse perso
' o una registration fosse duplicata, il RECV appenderebbe e il runner (CTS 30s)
' fallirebbe. Il totale deterministico verifica che nessun ciclo venga perso.

IMPORT NET
IMPORT CONCURRENCY

SUB Server(WG AS WAITGROUP)
  DIM srv AS LONG
  DIM cli AS LONG
  DIM data$
  DIM bytes AS LONG
  DIM i AS INTEGER
  DIM ok AS LONG = 0

  TCP OPEN 18791, srv
  IF srv <= 0 THEN
    PRINT "FAIL:NET-OWN-OPEN"
    DONE WG
    RETURN
  END IF

  ' 20 cicli: ogni ciclo un accept con timeout + un recv che termina per
  ' disconnect remoto. Un singolo wake perso/duplicato => hang o conteggio errato.
  FOR i = 1 TO 20
    cli = 0
    TCP ACCEPT srv, cli, 3000
    IF cli <= 0 THEN
      PRINT "FAIL:NET-OWN-ACCEPT-"; i
      DONE WG
      RETURN
    END IF
    data$ = ""
    bytes = -1
    ' RECV parcheggiato: il client invia "PING" poi chiude. Il server deve
    ' ricevere 4 byte (PING) e poi EOF (0 byte) -> due riprese distinte.
    TCP RECV cli, data$, bytes, 3000
    IF data$ = "PING" AND bytes = 4 THEN
      TCP RECV cli, data$, bytes, 3000
      IF bytes = 0 THEN
        ok = ok + 1
      ELSE
        PRINT "FAIL:NET-OWN-EOF-"; i; "-"; bytes
        DONE WG
        RETURN
      END IF
    ELSE
      PRINT "FAIL:NET-OWN-RECV-"; i; "-"; data$; "-"; bytes
      DONE WG
      RETURN
    END IF
  NEXT i

  TCP CLOSE srv
  IF ok = 20 THEN
    PRINT "PCODE-GPM-NET-OWN-OK"
  ELSE
    PRINT "FAIL:NET-OWN-COUNT="; ok
  END IF
  DONE WG
END SUB

SUB Client(Id AS INTEGER)
  ' Un client per ciclo: connette, invia PING, chiude. Il disconnect remoto è
  ' il segnale di EOF che il server osserva nel secondo RECV.
  DIM h AS LONG
  DIM i AS INTEGER
  FOR i = 1 TO 20
    h = 0
    TCP CONNECT "127.0.0.1", 18791, h, 3000
    IF h > 0 THEN
      TCP SEND h, "PING"
      TCP CLOSE h
    END IF
  NEXT i
END SUB

SUB Main()
  DIM wg AS WAITGROUP
  GO SUB Server(wg) IN wg
  GO SUB Client(0)
  WAIT wg
  PRINT "PCODE-GPM-NET-OWN-DONE"
END SUB
