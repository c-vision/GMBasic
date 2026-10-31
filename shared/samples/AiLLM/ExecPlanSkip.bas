' ExecPlanSkip.bas — §9.A M10: produttore di SKIP per il piano esecutivo (CED → layer skip).
'
' Un layer i cui tensori sono TUTTI ALL-ZERO è saltabile (il forward lo tratta come identità).
' Il produttore di skip è CED: `LLM TENSOR EXECPLAN` marca {"skip":true} per quei layer nell'array
' "layers" del piano per-layer, che l'engine applica via l'opzione `execplan`.
' Qui si ABLATE un intero layer (tutti i suoi tensori a zero), poi si emette il piano con skip.
IMPORT LLM

SUB Main()
  DIM mdl AS STRING = "work/modello.gguf"

  ' 1) ABLATE di un intero layer (tutti i suoi tensori -> zero) = "layer eliminabile" (CED exact).
  LLM MODEL ABLATE mdl, "work/modello_ablate.gguf", "blk.0."

  ' 2) Piano esecutivo per-layer: il layer ablato compare come {"skip":true} (riga con skip=1).
  LLM TENSOR EXECPLAN "work/modello_ablate.gguf", "work/execplan_skip.json"

  ' 3) Applica il piano (l'engine SALTA i layer marcati skip al forward).
  AI SET OPTION "execplan", "work/execplan_skip.json"
  PRINT "execplan con skip attivo"
END SUB
