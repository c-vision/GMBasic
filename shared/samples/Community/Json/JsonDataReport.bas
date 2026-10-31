' FILE: Json/JsonDataReport.bas
' =================================================================================================
' Report ordini da JSON — sample completo che esercita TUTTE le funzioni JSON:
'   JSON.PARSE      — parsing del documento
'   JSON.EXTRACT    — chiavi annidate ("customer.name") e indici array ("tags[0]")
'   JSON.TOJSON     — roundtrip dell'oggetto parsato
'   JSON.STRINGIFY  — serializzazione di collezioni native (list/dict)
' =================================================================================================
' NOTA parità (F2 2026-09-11): i path sono a segmenti (chiave annidata o [i] come
' segmento FINALE). Il path misto [i].key pianta la pipeline emitter C# (bug
' tracciato) — non usare "items[1].sku" finché non corretto.
' =================================================================================================
IMPORT JSON

SUB Main()
    CLS
    PRINT "=== Report ordine da JSON ==="
    PRINT

    ' --- 1) Documento sorgente (config/ordine) ---
    DIM q$ = CHR(34)
    DIM src$ = "{" + q$ + "customer" + q$ + ": {" + q$ + "name" + q$ + ": " + q$ + "Ada" + q$ + ", " + q$ + "city" + q$ + ": " + q$ + "Torino" + q$ + ", " + q$ + "id" + q$ + ": 7}, " + q$ + "tags" + q$ + ": [" + q$ + "priority" + q$ + ", " + q$ + "retail" + q$ + "]}"
    PRINT "Sorgente: "
    PRINT src$
    PRINT

    ' --- 2) Parse del documento ---
    DIM order = JSON.PARSE(src$)

    ' --- 3) Estrazione con chiavi annidate ---
    PRINT "Cliente:   "; JSON.EXTRACT(order, "customer.name")
    PRINT "Citta':    "; JSON.EXTRACT(order, "customer.city")
    PRINT "Id:        "; JSON.EXTRACT(order, "customer.id")
    PRINT

    ' --- 4) Indici di array (segmento finale) ---
    ' Gamba gate emitter C# pendente: i path [i] piantano la pipeline C#
    ' (bug preesistente tracciato: emitter C#-Orchestrator-Hang-Mixed-Extract-Path).
    ' Verificato verde su IL/PCode/VMC/C99Native.
    PRINT "Tag[0]:    "; JSON.EXTRACT(order, "tags[0]")
    PRINT "Tag[1]:    "; JSON.EXTRACT(order, "tags[1]")
    PRINT

    ' --- 5) Roundtrip: serializzazione dell'oggetto parsato ---
    PRINT "TOJSON: "
    PRINT JSON.TOJSON(order)
    PRINT

    ' --- 6) STRINGIFY di collezioni native ---
    DIM shipped AS LIST OF STRING = NEW LIST OF STRING
    shipped.Add(JSON.EXTRACT(order, "customer.city"))
    shipped.Add(JSON.EXTRACT(order, "tags[0]"))
    PRINT "SPEDITI:   "; JSON.STRINGIFY(shipped)

    DIM summary AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
    summary.Set("cliente", JSON.EXTRACT(order, "customer.name"))
    summary.Set("stato", "shipped")
    PRINT "SUMMARY:   "; JSON.STRINGIFY(summary)

    PRINT
    PRINT "=== FINE REPORT ==="
END SUB
