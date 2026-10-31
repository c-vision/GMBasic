' FILE: AI/AiDbAdmin.bas
' =================================================================
' AI admin: LISTCOLLECTIONS, AI.TEST, AI.CONFIGURE
' NOTA PARITÀ: i conteggi vettoriali sulle gambe C richiedono GMB_FIXTURE_EMBED=1
' (opt-in 2026-09-18; senza flag l'INDEX fallisce per P0e e COUNT resta 0).
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS
IMPORT JSON

SUB Main()
    PRINT "AI Database Admin"
    PRINT "======================"
    PRINT

    ' Provider fixture attivi (pattern FullRagPipeline): senza, il lato gestito rifiuta
    ' le operazioni vettoriali per P0a (ENSURE→eccezione→LIST []/COUNT -1). Sulle gambe C
    ' la registrazione è effettiva (registry condiviso); l'indicizzazione resta vincolata
    ' a P0e (serve un modello: senza, INDEX non vettorizza e COUNT resta 0).
    AI.SETACTIVE("embedding", "fixture-embedding")
    AI.SETACTIVE("completion", "fixture-completion")
    
    ' --- 1. Test provider ---
    PRINT "--- Provider check ---"
    DIM t1$ = AI.TEST("embedding", "fixture-embedding")
    PRINT "Fixture embedding: "; t1$
    DIM t2$ = AI.TEST("completion", "fixture-completion")
    PRINT "Fixture completion: "; t2$
    PRINT
    
    ' --- 2. Provider attivi ---
    PRINT "Provider embedding attivo: "; AI.ACTIVE("embedding")
    PRINT "Provider completion attivo: "; AI.ACTIVE("completion")
    PRINT
    
    ' --- 3. Crea collezioni in :memory: ---
    SQL OPEN ":memory:" ALIAS AdminDb
    AI RAG ENSURE COLLECTION "AdminDb", "knowledge", 8
    AI RAG ENSURE COLLECTION "AdminDb", "documents", 8
    AI RAG ENSURE COLLECTION "AdminDb", "logs", 8
    PRINT "Create 3 collezioni."
    PRINT
    
    ' --- 4. Lista collezioni ---
    PRINT "--- Lista collezioni ---"
    DIM colls$ = AI.LISTCOLLECTIONS("AdminDb")
    PRINT colls$
    PRINT
    
    ' --- 5. Index dati in knowledge e documents ---
    AI RAG INDEX TEXT "AdminDb", "knowledge", "k1", "Vettori e motori di search semantico"
    AI RAG INDEX TEXT "AdminDb", "knowledge", "k2", "Retrieval Augmented Generation con LLM"
    AI RAG INDEX TEXT "AdminDb", "documents", "d1", "Documento tecnico sui database vettoriali"
    PRINT
    
    ' --- 6. Conteggi per collezione ---
    PRINT "Knowlege count: "; AI.COUNT("AdminDb", "knowledge")
    PRINT "Documents count: "; AI.COUNT("AdminDb", "documents")
    PRINT "Logs count: "; AI.COUNT("AdminDb", "logs")
    PRINT
    
    ' --- 7. PROVvider list ---
    PRINT "--- Provider list ---"
    DIM embList() = AI.LIST("embedding")
    PRINT "Embedding providers:"; embList(0)
    PRINT
    
    PRINT "Test Completato."
END SUB
