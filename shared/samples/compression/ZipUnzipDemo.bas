' FILE: compression/ZipUnzipDemo.bas
' ============================================================================================================
' COMPRESSION — ZIP / UNZIP (statement legacy e funzioni qualificate)
' ============================================================================================================
IMPORT ARCHIVE WITH ALIAS
IMPORT IO WITH ALIAS

SUB Main()
    ' --- PREPARAZIONE: creo alcuni file di test ---
    OPEN "a.txt" FOR OUTPUT AS #1
    PRINT #1, "Contenuto del file A"
    CLOSE #1
    OPEN "b.txt" FOR OUTPUT AS #2
    PRINT #2, "Contenuto del file B"
    CLOSE #2
    PRINT "File di test creati."

    ' --- 1. Statement legacy: ZIP sorgente TO destinazione ---
    ZIP "a.txt" TO "archivio1.zip"
    PRINT "ZIP legacy creato: archivio1.zip"

    ' --- 2. Statement legacy: UNZIP archivio TO cartella ---
    UNZIP "archivio1.zip" TO "estratto1"
    PRINT "UNZIP legacy completato in: estratto1"
    PRINT

    ' --- 3. Funzione qualificata: ARCHIVE.ZIP(sources, dest) ---
    DIM ok1 = ARCHIVE.ZIP("b.txt", "archivio2.zip")
    PRINT "ARCHIVE.ZIP ok: "; ok1
    PRINT

    ' --- 4. Funzione qualificata: ARCHIVE.UNZIP(archive, dir) ---
    DIM ok2 = ARCHIVE.UNZIP("archivio2.zip", "estratto2")
    PRINT "ARCHIVE.UNZIP ok: "; ok2
    PRINT

    ' --- 5. Verifica: i file estratti esistono? ---
    IF IO.FILEEXISTS("estratto1/a.txt") THEN
        PRINT "estratto1/a.txt presente"
    ELSE
        PRINT "estratto1/a.txt MANCANTE"
    END IF
    IF IO.FILEEXISTS("estratto2/b.txt") THEN
        PRINT "estratto2/b.txt presente"
    ELSE
        PRINT "estratto2/b.txt MANCANTE"
    END IF
    PRINT
    PRINT "Test completato."
END SUB

ON ERROR RESUME NEXT
KILL "archivio1.zip"
KILL "archivio2.zip"
KILL "a.txt"
KILL "b.txt"
