' MiniTest — SYSTEM.TYPEINFO (semantica IlRuntime.TypeInfo, diversa da TYPENAME):
' scalari (currency/bool), UDT, DBF campo nudo, dict ("DICT"), lista/array ("LIST OF <elem>").
' EXPECT: cur=CURRENCY|str=STRING|num=DOUBLE|bool=BIT|nil=NIL|dict=DICT|list=LIST OF DOUBLE|arr=LIST OF DOUBLE
IMPORT DBF WITH ALIAS
IMPORT COLLECTIONS
IMPORT SYSTEM

SUB Main()
    DBF USE "../../samples/Fixtures/cust.dbf" ALIAS C
    DBF GO TOP
    DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
    DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER
    l.Add(1)
    DIM a(3) AS INTEGER
    PRINT "cur="; SYSTEM.TYPEINFO(BALANCE)
    PRINT "str="; SYSTEM.TYPEINFO(NAME)
    PRINT "num="; SYSTEM.TYPEINFO(1)
    PRINT "bool="; SYSTEM.TYPEINFO(TRUE)
    PRINT "nil="; SYSTEM.TYPEINFO(NIL)
    PRINT "dict="; SYSTEM.TYPEINFO(d)
    PRINT "list="; SYSTEM.TYPEINFO(l)
    PRINT "arr="; SYSTEM.TYPEINFO(a)
END SUB
