' MiniTest: TORCH.MODELFORWARD (parita' 4 backend).
' EXPECT: 1  1
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  f = TORCH.MODELFORWARD(m, x)
  DIM sh() = TORCH.SHAPE(f)
  PRINT sh(0); sh(1)
END SUB
