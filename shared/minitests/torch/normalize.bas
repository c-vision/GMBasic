' MiniTest: TORCH.NORMALIZE (parita' 4 backend).
' EXPECT: 0  1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 0, 2, 3.0)
  DIM n() = TORCH.NORMALIZE(a)
  PRINT ROUND(n(0), 6); ROUND(n(2), 6)
  TORCH.FREE(a)
END SUB
