' TAG: interactive
' @GET VALID (Clipper/xBase): il campo resta attivo finché la condizione è falsa.
' Input scriptato: "a"+INVIO (len 1 → rifiutato), "b"+INVIO → "ab".
' EXPECT: gv=ab
SUB Chiedi()
    DYNAMIC gv AS STRING = ""
    @ 2, 2 SAY "Codice (>=2):"
    @ 2, 18 GET gv VALID Len(gv) >= 2
    READ
    PRINT "gv="; gv
END SUB
CALL Chiedi()
