' PCodeVM: select su canale NIL (case disabilitato) — parità Go.
' In Go, un case su un canale nil è disabilitato per sempre: mai pronto, mai
' bloccante, mai scelto. Qui: due canali, uno nil (non inizializzato) e uno
' pronto (producer invia). Solo il case sul canale pronto deve vincere; il
' case nil non deve mai essere scelto né bloccare il SELECT.

IMPORT CONCURRENCY

SUB Producer(ready AS CHANNEL OF INTEGER)
  SLEEP 50
  SEND 42 TO ready
END SUB

SUB Main()
  DIM ready AS CHANNEL OF INTEGER (1)
  DIM nilCh AS CHANNEL OF INTEGER   ' mai assegnato → nil (disabilitato)
  DIM v AS INTEGER
  DIM picked AS INTEGER = 0

  GO SUB Producer(ready)
  GO SELECT
    CASE RECEIVE v FROM nilCh:
      PRINT "FAIL:NIL-CASE-SCELTO"
    CASE RECEIVE v FROM ready:
      PRINT "SELECT-READY-"; v
      picked = picked + 1
    CASE TIMEOUT 1000:
      PRINT "FAIL:TIMEOUT"
    END SELECT

  IF picked = 1 THEN
    PRINT "PCODE-GPM-SELECT-NIL-OK"
  ELSE
    PRINT "FAIL:PICKED="; picked
  END IF
END SUB
