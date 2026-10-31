' Campo TYPE `STRING * N` a LUNGHEZZA FISSA: il valore va paddato a N caratteri
' (gmb_string_adjust_fixed) su tutti i backend — emitter C# lo memorizzava grezzo.
' EXPECT: [AB        ][C         ]|len= 10|trim= 1
TYPE T
    S AS STRING * 10
END TYPE
SUB Main()
    DIM a(1) AS T
    a(0).S = "AB"
    a(1).S = "C"
    PRINT "["; a(0).S; "]["; a(1).S; "]"
    PRINT "len="; LEN(a(0).S)
    PRINT "trim="; LEN(TRIM(a(1).S))
END SUB
