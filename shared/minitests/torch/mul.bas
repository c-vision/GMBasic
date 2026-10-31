' MiniTest: TORCH.MUL (parita' 4 backend).
' EXPECT: 2
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  b = TORCH.ONES(2, 2)
  TORCH.SET(b, 0, 0, 2.0)
  c = TORCH.MUL(a, b)
  PRINT TORCH.GET(c, 0, 0)
  TORCH.FREE(a) : TORCH.FREE(b) : TORCH.FREE(c)
END SUB
