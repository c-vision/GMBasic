' MiniTest: TORCH.POW (parita' 4 backend).
' EXPECT: 9
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 1)
  TORCH.SET(a, 0, 0, 3.0)
  r = TORCH.POW(a, 2.0)
  PRINT ROUND(TORCH.GET(r, 0, 0), 6)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
