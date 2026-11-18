' MiniTest NEGATIVO — EMBEDFILE incorpora TESTO: un payload con byte NUL (binario) deve
' essere RIFIUTATO alla compilazione, non incorporato troncato al primo NUL.
' La risorsa shared/samples/Fixtures/embed_binary.bin contiene 41 00 FF 42.
' EXPECT: COMPILE ERROR: EMBEDFILE: il file contiene un byte NUL
EMBEDFILE "../../samples/Fixtures/embed_binary.bin" AS t$

SUB Main()
    PRINT LEN(t$)
END SUB
