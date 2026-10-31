' ============================================================================
' TorchOptimizers.bas — L10: AdamW vs Muon vs QHAdam vs DiffGrad (solo TORCH).
' Stesso compito, stessa lr, stesso numero di step: confronto della loss finale.
' L'ottimizzatore e' il 4o argomento di MODELTRAINSTEP.
' ============================================================================
IMPORT TORCH

SUB TrainOne(name AS STRING, steps AS LONG)
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM m AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM l0 AS DOUBLE = TORCH.MODELLOSS(m, tA, t2)
  DIM i AS LONG
  FOR i = 1 TO steps
    TORCH.MODELTRAINSTEP(m, tA, t2, name, "crossentropy", 0.02)
  NEXT
  DIM l1 AS DOUBLE = TORCH.MODELLOSS(m, tA, t2)
  PRINT name + ": " + STR(l0) + " -> " + STR(l1)
  TORCH.MODELFREE(m) : TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
END SUB

SUB Main()
  DIM steps AS LONG = 60
  TrainOne("adamw", steps)
  TrainOne("muon", steps)
  TrainOne("qhadam", steps)
  TrainOne("diffgrad", steps)
  TrainOne("galore", steps)
  PRINT "=== TorchOptimizers: done ==="
END SUB
