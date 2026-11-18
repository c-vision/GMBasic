' MiniTest: LLM.TENSOR / POLICY / ARCH / MODULE / KERNEL / KV su modello o sotto-verbo inesistente
' -> errore CATTURABILE (ON ERROR) con ERR.Number = 5 (illegal function call) e descrizione
' PER FAMIGLIA identica sulle 4 gambe (C99Native, IL, PCode, VM/C).
' Ancora il fix 2026-11-12: lo stesso fallimento dava un codice diverso per gamba e un testo
' diverso per famiglia — C99Native alzava 9/0 dagli helper `gmb_llm_out_or_raise`, IL/PCode 200
' (InvalidOperationException), VM/C 0; e POLICY/ARCH/MODULE/KERNEL/KV si presentavano tutte come
' "LLM.POLICYOUT" per un copia-incolla nell'emitter C99.
' Argomenti SCELTI per fallire in silenzio (l'oracolo confronta anche stderr): un sotto-verbo
' inesistente farebbe stampare al layer comando la lista dei verbi validi, e un path mancante
' stampa "LLM POLICY LOAD: file assente ...". Qui si usano argomenti ben formati che falliscono
' solo per modello/regola assenti.
' Le funzioni restituiscono il valore, quindi l'errore non e' un silenzio: lo statement "no_err"
' c'e' solo se il backend NON alza l'errore (fallimento del test).
' EXPECT: TENSOR 5 [LLM.TENSOR: verbo fallito]|POLICY 5 [LLM.POLICY: verbo fallito]|ARCH 5 [LLM.ARCH: verbo fallito]|MODULE 5 [LLM.MODULE: verbo fallito]|KERNEL 5 [LLM.KERNEL: verbo fallito]|KV 5 [LLM.KV: verbo fallito]
IMPORT LLM
SUB Main()
  TestTensor()
  TestPolicy()
  TestArch()
  TestModule()
  TestKernel()
  TestKv()
END SUB

SUB TestTensor()
  ON ERROR GOTO H
  DIM r$ AS STRING
  ' dir modello inesistente: il verbo LOGITS fallisce nel caricamento
  r$ = LLM.TENSOR("LOGITS", "%MODELS%/no_such_model_xyz", "ping")
  PRINT "TENSOR no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "TENSOR"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestPolicy()
  ON ERROR GOTO H
  DIM r$ AS STRING
  ' SET senza ruolo: sotto-verbo valido, regola mancante -> fallimento silenzioso
  r$ = LLM.POLICY("SET", "")
  PRINT "POLICY no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "POLICY"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestArch()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.ARCH("RESOLVE", "%MODELS%/no_such_model_xyz")
  PRINT "ARCH no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "ARCH"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestModule()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.MODULE("SHOW", "%MODELS%/no_such_model_xyz")
  PRINT "MODULE no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "MODULE"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestKernel()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.KERNEL("SHOW", "%MODELS%/no_such_model_xyz")
  PRINT "KERNEL no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "KERNEL"; ERR.Number; "["; ERR.Description; "]"
END SUB

SUB TestKv()
  ON ERROR GOTO H
  DIM r$ AS STRING
  r$ = LLM.KV("SHOW", "%MODELS%/no_such_model_xyz")
  PRINT "KV no_err["; r$; "]"
  EXIT SUB
H:
  PRINT "KV"; ERR.Number; "["; ERR.Description; "]"
END SUB
