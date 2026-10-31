' MiniTest DBF — campo nudo numerico late-binding (N/F -> CURRENCY), parità 4 backend.
' Un campo DBF nudo numerico (late-binding Clipper) deve rendere come il decimal PCode/IL:
' PRINT "100.00", CSTR " 100.00" (spazio iniziale), aritmetica/confronti numerici.
' Fixture: samples/Fixtures/cust.dbf, campo BALANCE N(12,2) = 100.00 (primo record).
' EXPECT: mul= 200|add= 101|cmp=si|str= 100.00|len= 7
IMPORT DBF WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS

SUB Main()
    DBF USE "../../samples/Fixtures/cust.dbf" ALIAS C
    DBF GO TOP
    PRINT "mul="; BALANCE * 2
    PRINT "add="; BALANCE + 1
    IF BALANCE > 50 THEN PRINT "cmp=si" ELSE PRINT "cmp=no"
    PRINT "str="; CONVERSIONS.CSTR(BALANCE)
    PRINT "len="; LEN(CONVERSIONS.CSTR(BALANCE))
END SUB
