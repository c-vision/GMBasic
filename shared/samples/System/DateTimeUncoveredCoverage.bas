' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: System/DateTimeUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: DATETIME - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni DATETIME che non comparivano in alcun esempio .bas:
'   CDOW, CMONTH, DOW, SECONDS, STOD, ISOWEEKOFYEAR.
'
' COPERTURA:
' - CDOW(date)         : nome inglese del giorno della settimana.
' - CMONTH(date)       : nome inglese del mese.
' - DOW(date)          : numero del giorno della settimana (1=domenica .. 7=sabato).
' - SECONDS()          : secondi trascorsi dall'ultima mezzanotte.
' - CTOD(dateString)   : stringa data -> data seriale.
' - DTOC(date)         : data seriale -> stringa.
' - STOD(dateString)   : stringa -> DATE.
' - ISOWEEKOFYEAR(date): settimana ISO -> INTEGER.
'
' NOTA STORICA: CDOW/CMONTH/DOW/SECONDS erano dichiarate in StdLib ma SENZA dispatch
' nel runtime (bug firma-vs-runtime). Implementate 2026-08-05: TokenKind -> Lexer ->
' StdLib.DateTime -> eval interpreter (Gpm.Dates) + PCodeVM + mappa compiler.
' =================================================================================================
IMPORT DATETIME AS DT

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "DATETIME - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    DIM d AS DATE
    d = DT.DATEVALUE("2023-05-15")

    ' 1. CDOW / CMONTH / DOW
    PRINT "-- CDOW / CMONTH / DOW --"
    PRINT "CDOW(2023-05-15)   = "; DT.CDOW(d)
    PRINT "CMONTH(2023-05-15) = "; DT.CMONTH(d)
    PRINT "DOW(2023-05-15)    = "; DT.DOW(d)
    PRINT

    ' 2. SECONDS (ore 12:00 -> ~43200)
    PRINT "-- SECONDS --"
    PRINT "SECONDS()          = "; DT.SECONDS()
    PRINT

    ' 3. CTOD / DTOC (conversione bidirezionale)
    PRINT "-- CTOD / DTOC --"
    PRINT "CTOD('05/15/2023') = "; DT.CTOD("05/15/2023")
    PRINT "DTOC(2023-05-15)   = "; DT.DTOC(d)
    PRINT

    ' 4. STOD (string -> DATE)
    PRINT "-- STOD --"
    DIM d2 AS DATE
    d2 = DT.STOD("31/12/2024")
    PRINT "STOD('31/12/2024') = "; DT.DTOS(d2)
    PRINT

    ' 5. ISOWEEKOFYEAR
    PRINT "-- ISOWEEKOFYEAR --"
    PRINT "ISOWEEKOFYEAR(2023-05-15) = "; DT.ISOWEEKOFYEAR(d)
    PRINT

    PRINT "Test Completato."
END SUB