' MiniTest: TORCH.MODELOUTPUT (parita' 4 backend).
' EXPECT: 1
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELOUTPUT(m)
  TORCH.MODELFREE(m)
END SUB
