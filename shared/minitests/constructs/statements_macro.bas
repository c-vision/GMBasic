' EXPECT: m1= 42|m2= 10|m3= 5|m4= 15|m5= 5|m6= 15
' Sintassi canone: samples/Clipper/ClipperMacroCanary.bas (macro espressione
' &("...") e EVAL; la macro di COMANDO + interleaving PRINT ha ordine di uscita
' storico divergente IL/PCode — non esercitata qui).
GLOBAL X AS INTEGER
SUB Main()
    X = 10
    DIM m1 = &("42")
    DIM m2 = &("X")
    DIM m3 = &("2+3")
    &("X = X + 5")
    DIM m4 = &("X")
    DIM m5 = EVAL("2+3")
    DIM m6 = EVAL("X")
    PRINT "m1="; m1
    PRINT "m2="; m2
    PRINT "m3="; m3
    PRINT "m4="; m4
    PRINT "m5="; m5
    PRINT "m6="; m6
END SUB
