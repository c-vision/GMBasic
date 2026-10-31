' RDD_Memory_Demo.bas — Database in memoria (ARRAY RDD)
' ===================================================================
IMPORT IO
IMPORT RDD

PRINT "=== Memory Database Demo ==="
PRINT

' 1. Apri database in memoria
PRINT "1. Apertura ARRAY RDD"
RDD USE ":memory:" VIA "ARRAY" ALIAS Temp
PRINT "   OK"
PRINT

' 2. Lock + Buffer
PRINT "2. Lock + Buffer"
ok = RDD.RLOCK(1)
PRINT "   RLOCK(1): "; ok
RDD SET LOCK TIMEOUT 500
RDD UNLOCK 1
ok = RDD.BUFFER(1, 3)
PRINT "   BUFFER(1,3): "; ok
RDD.TABLEREVERT(1)
PRINT

' 3. Eventi
PRINT "3. Eventi ONEVENT/OFFEVENT"
RDD ON EVENT "BeforeAppend"
RDD ON EVENT "AfterAppend"
RDD OFF EVENT "BeforeAppend"
PRINT "   OK"
PRINT

' 4. Date/Currency/SETREPROCESS
PRINT "4. Tipi + SETREPROCESS"
d = RDD.TODAY()
PRINT "   TODAY: "; d
c = RDD.CURVAL(49.99)
PRINT "   CURVAL: "; c
RDD SET REPROCESS 1000
RDD SET REPROCESS 0
PRINT "   SETREPROCESS OK"
PRINT

PRINT "=== Memory Demo OK ==="
