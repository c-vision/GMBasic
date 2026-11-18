' MiniTest BITFIELD — larghezze e segno (2026-11-11): `campo AS <intero> WIDTH n` è un bitfield
' C: valore mascherato a n bit, esteso al segno per i tipi signed, impacchettato LSB-first
' nell'unità del tipo (8 byte per LONG/ULONG/QUAD/UQUAD, 2 per INTEGER/UINTEGER, 1 per
' BYTE/SBYTE), nuova unità quando il campo non ci sta. Copre 1..64 bit, signed/unsigned,
' attraversamento dell'unità, PACKED TYPE e SIZEOF.
' EXPECT: sz64= 8|a=-1|b=-1|c=-1|d=-25536|e= 123456789|cr= 4|x=-1|y= 1000|pb= 2|pa= 5|pb2= 17|pc= 3|fu= 8|f1= 1|f2= 2147483647|fw= 8|w= 123456789
TYPE Bits64
    A AS LONG WIDTH 1
    B AS LONG WIDTH 7
    C AS LONG WIDTH 8
    D AS LONG WIDTH 16
    E AS LONG WIDTH 32
END TYPE

TYPE Cross
    X AS INTEGER WIDTH 12
    Y AS INTEGER WIDTH 12
END TYPE

PACKED TYPE PBits
    A AS BYTE WIDTH 3
    B AS BYTE WIDTH 5
    C AS BYTE WIDTH 3
END TYPE

TYPE FlagsU
    F1 AS ULONG WIDTH 1
    F2 AS ULONG WIDTH 31
END TYPE

TYPE FullWidth
    W AS LONG WIDTH 64
END TYPE

SUB Main()
    DIM t AS Bits64
    PRINT "sz64="; SIZEOF(Bits64)
    t.A = 1 : t.B = 127 : t.C = 255 : t.D = 40000 : t.E = 123456789
    PRINT "a="; t.A
    PRINT "b="; t.B
    PRINT "c="; t.C
    PRINT "d="; t.D
    PRINT "e="; t.E

    DIM cr AS Cross
    PRINT "cr="; SIZEOF(Cross)
    cr.X = 4095 : cr.Y = 1000
    PRINT "x="; cr.X
    PRINT "y="; cr.Y

    DIM pb AS PBits
    PRINT "pb="; SIZEOF(PBits)
    pb.A = 5 : pb.B = 17 : pb.C = 3
    PRINT "pa="; pb.A
    PRINT "pb2="; pb.B
    PRINT "pc="; pb.C

    DIM fu AS FlagsU
    PRINT "fu="; SIZEOF(FlagsU)
    fu.F1 = 1 : fu.F2 = 2147483647
    PRINT "f1="; fu.F1
    PRINT "f2="; fu.F2

    DIM fw AS FullWidth
    PRINT "fw="; SIZEOF(FullWidth)
    fw.W = 123456789
    PRINT "w="; fw.W
END SUB
