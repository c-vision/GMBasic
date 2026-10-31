' FILE: Concurrency/GpmV2GoExit.bas
' =================================================================================================
'       G3 (parità Go runtime.Goexit): GO EXIT attraversa i FINALLY sotto la GPM v2
' =================================================================================================
'
' GO EXIT termina il task MA attraversa il FINALLY (sospendibile, con SLEEP). Dopo il FINALLY
' il task esce: "DOPO" NON deve comparire.
'
' Output atteso:
'   TRY
'   FINALLY

SUB Main()
  TRY
    PRINT "TRY"
    GO EXIT
  FINALLY
    SLEEP 5
    PRINT "FINALLY"
  END TRY
  PRINT "DOPO"
END SUB
