' FILE: Concurrency/GpmV2HttpFetch.bas
' =================================================================================================
'       G5 (piano ILBackend-GPM): HTTP FETCH espressione sotto la GPM v2
' =================================================================================================
'
' FETCH (forma espressione) ritorna un future (async, Task.Run); la lettura della variabile
' target dereferenzia il future (implicit-await lite). Il test spina un listener locale su
' 127.0.0.1:8137 che risponde 200 con body "OK".
'
' Output atteso:
'   S-200
'   MAIN-DONE

SUB Main()
  DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8137/api/test")
  SLEEP 100
  PRINT "S-"; r.StatusCode
  PRINT "MAIN-DONE"
END SUB
