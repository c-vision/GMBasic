' MiniTest: L4 freeze tuning — full vs LoRA vs freeze (parita' 4 backend).
' Senza seed: convergenza per tutti + valori distinti (dinamiche diverse).
' NOTA: l'ordinamento per capacita' (full < freeze < lora) NON regge su questo task toy:
' misurato 3x, freeze ~= full (1e-05) >> lora (~0.5) — l'ultimo layer da solo risolve il
' rovesciamento di 4 token. Non asserito; asserita convergenza + diversita'.
' EXPECT: full: 1|lora: 1|frz: 1|distinct: 1
IMPORT TORCH

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":2,""n_heads"":1,""block_size"":8}"
  DIM i AS LONG

  DIM bf AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM bi AS DOUBLE = TORCH.MODELLOSS(bf, tA, t2)
  FOR i = 1 TO 50
    TORCH.MODELTRAINSTEP(bf, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM be AS DOUBLE = TORCH.MODELLOSS(bf, tA, t2)
  IF be < bi THEN PRINT "full: 1" ELSE PRINT "full: 0"

  DIM ll AS LONG = TORCH.MODELCREATE("lora", cfg + ",""rank"":4}")
  DIM li AS DOUBLE = TORCH.MODELLOSS(ll, tA, t2)
  FOR i = 1 TO 50
    TORCH.MODELTRAINSTEP(ll, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM le AS DOUBLE = TORCH.MODELLOSS(ll, tA, t2)
  IF le < li THEN PRINT "lora: 1" ELSE PRINT "lora: 0"

  DIM fz AS LONG = TORCH.MODELCREATE("freeze", cfg + ",""freeze_layers"":1}")
  DIM fi AS DOUBLE = TORCH.MODELLOSS(fz, tA, t2)
  FOR i = 1 TO 50
    TORCH.MODELTRAINSTEP(fz, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM fe AS DOUBLE = TORCH.MODELLOSS(fz, tA, t2)
  IF fe < fi THEN PRINT "frz: 1" ELSE PRINT "frz: 0"
  IF be <> fe AND fe <> le AND be <> le THEN PRINT "distinct: 1" ELSE PRINT "distinct: 0"

  TORCH.MODELFREE(bf) : TORCH.MODELFREE(ll) : TORCH.MODELFREE(fz)
  TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
END SUB
