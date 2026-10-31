' MiniTest: TORCH aritmetica SUB/MUL/DIV (parita' 4 backend).
' EXPECT: 0|1|1
IMPORT TORCH
SUB Main()
  a = TORCH.ONES(2, 2)
  b = TORCH.ONES(2, 2)
  c = TORCH.SUB(a, b)
  PRINT TORCH.GET(c, 0, 0)
  d = TORCH.MUL(a, b)
  PRINT TORCH.GET(d, 1, 1)
  e = TORCH.DIV(a, b)
  PRINT TORCH.GET(e, 0, 1)
  TORCH.FREE(a) : TORCH.FREE(b) : TORCH.FREE(c) : TORCH.FREE(d) : TORCH.FREE(e)
END SUB
