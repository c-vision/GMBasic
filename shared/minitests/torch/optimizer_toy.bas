' MiniTest: L10 ottimizzatori (Muon/QHAdam/DiffGrad) vs AdamW — parita' 4 backend.
' Senza seed: solo convergenza (end < init) per ciascuno. L'opzione e' il 4o argomento di
' MODELTRAINSTEP (opt), non una chiave cfg: "adamw"|"sgd"|"muon"|"qhadam"|"diffgrad".
' Tutto inline in Main: niente FOR in FUNCTION (bug PCode aperto).
' EXPECT: adamw: 1|muon: 1|qhadam: 1|diffgrad: 1|galore: 1
IMPORT TORCH

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM i AS LONG

  DIM m0 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM a0 AS DOUBLE = TORCH.MODELLOSS(m0, tA, t2)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(m0, tA, t2, "adamw", "crossentropy", 0.02)
  NEXT
  IF TORCH.MODELLOSS(m0, tA, t2) < a0 THEN PRINT "adamw: 1" ELSE PRINT "adamw: 0"

  DIM m1 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM a1 AS DOUBLE = TORCH.MODELLOSS(m1, tA, t2)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(m1, tA, t2, "muon", "crossentropy", 0.02)
  NEXT
  IF TORCH.MODELLOSS(m1, tA, t2) < a1 THEN PRINT "muon: 1" ELSE PRINT "muon: 0"

  DIM m2 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM a2 AS DOUBLE = TORCH.MODELLOSS(m2, tA, t2)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(m2, tA, t2, "qhadam", "crossentropy", 0.02)
  NEXT
  IF TORCH.MODELLOSS(m2, tA, t2) < a2 THEN PRINT "qhadam: 1" ELSE PRINT "qhadam: 0"

  DIM m3 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM a3 AS DOUBLE = TORCH.MODELLOSS(m3, tA, t2)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(m3, tA, t2, "diffgrad", "crossentropy", 0.02)
  NEXT
  IF TORCH.MODELLOSS(m3, tA, t2) < a3 THEN PRINT "diffgrad: 1" ELSE PRINT "diffgrad: 0"

  DIM m4 AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM a4 AS DOUBLE = TORCH.MODELLOSS(m4, tA, t2)
  FOR i = 1 TO 60
    TORCH.MODELTRAINSTEP(m4, tA, t2, "galore", "crossentropy", 0.02)
  NEXT
  IF TORCH.MODELLOSS(m4, tA, t2) < a4 THEN PRINT "galore: 1" ELSE PRINT "galore: 0"

  TORCH.MODELFREE(m0) : TORCH.MODELFREE(m1) : TORCH.MODELFREE(m2) : TORCH.MODELFREE(m3) : TORCH.MODELFREE(m4)
  TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
END SUB
