' MiniTest — TYPEINFO.HAS(v) (1 se non NIL, 0 se NIL) + TYPEINFO(UDT).
' EXPECT: h.cur= 1|h.nil= 0|h.num= 1|h.str= 1|tn.udt=Person|h.udt= 1
IMPORT DBF WITH ALIAS
IMPORT TYPEINFO

TYPE Person
    Nome AS STRING * 10
    Eta AS INTEGER
END TYPE

SUB Main()
    DBF USE "../../samples/Fixtures/cust.dbf" ALIAS C
    DBF GO TOP
    DIM u AS Person
    u.Nome = "ANN"
    PRINT "h.cur="; TYPEINFO.HAS(BALANCE)
    PRINT "h.nil="; TYPEINFO.HAS(NIL)
    PRINT "h.num="; TYPEINFO.HAS(1)
    PRINT "h.str="; TYPEINFO.HAS(NAME)
    PRINT "tn.udt="; TYPEINFO(u)
    PRINT "h.udt="; TYPEINFO.HAS(u)
END SUB
