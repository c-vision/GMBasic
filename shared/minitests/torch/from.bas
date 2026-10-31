' MiniTest: TORCH.FROM (parita' 4 backend).
' EXPECT: 6
IMPORT TORCH
SUB Main()
  DIM v(2) AS DOUBLE = {1, 2, 3}
  a = TORCH.FROM(v)
  PRINT TORCH.SUM(a)
  TORCH.FREE(a)
END SUB
