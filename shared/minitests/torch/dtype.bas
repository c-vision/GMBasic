' MiniTest: TORCH.DTYPE (parita' 4 backend).
' EXPECT: ok
IMPORT TORCH
SUB Main()
  t = TORCH.ONES(1, 1)
  IF LEN(TORCH.DTYPE(t)) > 0 THEN PRINT "ok" ELSE PRINT "no"
  TORCH.FREE(t)
END SUB
