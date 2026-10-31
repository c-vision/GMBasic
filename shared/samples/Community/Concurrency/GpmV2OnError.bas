' FILE: Concurrency/GpmV2OnError.bas
' =================================================================================================
'       G3 (parità BASIC): ON ERROR GOTO + RESUME NEXT sotto la GPM v2
' =================================================================================================
'
' L'handler ON ERROR cattura l'errore runtime (ERROR 5) e salta al label; RESUME NEXT riprende
' dal blocco successivo a quello che ha sollevato l'errore. L'handler contiene un park (SLEEP):
' lo stato ON ERROR (ErrorLinePc/Suppressed) sopravvive al park.
'
' Output atteso:
'   HANDLER
'   DOPO-ERRORE

SUB Main()
  ON ERROR GOTO Errore
  ERROR 5
  PRINT "DOPO-ERRORE"
  END

  Errore:
    SLEEP 1
    PRINT "HANDLER"
    RESUME NEXT
END SUB
