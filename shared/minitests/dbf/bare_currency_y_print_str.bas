' MiniTest DBF — campo nudo CURRENCY di tipo Y (Currency, 8 byte): parità 4 backend.
' Un campo Y letto come valore (late-binding Clipper) deve essere CURRENCY (numerico),
' NON una stringa: PRINT/CSTR con lo spazio-segno, STR$ numerico, VALTYPE "N",
' TYPENAME "CURRENCY" e confronti numerici. Prima i backend C (VM/C, C99, C99Native)
' lo leggevano come STRINGA (VALTYPE "C", TYPENAME "STRING", STR$ 0, confronto falso).
' Self-contained (/tmp).
' EXPECT: print= 100|str= 100|cstr= 100|vt=N|tn=CURRENCY|cmp=yes
IMPORT DBF
IMPORT SYSTEM
IMPORT CONVERSIONS

TYPE Sale
    Code   AS STRING * 10
    Amount AS CURRENCY
END TYPE

SUB Main()
    DBF CREATE "/tmp/gmb_mt_ycur.dbf" FROM TYPE Sale
    DBF USE "/tmp/gmb_mt_ycur.dbf" ALIAS Y EXCLUSIVE NEW
    DBF APPEND BLANK
    DBF REPLACE Code WITH "A", Amount WITH 100.00
    DBF GO TOP
    PRINT "print="; Amount
    PRINT "str="; STR(Amount)
    PRINT "cstr="; CONVERSIONS.CSTR(Amount)
    PRINT "vt="; SYSTEM.VALTYPE(Amount)
    PRINT "tn="; TYPENAME(Amount)
    IF Amount > 50.00 THEN PRINT "cmp=yes" ELSE PRINT "cmp=no"
    DBF CLOSE DATABASES
END SUB
