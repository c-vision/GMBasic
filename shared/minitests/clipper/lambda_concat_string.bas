' CLIPPER 5.3 — block con concat stringa: varianti del tipo dell'operando.
' Copre: letterale, variabile globale stringa, call stringa, param solo-concat,
' variabile locale catturata, due param annotati AS STRING.
' EXPECT: l1=X!|l2=x!|l3=!x|l4=aX|l5= 9a|loc=x!|ann=ab
GLOBAL gSuffix AS STRING
GLOBAL gPrefix AS STRING
SUB Main()
    gSuffix = "!"
    gPrefix = "!"
    DIM b1 = { |s| RETURN UPPER(s) + "!" }
    PRINT "l1="; EVAL(b1, "x")      ' X!
    DIM b2 = { |s| RETURN s + "!" }
    PRINT "l2="; EVAL(b2, "x")      ' x!
    DIM b3 = { |s| RETURN gPrefix + s }
    PRINT "l3="; EVAL(b3, "x")      ' !x
    DIM b4 = { |s| RETURN s + UPPER("x") }
    PRINT "l4="; EVAL(b4, "a")      ' aX
    DIM b5 = { |s| RETURN STR(9) + s }
    PRINT "l5="; EVAL(b5, "a")      ' " 9a"
    DIM loc AS STRING = "!"
    DIM b6 = { |s| RETURN s + loc }
    PRINT "loc="; EVAL(b6, "x")     ' x!
    DIM b7 = { |s AS STRING, t AS STRING| RETURN s + t }
    PRINT "ann="; EVAL(b7, "a", "b") ' ab
END SUB
