' MiniTest: comandi sessione KV agente — LLM PREFILL/DECODE/CACHE CLEAR/TRIM/BENCHMARK.
' I backend C non hanno la sessione KV (AgentServer è C#) → il runtime condiviso ritorna
' l'errore ESPLICITO del contratto IL (mai "builtin non implementato" / stub silenzioso).
' Chiuso 2026-09-18 (AIPREFILL 833, AIDECODE 834, AICACHECLEAR 835, AICACHETRIM 836,
' AIBENCHMARK 1257 erano assenti sul VM/C e su C99Native).
IMPORT AI
IMPORT LLM
SUB Main()
  LLM PREFILL "nope", "hi"
  LLM DECODE "nope", 4
  LLM CACHE CLEAR "nope"
  LLM CACHE TRIM "nope", 2
  LLM BENCHMARK "nope", PREFILL
  LLM BENCHMARK "nope", DECODE
  LLM BENCHMARK "nope", ALL
  PRINT "ok"
END SUB
' EXPECT: ok
