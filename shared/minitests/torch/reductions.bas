' MiniTest: TORCH ALLCLOSE/ARGMAX/ARGSORT/TOPK (parita' 4 backend).
' EXPECT: close: ok|1  2  0|3  2
IMPORT TORCH
SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, 3.0) : TORCH.SET(a, 0, 1, 1.0) : TORCH.SET(a, 0, 2, 2.0)
  b = TORCH.ZEROS(1, 3)
  TORCH.SET(b, 0, 0, 3.0) : TORCH.SET(b, 0, 1, 1.0) : TORCH.SET(b, 0, 2, 2.0)
  IF TORCH.ALLCLOSE(a, b, 0.0001) THEN PRINT "close: ok" ELSE PRINT "close: fail"
  DIM so() = TORCH.ARGSORT(a)
  PRINT so(0); so(1); so(2)
  DIM tk() = TORCH.TOPK(a, 2)
  PRINT tk(0); tk(1)
  TORCH.FREE(a) : TORCH.FREE(b)
END SUB
