' MiniTest: TORCH.MEAN (parita' 4 backend).
' EXPECT: 0.666667
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 3.0)
  PRINT ROUND(TORCH.MEAN(a), 6)
  TORCH.FREE(a)
END SUB
