' RDD_Demo.bas — minimal test
IMPORT IO
IMPORT ISAM
IMPORT DBF
IMPORT RDD

PRINT "=== RDD Demo ==="

' SETDEFAULT
RDD SET DEFAULT "DBF"
RDD SET DEFAULT "ISAM"
PRINT "SETDEFAULT OK"

' Date/Currency
d = RDD.TODAY()
PRINT "TODAY: "; d
c = RDD.CURVAL(99.99)
PRINT "CURVAL: "; c
' RDD.FLOAT: conversione numerica del driver (stessa semantica di CURVAL sui numeri).
f = RDD.FLOAT(12.5)
PRINT "FLOAT: "; f

' SETLOCKTIMEOUT
RDD SET LOCK TIMEOUT 2000
RDD SET LOCK TIMEOUT 0
PRINT "LOCKTIMEOUT OK"

PRINT "=== Demo OK ==="

' --- Auto-detect driver (VIA opzionale) ---
PRINT "4. Auto-detect USE"
PRINT "   RDD.USE 'file.dbf' ALIAS X  -> auto DBF"
PRINT "   RDD.USE ':memory:' ALIAS M  -> auto ARRAY"
PRINT
