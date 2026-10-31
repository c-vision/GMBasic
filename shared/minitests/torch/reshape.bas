' MiniTest: TORCH.RESHAPE (parita' 4 backend).
' EXPECT: 1  4  4
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 1, 0, 3.0) : TORCH.SET(a, 1, 1, 4.0)
  r = TORCH.RESHAPE(a, 1, 4)
  DIM sh() = TORCH.SHAPE(r)
  PRINT sh(0); sh(1); TORCH.GET(r, 0, 3)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
