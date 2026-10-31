' ============================================================================
' TorchAll.bas — TORCH: copertura COMPLETA di tutte le funzioni del namespace
' ============================================================================
' Un solo sample che tocca ogni builtin TORCH implementato, come regressione di
' superficie. Gruppi: device/loader, costruttori, aritmetica, attivazioni,
' puntuali, shape ops, riduzioni, accesso/confronto, model/training.
' ============================================================================
IMPORT TORCH

SUB Main()
  ' ---- device / loader ----
  PRINT "== 1. device/loader =="
  PRINT "DEVICE()  ="; TORCH.DEVICE()
  PRINT "LOADED()  ="; TORCH.LOADED()
  PRINT "LIBPATH() ="; TORCH.LIBPATH()

  ' ---- costruttori ----
  PRINT "== 2. costruttori =="
  PRINT "CREATE  ="; TORCH.SUM(TORCH.CREATE(2, 2))
  PRINT "FROM    ="; TORCH.SUM(TORCH.FROM({1, 2, 3, 4}))
  PRINT "ZEROS   ="; TORCH.SUM(TORCH.ZEROS(2, 2))
  PRINT "ONES    ="; TORCH.SUM(TORCH.ONES(2, 2))
  PRINT "EYE     ="; TORCH.SUM(TORCH.EYE(3))
  PRINT "RANDN   ="; TORCH.SUM(TORCH.RANDN(2, 2))

  ' ---- aritmetica ----
  PRINT "== 3. aritmetica =="
  ta = TORCH.FROM({1, 2, 3, 4})
  tb = TORCH.FROM({5, 6, 7, 8})
  PRINT "ADD     ="; TORCH.SUM(TORCH.ADD(ta, tb))
  PRINT "SUB     ="; TORCH.SUM(TORCH.SUB(ta, tb))
  PRINT "MUL     ="; TORCH.SUM(TORCH.MUL(ta, tb))
  PRINT "DIV     ="; TORCH.SUM(TORCH.DIV(ta, tb))
  m2 = TORCH.RESHAPE(ta, 2, 2)
  PRINT "MATMUL  ="; TORCH.SUM(TORCH.MATMUL(m2, m2))

  ' ---- attivazioni ----
  PRINT "== 4. attivazioni =="
  PRINT "RELU    ="; TORCH.SUM(TORCH.RELU(ta))
  PRINT "SIGMOID ="; TORCH.SUM(TORCH.SIGMOID(ta))
  PRINT "TANH    ="; TORCH.SUM(TORCH.TANH(ta))
  PRINT "SOFTMAX ="; TORCH.SUM(TORCH.SOFTMAX(ta))
  PRINT "GELU    ="; TORCH.SUM(TORCH.GELU(ta))

  ' ---- puntuali ----
  PRINT "== 5. puntuali =="
  PRINT "EXP     ="; TORCH.SUM(TORCH.EXP(ta))
  PRINT "LOG     ="; TORCH.SUM(TORCH.LOG(TORCH.EXP(ta)))
  PRINT "SQRT    ="; TORCH.SUM(TORCH.SQRT(ta))
  PRINT "POW     ="; TORCH.SUM(TORCH.POW(ta, 2))
  DIM neg(3) AS DOUBLE = {-1, 2, -3, 4}
  PRINT "ABS     ="; TORCH.SUM(TORCH.ABS(TORCH.FROM(neg)))

  ' ---- shape ops ----
  PRINT "== 6. shape ops =="
  PRINT "TRANSPOSE ="; TORCH.SUM(TORCH.TRANSPOSE(m2))
  PRINT "PERMUTE   ="; TORCH.SUM(TORCH.PERMUTE(m2, 1, 0))
  PRINT "SLICE     ="; TORCH.SUM(TORCH.SLICE(m2, 0, 0, 1))
  PRINT "CONCAT    ="; TORCH.SUM(TORCH.CONCAT(ta, ta))
  PRINT "STACK     ="; TORCH.SUM(TORCH.STACK(ta, ta))
  PRINT "SPLIT     ="; TORCH.SUM(TORCH.SPLIT(ta, 0, 2))

  ' ---- riduzioni / accesso / confronto ----
  PRINT "== 7. riduzioni/accesso/confronto =="
  PRINT "SUM/MEAN  ="; TORCH.SUM(ta); "/"; TORCH.MEAN(ta)
  PRINT "MAX/MIN   ="; TORCH.MAX(ta); "/"; TORCH.MIN(ta)
  PRINT "GET(0,0)  ="; TORCH.GET(ta, 0, 0)
  TORCH.SET(ta, 0, 0, 99)
  PRINT "SET->GET  ="; TORCH.GET(ta, 0, 0)
  PRINT "ALLCLOSE  ="; TORCH.ALLCLOSE(ta, ta, 0.000000001)
  DIM sh() = TORCH.SHAPE(m2)
  PRINT "SHAPE     ="; sh(0); "x"; sh(1)
  DIM arr() = TORCH.TOARRAY(ta)
  PRINT "TOARRAY   ="; arr(0); arr(1); arr(2); arr(3)

  ' ---- model / training (2 passi, per non rallentare) ----
  PRINT "== 8. model/training =="
  DIM xd(7) AS DOUBLE = {0,0, 1,1, 2,2, 3,3}
  DIM yd(3) AS DOUBLE = {0, 1, 2}
  x = TORCH.RESHAPE(TORCH.FROM(xd), 4, 2)
  y = TORCH.RESHAPE(TORCH.FROM(yd), 4, 1)
  mm = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT "PARAMETERS ="; TORCH.MODELPARAMETERS(mm)
  PRINT "LOSS(prima)="; TORCH.MODELLOSS(mm, x, y)
  TORCH.MODELTRAINSTEP(mm, x, y, "adam", "mse", 0.05)
  TORCH.MODELTRAINSTEP(mm, x, y, "adam", "mse", 0.05)
  PRINT "LOSS(dopo) ="; TORCH.MODELLOSS(mm, x, y)
  fw = TORCH.MODELFORWARD(mm, x)
  PRINT "FORWARD    ="; TORCH.SUM(fw)
  PRINT "STATE      ="; TORCH.MODELSTATE(mm)
  TORCH.MODELFREE(mm)

  PRINT "=== TORCH: tutte le funzioni eseguite ==="
END SUB
