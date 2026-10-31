' FILE: samples/Community/Ai/AiDocFields.bas
' =================================================================================================
'       AI.DOCFIELDS — campi fattura strutturati (offline, deterministico)
'
'       Ritorna JSON {number,date,total,vat} da euristiche IT; campo assente = null.
'       Guida: Guide/utente/AI-Documenti-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM inv AS STRING
    inv = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/fattura_it.txt"

    PRINT "FIELDS: ", AI.DOCFIELDS(inv)

    ' File mancante → errore JSON esplicito.
    PRINT "FIELDS: ", AI.DOCFIELDS("fattura-inesistente.txt")
END SUB
