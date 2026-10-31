' FILE: Concurrency/GpmV2ControlFlow.bas
' =================================================================================================
'       G2 (piano ILBackend-GPM-Stackless-Coroutines): CONTROLLO DI FLUSSO + GPM
' =================================================================================================
'
' Esercita il sottoinsieme G2 dell'emitter StepV2_*: sequenze, IF, SELECT, FOR, WHILE,
' con SLEEP / SEND / RECEIVE / WAIT / DONE sotto la GPM (goroutine + canali + waitgroup).
'
' Output atteso (ordine deterministico con GOMAXPROCS=1):
'   UNO
'   DUE
'   ALTRO 3
'   ALTRO 4
'   FINE
'
' Gira sia sul percorso flat IL standard sia sul driver v2 (GMBASIC_IL_GPM_V2=1).

SUB Produttore(C AS CHANNEL OF INTEGER)
  FOR I = 1 TO 4
    SLEEP 5
    SEND I TO C
  NEXT I
  SEND -1 TO C
END SUB

SUB Consumatore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
  DIM X AS INTEGER
  RECEIVE X FROM C
  WHILE X <> -1
    IF X = 1 THEN
      PRINT "UNO"
    ELSE
      SELECT CASE X
        CASE 2
          PRINT "DUE"
        CASE ELSE
          PRINT "ALTRO "; X
      END SELECT
    END IF
    RECEIVE X FROM C
  WEND
  DONE WG
END SUB

SUB Main()
  DIM C AS CHANNEL OF INTEGER
  DIM WG AS WAITGROUP
  GO SUB Produttore(C) IN WG
  GO SUB Consumatore(C, WG) IN WG
  WAIT WG
  PRINT "FINE"
END SUB
