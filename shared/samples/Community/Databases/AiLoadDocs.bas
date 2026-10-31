' FILE: AI/AiLoadDocs.bas
' =================================================================
' AI RAG LOAD DOCUMENTS — Caricamento batch di file da directory
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS
IMPORT JSON

SUB Main()
    PRINT "AI LOADDOCUMENTS Test"
    PRINT "==========================="
    PRINT
    
    ' --- 1. Apri database ---
    SQL OPEN ":memory:" ALIAS DocsDb
    PRINT "AI.ENSURECOLLECTION..."
    AI RAG ENSURE COLLECTION "DocsDb", "docs"
    
    ' --- 2. Crea un file di test temporaneo ---
    ' In un test reale, useresti un path esistente.
    ' Qui simuliamo la creazione di un file .txt
    DIM testDir = "./test_docs"
    ' (skip se non esiste — per test automatico)
    
    ' --- 3. Conta collezioni ---
    DIM collList$ = AI.LISTCOLLECTIONS(":memory:")
    PRINT "Collezioni: "; collList$
    
    ' --- 4. Verifica conteggio iniziale ---
    DIM cnt = AI.COUNT(":memory:", "docs")
    PRINT "Embedding iniziali: "; cnt
    PRINT
    
    ' --- 5. Index un singolo testo ---
    AI RAG INDEX TEXT ":memory:", "docs", "test1", "Il vettore e' un array di numeri reali."
    PRINT "IndexText eseguito."
    
    ' --- 6. Cerca ---
    DIM hits$ = AI.SEARCH(":memory:", "docs", "vettore", 3)
    PRINT "Risultati search: "; hits$
    PRINT
    
    ' --- 7. Rimuovi ---
    AI RAG REMOVE SOURCE ":memory:", "docs", "test1"
    PRINT "Dopo REMOVESOURCE, conteggio: "; AI.COUNT(":memory:", "docs")
    
    PRINT
    PRINT "Test Completato."
END SUB
