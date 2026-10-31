' ISAM_Simple_Test - Test CRUD base
IMPORT ISAM
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

CLS
PRINT "=== ISAM_Simple_Test ==="
PRINT ""

TYPE Utente
    ID AS STRING * 10
    Nome AS STRING * 50
    Eta AS INTEGER
END TYPE

DIM rec AS Utente
DIM found AS Utente

' Setup: crea database con un record iniziale
ON ERROR RESUME NEXT
KILL g_work + "/minimal_crud.isdb"
ON ERROR GOTO 0
ISAM OPEN g_work + "/minimal_crud.isdb" OF Utente ALIAS T INDEXED BY ID
rec.ID = "AA123" : rec.Nome = "Existing" : rec.Eta = 42
ISAM APPEND T, rec
ISAM CLOSE T

' Apri file esistente
ISAM OPEN g_work + "/minimal_crud.isdb" OF Utente ALIAS T INDEXED BY ID
PRINT "OPEN: OK"

' Leggi primo record (TOP)
ISAM GO T, TOP
ISAM GET T, found
PRINT "Record esistente: ID="; found.ID

' Append nuovo record
rec.ID = "ZZ999"
rec.Nome = "TestRecord"
rec.Eta = 77
ISAM APPEND T, rec
PRINT "APPEND: OK"

' Seek per il nuovo record
ISAM SEEK T, "ZZ999"
IF ISAM.FOUND(T) THEN
    DIM chk AS Utente
    ISAM GET T, chk
    PRINT "SEEK+GET: ID="; chk.ID; " Nome="; chk.Nome; " Eta="; chk.Eta
    IF chk.ID = "ZZ999" AND chk.Nome = "TestRecord" THEN
        PRINT "VERIFICA: OK - dati corretti!"
    ELSE
        PRINT "VERIFICA: ERRORE - dati non corrispondono"
    END IF
ELSE
    PRINT "SEEK: RECORD NON TROVATO"
END IF

ISAM CLOSE T
PRINT ""
PRINT "=== Test completato ==="
