' MiniTest: TORCH.MODELRESET (parita' 4 backend).
' EXPECT: 0
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELRESET(m)
  TORCH.MODELFREE(m)
END SUB
