' MiniTest: TORCH.SLICE (parita' 4 backend).
' EXPECT: 1  2
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 0, 2, 3.0)
  s = TORCH.SLICE(a, 0, 0, 2)
  PRINT TORCH.GET(s, 0, 0); TORCH.GET(s, 0, 1)
  TORCH.FREE(a) : TORCH.FREE(s)
END SUB
