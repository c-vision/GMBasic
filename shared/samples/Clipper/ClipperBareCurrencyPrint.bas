' FILE: Clipper/ClipperBareCurrencyPrint.bas
'
' Sample completo della superficie "campo DBF nudo numerico (CURRENCY)" e delle
' coercizioni di STR$ (numeric-to-string) — parità sui 4 backend.
'
' Copre:
'   - PRINT di un campo nudo CURRENCY (late-binding Clipper) → " 100.00"
'     (spazio-segno + decimali del currency, NON i decimali troncati di STR$);
'   - STR$ di un campo nudo CURRENCY → " 100" (gmb_str_basic);
'   - CONVERSIONS.CSTR di un campo nudo CURRENCY → " 100.00";
'   - confronto relazionale su un campo nudo CURRENCY (type-aware, non stringa);
'   - SYSTEM.VALTYPE / TYPENAME del valore del campo;
'   - coercizioni STR$: booleano (TRUE=-1, FALSE=0), NIL (→0), campo stringa (→0).
'
' Scrive gli artefatti in work/ accanto al sample (mai nella root del repo).
IMPORT DBF WITH ALIAS
IMPORT IO
IMPORT CONVERSIONS
IMPORT SYSTEM

TYPE Sale
    Code   AS STRING * 10
    Amount AS CURRENCY
END TYPE

SUB Prep()
    IF NOT IO.DIREXISTS("work") THEN MKDIR "work"
    IF IO.FILEEXISTS("work/cur.dbf") THEN KILL "work/cur.dbf"
    IF IO.FILEEXISTS("work/cur.ntx") THEN KILL "work/cur.ntx"
    DBF CREATE "work/cur.dbf" FROM TYPE Sale
    DBF USE "work/cur.dbf" ALIAS Cur EXCLUSIVE NEW
    DBF APPEND BLANK
    DBF REPLACE Code WITH "ALICE", Amount WITH 100.00
    DBF APPEND BLANK
    DBF REPLACE Code WITH "BOB", Amount WITH 250.50
    DBF GO TOP
END SUB

SUB Main()
    Prep()

    PRINT "--- campo nudo CURRENCY ---"
    PRINT "print="; Amount
    PRINT "str="; STR(Amount)
    PRINT "cstr="; CONVERSIONS.CSTR(Amount)
    IF Amount > 50.00 THEN PRINT "cmp=yes" ELSE PRINT "cmp=no"
    PRINT "valtype="; SYSTEM.VALTYPE(Amount)
    PRINT "typename="; TYPENAME(Amount)

    PRINT "--- coercizioni STR$ ---"
    PRINT "bool_t="; STR(TRUE)
    PRINT "bool_f="; STR(FALSE)
    PRINT "nil="; STR(NIL)
    PRINT "strfield="; STR(Code)

    PRINT "--- secondo record ---"
    DBF GO BOTTOM
    PRINT "print2="; Amount
    PRINT "str2="; STR(Amount)
    PRINT "cstr2="; CONVERSIONS.CSTR(Amount)
    IF Amount > 50.00 THEN PRINT "cmp2=yes" ELSE PRINT "cmp2=no"

    DBF CLOSE DATABASES
END SUB
