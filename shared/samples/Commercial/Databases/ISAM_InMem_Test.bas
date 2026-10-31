' ISAM_InMem_Test - Test CRUD in memoria
IMPORT ISAM

CLS
PRINT "=== ISAM_InMem_Test ==="

TYPE R
    ID AS STRING * 10
    V AS STRING * 30
END TYPE

DIM rec AS R
DIM chk AS R

' Apri in memoria - schema creato automaticamente
ISAM OPEN "mem://test_mem" OF R ALIAS M INDEXED BY ID
PRINT "OPEN OK"

' Append
rec.ID = "A1"
rec.V = "Valore1"
ISAM APPEND M, rec
PRINT "APPEND OK"

rec.ID = "A2"
rec.V = "Valore2"
ISAM APPEND M, rec
PRINT "APPEND2 OK"

' Go TOP e leggi
ISAM GO M, TOP
ISAM GET M, chk
PRINT "TOP: ID="; chk.ID; " V="; chk.V

' Seek A2
ISAM SEEK M, "A2"
IF ISAM.FOUND(M) THEN
    ISAM GET M, chk
    PRINT "SEEK A2: ID="; chk.ID; " V="; chk.V
ELSE
    PRINT "SEEK A2: NOT FOUND"
END IF

ISAM CLOSE M
PRINT "DONE"
