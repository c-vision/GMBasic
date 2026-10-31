' FILE: Concurrency/GpmV2WaitKey.bas
' =================================================================================================
'       G5 (piano ILBackend-GPM): WAITKEY sotto la GPM v2
' =================================================================================================
'
' WAITKEY parcheggia il task finché un tasto non è disponibile (park cooperativo via
' TryWaitKeyPark → IlRuntime.InkeyWaiters); il test inietta un tasto con IlRuntime.QueueInkey
' che sveglia il task.
'
' Output atteso:
'   KEY-PRESSED
'   MAIN-DONE

SUB Main()
  WAITKEY
  PRINT "KEY-PRESSED"
  PRINT "MAIN-DONE"
END SUB
