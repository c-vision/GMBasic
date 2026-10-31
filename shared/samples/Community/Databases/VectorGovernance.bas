' FILE: AI/VectorGovernance.bas
' Tenant-scoped indexing and filtered search via INDEXMETA + SEARCH filter JSON.

IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
  DIM json$ AS STRING
  DIM meta$ AS STRING
  DIM ok AS LONG
  DIM hasPol1 AS BIT
  DIM hasPol2 AS BIT
  DIM purged AS LONG

  SQL OPEN ":memory:" ALIAS govdb

  AI RAG ENSURE COLLECTION "govdb", "policies", 8

  meta$ = "{""status"":""current"",""tenant"":""acme"",""role"":""analyst""}"
  AI RAG INDEX META "govdb", "policies", "pol-1", "ACME vacation policy allows 25 days.", meta$

  meta$ = "{""status"":""current"",""tenant"":""globex"",""role"":""analyst""}"
  AI RAG INDEX META "govdb", "policies", "pol-2", "Globex vacation policy allows 30 days.", meta$

  json$ = AI.SEARCH("govdb", "policies", "vacation policy", 3, 1,
    "{""tenant"":""acme""}")
  hasPol1 = (INSTR(json$, "pol-1") > 0)
  hasPol2 = (INSTR(json$, "pol-2") > 0)
  PRINT "ACME filter contains pol-1:"; hasPol1
  PRINT "ACME filter excludes pol-2:"; (NOT hasPol2)

  meta$ = "{""status"":""deprecated"",""tenant"":""acme"",""role"":""analyst""}"
  AI RAG INDEX META "govdb", "policies", "pol-1", "obsolete", meta$
  AI RAG PURGE DEPRECATED "govdb", "policies"
  PRINT "Purged deprecated."

  SQL CLOSE "govdb"
END SUB
