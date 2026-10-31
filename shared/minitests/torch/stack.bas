' MiniTest: TORCH.STACK (parita' 4 backend).
' EXPECT: 1  3
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  b = TORCH.ONES(2, 2)
  c = TORCH.STACK(a, b)
  PRINT TORCH.GET(c, 0, 0); TORCH.GET(c, 1, 0)
  TORCH.FREE(a) : TORCH.FREE(b) : TORCH.FREE(c)
END SUB
