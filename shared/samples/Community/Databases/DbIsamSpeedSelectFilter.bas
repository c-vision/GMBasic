' FILE: Databases/DbIsamSpeedSelectFilter.bas
' Sample completo: dimostra ISAM SELECT (commutazione area multi-file).
' NOTA SPEED: SPEED SET FILTER è rotto nel motore C# (bug (open) - SPEED-SET-FILTER.md:
' i campi UDT non sono iniettati nell'evaluator PCode) — NON inclusa nel sample perché
' non gira. Il sample si limita alla parte funzionante (ISAM SELECT).
' Parità: IL / PCodeVM / VM-C / C99 / C99Native (gamba complete per ISAM SELECT)
IMPORT ISAM
IMPORT IO

TYPE Cliente
  ID AS STRING * 10
  Cognome AS STRING * 50
  Eta AS INTEGER
END TYPE

SUB Main()
DIM C AS Cliente
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

' ISAM SELECT: due file distinti, commutazione per alias
ON ERROR RESUME NEXT
KILL g_work + "/clienti_nord.isdb"
KILL g_work + "/clienti_sud.isdb"
ON ERROR GOTO 0

ISAM OPEN g_work + "/clienti_nord.isdb" OF Cliente ALIAS Nord INDEXED BY ID
ISAM OPEN g_work + "/clienti_sud.isdb" OF Cliente ALIAS Sud INDEXED BY ID
PRINT "--- ISAM SELECT: due aree commutate ---"
ISAM SELECT Nord
C.ID = "N1" : C.Cognome = "Bianchi" : C.Eta = 40 : ISAM APPEND Nord, C
C.ID = "N2" : C.Cognome = "Gialli" : C.Eta = 20 : ISAM APPEND Nord, C

ISAM SELECT Sud
C.ID = "S1" : C.Cognome = "Rossi" : C.Eta = 30 : ISAM APPEND Sud, C

ISAM SELECT Nord
ISAM GO Nord, TOP
ISAM GET Nord, C
PRINT "  Nord TOP: " + C.ID        ' N1

ISAM SELECT Sud
ISAM GO Sud, TOP
ISAM GET Sud, C
PRINT "  Sud TOP: " + C.ID         ' S1

ISAM CLOSE Nord
ISAM CLOSE Sud
END SUB

