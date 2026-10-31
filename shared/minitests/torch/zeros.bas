' MiniTest: TORCH.ZEROS (parita' 4 backend).
' EXPECT: 0  0
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(2, 3)
  PRINT TORCH.GET(a, 0, 0); TORCH.GET(a, 1, 2)
  TORCH.FREE(a)
END SUB
