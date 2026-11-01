' FILE: samples/Ai/AiDocExtract.bas
' =================================================================================================
'       AI.DOCEXTRACT — estrazione guidata da schema JSON via provider %CHAT
'
'       Richiede un provider LLM attivo; offline ritorna un errore JSON esplicito
'       (il programma NON si interrompe). Guida: Guide/utente/AI-Documenti-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM inv AS STRING
    inv = "../Fixtures/ai/fattura_it.txt"

    DIM schema AS STRING
    schema = "{""total"":""number"",""vat"":""string""}"

    PRINT "EXTRACT: ", AI.DOCEXTRACT(inv, schema)

    ' File mancante → errore JSON esplicito.
    PRINT "EXTRACT: ", AI.DOCEXTRACT("fattura-inesistente.txt", schema)
END SUB
