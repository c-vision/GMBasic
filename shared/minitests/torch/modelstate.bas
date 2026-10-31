' MiniTest: TORCH.MODELSTATE (parita' 4 backend).
' EXPECT: ok
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  IF LEN(TORCH.MODELSTATE(m)) > 0 THEN PRINT "ok" ELSE PRINT "no"
  TORCH.MODELFREE(m)
END SUB
