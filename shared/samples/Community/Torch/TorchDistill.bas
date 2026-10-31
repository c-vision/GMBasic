' ============================================================================
' TorchDistill.bas — distillazione (item 8): studente PICCOLO addestrato SOLO
' sui logits (soft target) del teacher, senza etichette hard.
'   teacher = transformer d32/2L addestrato su taskA (1→2→3→4→1)
'   student = transformer d16/1L, train con loss "distill" (KL) su MODELFORWARD(teacher)
' loss "distill" = KL(teacher || student) (y = logits teacher, detach; T=1)
' ============================================================================
IMPORT TORCH

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yA(3) AS DOUBLE = {2, 3, 4, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM tY AS LONG = TORCH.RESHAPE(TORCH.FROM(yA), 1, 4)
  DIM cfgT AS STRING = "{""vocab"":8,""d_model"":32,""n_layers"":2,""n_heads"":2,""block_size"":8}"
  DIM cfgS AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"

  ' teacher
  DIM tea AS LONG = TORCH.MODELCREATE("transformer", cfgT)
  DIM i AS LONG
  FOR i = 1 TO 150
    TORCH.MODELTRAINSTEP(tea, tA, tY, "adamw", "crossentropy", 0.02)
  NEXT
  PRINT "teacher CE  ="; TORCH.MODELLOSS(tea, tA, tY)

  ' soft target
  DIM soft AS LONG = TORCH.MODELFORWARD(tea, tA)

  ' student distill
  DIM stu AS LONG = TORCH.MODELCREATE("transformer", cfgS)
  FOR i = 1 TO 400
    TORCH.MODELTRAINSTEP(stu, tA, soft, "adamw", "distill", 0.05)
  NEXT
  PRINT "student distill loss ="; TORCH.MODELLOSS(stu, tA, soft)
  PRINT "STATE =" + TORCH.MODELSTATE(stu)
  TORCH.MODELFREE(tea) : TORCH.MODELFREE(stu)
  PRINT "=== TorchDistill: done ==="
END SUB
