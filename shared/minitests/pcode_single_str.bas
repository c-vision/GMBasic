' MiniTest STR$/CDBL/VAL di un SINGLE — regression guard.
' Bug: Bugs/(open) - PCode-STR-Single-Zero.md — in PCode SafeDouble(float) cadeva sul parse di ToString()
' (culture it-IT "1,25") e STR$ di un SINGLE dava 0 (IL: 1.25).
' EXPECT: 1.25|0.5|-2.5|2.25|3|0.1
SUB Main()
    DIM x AS SINGLE
    DIM y AS SINGLE
    DIM z AS SINGLE
    x = 1.25 : y = 0.5 : z = -2.5
    PRINT LTRIM$(STR$(x)) + "|" + LTRIM$(STR$(y)) + "|" + LTRIM$(STR$(z)) + "|" + LTRIM$(STR$(x + 1)) + "|" + LTRIM$(STR$(CINT(CDBL(x) + 1.75))) + "|" + LTRIM$(STR$(CSNG(0.1)))
END SUB
