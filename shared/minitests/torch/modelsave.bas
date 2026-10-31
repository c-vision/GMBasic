' MiniTest: TORCH.MODELSAVE (parita' 4 backend).
' EXPECT: 0
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELSAVE(m, "/tmp/gmb_torch_modelsave.pt")
  TORCH.MODELFREE(m)
END SUB
