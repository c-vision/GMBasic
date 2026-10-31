' ClipperGetFullDemo.bas — superficie @SAY/@GET/READ completa su DIFFERENTI clausole.
' Copre: @...SAY, @...GET con valore iniziale (prefill) e digitazione in coda,
' VALID (ri-prompt finche' falso), WHEN (campo saltato quando falso), DYNAMIC,
' READ (loop unificato), PRINT. Output deterministico:
'   nome=Mario  codice=ab  conferma=noS
' Input scriptato (GMB_TUI_SCRIPT): INVIO (nome gia' valido) + "a" INVIO (rifiutato) +
' "b" INVIO (codice=ab) + "S" INVIO (conferma=no + S).
SUB Anagrafica()
    DYNAMIC nome AS STRING = "Mario"
    DYNAMIC codice AS STRING = ""
    DYNAMIC conferma AS STRING = "no"
    @ 2, 2 SAY "Nome (prefill, min 5):"
    @ 2, 26 GET nome VALID Len(nome) >= 5
    @ 4, 2 SAY "Codice (min 2):"
    @ 4, 20 GET codice VALID Len(codice) >= 2
    @ 6, 2 SAY "Conferma (solo con codice):"
    @ 6, 32 GET conferma WHEN Len(codice) >= 2
    READ
    PRINT "nome="; nome
    PRINT "codice="; codice
    PRINT "conferma="; conferma
END SUB
CALL Anagrafica()
