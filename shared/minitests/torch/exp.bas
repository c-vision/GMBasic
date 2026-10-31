' MiniTest: TORCH.EXP (parita' 4 backend).
' EXPECT: 1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 3.0)
  r = TORCH.EXP(a)
  PRINT ROUND(TORCH.GET(r, 0, 1), 6)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
