' ClipperValidDemo.bas — @GET con WHEN e VALID (code block Clipper/xBase).
' VALID: il campo resta attivo finché la condizione non è vera (ri-prompt).
' WHEN: il campo viene saltato se la condizione è falsa.
SUB ChiediCodice()
    DYNAMIC codice AS STRING = ""
    DYNAMIC conferma AS STRING = ""
    @ 2, 2 SAY "Codice (min 2):"
    @ 2, 20 GET codice VALID Len(codice) >= 2
    @ 4, 2 SAY "Conferma (solo se codice non vuoto):"
    @ 4, 20 GET conferma WHEN Len(codice) >= 2
    READ
    PRINT "codice="; codice
    PRINT "conferma="; conferma
END SUB
CALL ChiediCodice()
