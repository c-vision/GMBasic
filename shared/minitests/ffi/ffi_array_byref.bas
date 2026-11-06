' EXPECT: v0= 0|v1= 10|v4= 40
' Array BYREF (2026-09-28): il parametro array viaggia come puntatore nativo — nessun
' bound auto-iniettato, n e' un parametro AS LONG separato (design decision #1, vedi
' AiWiki Bugs/(open) - FFI-Marshalling-Limitato-No-BYREF-Array-Struct.md punto 1).
DECLARE SUB VecFill LIB "libvec_module" ALIAS "vec_fill" (buf() AS DOUBLE)

SUB Main()
    DIM v(5) AS DOUBLE
    VecFill(v)
    PRINT "v0="; v(0)
    PRINT "v1="; v(1)
    PRINT "v4="; v(4)
END SUB
