' FILE: Databases/VectorDbAdmin.bas
' =================================================================
' VECTORDB — modulo database vettoriale AUTONOMO su SQLite.
' Comandi legacy a spazio + funzioni qualificate VECTORDB.XXX(...).
' Nessuna dipendenza da AI: il motore usa embedding deterministici.
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT VECTORDB WITH ALIAS
IMPORT JSON

SUB Main()
    PRINT "VECTORDB Database Admin"
    PRINT "======================="
    PRINT

    ' --- 1. Crea collezioni in :memory: ---
    SQL OPEN ":memory:" ALIAS AdminDb
    VECTORDB ENSURE COLLECTION "AdminDb", "knowledge"
    VECTORDB ENSURE COLLECTION "AdminDb", "documents"
    VECTORDB ENSURE COLLECTION "AdminDb", "logs"
    PRINT "Create 3 collezioni."
    PRINT

    ' --- 2. Lista collezioni (funzione qualificata) ---
    PRINT "--- Lista collezioni ---"
    DIM colls$ = VECTORDB.LISTCOLLECTIONS("AdminDb")
    PRINT colls$
    PRINT

    ' --- 3. Index testi (comando legacy) ---
    VECTORDB INDEX TEXT "AdminDb", "knowledge", "k1", "Vettori e motori di search semantico"
    VECTORDB INDEX TEXT "AdminDb", "knowledge", "k2", "Retrieval Augmented Generation con LLM"
    VECTORDB INDEX TEXT "AdminDb", "documents", "d1", "Documento tecnico sui database vettoriali"
    PRINT

    ' --- 4. Index con testo + metadata (comando legacy) ---
    VECTORDB INDEX META "AdminDb", "documents", "d2", "Report annuale 2026", "{ ""type"": ""report"", ""year"": 2026 }"
    PRINT

    ' --- 5. Conteggi per collezione (funzione qualificata) ---
    PRINT "knowledge count: "; VECTORDB.COUNT("AdminDb", "knowledge")
    PRINT "documents count: "; VECTORDB.COUNT("AdminDb", "documents")
    PRINT "logs count: "; VECTORDB.COUNT("AdminDb", "logs")
    PRINT

    ' --- 6. Count come comando legacy INTO ---
    VECTORDB COUNT "AdminDb", "knowledge" INTO nKb
    PRINT "knowledge count (legacy): "; nKb
    PRINT

    ' --- 7. Ricerca semantica come comando legacy INTO ---
    PRINT "--- Search (comando legacy) ---"
    VECTORDB SEARCH "AdminDb", "knowledge", "database vettoriali", 3 INTO ris$
    PRINT ris$
    PRINT

    ' --- 8. Ricerca semantica come funzione qualificata ---
    PRINT "--- Search (funzione) ---"
    DIM ris2$ = VECTORDB.SEARCH("AdminDb", "knowledge", "retrieval e generazione", 2)
    PRINT ris2$
    PRINT

    ' --- 9. Rimozione sorgente (comando legacy) ---
    VECTORDB REMOVE SOURCE "AdminDb", "documents", "d1"
    PRINT "documents dopo REMOVE SOURCE: "; VECTORDB.COUNT("AdminDb", "documents")
    PRINT

    ' --- 10. Purge deprecati (comando legacy) ---
    VECTORDB PURGE DEPRECATED "AdminDb", "logs"
    PRINT "logs dopo PURGE DEPRECATED: "; VECTORDB.COUNT("AdminDb", "logs")
    PRINT

    PRINT "Test Completato."
END SUB
