' FILE: AI/SqlVecSearch.bas
' SqlVecSearch — vector/RAG via AI on SQLite (requires SQL OPEN first).

IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
  PRINT "Active embedding:"; AI.ACTIVE("embedding")
  PRINT "Active completion:"; AI.ACTIVE("completion")

  SQL OPEN ":memory:" ALIAS vecdb

  DIM outcome AS LONG
  AI RAG INDEX TEXT "vecdb", "kb", "doc-1", "Vector databases enable semantic retrieval over unstructured text."

  DIM ctx AS STRING
  ctx = AI.RAGCONTEXT("vecdb", "kb", "semantic retrieval", 3)

  DIM answer AS STRING
  answer = AI.ASK("vecdb", "kb", "What is semantic retrieval?", 3)

  DIM dims(7) AS SINGLE
  DIM i AS INTEGER
  dims = AI.EMBED("hello world", 8)
  FOR i = LBound(dims) TO UBound(dims)
    PRINT dims(i);
  NEXT i
  PRINT ""

  PRINT "RAG context lines:"
  PRINT ctx
  PRINT "ASK answer:"
  PRINT answer
  PRINT "Index outcome:"; outcome

  SQL CLOSE vecdb
END SUB
