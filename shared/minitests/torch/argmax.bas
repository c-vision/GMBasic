' MiniTest: TORCH.ARGMAX (parita' 4 backend).
' EXPECT: 1
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 3.0) : TORCH.SET(a, 0, 2, 2.0)
  PRINT TORCH.ARGMAX(a)
  TORCH.FREE(a)
END SUB
