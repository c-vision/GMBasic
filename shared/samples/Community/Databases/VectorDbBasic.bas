' FILE: Databases/VectorDbBasic.bas
' =================================================================
' VectorDB: esempio base di utilizzo come database vettoriale puro
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "VectorDB Basic Usage"
    PRINT "==================="
    PRINT

    ' --- 1. Setup database ---
    SQL OPEN ":memory:" ALIAS vdb
    AI RAG ENSURE COLLECTION ":memory:", "products", 8
    PRINT "Collection 'products' created."
    PRINT

    ' --- 2. Index dati ---
    AI RAG INDEX TEXT ":memory:", "products", "prod-1", "Notebook leggero con processore i7"
    AI RAG INDEX TEXT ":memory:", "products", "prod-2", "Smartwatch con GPS e cardiofrequenzimetro"
    AI RAG INDEX TEXT ":memory:", "products", "prod-3", "Cuffie wireless cancellazione rumore"
    PRINT "Indexed 3 products. Total embeddings: "; AI.COUNT(":memory:", "products")
    PRINT

    ' --- 3. Search vettoriale ---
    DIM hits$ = AI.SEARCH(":memory:", "products", "notebook", 3, 0)
    PRINT "Vector search results:"
    PRINT hits$
    PRINT

    ' --- 4. Search ibrido ---
    DIM hybrid$ = AI.SEARCH(":memory:", "products", "cuffie wireless", 3, 1)
    PRINT "Hybrid search results:"
    PRINT hybrid$
    PRINT

    ' --- 5. Conteggio ---
    PRINT "Total embeddings: "; AI.COUNT(":memory:", "products")
    PRINT

    ' --- 6. Lista collezioni ---
    PRINT "Collections:"
    PRINT AI.LISTCOLLECTIONS(":memory:")
    PRINT

    PRINT "Test completato."
END SUB
