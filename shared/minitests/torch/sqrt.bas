' MiniTest: TORCH.SQRT (parita' 4 backend).
' EXPECT: 3
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 2)
  TORCH.SET(a, 0, 0, 4.0) : TORCH.SET(a, 0, 1, 9.0)
  r = TORCH.SQRT(a)
  PRINT ROUND(TORCH.GET(r, 0, 1), 6)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
