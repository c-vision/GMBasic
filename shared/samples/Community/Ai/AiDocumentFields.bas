' FILE: samples/Community/Ai/AiDocumentFields.bas
' =================================================================================================
'       AI.DOCFIELDS / AI.DOCEXTRACT — estrazione strutturata da documenti
'
'       DOCFIELDS: euristiche deterministiche offline (fatture IT: numero, data, totale,
'       partita IVA) -> oggetto JSON. DOCEXTRACT: estrazione guidata da schema via
'       provider %CHAT attivo (-> JSON); senza provider, errore JSON esplicito (mai
'       eccezioni). Stessa semantica sui 4 backend. Vedi guida:
'       Guide/utente/AI-Documenti-Guide.md (vault).
' =================================================================================================

IMPORT AI

SUB Main()
    DIM inv AS STRING
    inv = "../../../samples/Fixtures/ai/fattura_it.txt"

    ' ── FIELDS: numero/data/totale/IVA (null se assenti) ──────────────────────
    PRINT "FIELDS: ", AI.DOCFIELDS(inv)

    ' ── Errore esplicito su file mancante ─────────────────────────────────────
    PRINT "FIELDS: ", AI.DOCFIELDS("inesistente.txt")

    ' ── EXTRACT con schema (richiede provider %CHAT; offline -> errore JSON) ──
    DIM schema AS STRING
    schema = "{""total"":""number"",""vat"":""string""}"
    PRINT "EXTRACT: ", AI.DOCEXTRACT(inv, schema)
    PRINT "EXTRACT: ", AI.DOCEXTRACT("inesistente.txt", schema)
END SUB
