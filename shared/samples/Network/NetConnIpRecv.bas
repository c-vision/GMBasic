' FILE: samples/Community/Network/NetConnIpRecv.bas
' Sample completo NET — tutte le 8 funzioni handle-based.
' Handle 0 = nessuna connessione → output deterministico.
' MiniTest: MiniTests/net/ per verifica EXPECT esatta.
IMPORT NET
SUB Main()
    DIM h AS LONG = 0
    PRINT "NET.CONN(0):       "; NET.CONN(h)
    PRINT "NET.LOCALHOST:     "; NET.LOCALHOST()
    PRINT "NET.LOCALPORT(0):  "; NET.LOCALPORT(h)
    PRINT "NET.REMOTEIP(0):   "; NET.REMOTEIP(h)
    PRINT "NET.REMOTEPORT(0): "; NET.REMOTEPORT(h)
    PRINT "NET.GETHOSTIP(127.0.0.1):  "; NET.GETHOSTIP("127.0.0.1")
    PRINT "NET.RECV(0):       "; NET.RECV(h)
    PRINT "NET.PEEK(0,1):     "; NET.PEEK(h, 1)
    PRINT "Sample NET completato."
END SUB
