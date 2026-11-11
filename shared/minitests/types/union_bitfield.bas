' MiniTest UNION e BITFIELD (2026-11-11): un UNION dichiara campi SOVRAPPOSTI (semantica C):
' scrivere un campo e leggerne un altro restituisce la reinterpretazione dei byte, identica su
' tutte e 4 le gambe. Un campo `... WIDTH n` è un BITFIELD: valore mascherato a n bit e (per i
' tipi signed) esteso al segno, come in C (`int64_t A : 3` con A = 5 vale -3).
' EXPECT: i= 42|f= 2.07507571253324E-322|i2= 42|a=-3|b= 4|a2= 1|raw= 5|sz= 8|usz= 8
UNION Val
    I AS LONG       ' 8 byte (canone del linguaggio: LONG = 8)
    F AS DOUBLE
    Q AS QUAD
END UNION

TYPE Flags
    A AS LONG WIDTH 3
    B AS LONG WIDTH 4
END TYPE

' Union annidata in un TYPE + campo normale
TYPE Wrap
    Tag AS LONG
    V AS Val
END TYPE

SUB Main()
    DIM u AS Val
    u.I = 42
    PRINT "i="; u.I
    PRINT "f="; u.F            ' reinterpretazione dei byte: double con bit pattern 42
    u.F = 2.07507571253324E-322
    PRINT "i2="; u.I           ' ...e ritorno: 42

    DIM g AS Flags
    g.A = 5 : g.B = 4
    PRINT "a="; g.A            ' 5 su 3 bit signed = -3 (C)
    PRINT "b="; g.B
    g.A = 9                    ' 9 AND 7 = 1
    PRINT "a2="; g.A

    ' Union annidata: stessa semantica dentro un TYPE
    DIM w AS Wrap
    w.V.I = 5
    PRINT "raw="; w.V.I

    PRINT "sz="; SIZEOF(Flags)    ' due bitfield LONG = una unità da 8 byte
    PRINT "usz="; SIZEOF(Val)     ' massimo dei campi
END SUB
