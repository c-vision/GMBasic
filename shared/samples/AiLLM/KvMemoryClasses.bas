' KvMemoryClasses.bas — §9.A M10 MEMORY OBJECT (M10d): classi KV RUNTIME vs PERSISTENT.
'
' Il KV è separato in due classi esplicite con policy indipendenti:
'   - KV_RUNTIME    : KV vivo in RAM (decode corrente);
'   - KV_PERSISTENT : KV persistito su disco (prefix-cache/kvdisk), ricostruibile/ricaricabile.
' `LLM TENSOR KVCLASSES` riporta le due quote (byte + numero oggetti) dal manager runtime dell'engine
' (llm_mlx_kvclasses, LLMEnv); il 2º arg opzionale `.json` le serializza.
'
' Nota: le quote crescono con l'uso (prefix-cache aperta, KV su disco); a modello non caricato → 0/0.
IMPORT LLM

SUB Main()
  ' Quote delle classi KV (runtime RAM vs persistent SSD).
  LLM TENSOR KVCLASSES

  ' Serializzazione opzionale su file.
  LLM TENSOR KVCLASSES, "work/kvclasses.json"
  PRINT "quote KV serializzate: work/kvclasses.json"

  ' Le due quote hanno policy indipendenti (limiti per-classe nel manager engine).
  PRINT "done"
END SUB
