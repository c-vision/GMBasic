' RDD_Buffer_Demo.bas — Table Buffering + SETREPROCESS
' ===================================================================
IMPORT IO
IMPORT ISAM
IMPORT RDD
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== Table Buffering Demo ==="
PRINT

TYPE Employee
    ID AS STRING * 8
    Name AS STRING * 32
    Salary AS DOUBLE
    Dept AS STRING * 16
END TYPE

KILL g_work + "/buf_demo.isdb"
ON ERROR RESUME NEXT

DIM e AS Employee
ISAM OPEN g_work + "/buf_demo.isdb" OF Employee ALIAS EMP INDEXED BY ID

' Popola dati
e.ID = "E001" : e.Name = "Mario Rossi" : e.Salary = 45000 : e.Dept = "IT"
ISAM APPEND EMP, e
e.ID = "E002" : e.Name = "Luigi Bianchi" : e.Salary = 52000 : e.Dept = "HR"
ISAM APPEND EMP, e
e.ID = "E003" : e.Name = "Anna Verdi" : e.Salary = 61000 : e.Dept = "IT"
ISAM APPEND EMP, e

' --- 1. BUFFER pessimistico (mode 2) ---
PRINT "1. BUFFER pessimistico"
ISAM GO EMP, TOP
ISAM GET EMP, e
PRINT "   Originale: "; e.Name; " $"; e.Salary

ok = RDD.BUFFER(1, 2)
PRINT "   BUFFER(1,2): "; ok

' Indici di campo 1-based secondo l'ordine di dichiarazione del TYPE Employee:
' 1=ID, 2=Name, 3=Salary, 4=Dept (convenzione Clipper FieldGet/FieldPut).
RDD SET FIELD 1, 3, 99999
v = RDD.GETFIELD(1, 3)
PRINT "   Modificato Salary -> "; v

old = RDD.OLDVAL(1, 3)
PRINT "   OLDVAL(1,3): "; old

state = RDD.GETFLDSTATE(1, 3)
PRINT "   GETFLDSTATE(1,3): "; state

' Revert
RDD.TABLEREVERT(1)
ISAM GET EMP, e
PRINT "   Dopo TABLEREVERT: "; e.Name; " $"; e.Salary
PRINT

' --- 2. BUFFER optimistico (mode 3) ---
PRINT "2. BUFFER optimistico"
ISAM GO EMP, TOP
ISAM GET EMP, e

ok = RDD.BUFFER(1, 3)
PRINT "   BUFFER(1,3): "; ok

RDD SET FIELD 1, 3, 77000
RDD SET FIELD 1, 4, "R&D"

v = RDD.GETFIELD(1, 3)
PRINT "   Salary buffer: "; v
d$ = RDD.GETFIELD(1, 4)
PRINT "   Dept buffer: "; d$

' Commit
ok = RDD.TABLEUPDATE(1)
PRINT "   TABLEUPDATE(1): "; ok

ISAM GET EMP, e
PRINT "   Dopo commit: "; e.Name; " $"; e.Salary; " Dept:"; e.Dept
PRINT

' --- 3. SETREPROCESS ---
PRINT "3. SETREPROCESS"
RDD SET REPROCESS 3000
PRINT "   SETREPROCESS(3000) = retry 3 secondi"
RDD SET REPROCESS 0
PRINT "   SETREPROCESS(0) = nessun retry"
PRINT

' Verifica record non toccati
ISAM GO EMP, TOP
ISAM SKIP EMP
ISAM GET EMP, e
PRINT "   Record 2 invariato: "; e.Name; " $"; e.Salary

ISAM CLOSE EMP
KILL g_work + "/buf_demo.isdb"
PRINT
PRINT "=== Demo Buffer OK ==="
