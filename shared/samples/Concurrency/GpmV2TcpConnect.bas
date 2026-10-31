' FILE: Concurrency/GpmV2TcpConnect.bas
' =================================================================================================
'       G5 (piano ILBackend-GPM): TCP CONNECT sotto la GPM v2 (await di rete §11.4)
' =================================================================================================
'
' Il connect è un await (IlNetworkAwait su BeginWait/PCodeWaitRegistration, parità PCodeVM):
' il task parcheggia finché la connessione non è stabilita. Il test spina un TcpListener locale
' su 127.0.0.1:8140.
'
' Output atteso:
'   C-<handle>        (handle > 0)
'   MAIN-DONE

IMPORT NET

SUB Main()
  DIM h AS LONG
  TCP CONNECT "127.0.0.1", 8140, h, 3000
  PRINT "C-"; h
  PRINT "MAIN-DONE"
END SUB
