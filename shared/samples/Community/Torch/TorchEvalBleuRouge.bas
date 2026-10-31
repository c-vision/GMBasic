' ============================================================================
' TorchEvalBleuRouge.bas — valutazione testuale con TORCH.BLEU / TORCH.ROUGE (L7).
' Addestra un transformer toy + un LoRA sul rovesciamento di sequenza, DECODIFICA le
' predizioni (SLICE + ARGMAX + GET per posizione) e le confronta ai riferimenti con
' BLEU-4 e ROUGE-L. Esercita insieme L1 (LoRA) e L7 (metriche): il confronto base-vs-LoRA
' e' la versione toy della eval paired (gate D4: token E pass-rate, qui BLEU E ROUGE).
' ============================================================================
IMPORT TORCH

' Decodifica i logits [1,S,V] in stringa "id id ..." (un id per posizione).
' NOTA: il ciclo e' SROTOLATO a mano invece che FOR perche' PCode ha un bug
' (FOR in FUNCTION con corpo complesso itera un numero errato di volte — vedi
' Bugs/(open) - PCode-For-In-Function-Corpo-Loop.md). Ripristinare il FOR quando e' risolto.
FUNCTION DecodeIds(logits AS LONG, npos AS LONG) AS STRING
  DIM r0 AS LONG = TORCH.SLICE(logits, 1, 0, 1)
  DIM i0 AS LONG = TORCH.ARGMAX(r0)
  DIM d0 AS STRING = STR(INT(TORCH.GET(i0, 0, 0)))
  DIM r1 AS LONG = TORCH.SLICE(logits, 1, 1, 2)
  DIM i1 AS LONG = TORCH.ARGMAX(r1)
  DIM d1 AS STRING = STR(INT(TORCH.GET(i1, 0, 0)))
  DIM r2 AS LONG = TORCH.SLICE(logits, 1, 2, 3)
  DIM i2 AS LONG = TORCH.ARGMAX(r2)
  DIM d2 AS STRING = STR(INT(TORCH.GET(i2, 0, 0)))
  DIM r3 AS LONG = TORCH.SLICE(logits, 1, 3, 4)
  DIM i3 AS LONG = TORCH.ARGMAX(r3)
  DIM d3 AS STRING = STR(INT(TORCH.GET(i3, 0, 0)))
  TORCH.MODELFREE(r0) : TORCH.MODELFREE(i0)
  TORCH.MODELFREE(r1) : TORCH.MODELFREE(i1)
  TORCH.MODELFREE(r2) : TORCH.MODELFREE(i2)
  TORCH.MODELFREE(r3) : TORCH.MODELFREE(i3)
  RETURN TRIM(d0) + " " + TRIM(d1) + " " + TRIM(d2) + " " + TRIM(d3)
END FUNCTION

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM ref$ AS STRING = "4 3 2 1"
  DIM i AS LONG

  ' 1) base sul rovesciamento
  DIM b AS LONG = TORCH.MODELCREATE("transformer", cfg)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(b, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT

  ' 2) LoRA sullo stesso compito (rank 4, alpha 8)
  DIM l AS LONG = TORCH.MODELCREATE("lora", cfg + ",""rank"":4,""alpha"":8}")
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(l, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT

  ' 2bis) LoRA con scheduler cosine+warmup (L2: 7o parametro opts$)
  DIM lc AS LONG = TORCH.MODELCREATE("lora", cfg + ",""rank"":4,""alpha"":8}")
  DIM copts AS STRING = "{""scheduler"":""cosine"",""warmup_steps"":10,""decay_steps"":60}"
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(lc, tA, t2, "adamw", "crossentropy", 0.05, copts)
  NEXT
  PRINT "sched cosine: "; TORCH.MODELSTATE(lc)

  ' 3) decode + metriche: base vs LoRA sullo stesso input
  DIM pb$ AS STRING = DecodeIds(TORCH.MODELFORWARD(b, tA), 4)
  DIM pl$ AS STRING = DecodeIds(TORCH.MODELFORWARD(l, tA), 4)
  DIM pc$ AS STRING = DecodeIds(TORCH.MODELFORWARD(lc, tA), 4)
  PRINT "ref:       "; ref$
  PRINT "pred base: "; pb$
  PRINT "pred lora: "; pl$
  PRINT "pred cos:  "; pc$
  PRINT "BLEU  base ="; ROUND(TORCH.BLEU(ref$, pb$), 4)
  PRINT "BLEU  lora ="; ROUND(TORCH.BLEU(ref$, pl$), 4)
  PRINT "BLEU  cos  ="; ROUND(TORCH.BLEU(ref$, pc$), 4)
  PRINT "ROUGE base ="; ROUND(TORCH.ROUGE(ref$, pb$), 4)
  PRINT "ROUGE lora ="; ROUND(TORCH.ROUGE(ref$, pl$), 4)
  PRINT "ROUGE cos  ="; ROUND(TORCH.ROUGE(ref$, pc$), 4)

  TORCH.MODELFREE(b) : TORCH.MODELFREE(l) : TORCH.MODELFREE(lc)
  TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
  PRINT "=== TorchEvalBleuRouge: done ==="
END SUB
