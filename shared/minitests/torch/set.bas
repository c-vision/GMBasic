' MiniTest: TORCH.SET (parita' 4 backend).
' EXPECT: 7.5
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 1)
  TORCH.SET(a, 0, 0, 7.5)
  PRINT TORCH.GET(a, 0, 0)
  TORCH.FREE(a)
END SUB
