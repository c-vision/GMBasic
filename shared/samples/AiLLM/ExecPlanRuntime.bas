' ExecPlanRuntime.bas — §9.A M10 EXECUTION PLANNER: piano esecutivo per-layer (runtime manager).
'
' Pipeline:
'   1) LLM TENSOR EXECPLAN <model>, <out>  -> produce un JSON con l'array "layers" (per-layer,
'      ops) + memory/report. È il piano prodotto dal model compiler (PUL).
'   2) AI SET OPTION "execplan", <out>     -> l'engine MLX (Apache) INGERISCE il piano a load_model.
'   3) al forward, `layer_forward` applica la pipeline per-layer del piano; una entry
'      {"skip":true} SALTA il layer (identità out=x). Generalizza il pipeline del `.llmarch`.
'
' Il piano è un DATO: il compilatore (GMBasic/gmbllm) lo produce, l'engine lo esegue — nessuna
' dipendenza inversa (PUL -> Apache solo come dati).
'
' NOTA: richiede un modello .gguf reale e un modello MLX caricabile per il forward (heavy).
IMPORT LLM
IMPORT AI

SUB Main()
  DIM model AS STRING = "work/modello.gguf"     ' modello GGUF di input (fornito dall'utente)
  DIM plan  AS STRING = "work/execplan.json"    ' piano esecutivo per-layer (output)

  ' 1) Piano esecutivo: arch -> ops per layer + memoria/KV.
  LLM TENSOR EXECPLAN model, plan
  PRINT "piano scritto: " + plan

  ' 2) Attiva il runtime manager nell'engine MLX (letto a load_model).
  AI SET OPTION "execplan", plan
  PRINT "execplan attivo: l'engine applicherà la pipeline per-layer del piano"

  ' 3) Con un modello caricato e in uso, il forward segue il piano per-layer (+skip).
  '    (Caricamento/esecuzione del modello omessi: richiede pesi reali.)
  PRINT "done"
END SUB
