' Array di STRING (char**), SOLO INPUT: `a() AS STRING` passa al nativo un buffer di
' puntatori a stringhe UTF-8, valido durante la chiamata (le stringhe non si riscrivono).
' Con le estensioni automatiche il C riceve (a, n); con DECLARE NATIVE solo (a, n) esplicito.
' EXPECT: t= 6|t2= 6
DECLARE FUNCTION StrsTotal LIB "liblayout_ext" ALIAS "strs_total" (a() AS STRING) AS LONG
DECLARE NATIVE FUNCTION StrsTotalN LIB "liblayout_ext" ALIAS "strs_total" (a() AS STRING, n AS LONG) AS LONG

SUB Main()
    DIM a(2) AS STRING
    a(0) = "x" : a(1) = "yy" : a(2) = "zzz"
    PRINT "t="; StrsTotal(a)
    PRINT "t2="; StrsTotalN(a, 3)
END SUB
