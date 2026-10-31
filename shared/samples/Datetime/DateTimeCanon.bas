' FILE: Datetime/DateTimeCanon.bas
' =================================================================================================
' DATA/ORA CORRENTE (UTC) — il canone uguale su IL, PCodeVM, VM/C, C99Native
' =================================================================================================
'   TIME()                 → seriale data+ora, numerico   (es. 46302.9034027778)
'   DATE()                 → seriale del solo giorno      (es. 46302)
'   STRTIME() / TIME$ / TI$ → "HH:MM:SS"                  (es. 21:40:54)
'   STRDATE() / DATE$       → "dd/MM/yyyy"                (es. 07/10/2026)
'   TI()                   → contatore in millisecondi (solo i DELTA sono significativi)
' =================================================================================================
SUB Main()
    PRINT "TIME()    = "; TIME()
    PRINT "DATE()    = "; DATE()
    PRINT "STRTIME() = "; STRTIME()
    PRINT "STRDATE() = "; STRDATE()
    PRINT "TIME$     = "; TIME$
    PRINT "TI$       = "; TI$
    PRINT "DATE$     = "; DATE$

    ' La parte frazionaria di TIME() e' l'ora del giorno: TIME() - DATE() in [0, 1)
    DIM frazione AS DOUBLE = TIME() - DATE()
    IF frazione >= 0 AND frazione < 1 THEN PRINT "TIME() - DATE() = frazione di giorno (ok)"

    ' TI() e' un contatore: serve per misurare intervalli
    DIM t0 AS LONG = TI()
    SLEEP 20
    PRINT "Trascorsi (ms, circa): "; TI() - t0
END SUB
