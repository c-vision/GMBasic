' MiniTest: TORCH.ALLCLOSE (parita' 4 backend).
' EXPECT: -1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 3.0)
  b = TORCH.ZEROS(1, 3)
  TORCH.SET(b, 0, 0, -1.0) : TORCH.SET(b, 0, 2, 3.0)
  PRINT TORCH.ALLCLOSE(a, b, 0.0001)
  TORCH.FREE(a) : TORCH.FREE(b)
END SUB
