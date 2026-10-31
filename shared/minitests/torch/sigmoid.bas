' MiniTest: TORCH.SIGMOID (parita' 4 backend).
' EXPECT: 0.5
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 3.0)
  r = TORCH.SIGMOID(a)
  PRINT ROUND(TORCH.GET(r, 0, 1), 6)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
