' MiniTest: AI.Ask senza provider embedding -> errore esplicito (non silenzio).
' Stesso contratto di AI.SEARCH / BRAINVAULTQUERY (fix 86427f92e / e4e58df03).
' EXPECT: ERROR: Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)
IMPORT AI WITH ALIAS
SUB Main()
  PRINT AI.Ask("kb", "docs", "q?", 1)
END SUB
