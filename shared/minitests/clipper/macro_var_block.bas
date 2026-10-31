' CLIPPER 5.3 — macro su VARIABILE con code block LETTERALE (espansione compile-time).
' RISOLTO 2026-10-02 (sorgente unica: Linker RewriteMacroExpr, commit 545b782e6; tracking
' parser rimosso come duplicato in 2ee5fbde4): la variabile assegnata UNA volta a un letterale
' e' espansa a compile-time come la forma costante -> parita' 5/5. Vedi
' Bugs/(solved) - Clipper-Macro-Variabile-Block-Runtime-5-Backend.md.
' EXPECT: g100= 122|g200= 244|area= 42|up=X!
GLOBAL TaxRate AS DOUBLE
SUB Main()
    TaxRate = 0.22
    DIM cBlockDef AS STRING = "{ |p| RETURN p * (1 + TaxRate) }"
    DIM bGross = &(cBlockDef)
    PRINT "g100="; EVAL(bGross, 100)      ' 122
    PRINT "g200="; EVAL(bGross, 200)      ' 244
    DIM cAreaDef AS STRING = "{ |w, h| RETURN w * h }"
    DIM bArea = &(cAreaDef)
    PRINT "area="; EVAL(bArea, 6, 7)      ' 42
    DIM cUpDef AS STRING = "{ |s| RETURN UPPER(s) + ""!"" }"
    DIM bUp = &(cUpDef)
    PRINT "up="; EVAL(bUp, "x")           ' X!
END SUB
