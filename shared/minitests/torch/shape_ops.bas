' MiniTest: TORCH shape ops TRANSPOSE/SHAPE/TOARRAY (parita' 4 backend).
' EXPECT: 1  2  6|2  3|1  6
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 0, 2, 3.0)
  TORCH.SET(a, 1, 0, 4.0) : TORCH.SET(a, 1, 1, 5.0) : TORCH.SET(a, 1, 2, 6.0)
  t = TORCH.TRANSPOSE(a)
  PRINT TORCH.GET(t, 0, 0); TORCH.GET(t, 1, 0); TORCH.GET(t, 2, 1)
  DIM sh() = TORCH.SHAPE(a)
  PRINT sh(0); sh(1)
  DIM flat() = TORCH.TOARRAY(a)
  PRINT flat(0); flat(5)
  TORCH.FREE(a) : TORCH.FREE(t)
END SUB
