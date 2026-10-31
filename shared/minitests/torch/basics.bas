' MiniTest: TORCH — costruttori, accesso, aritmetica, riduzioni (parita' 4 backend).
' EXPECT: 1  4|10  2.5  4  1|5|3
IMPORT TORCH
SUB Main()
  a = TORCH.CREATE(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0)
  TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  PRINT TORCH.GET(a, 0, 0); TORCH.GET(a, 1, 1)
  PRINT TORCH.SUM(a); TORCH.MEAN(a); TORCH.MAX(a); TORCH.MIN(a)
  b = TORCH.CREATE(2, 2)
  TORCH.SET(b, 0, 0, 1.0) : TORCH.SET(b, 0, 1, 1.0)
  TORCH.SET(b, 1, 0, 1.0) : TORCH.SET(b, 1, 1, 1.0)
  c = TORCH.ADD(a, b)
  PRINT TORCH.GET(c, 1, 1)
  d = TORCH.MATMUL(a, b)
  PRINT TORCH.GET(d, 0, 0)
  TORCH.FREE(a) : TORCH.FREE(b) : TORCH.FREE(c) : TORCH.FREE(d)
END SUB
