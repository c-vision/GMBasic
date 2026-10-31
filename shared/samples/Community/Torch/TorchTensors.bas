' TORCH — tensori, aritmetica, attivazioni, riduzioni e un modello lineare.
' Richiede le librerie ML (SETUP TORCH / GMB_TORCH_PATH).
IMPORT TORCH

SUB Main()
  ' --- tensore 2x3 con valori 1..6 ---
  a = TORCH.ZEROS(2, 3)
  FOR i = 0 TO 1
    FOR j = 0 TO 2
      TORCH.SET(a, i, j, i * 3 + j + 1)
    NEXT j
  NEXT i
  PRINT "somma="; TORCH.SUM(a); " media="; TORCH.MEAN(a); " max="; TORCH.MAX(a); " min="; TORCH.MIN(a)

  ' --- trasposta e shape ---
  t = TORCH.TRANSPOSE(a)
  DIM sh() = TORCH.SHAPE(t)
  PRINT "trasposta "; sh(0); "x"; sh(1); " (0,0)="; TORCH.GET(t, 0, 0); " (2,1)="; TORCH.GET(t, 2, 1)

  ' --- aritmetica + attivazioni ---
  b = TORCH.ONES(2, 3)
  r = TORCH.RELU(TORCH.SUB(a, TORCH.ONES(2, 3)))
  PRINT "relu(sub(a,1))(0,0)="; TORCH.GET(r, 0, 0)
  e = TORCH.EXP(TORCH.ZEROS(1, 1))
  PRINT "exp(0)="; TORCH.GET(e, 0, 0)

  ' --- riduzioni / normalizzazione ---
  DIM flat() = TORCH.TOARRAY(a)
  PRINT "flat(0)="; flat(0); " flat(5)="; flat(5)
  DIM norm() = TORCH.NORMALIZE(a)
  PRINT "normalize(0)="; ROUND(norm(0), 4); " normalize(5)="; ROUND(norm(5), 4)

  ' --- similarità ---
  PRINT "similarity(a,a)="; TORCH.SIMILARITY(a, a, "euclidean")

  ' --- modello lineare: 2 ingressi -> 1 uscita ---
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT "model in/out/params="; TORCH.MODELINPUT(m); TORCH.MODELOUTPUT(m); TORCH.MODELPARAMETERS(m)
  x = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 1.0) : TORCH.SET(x, 0, 1, 2.0)
  y = TORCH.ZEROS(1, 1)
  TORCH.SET(y, 0, 0, 1.0)
  f = TORCH.MODELFORWARD(m, x)
  DIM fsh() = TORCH.SHAPE(f)
  PRINT "forward shape="; fsh(0); "x"; fsh(1)
  PRINT "train step="; TORCH.MODELTRAINSTEP(m, x, y, "adam", "mse", 0.01)
  PRINT "loss="; ROUND(TORCH.MODELLOSS(m, x, y), 6)

  ' --- cleanup ---
  TORCH.MODELFREE(m)
  TORCH.FREE(a) : TORCH.FREE(b) : TORCH.FREE(t) : TORCH.FREE(r) : TORCH.FREE(e)
  TORCH.FREE(x) : TORCH.FREE(y) : TORCH.FREE(f)
END SUB

