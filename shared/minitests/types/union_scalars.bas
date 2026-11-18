' MiniTest UNION — matrice dei tipi ammessi (2026-11-11): tutti i campi di una union partono
' dall'offset 0, quindi scrivere un campo e leggerne un altro dà la reinterpretazione dei byte
' (little-endian), come in C. Ammessi (v1): i tipi con la STESSA larghezza in memoria sulle 4
' gambe — BYTE/SBYTE (1), LONG/ULONG/QUAD/UQUAD (8), DOUBLE (8). Copre scritture parziali,
' segno (BYTE vs SBYTE), copia per valore, array di union e passaggio BYREF.
' EXPECT: sz= 8|nf= 7|l= 258|b= 2|sb= 2|b2= 255|l2= 511|sb2=-1|b3= 0|d= 8|copy= 8|arr0= 1|arr1= 2|byref= 7
UNION UAll
    B AS BYTE
    SB AS SBYTE
    L AS LONG
    UL AS ULONG
    Q AS QUAD
    UQ AS UQUAD
    D AS DOUBLE
END UNION

SUB ChangeRef(BYREF u AS UAll)
    u.L = 7
END SUB

SUB Main()
    DIM u AS UAll
    PRINT "sz="; SIZEOF(UAll)
    PRINT "nf="; UDTPACKED(u)

    ' Scrittura a 8 byte: le viste strette leggono i byte bassi
    u.L = 258
    PRINT "l="; u.L
    PRINT "b="; u.B
    PRINT "sb="; u.SB

    ' Scrittura di un byte: gli ALTRI byte della storage restano quelli di prima (0x0102 -> 0x01FF),
    ' e SBYTE legge il segno del byte scritto
    u.B = 255
    PRINT "b2="; u.B
    PRINT "l2="; u.L
    PRINT "sb2="; u.SB

    ' Scrittura di un double: la vista BYTE legge il byte basso di 8.0 (= 0)
    u.D = 8
    PRINT "b3="; u.B
    PRINT "d="; u.D

    ' Copia per valore: v non segue le modifiche di u
    DIM v AS UAll
    v = u
    u.D = 1
    PRINT "copy="; v.D

    ' Array di union: ogni elemento ha la propria storage
    DIM arr(2) AS UAll
    arr(0).L = 1 : arr(1).L = 2
    PRINT "arr0="; arr(0).L
    PRINT "arr1="; arr(1).L

    ' BYREF: la SUB scrive nella union del chiamante
    ChangeRef(u)
    PRINT "byref="; u.L
END SUB
