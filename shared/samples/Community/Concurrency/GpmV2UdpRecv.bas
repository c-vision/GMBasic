' FILE: Concurrency/GpmV2UdpRecv.bas
' =================================================================================================
'       G5 (piano ILBackend-GPM): UDP RECV sotto la GPM v2 (park)
' =================================================================================================
'
' UDP OPEN su una porta locale e UDP RECV: il task parcheggia finché un datagramma non arriva.
' Il test invia "hello" al socket via UdpClient.
'
' Output atteso:
'   U-hello
'   MAIN-DONE

IMPORT NET

SUB Main()
  DIM h AS LONG
  UDP OPEN 48156, h
  DIM buf AS STRING
  DIM rip AS STRING
  DIM rport AS LONG
  UDP RECV h, buf, rip, rport
  PRINT "U-"; buf
  PRINT "MAIN-DONE"
END SUB
