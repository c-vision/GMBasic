' MiniTest: TORCH.SPLIT (parita' 4 backend).
' EXPECT: 1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  s = TORCH.SPLIT(a, 0, 2)
  PRINT TORCH.GET(s, 0, 0)
  TORCH.FREE(a) : TORCH.FREE(s)
END SUB
