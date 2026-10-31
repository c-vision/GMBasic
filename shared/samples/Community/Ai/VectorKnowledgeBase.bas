' FILE: AI/VectorKnowledgeBase.bas
' Full knowledge-base workflow: providers, batch index, search JSON, RAG, ASK.

IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
  DIM ok AS BIT
  DIM cid AS LONG
  DIM n AS LONG
  DIM json$ AS STRING
  DIM answer$ AS STRING
  DIM ctx$ AS STRING
  DIM hasManual3 AS BIT

  PRINT "=== Vector Knowledge Base ==="
  PRINT "Embedding provider:"; AI.ACTIVE("embedding")
  PRINT "Completion provider:"; AI.ACTIVE("completion")

  SQL OPEN ":memory:" ALIAS kbdb
  AI RAG ENSURE COLLECTION "kbdb", "manuals", 8

  AI RAG INDEX TEXT "kbdb", "manuals", "manual-1", "Chapter 1: Vector search enables semantic retrieval over manuals."
  AI RAG INDEX TEXT "kbdb", "manuals", "manual-2", "Chapter 2: Vector search enables semantic retrieval over manuals."
  AI RAG INDEX TEXT "kbdb", "manuals", "manual-3", "Chapter 3: Vector search enables semantic retrieval over manuals."
  AI RAG INDEX TEXT "kbdb", "manuals", "manual-4", "Chapter 4: Vector search enables semantic retrieval over manuals."

  n = AI.COUNT("kbdb", "manuals")
  PRINT "Embeddings indexed:"; n

  json$ = AI.SEARCH("kbdb", "manuals", "semantic retrieval chapter", 4, 1, "")
  PRINT "SEARCH JSON (first 120 chars):"; LEFT(json$, 120)

  json$ = AI.SEARCH("kbdb", "manuals", "chapter 3", 3, 0, "")
  hasManual3 = (INSTR(json$, "manual-3") > 0)
  PRINT "Vector-only hits contain manual-3:"; hasManual3

  answer$ = AI.ASK("kbdb", "manuals", "What enables semantic retrieval?", 4)
  PRINT "ASK:"; answer$

  ctx$ = AI.RAGCONTEXT("kbdb", "manuals", "semantic retrieval", 2)
  PRINT "RAG context excerpt:"
  PRINT LEFT(ctx$, 200)

  AI RAG REMOVE SOURCE "kbdb", "manuals", "manual-4"
  n = AI.COUNT("kbdb", "manuals")
  PRINT "Count after remove:"; n

END SUB
