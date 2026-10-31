' MiniTest: TORCH.MODELINPUT (parita' 4 backend).
' EXPECT: 2
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELINPUT(m)
  TORCH.MODELFREE(m)
END SUB
