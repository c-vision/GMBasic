' EXPECT: gethostip-ok|localhost-ok|localport-ok|recv-ok|remoteip-ok|remoteport-ok|peek-ok
IMPORT NET
SUB Main()
  IF LEN(NET.GETHOSTIP("127.0.0.1")) > 0 THEN PRINT "gethostip-ok"
  IF LEN(NET.LOCALHOST()) > 0 THEN PRINT "localhost-ok"
  IF NET.LOCALPORT(0) >= 0 THEN PRINT "localport-ok"
  IF NET.RECV(0) >= 0 THEN PRINT "recv-ok"
  IF LEN(NET.REMOTEIP(0)) >= 0 THEN PRINT "remoteip-ok"
  IF NET.REMOTEPORT(0) >= 0 THEN PRINT "remoteport-ok"
  IF LEN(NET.PEEK(0, 10)) >= 0 THEN PRINT "peek-ok"
END SUB