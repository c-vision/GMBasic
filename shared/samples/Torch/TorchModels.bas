' TORCH — loss/accuracy, info e ciclo di vita di un modello lineare.
IMPORT TORCH

SUB Main()
  ' loss / accuracy
  y = TORCH.ZEROS(1, 2)
  TORCH.SET(y, 0, 0, 1.0) : TORCH.SET(y, 0, 1, 0.0)
  p = TORCH.ZEROS(1, 2)
  TORCH.SET(p, 0, 0, 0.9) : TORCH.SET(p, 0, 1, 0.1)
  PRINT "mse="; ROUND(TORCH.LOSSMSE(y, p), 6)
  PRINT "crossentropy="; ROUND(TORCH.LOSSCROSSENTROPY(y, p), 6)
  PRINT "binary="; ROUND(TORCH.LOSSBINARY(y, p), 6)
  PRINT "accuracy="; ROUND(TORCH.ACCURACY(y, p), 6)

  ' info
  t = TORCH.ONES(1, 1)
  PRINT "loaded="; TORCH.LOADED()
  PRINT "device="; TORCH.DEVICE()
  PRINT "available="; TORCH.AVAILABLE()
  PRINT "dtype="; TORCH.DTYPE(t)
  PRINT "deviceof="; TORCH.DEVICEOF(t)
  PRINT "libpath len="; LEN(TORCH.LIBPATH())

  ' modello
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT "in/out/params="; TORCH.MODELINPUT(m); TORCH.MODELOUTPUT(m); TORCH.MODELPARAMETERS(m)
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  f = TORCH.MODELFORWARD(m, x)
  DIM fsh() = TORCH.SHAPE(f)
  PRINT "forward="; fsh(0); "x"; fsh(1)
  PRINT "state len="; LEN(TORCH.MODELSTATE(m))
  PRINT "trainstep="; TORCH.MODELTRAINSTEP(m, x, y, "adam", "mse", 0.01)
  PRINT "modelloss ok="; TORCH.MODELLOSS(m, x, y) >= 0
  PRINT "save="; TORCH.MODELSAVE(m, "/tmp/gmb_torch_sample_model.pt")
  PRINT "reset="; TORCH.MODELRESET(m)
  TORCH.MODELFREE(m)
  TORCH.FREE(t) : TORCH.FREE(y) : TORCH.FREE(p) : TORCH.FREE(x) : TORCH.FREE(f)
END SUB

