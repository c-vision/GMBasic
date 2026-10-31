' TAG: interactive
' @GET WHEN (Clipper/xBase): se il predicato e' .F. il campo viene SALTATO
' (nessun fuoco, nessun tasto consumato) e la variabile resta invariata.
' Input: "ab"+INVIO -> a="ab"; WHEN a="xy" e' falso -> b resta "orig".
' EXPECT: a=ab|b=orig
SUB Chiedi()
    DYNAMIC a AS STRING = ""
    DYNAMIC b AS STRING = "orig"
    @ 2, 2 SAY "A:"
    @ 2, 10 GET a
    @ 3, 2 SAY "B (solo se A=xy):"
    @ 3, 24 GET b WHEN a = "xy"
    READ
    PRINT "a="; a
    PRINT "b="; b
END SUB
CALL Chiedi()
