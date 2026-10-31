' FILE: Json/JsonExtractPath.bas
' =================================================================================================
' JSON.EXTRACT — percorsi ANNIDATI (.k) e INDICI DI ARRAY ([i] come segmento finale),
' con roundtrip JSON.TOJSON.
' =================================================================================================
' JSON.PARSE(source$)          → oggetto JSON
' JSON.EXTRACT(obj, path$)     → valore al percorso: "user.name", "tags[0]", "items"
' JSON.TOJSON(value)           → serializzazione JSON (ordine di inserimento per i dict)
' NOTA: il path misto [i].key (es. "items[1].sku") pianta la pipeline emitter C#
' (hang in Orchestrator/emitter C#, bug preesistente tracciato in vault) — non usare
' fino a fix; su IL/PCode/VMC/C99Native funziona.
' =================================================================================================
IMPORT JSON

SUB Main()
    CLS
    PRINT "JSON.EXTRACT — path annidati e indici array"
    PRINT "==========================================="
    PRINT

    DIM q$ = CHR(34)
    DIM src$ = "{" + q$ + "user" + q$ + ":{" + q$ + "name" + q$ + ": " + q$ + "Ada" + q$ + ", " + q$ + "id" + q$ + ": 7}, " + q$ + "items" + q$ + ": [{" + q$ + "sku" + q$ + ": " + q$ + "B1" + q$ + "}, {" + q$ + "sku" + q$ + ": " + q$ + "B2" + q$ + "}], " + q$ + "tags" + q$ + ": [" + q$ + "home" + q$ + ", " + q$ + "dev" + q$ + "]}"
    PRINT "Sorgente: "; src$
    PRINT

    DIM obj = JSON.PARSE(src$)

    PRINT "-- Campi piatti --"
    PRINT "user.name = "; JSON.EXTRACT(obj, "user.name")
    PRINT "user.id   = "; JSON.EXTRACT(obj, "user.id")
    PRINT

    PRINT "-- Indici di array (segmento finale) --"
    PRINT "tags[0] = "; JSON.EXTRACT(obj, "tags[0]")
    PRINT "tags[1] = "; JSON.EXTRACT(obj, "tags[1]")
    PRINT

    PRINT "-- Roundtrip JSON.TOJSON --"
    PRINT "ToJSON: "; JSON.TOJSON(obj)
END SUB
