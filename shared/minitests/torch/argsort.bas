' MiniTest: TORCH.ARGSORT (parita' 4 backend).
' EXPECT: 1  2  0
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 3.0) : TORCH.SET(a, 0, 1, 1.0) : TORCH.SET(a, 0, 2, 2.0)
  DIM d() = TORCH.ARGSORT(a)
  PRINT d(0); d(1); d(2)
  TORCH.FREE(a)
END SUB
