' RDD_Relation.bas — SET RELATION demo
' ===================================================================
IMPORT IO
IMPORT ISAM
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

PRINT "=== SET RELATION Demo ==="
PRINT

TYPE CustT
    ID AS STRING * 8
    Name AS STRING * 32
END TYPE

KILL g_work + "/rel_cust.isdb"
ON ERROR RESUME NEXT
DIM c AS CustT
ISAM OPEN g_work + "/rel_cust.isdb" OF CustT ALIAS CUST INDEXED BY ID
c.ID = "C01" : c.Name = "Mario" : ISAM APPEND CUST, c
c.ID = "C02" : c.Name = "Luigi" : ISAM APPEND CUST, c
c.ID = "C03" : c.Name = "Anna"  : ISAM APPEND CUST, c
PRINT "Clienti: 3"

TYPE OrdT
    OID AS STRING * 8
    CustID AS STRING * 8
    Amount AS DOUBLE
END TYPE

KILL g_work + "/rel_ord.isdb"
ON ERROR RESUME NEXT
DIM o AS OrdT
ISAM OPEN g_work + "/rel_ord.isdb" OF OrdT ALIAS ORD INDEXED BY OID
o.OID = "O01" : o.CustID = "C01" : o.Amount = 150 : ISAM APPEND ORD, o
o.OID = "O02" : o.CustID = "C01" : o.Amount = 320 : ISAM APPEND ORD, o
o.OID = "O03" : o.CustID = "C02" : o.Amount = 89  : ISAM APPEND ORD, o
PRINT "Ordini: 3"
PRINT

PRINT "Relazione Ordini->Clienti via CustID"
PRINT "CoreDb.SetRelation sincronizza automaticamente"
PRINT

ISAM CLOSE CUST : ISAM CLOSE ORD
KILL g_work + "/rel_cust.isdb" : KILL g_work + "/rel_ord.isdb"
PRINT "=== OK ==="
