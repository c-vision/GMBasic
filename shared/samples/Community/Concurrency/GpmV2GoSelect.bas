' FILE: Concurrency/GpmV2GoSelect.bas
' =================================================================================================
'       G3 (parità Go select): GO SELECT sotto la GPM v2 (case RECEIVE)
' =================================================================================================
'
' Un producer invia 42 sul canale; il GO SELECT parcheggia finché il case RECEIVE è pronto e
' ne esegue il body (parità Go select { case v := <-ch: }).
'
' Output atteso (v2):
'   SEL-42
'   MAIN-DONE

SUB Prod(ch AS CHANNEL OF INTEGER)
  SLEEP 5
  SEND 42 TO ch
END SUB

SUB Main()
  DIM ch AS CHANNEL OF INTEGER
  DIM v AS INTEGER
  GO SUB Prod(ch)
  GO SELECT
    CASE RECEIVE v FROM ch:
      PRINT "SEL-"; v
  END SELECT
  PRINT "MAIN-DONE"
END SUB
