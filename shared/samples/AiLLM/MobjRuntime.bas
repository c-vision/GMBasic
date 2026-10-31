' MobjRuntime.bas — §9.A M10: MEMORY OBJECT runtime manager unificato (live).
'
' Un solo ente per TUTTI gli oggetti-memoria del runtime (pesi/esperti/KV/prefix/temp): categoria,
' residenza (RESIDENT/SWAP/DISK), byte e lifetime. I tier esistenti (KV di sessione, prefix cache,
' cache esperti) REGISTRANO qui i loro oggetti → vista unificata + budget globale + priorità di
' eviction (K3: temp/prefix per primi, trunk/pesi per ultimi).
' `LLM TENSOR MOBJLIVE[, <out.json>]` riporta totali + quote per categoria (engine llm_mlx_mobj).
IMPORT LLM

SUB Main()
  ' Vista unificata degli oggetti-memoria live (totali + per categoria).
  LLM TENSOR MOBJLIVE

  ' Serializzazione opzionale.
  LLM TENSOR MOBJLIVE, "work/mobj.json"
  PRINT "memory objects serializzati: work/mobj.json"
END SUB
