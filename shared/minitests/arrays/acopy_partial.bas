' ACOPY parziale (start/count/destStart) in-place + default elemento numerico = 0.0
' su array DIM senza inizializzatore (parita' IL/C99 vs PCode/VM-C, 2026-09-24).
' EXPECT: copy= 0 | 0 | 20 | 30 | 0|def= 0 | 0 | 0
IMPORT ARRAYS
SUB Main()
    DIM src(4) = {10, 20, 30, 40, 50}
    DIM dst(4) AS DOUBLE
    ACOPY src, dst, 1, 2, 2
    PRINT "copy="; dst(0); "|"; dst(1); "|"; dst(2); "|"; dst(3); "|"; dst(4)
    DIM z(2) AS DOUBLE
    PRINT "def="; z(0); "|"; z(1); "|"; z(2)
END SUB
