' FILE: AI/VectorSearchJsonParsing.bas
' =================================================================
' Parsing JSON da AI.SEARCH con JSON.PARSE
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS
IMPORT JSON

SUB Main()
    PRINT "Vector Search JSON Parsing"
    PRINT "========================="
    PRINT
    
    ' --- Setup ---
    SQL OPEN ":memory:" ALIAS JsonDb
    AI RAG ENSURE COLLECTION "JsonDb", "json_test", 8
    
    ' --- Index documenti ---
    AI RAG INDEX TEXT "JsonDb", "json_test", "doc1", "La casa e' in vendita a 250000 euro."
    AI RAG INDEX TEXT "JsonDb", "json_test", "doc2", "L'appartamento ha 3 camere e giardino."
    PRINT
    
    ' --- Search e parse JSON ---
    DIM json$ = AI.SEARCH("JsonDb", "json_test", "casa vendita", 5)
    PRINT "JSON raw:"
    PRINT json$
    PRINT
    
    ' --- Estrai campo status ---
    DIM result = JSON.PARSE(json$)
    PRINT "Status: "; JSON.Extract(result, "status")
    PRINT
    
    ' --- Estrai singoli hits (no WHILE loop per compatibilita P-CODE) ---
    DIM hit0 = JSON.Extract(result, "hits[0]")
    PRINT "Hit 0 sourceId: "; JSON.Extract(hit0, "sourceId")
    PRINT "Hit 0 score: "; JSON.Extract(hit0, "score")
    
    DIM hit1 = JSON.Extract(result, "hits[1]")
    PRINT "Hit 1 sourceId: "; JSON.Extract(hit1, "sourceId")
    PRINT "Hit 1 score: "; JSON.Extract(hit1, "score")
    PRINT
    
    PRINT "Test Completato."
END SUB
