' MiniTest: TORCH.TOARRAY (parita' 4 backend).
' EXPECT: 1  4
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 1, 1, 4.0)
  DIM f() = TORCH.TOARRAY(a)
  PRINT f(0); f(3)
  TORCH.FREE(a)
END SUB
