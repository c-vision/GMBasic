' FILE: Json/JsonParseExtract.bas
' =================================================================================================
' JSON.PARSE / JSON.EXTRACT — parsing di una stringa JSON e lettura di campi.
' =================================================================================================
' JSON.PARSE(source$)        → oggetto JSON
' JSON.EXTRACT(obj, path$)   → valore del campo (stringa)
' =================================================================================================
IMPORT JSON

SUB Main()
    CLS
    PRINT "JSON.PARSE / JSON.EXTRACT"
    PRINT "========================="
    PRINT

    DIM q$ = CHR(34)
    DIM src$ = "{" + q$ + "name" + q$ + ": " + q$ + "Ada" + q$ + ", " + q$ + "city" + q$ + ": " + q$ + "Torino" + q$ + "}"

    PRINT "Sorgente: "; src$
    PRINT

    DIM obj = JSON.PARSE(src$)
    PRINT "name = "; JSON.EXTRACT(obj, "name")
    PRINT "city = "; JSON.EXTRACT(obj, "city")
END SUB
