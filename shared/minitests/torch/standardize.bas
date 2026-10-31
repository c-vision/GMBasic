' MiniTest: TORCH.STANDARDIZE (parita' 4 backend).
' EXPECT: -1  1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 0, 2, 3.0)
  DIM s() = TORCH.STANDARDIZE(a)
  PRINT ROUND(s(0), 6); ROUND(s(2), 6)
  TORCH.FREE(a)
END SUB
