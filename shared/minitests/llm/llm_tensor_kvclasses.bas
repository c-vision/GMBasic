' EXPECT: LLM TENSOR KVCLASSES: runtime_bytes=0 persistent_bytes=0 (objects r=0 p=0)
' MiniTest: LLM TENSOR KVCLASSES — quote delle classi KV (RUNTIME vs PERSISTENT) dal manager
' engine (§9.A M10 MEMORY OBJECT / M10d). Parità 4 backend (params + risultato).
IMPORT LLM
SUB Main()
  LLM TENSOR KVCLASSES
END SUB
