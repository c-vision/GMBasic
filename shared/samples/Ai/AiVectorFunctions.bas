' FILE: Ai/AiVectorFunctions.bas
' Vector DB — funzioni AI.* del namespace vettoriale (RAG).
' Copre: AI.ENSURECOLLECTION, AI.INDEXTEXT, AI.INDEXMETA, AI.COUNT,
' AI.SEARCH, AI.RERANK, AI.LISTCOLLECTIONS, AI.REMOVESOURCE,
' AI.PURGEDEPRECATED, AI.LOADDOCUMENTS, AI.EMBED.

IMPORT AI

SUB Main()
    PRINT "=== AI Vector Functions ==="

    DIM db AS STRING = "work/vector-functions.db"
    DIM coll AS STRING = "docs"

    ' Crea la collezione e indicizza un documento.
    PRINT "ensure:   "; AI.ENSURECOLLECTION(db, coll, 384)
    PRINT "index:    "; AI.INDEXTEXT(db, coll, "doc1", "il gatto dorme sul divano")
    PRINT "indexMeta:"; AI.INDEXMETA(db, coll, "doc2", "il cane corre nel parco", "{""lang"":""it""}")
    PRINT "count:    "; AI.COUNT(db, coll)
    PRINT "listColl: "; AI.LISTCOLLECTIONS(db)

    ' Ricerca (vettoriale) + rerank.
    DIM hits AS STRING = AI.SEARCH(db, coll, "gatto divano", 3)
    PRINT "search:   "; LEFT(hits, 200)
    PRINT "rerank:   "; LEFT(AI.RERANK("gatto", hits, 3), 200)

    ' Embedding di un testo.
    PRINT "embed:    "; AI.EMBED("testo di prova", 8)

    ' Rimozione e pulizia.
    PRINT "remove:   "; AI.REMOVESOURCE(db, coll, "doc1")

    PRINT "=== AiVectorFunctions ok ==="
END SUB
