' MiniTest: TORCH.LOG (parita' 4 backend).
' EXPECT: 1.386294
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 2)
  TORCH.SET(a, 0, 0, 1.0) : TORCH.SET(a, 0, 1, 4.0)
  r = TORCH.LOG(a)
  PRINT ROUND(TORCH.GET(r, 0, 1), 6)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
