' FILE: Concurrency/GpmV2TryFinally.bas
' =================================================================================================
'       G3 (piano ILBackend-GPM-Stackless-Coroutines): TRY/FINALLY sospendibile sotto la GPM v2
' =================================================================================================
'
' Il FINALLY viene eseguito al completamento normale del TRY e PUÒ parcheggiare (SLEEP):
' l'unwind/continuazione del FINALLY resta nel frame attraverso il park.
'
' Output atteso:
'   TRY 1
'   FINALLY 11
'   DOPO 11
'   MAIN-DONE

SUB Main()
  DIM N AS LONG = 0
  TRY
    N = N + 1
    PRINT "TRY "; N
  FINALLY
    SLEEP 5
    N = N + 10
    PRINT "FINALLY "; N
  END TRY
  PRINT "DOPO "; N
  PRINT "MAIN-DONE"
END SUB
