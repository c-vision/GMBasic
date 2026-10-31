' MiniTest: AI.RagContext senza provider embedding -> errore esplicito (non silenzio).
' EXPECT: ERROR: Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)
IMPORT AI WITH ALIAS
SUB Main()
  PRINT AI.RagContext("kb", "docs", "q?")
END SUB
