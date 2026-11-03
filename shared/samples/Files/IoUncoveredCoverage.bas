' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Files/IoUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: IO - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre i comandi/funzioni IO che non comparivano in alcun esempio .bas:
'   XCOPY, FOPEN.
'
' COPERTURA:
' - XCOPY sorgente TO destinazione [, ricorsivo [, sovrascrivi]]  : copia ricorsiva.
' - IO.FOPEN(fileName) -> LONG (handle) : apre un file e restituisce il handle.
' - IO.READLINES(fileName) -> array di stringhe : lettura in blocco, una riga per elemento.
' =================================================================================================
IMPORT IO
IMPORT CONVERSIONS AS CV

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "IO - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Setup: crea un file sorgente
    MKDIR g_work
    OPEN g_work + "/sorgente.txt" FOR WRITE AS #1
    PRINT #1, "contenuto sorgente"
    CLOSE #1
    DIM h AS LONG

    ' 1. IO.FOPEN
    PRINT "-- IO.FOPEN --"
    h = IO.FOPEN(g_work + "/sorgente.txt")
    IF h > 0 THEN
        PRINT "Handle FOPEN = "; h
    ELSE
        PRINT "FOPEN fallito (handle 0)"
    END IF
    PRINT

    ' 2. IO.XCOPY
    PRINT "-- IO.XCOPY --"
    MKDIR g_work + "/source dir"
    OPEN g_work + "/source dir/pippo.txt" FOR WRITE AS #2
    PRINT #2, "pippo"
    CLOSE #2
    ' XCOPY sorgente TO destinazione [, ricorsivo]: il 2o argomento (TRUE) abilita
    ' la copia ricorsiva (sintassi usata nel test ufficiale Interpreter/Tests/Tests.cs).
    ' La destinazione deve essere fresca: si rimuove se presente e si ricrea.
    RMDIR g_work + "/dest dir", TRUE
    XCOPY g_work + "/source dir" TO g_work + "/dest dir", TRUE
    PRINT "XCOPY eseguito (copia ricorsiva)."
    PRINT

    ' 3. IO.FERROR (codice dell'ultimo errore I/O; 0 = nessun errore)
    PRINT "-- IO.FERROR --"
    PRINT "FError = "; IO.FERROR()
    PRINT

    ' 4. IO.READLINES (lettura in blocco: una riga per elemento, terminatori rimossi)
    PRINT "-- IO.READLINES --"
    OPEN g_work + "/righe.txt" FOR WRITE AS #3
    PRINT #3, "prima"
    PRINT #3, "seconda"
    PRINT #3, "terza"
    CLOSE #3
    DIM righe = IO.READLINES(g_work + "/righe.txt")
    PRINT "Righe lette = "; UBOUND(righe) + 1
    PRINT "Prima riga  = "; righe(0)
    PRINT "Ultima riga = "; righe(UBOUND(righe))
    PRINT

    PRINT "Test Completato."
END SUB
