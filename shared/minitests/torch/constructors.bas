' MiniTest: TORCH costruttori ZEROS/ONES/EYE/FROM (parita' 4 backend).
' EXPECT: 0  0|6|1  0  1|1  3
IMPORT TORCH
SUB Main()
  z = TORCH.ZEROS(2, 2)
  PRINT TORCH.GET(z, 0, 0); TORCH.GET(z, 1, 1)
  o = TORCH.ONES(2, 3)
  PRINT TORCH.SUM(o)
  e = TORCH.EYE(3)
  PRINT TORCH.GET(e, 0, 0); TORCH.GET(e, 0, 1); TORCH.GET(e, 2, 2)
  DIM arr(2) AS DOUBLE = {1, 2, 3}
  f = TORCH.FROM(arr)
  PRINT TORCH.GET(f, 0, 0); TORCH.GET(f, 0, 2)
  TORCH.FREE(z) : TORCH.FREE(o) : TORCH.FREE(e) : TORCH.FREE(f)
END SUB
