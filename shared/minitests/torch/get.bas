' MiniTest: TORCH.GET (parita' 4 backend).
' EXPECT: 9
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 2)
  TORCH.SET(a, 0, 1, 9.0)
  PRINT TORCH.GET(a, 0, 1)
  TORCH.FREE(a)
END SUB
