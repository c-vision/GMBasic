' FILE: Datetime/DateTimeDate.bas
' =================================================================================================
' DATETIME.DATE() — seriale numerico del giorno corrente UTC (es. 46302); STRDATE() = "dd/MM/yyyy"
' =================================================================================================
IMPORT DATETIME

SUB Main()
    CLS
    PRINT "=== DATETIME.DATE ==="
    DIM d = DATETIME.DATE()
    PRINT "Current date (serial): "; d
    PRINT "Current date (text):   "; DATETIME.STRDATE()
    PRINT "=== OK ==="
END SUB