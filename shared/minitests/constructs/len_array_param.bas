' LEN(array PARAMETRO): l'ABI nativa non porta la lunghezza → C99Native non compila.
' EXPECT: len= 3
' BUG APERTO (Bugs/(open) - Clipper-LEN-Array-Parametro-C99Native.md):
' IL/PCode/VM-C PASS; C99Native "use of undeclared identifier 'a_len'".
SUB F(a())
    PRINT "len="; LEN(a)
END SUB
SUB Main()
    DIM v = {1, 2, 3}
    F(v)
END SUB
