' MiniTest: TORCH.PERMUTE (parita' 4 backend).
' EXPECT: 1  2
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  p = TORCH.PERMUTE(a, 1, 0)
  PRINT TORCH.GET(p, 0, 0); TORCH.GET(p, 1, 0)
  TORCH.FREE(a) : TORCH.FREE(p)
END SUB
