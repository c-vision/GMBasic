' FILE: Databases/DbDbfUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: DBF - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre i comandi/funzioni DBF che non comparivano in alcun esempio .bas:
'   DBFILTER, LOOKUP.
'
' COPERTURA:
' - DBF.DBFILTER(area) : filtro attivo -> STRING.
' - DBF.LOOKUP(campoRisultato, valore, campoRicerca [, area]) : valore del campo cercando
'   su un altro campo (formato del MiniTest dbf_lock_filter: i pad del campo stringa
'   vengono rimossi).
' =================================================================================================
IMPORT DBF
IMPORT IO

TYPE Row
    Name AS STRING * 20
END TYPE

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "DBF - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    MKDIR g_work

    ' Setup: crea e apre un DBF di prova
    DBF CREATE g_work + "/dbfilter.dbf" FROM TYPE Row
    DBF USE g_work + "/dbfilter.dbf" ALIAS "FilterDbf" AS Row

    ' 1. DBFILTER (senza parametro, area attiva)
    PRINT "-- DBFILTER --"
    DIM f AS STRING
    f = DBF.DBFILTER()
    PRINT "DBFILTER() = '"; f; "' (vuoto se nessun filtro attivo)."
    PRINT

    ' 2. DBF.LOOKUP: valore di un campo cercando su un altro (record reali).
    PRINT "-- LOOKUP --"
    DIM r AS Row
    DIM i AS LONG
    FOR i = 1 TO 3
        r.Name = "N" + LTRIM$(STR$(i))
        DBF APPEND RECORD r
    NEXT i
    DBF GO TOP
    PRINT "LOOKUP(Name, N2, Name, FilterDbf) = '"; DBF.LOOKUP("Name", "N2", "Name", "FilterDbf"); "'"
    PRINT

    DBF CLOSE
    PRINT "Test Completato."
END SUB
