' MiniTest: TORCH.RELU (parita' 4 backend).
' EXPECT: 0  3
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 3.0)
  r = TORCH.RELU(a)
  PRINT TORCH.GET(r, 0, 0); TORCH.GET(r, 0, 2)
  TORCH.FREE(a) : TORCH.FREE(r)
END SUB
