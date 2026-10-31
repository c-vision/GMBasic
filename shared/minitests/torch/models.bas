' MiniTest: TORCH model* (parita' 4 backend).
' EXPECT: 2  1  3|state: ok|0|0|0
IMPORT TORCH
SUB Main()
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT TORCH.MODELINPUT(m); TORCH.MODELOUTPUT(m); TORCH.MODELPARAMETERS(m)
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  y = TORCH.ZEROS(1, 1)
  TORCH.SET(y, 0, 0, 1.0)
  f = TORCH.MODELFORWARD(m, x)
  IF LEN(TORCH.MODELSTATE(m)) > 0 THEN PRINT "state: ok" ELSE PRINT "state: fail"
  PRINT TORCH.MODELTRAINSTEP(m, x, y, "adam", "mse", 0.01)
  PRINT TORCH.MODELRESET(m)
  PRINT TORCH.MODELSAVE(m, "/tmp/gmb_torch_model_test.pt")
  TORCH.MODELFREE(m)
  TORCH.FREE(x) : TORCH.FREE(y) : TORCH.FREE(f)
END SUB
