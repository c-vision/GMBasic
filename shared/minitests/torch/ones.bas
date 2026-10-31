' MiniTest: TORCH.ONES (parita' 4 backend).
' EXPECT: 1  1
IMPORT TORCH
SUB Main()
  a = TORCH.ONES(2, 3)
  PRINT TORCH.GET(a, 0, 0); TORCH.GET(a, 1, 2)
  TORCH.FREE(a)
END SUB
