' FILE: AI/VectorSearchJson.bas
' Compare hybrid vs vector-only SEARCH JSON output.

IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
  DIM hybrid$ AS STRING
  DIM vector$ AS STRING
  DIM ok AS LONG
  DIM hybridHit AS BIT
  DIM vectorHit AS BIT

  SQL OPEN ":memory:" ALIAS sdb
  AI RAG INDEX TEXT "sdb", "articles", "a1", "Keyword alpha appears once. Semantic topic: database indexing."
  AI RAG INDEX TEXT "sdb", "articles", "a2", "Keyword beta. Semantic topic: unrelated cooking recipes."

  hybrid$ = AI.SEARCH("sdb", "articles", "database indexing", 2, 1, "")
  vector$ = AI.SEARCH("sdb", "articles", "database indexing", 2, 0, "")

  hybridHit = (INSTR(hybrid$, "a1") > 0)
  vectorHit = (INSTR(vector$, "a1") > 0)
  PRINT "Hybrid top hit:"; hybridHit
  PRINT "Vector top hit:"; vectorHit
  PRINT "Hybrid JSON len:"; LEN(hybrid$)
  PRINT "Vector JSON len:"; LEN(vector$)

  SQL CLOSE "sdb"
END SUB
