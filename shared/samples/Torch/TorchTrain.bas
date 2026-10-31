' ============================================================================
' TorchTrain.bas — TORCH: addestramento minimo di una rete (MODEL.*)
' ============================================================================
' MODELLINEARE 2->1: impara y = 2*x0 - x1 dai dati (x = [[0,0],[1,1],[2,2],[3,3]],
' y = [0,1,2,0]). Ottimizzatore Adam, loss MSE, 500 passi.
' Funzioni: MODELCREATE / MODELTRAINSTEP / MODELLOSS / MODELFORWARD /
' MODELPARAMETERS / MODELSTATE / MODELFREE.
' ============================================================================
IMPORT TORCH

SUB Main()
  PRINT "== TORCH: crea modello (linear 2->1) =="
  m = TORCH.MODELCREATE("linear", "{""in"":2,""out"":1}")
  PRINT "params ="; TORCH.MODELPARAMETERS(m)

  PRINT "== TORCH: dati di addestramento =="
  DIM xd(7) AS DOUBLE = {0,0, 1,1, 2,2, 3,3}
  DIM yd(3) AS DOUBLE = {0, 1, 2}
  x = TORCH.RESHAPE(TORCH.FROM(xd), 4, 2)
  y = TORCH.RESHAPE(TORCH.FROM(yd), 4, 1)

  PRINT "== TORCH: training (500 passi Adam/MSE) =="
  PRINT "loss iniziale ="; TORCH.MODELLOSS(m, x, y)
  FOR i = 1 TO 500
    TORCH.MODELTRAINSTEP(m, x, y, "adam", "mse", 0.05)
  NEXT i
  PRINT "loss finale   ="; TORCH.MODELLOSS(m, x, y)

  PRINT "== TORCH: predizione =="
  f = TORCH.MODELFORWARD(m, x)
  DIM pred() = TORCH.TOARRAY(f)
  PRINT "pred  ="; pred(0); pred(1); pred(2); pred(3)
  PRINT "atteso= 0  1  2  3"
  PRINT "stato ="; TORCH.MODELSTATE(m)

  TORCH.MODELFREE(m)
END SUB
