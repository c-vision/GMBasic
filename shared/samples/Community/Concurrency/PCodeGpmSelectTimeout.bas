' PCodeVM: TIMEOUT case in GO SELECT (parità Go case <-time.After(d)).
' Seleziona il case canale se un valore arriva entro la deadline, altrimenti
' esegue il case TIMEOUT. Verifica entrambi i rami: un producer che invia in
' tempo (vince il RECEIVE) e un producer silenzioso (vince il TIMEOUT).

IMPORT CONCURRENCY

SUB FastProducer(ch AS CHANNEL OF INTEGER)
  SLEEP 100
  SEND 42 TO ch
END SUB

SUB SlowProducer(ch AS CHANNEL OF INTEGER)
  ' Non invia mai: il SELECT deve scattare il TIMEOUT.
  SLEEP 5000
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM v AS INTEGER
  DIM picked AS INTEGER = 0

  ' Caso 1: il valore arriva prima del timeout (100ms < 1000ms) -> RECEIVE.
  GO SUB FastProducer(ch)
  GO SELECT
    CASE RECEIVE v FROM ch:
      PRINT "SELECT-RECV-"; v
      picked = picked + 1
    CASE TIMEOUT 1000:
      PRINT "FAIL:TIMEOUT-MANCATO"
    END SELECT

  ' Caso 2: nessun valore -> TIMEOUT dopo 200ms.
  GO SUB SlowProducer(ch)
  GO SELECT
    CASE RECEIVE v FROM ch:
      PRINT "FAIL:RECV-INESPERATO"
    CASE TIMEOUT 200:
      PRINT "SELECT-TIMEOUT-OK"
      picked = picked + 1
    END SELECT

  IF picked = 2 THEN
    PRINT "PCODE-GPM-SELECT-TIMEOUT-OK"
  ELSE
    PRINT "FAIL:SELECT-PICKED="; picked
  END IF
END SUB
