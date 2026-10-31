' MiniTest: AI.AskMulti senza documenti rilevanti -> messaggio deterministico.
' Non richiede provider (path lessicale): oracolo stabile.
' EXPECT: askmulti=[insufficient evidence] No relevant documents found.
IMPORT AI WITH ALIAS
SUB Main()
  PRINT "askmulti="; AI.AskMulti("q?")
END SUB
