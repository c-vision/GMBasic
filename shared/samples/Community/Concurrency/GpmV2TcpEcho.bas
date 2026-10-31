' FILE: Concurrency/GpmV2TcpEcho.bas
' =================================================================================================
'       G5 (piano ILBackend-GPM): TCP CONNECT/SEND/RECV sotto la GPM v2 (echo)
' =================================================================================================
'
' Connette a un echo server locale, invia "hello" e riceve l'eco. Il test spina un TcpListener
' che legge e rispedisce i byte ricevuti.
'
' Output atteso:
'   N-5   (5 byte = "hello")
'   MAIN-DONE

IMPORT NET

SUB Main()
  DIM h AS LONG
  TCP CONNECT "127.0.0.1", 8140, h, 3000
  TCP SEND h, "hello"
  DIM buf AS STRING
  DIM n AS LONG
  TCP RECV h, buf, n
  PRINT "N-"; n
  PRINT "MAIN-DONE"
END SUB
