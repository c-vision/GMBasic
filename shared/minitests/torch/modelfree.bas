' MiniTest: TORCH.MODELFREE (parita' 4 backend).
' EXPECT: 0
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELFREE(m)
END SUB
