' MiniTest NAMESPACE NET — tutte le 8 funzioni della superficie.
' CONN, GETHOSTIP, LOCALHOST, LOCALPORT, PEEK, RECV, REMOTEIP, REMOTEPORT.
' Handle 0 = socket non connesso: si verificano i valori di fallback deterministici.
' EXPECT: conn=-1|gethostip=127.0.0.1|localhost>0=-1|localport= 0|recv= 0|remoteip=|remoteport= 0|peek=[;]
IMPORT NET

SUB Main()
  PRINT "conn="; (NET.CONN(0) >= 0)
  PRINT "gethostip="; NET.GETHOSTIP("127.0.0.1")
  PRINT "localhost>0="; (LEN(NET.LOCALHOST()) > 0)
  PRINT "localport="; NET.LOCALPORT(0)
  PRINT "recv="; NET.RECV(0)
  PRINT "remoteip="; NET.REMOTEIP(0)
  PRINT "remoteport="; NET.REMOTEPORT(0)
  PRINT "peek=[;"; NET.PEEK(0, 10); "]"
END SUB
