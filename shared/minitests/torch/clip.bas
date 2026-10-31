' MiniTest: TORCH.CLIP (parita' 4 backend).
' EXPECT: 1.5  2  2.5
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 2.0) : TORCH.SET(a, 0, 2, 3.0)
  DIM c() = TORCH.CLIP(a, 1.5, 2.5)
  PRINT ROUND(c(0), 6); ROUND(c(1), 6); ROUND(c(2), 6)
  TORCH.FREE(a)
END SUB
