' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Databases/DbIsamUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: ISAM - FUNZIONI/COMANDI NON ANCORA COPERTI DAGLI ESEMPI
' =================================================================================================
' Questo test copre il comando/funzione ISAM che non compariva in alcun esempio .bas:
'   REPAIR, DBFILTER.
'
' COPERTURA:
' - ISAM.REPAIR          : ripara la tabella.
' - ISAM.DBFILTER(area)  : filtro attivo -> STRING.
' =================================================================================================
IMPORT ISAM
IMPORT IO

TYPE Item
    ID AS STRING * 10
    Qty AS INTEGER
END TYPE

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "ISAM - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    MKDIR g_work
    KILL g_work + "/repair.isdb": ON ERROR RESUME NEXT

    ' Setup: crea e popola una tabella ISAM
    DIM rec AS Item
    rec.ID = "SKU-001"
    rec.Qty = 5
    ISAM OPEN g_work + "/repair.isdb" OF Item ALIAS RepDb INDEXED BY ID
    ISAM APPEND RepDb, rec
    ISAM CLOSE RepDb

    ' Riapre per il test
    ISAM OPEN g_work + "/repair.isdb" OF Item ALIAS RepDb INDEXED BY ID

    ' 1. ISAM.REPAIR
    PRINT "-- ISAM.REPAIR --"
    ISAM REPAIR -1
    PRINT "REPAIR eseguito sulla tabella."
    PRINT

    ' 2. ISAM.DBFILTER
    PRINT "-- ISAM.DBFILTER --"
    DIM f AS STRING
    f = ISAM.DBFILTER(RepDb)
    PRINT "DBFILTER(RepDb) = '"; f; "' (vuoto se nessun filtro)."
    PRINT

    ISAM CLOSE RepDb
    PRINT "Test Completato."
END SUB
