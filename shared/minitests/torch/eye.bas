' MiniTest: TORCH.EYE (parita' 4 backend).
' EXPECT: 1  0  1
IMPORT TORCH
SUB Main()
  a = TORCH.EYE(3)
  PRINT TORCH.GET(a, 0, 0); TORCH.GET(a, 0, 1); TORCH.GET(a, 2, 2)
  TORCH.FREE(a)
END SUB
