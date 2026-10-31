' ============================================================================
' TorchDpoToy.bas — preference learning toy con TORCH.MODELDPOSTEP (L5).
' prompt [1], chosen [2], rejected [3]: DPO spinge P(2) sopra P(3) (margine), con o senza
' modello di riferimento (mr>0 = DPO, mr=0 = ORPO). Stampa margine prima/dopo.
' ============================================================================
IMPORT TORCH

SUB ShowMargin(m AS LONG, p AS STRING, tx AS LONG)
  DIM lg AS LONG = TORCH.MODELFORWARD(m, tx)
  DIM sl AS LONG = TORCH.SLICE(lg, 1, 0, 1)
  DIM pr AS LONG = TORCH.SOFTMAX(TORCH.RESHAPE(sl, 1, 6))
  DIM p2 AS DOUBLE = TORCH.GET(pr, 0, 2)
  DIM p3 AS DOUBLE = TORCH.GET(pr, 0, 3)
  PRINT p; "P(2)="; p2; " P(3)="; p3
  TORCH.MODELFREE(lg) : TORCH.MODELFREE(sl) : TORCH.MODELFREE(pr)
END SUB

SUB Main()
  DIM cfg AS STRING = "{""vocab"":6,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM xv(0) AS DOUBLE = {1}
  DIM cv(0) AS DOUBLE = {2}
  DIM rv(0) AS DOUBLE = {3}
  DIM tx AS LONG = TORCH.RESHAPE(TORCH.FROM(xv), 1, 1)
  DIM tc AS LONG = TORCH.RESHAPE(TORCH.FROM(cv), 1, 1)
  DIM tr AS LONG = TORCH.RESHAPE(TORCH.FROM(rv), 1, 1)
  DIM i AS LONG

  ' DPO con reference congelato
  DIM ms AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM mr AS LONG = TORCH.MODELCREATE("transformer", cfg)
  ShowMargin(ms, "prima:", tx)
  FOR i = 1 TO 60
    TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")
  NEXT
  ShowMargin(ms, "dopo: ", tx)

  ' ORPO senza reference
  DIM mo AS LONG = TORCH.MODELCREATE("transformer", cfg)
  ShowMargin(mo, "orpo0:", tx)
  FOR i = 1 TO 60
    TORCH.MODELDPOSTEP(mo, 0, tx, tc, tr)
  NEXT
  ShowMargin(mo, "orpo1:", tx)

  TORCH.MODELFREE(ms) : TORCH.MODELFREE(mr) : TORCH.MODELFREE(mo)
  TORCH.MODELFREE(tx) : TORCH.MODELFREE(tc) : TORCH.MODELFREE(tr)
  PRINT "=== TorchDpoToy: done (P(2) sopra P(3) dopo il training) ==="
END SUB
