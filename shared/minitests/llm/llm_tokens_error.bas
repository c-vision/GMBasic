' MiniTest: LLM.TOKENS / LLM.DETOKENIZE su modello inesistente -> errore CATTURABILE (ON ERROR) con
' ERR.Number = 5 (illegal function call) e STESSO testo sulle 4 gambe (C99Native, IL, PCode, VM/C).
' Ancora il fix 2026-11-12: prima lo stesso fallimento dava ERR 9 su C99Native (gmb_llm_out_or_raise
' alzava 9 = subscript out of range), ERR 200 su IL/PCode (InvalidOperationException) e 5 su VM/C.
' Le due funzioni restituiscono il valore, quindi l'errore non prone nulla: lo statement "no_err" c'e'
' solo se il backend NON alza l'errore (fallimento del test).
' EXPECT: TOKENS 5 [LLM.TOKENS: tokenizer non disponibile per il modello indicato]|DETOKENIZE 5 [LLM.DETOKENIZE: tokenizer non disponibile o id non validi]
IMPORT LLM
SUB Main()
  TestTokens()
  TestDetokenize()
END SUB

SUB TestTokens()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.TOKENS("%MODELS%/no_such_model_xyz", "ping")
  PRINT "TOKENS no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "TOKENS"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestDetokenize()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.DETOKENIZE("%MODELS%/no_such_model_xyz", "1,2,3")
  PRINT "DETOKENIZE no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "DETOKENIZE"; ERR.Number; "["; ERR.Description; "]"
END SUB
