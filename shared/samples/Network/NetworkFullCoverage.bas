' FILE: Network/NetworkFullCoverage.bas
' Copertura completa delle funzioni Network non ancora coperte.
IMPORT NET

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Network - Copertura Completa"
    PRINT "============================================================"
    PRINT

    PRINT "-- REMOTEIP --"
    PRINT "REMOTEIP len: "; LEN(NET.REMOTEIP(0))
    PRINT "-- REMOTEPORT --"
    PRINT "REMOTEPORT: "; NET.REMOTEPORT(0)
    PRINT "-- LOCALPORT --"
    PRINT "LOCALPORT: "; NET.LOCALPORT(0)
    PRINT "-- LOCALHOST --"
    PRINT "LOCALHOST len: "; LEN(NET.LOCALHOST())
    PRINT

    PRINT "-- PEEK --"
    PRINT "PEEK len (handle invalido): "; LEN(NET.PEEK(0, 16))
    PRINT

    PRINT "Test Completato."
END SUB
