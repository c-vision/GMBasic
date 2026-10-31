' MiniTest: TORCH.SHAPE (parita' 4 backend).
' EXPECT: 2  3
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 3)
  DIM sh() = TORCH.SHAPE(a)
  PRINT sh(0); sh(1)
  TORCH.FREE(a)
END SUB
