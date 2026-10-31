' MiniTest: TORCH LIBPATH/MODELLOSS (parita' 4 backend).
' EXPECT: libpath: ok|modelloss: ok
IMPORT TORCH
SUB Main()
  t = TORCH.ONES(1, 1)
  IF LEN(TORCH.LIBPATH()) > 0 THEN PRINT "libpath: ok" ELSE PRINT "libpath: empty"
  TORCH.FREE(t)
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  y = TORCH.ZEROS(1, 1)
  TORCH.SET(y, 0, 0, 1.0)
  IF TORCH.MODELLOSS(m, x, y) >= 0 THEN PRINT "modelloss: ok" ELSE PRINT "modelloss: bad"
  TORCH.MODELFREE(m) : TORCH.FREE(x) : TORCH.FREE(y)
END SUB
