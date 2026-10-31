' MiniTest — AADD su array dinamico NON inizializzato (DIM e()): parità 4 backend.
' IL non convertiva il valore al tipo dell'elemento prima dello store (stelem.i2 con un
' double → e(0)=0); PCode convertiva l'array a List (TYPENAME "LIST`1"); emitter C#
' emetteva gmb_make_val_double(<int>) in un array gmb_int_t (C invalido).
' EXPECT: noidx|ARRAY|A|noidx2|ARRAY| 5  6
IMPORT SYSTEM

SUB Main()
    DIM e() AS INTEGER
    PRINT "noidx|"; TYPENAME(e); "|"; SYSTEM.VALTYPE(e)
    AADD e, 5
    AADD e, 6
    PRINT "noidx2|"; TYPENAME(e); "|"; e(0); e(1)
END SUB
