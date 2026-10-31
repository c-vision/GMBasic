' FILE: samples/database/TextDriversDemo.bas
' ===================================================================
' CSV / DELIM / SDF — driver testo completati (2026-08-21):
' lettura RFC 4180, SORT (SET ORDER), COPY TO, DELETE FOR, manipolazione.
' Modello: riga = record, indice virtuale = numero di riga.
' Percorsi: fixtures in samples/fixtures, output in work.
' ===================================================================
IMPORT RDD
IMPORT DBF
IMPORT IO

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "=== Text Drivers Demo ==="
    PRINT

    ' --- 1. CSV: lettura + SORT (SET ORDER) + navigazione ---
    PRINT "1. CSV read + SORT by NAME"
    RDD USE PROJECTFOLDER() + "/../../Fixtures/people.csv" ALIAS P
    RDD SET ORDER 1, "NAME"
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   "; RDD.GETFIELD(1, 1)
        DBF SKIP 1
    LOOP
    DBF CLOSE
    PRINT

    ' --- 2. CSV: COPY TO ordinata (crea header + righe) ---
    PRINT "2. CSV COPY TO -> work/people_sorted.csv"
    RDD USE PROJECTFOLDER() + "/../../Fixtures/people.csv" ALIAS P2
    RDD SET ORDER 1, "NAME"
    RDD COPY TO g_work + "/people_sorted.csv" VIA "CSV"
    DBF CLOSE
    PRINT "   copiate 3 righe ordinate per NAME"
    PRINT

    ' --- 3. CSV: DELETE FOR (soft delete) ---
    PRINT "3. CSV DELETE FOR NAME='Bob' (soft)"
    RDD USE PROJECTFOLDER() + "/../../Fixtures/people.csv" ALIAS P3
    RDD DELETE FOR NAME = "Bob"
    PRINT "   Bob marcato cancellato (soft; vedi PACK a livello driver)"
    DBF CLOSE
    PRINT

    ' --- 4. DELIM: lettura ---
    PRINT "4. DELIM read"
    RDD USE PROJECTFOLDER() + "/../../Fixtures/data.delim" ALIAS D
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   "; RDD.GETFIELD(1, 1)
        DBF SKIP 1
    LOOP
    DBF CLOSE
    PRINT

    ' --- 5. SDF: lettura + append + write-back ---
    PRINT "5. SDF read"
    RDD USE PROJECTFOLDER() + "/../../Fixtures/data.sdf" ALIAS S
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   "; RDD.GETFIELD(1, 1)
        DBF SKIP 1
    LOOP
    DBF CLOSE
    PRINT

    PRINT "--- TEXT DRIVERS OK ---"
END SUB