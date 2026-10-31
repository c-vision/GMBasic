' MiniTest: TORCH.LOSSMSE (parita' 4 backend).
' EXPECT: 0.01
IMPORT TORCH
SUB Main()
  y = TORCH.ZEROS(1, 2)
  TORCH.SET(y, 0, 0, 1.0) : TORCH.SET(y, 0, 1, 0.0)
  p = TORCH.ZEROS(1, 2)
  TORCH.SET(p, 0, 0, 0.9) : TORCH.SET(p, 0, 1, 0.1)
  PRINT ROUND(TORCH.LOSSMSE(y, p), 6)
  TORCH.FREE(y) : TORCH.FREE(p)
END SUB
