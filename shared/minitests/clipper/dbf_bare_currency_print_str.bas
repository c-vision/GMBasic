' MiniTest DBF — PRINT e STR$ di un campo nudo CURRENCY (N/F), parità 4 backend.
' Un campo DBF nudo numerico (late-binding Clipper) stampato direttamente con PRINT
' deve rendere " 100.00" (spazio-segno + scale, come CSTR/IL/PCode/VM-C), mentre STR$
' (numeric-to-string) deve rendere " 100" (gmb_str_basic, NON i decimali del currency).
' Fixture: samples/Fixtures/cust.dbf, campo BALANCE N(12,2) = 100.00 (primo record).
' EXPECT: print= 100.00|str= 100|cstr= 100.00
IMPORT DBF WITH ALIAS
IMPORT CONVERSIONS WITH ALIAS

SUB Main()
    DBF USE "../../samples/Fixtures/cust.dbf" ALIAS C
    DBF GO TOP
    PRINT "print="; BALANCE
    PRINT "str="; STR(BALANCE)
    PRINT "cstr="; CONVERSIONS.CSTR(BALANCE)
END SUB
