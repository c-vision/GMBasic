' FILE: AI/VectorProviderFailover.bas
' Rotate embedding providers during intensive ingest/search.

IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
  DIM rotated$ AS STRING
  DIM ok AS BIT
  DIM json$ AS STRING
  DIM answer$ AS STRING

  ok = AI.REGISTER("embedding", "emb-backup", "fixture", "")
  SQL OPEN ":memory:" ALIAS pfail

  rotated$ = AI.ROTATE("embedding")
  PRINT "Rotate 1 ->"; rotated$
  AI RAG INDEX TEXT "pfail", "docs", "doc-1", "Provider rotation test document one."
  json$ = AI.SEARCH("pfail", "docs", "rotation test", 2, 1, "")
  PRINT "SEARCH len after doc-1:"; LEN(json$)

  rotated$ = AI.ROTATE("embedding")
  PRINT "Rotate 2 ->"; rotated$
  AI RAG INDEX TEXT "pfail", "docs", "doc-2", "Provider rotation test document two."
  json$ = AI.SEARCH("pfail", "docs", "rotation test", 2, 1, "")
  PRINT "SEARCH len after doc-2:"; LEN(json$)

  answer$ = AI.ASK("pfail", "docs", "rotation test document", 2)
  PRINT "ASK:"; answer$
  PRINT "Final embedding provider:"; AI.ACTIVE("embedding")
END SUB
