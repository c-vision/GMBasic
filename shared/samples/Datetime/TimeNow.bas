' FILE: Datetime/TimeNow.bas
' =================================================================================================
' DATETIME — ora corrente: SECONDS / STRTIME / TI.
' =================================================================================================
' DATETIME.SECONDS()  → secondi dalla mezzanotte (0..86399)
' DATETIME.STRTIME()  → ora corrente (UTC) come stringa "HH:MM:SS"
' DATETIME.TI()       → contatore in millisecondi (solo i delta sono portabili)
' =================================================================================================
IMPORT DATETIME

SUB Main()
    CLS
    PRINT "DATETIME — ora corrente"
    PRINT "======================="
    PRINT

    DIM s = DATETIME.SECONDS()
    PRINT "SECONDS() = "; s
    IF s >= 0 AND s < 86400 THEN PRINT "  (valido: secondi dalla mezzanotte)"
    PRINT

    PRINT "STRTIME() = "; DATETIME.STRTIME()
    PRINT

    DIM t = DATETIME.TI()
    PRINT "TI()      = "; t
    IF t > 0 THEN PRINT "  (contatore positivo)"

    ' Composizione: ora corrente da TIMESERIAL e lettura componenti
    DIM d = DATETIME.TIMESERIAL(23, 45, 30)
    PRINT
    PRINT "TIMESERIAL(23,45,30): H="; DATETIME.HOUR(d); " M="; DATETIME.MINUTE(d); " S="; DATETIME.SECOND(d)
END SUB
