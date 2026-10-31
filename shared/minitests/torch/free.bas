' MiniTest: TORCH.FREE (parita' 4 backend).
' EXPECT: 0
IMPORT TORCH
SUB Main()
  a = TORCH.ONES(1, 1)
  PRINT TORCH.FREE(a)
END SUB
