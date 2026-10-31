' MiniTest: TORCH.CREATE (parita' 4 backend).
' EXPECT: 1.5
IMPORT TORCH
SUB Main()
  a = TORCH.CREATE(2, 2)
  TORCH.SET(a, 0, 0, 1.5)
  PRINT TORCH.GET(a, 0, 0)
  TORCH.FREE(a)
END SUB
