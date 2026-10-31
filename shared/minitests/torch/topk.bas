' MiniTest: TORCH.TOPK (parita' 4 backend).
' EXPECT: 3  2
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 3.0) : TORCH.SET(a, 0, 1, 1.0) : TORCH.SET(a, 0, 2, 2.0)
  DIM d() = TORCH.TOPK(a, 2)
  PRINT d(0); d(1)
  TORCH.FREE(a)
END SUB
