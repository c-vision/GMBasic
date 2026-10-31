' MiniTest: TORCH.MODELLOSS (parita' 4 backend).
' EXPECT: ok
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  y = TORCH.ZEROS(1, 1)
  TORCH.SET(y, 0, 0, 1.0)
  IF TORCH.MODELLOSS(m, x, y) >= 0 THEN PRINT "ok" ELSE PRINT "no"
  TORCH.MODELFREE(m) : TORCH.FREE(x) : TORCH.FREE(y)
END SUB
