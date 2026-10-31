' ExpertResidencyPolicy.bas — §9.A EXPERT_RESIDENCY_POLICY (applicazione LIVE).
'
' Pipeline:
'   1) LLM TENSOR EXPOLICY <model>, <out.json>  -> emette il PIANO POLICY (formato runtime MLX:
'      mode OFFLOAD/RESIDENT, hotness, prefetch_topn) derivato dal modello (n_esperti/top_k).
'   2) AI SET OPTION "expolicy", <out.json>     -> l'engine MLX INGERISCE e APPLICA la policy alle
'      opzioni MoE (cache budget, hotness ROUTING, prefetch distance) prima della creazione dei
'      componenti (llm_mlx_expolicy → llmengine_moe_config_defaults). La policy dichiarata diventa
'      comportamento live: esperti RESIDENT vs streaming + prefetch dal router.
'
' Il piano è un DATO (PUL -> Apache), nessuna dipendenza inversa.
' NOTA: richiede un modello GGUF reale; l'applicazione piena si vede a runtime (AI.STREAMSTATS).
IMPORT LLM
IMPORT AI

SUB Main()
  DIM model AS STRING = "work/modello.gguf"     ' modello MoE GGUF (fornito dall'utente)
  DIM pol   AS STRING = "work/expolicy.json"    ' piano policy (output)

  ' 1) Policy di residenza esperti (model-aware): router->prefetch, streaming/RESIDENT.
  LLM TENSOR EXPOLICY model, pol
  PRINT "policy scritta: " + pol

  ' 2) Attivala nell'engine MLX (letta e applicata a load_model).
  AI SET OPTION "expolicy", pol
  PRINT "expolicy attivo: cache/hotness/prefetch regolati dal piano"

  ' 3) Con un modello grande e streaming attivo (AI.STREAMSTATS) si vedono le quote resident/cold.
  PRINT "done"
END SUB
