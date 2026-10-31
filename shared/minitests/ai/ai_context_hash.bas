' EXPECT: 44bd98d473cdb0d0|1|0
' MiniTest: §3.6 PARSEC — AI.CONTEXTHASH$/AI.CONTEXTVALID (cache con invalidazione per version hash).
' La vista committata ("s") ha hash 44bd98d473cdb0d0: valid=1 con l'hash corrente, 0 con uno estraneo.
IMPORT AI
SUB Main()
  AI CONTEXT CREATE "a"
  AI CONTEXT ADD "a", "SYSTEM", "s"
  AI CONTEXT COMMIT "a"
  PRINT AI.CONTEXTHASH("a")
  PRINT AI.CONTEXTVALID("a", "44bd98d473cdb0d0")
  PRINT AI.CONTEXTVALID("a", "rev=0000000000000000")
END SUB
