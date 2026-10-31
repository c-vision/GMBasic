' MiniTest: L5 DPO/ORPO — cortese/scortese toy (parita' 4 backend).
' DPO impara il MARGINE P(chosen)>P(rejected), non l'argmax assoluto (che resta dominato
' da altri id): il check corretto legge le due probabilita' via SOFTMAX+GET, non l'argmax.
' Senza seed: convergenza-loss + margine positivo; MAI valori esatti.
' EXPECT: dpoloss: 1|dpomargin: 1|orpo: 1|badref: 1
IMPORT TORCH

SUB Main()
  DIM cfg AS STRING = "{""vocab"":6,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM xv(0) AS DOUBLE = {1}
  DIM cv(0) AS DOUBLE = {2}
  DIM rv(0) AS DOUBLE = {3}
  DIM tx AS LONG = TORCH.RESHAPE(TORCH.FROM(xv), 1, 1)
  DIM tc AS LONG = TORCH.RESHAPE(TORCH.FROM(cv), 1, 1)
  DIM tr AS LONG = TORCH.RESHAPE(TORCH.FROM(rv), 1, 1)
  DIM i AS LONG

  ' DPO con ref congelato
  DIM ms AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM mr AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM initL AS DOUBLE = TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")
  FOR i = 1 TO 80
    TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")
  NEXT
  DIM endL AS DOUBLE = TORCH.MODELDPOSTEP(ms, mr, tx, tc, tr, "{""beta"":0.5}")
  IF endL < initL THEN PRINT "dpoloss: 1" ELSE PRINT "dpoloss: 0"
  DIM lg AS LONG = TORCH.MODELFORWARD(ms, tx)
  DIM sl AS LONG = TORCH.SLICE(lg, 1, 0, 1)
  DIM flat AS LONG = TORCH.RESHAPE(sl, 1, 6)
  DIM pr AS LONG = TORCH.SOFTMAX(flat)
  DIM p2 AS DOUBLE = TORCH.GET(pr, 0, 2)
  DIM p3 AS DOUBLE = TORCH.GET(pr, 0, 3)
  IF p2 > p3 THEN PRINT "dpomargin: 1" ELSE PRINT "dpomargin: 0"

  ' ORPO senza ref: gira e converge
  DIM mo AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM initO AS DOUBLE = TORCH.MODELDPOSTEP(mo, 0, tx, tc, tr)
  FOR i = 1 TO 80
    TORCH.MODELDPOSTEP(mo, 0, tx, tc, tr)
  NEXT
  DIM endO AS DOUBLE = TORCH.MODELDPOSTEP(mo, 0, tx, tc, tr)
  IF endO < initO THEN PRINT "orpo: 1" ELSE PRINT "orpo: 0"

  ' ref invalido -> errore esplicito (-1)
  DIM bad AS DOUBLE = TORCH.MODELDPOSTEP(ms, 999999, tx, tc, tr)
  IF bad = -1 THEN PRINT "badref: 1" ELSE PRINT "badref: 0"

  TORCH.MODELFREE(ms) : TORCH.MODELFREE(mr) : TORCH.MODELFREE(mo)
  TORCH.MODELFREE(tx) : TORCH.MODELFREE(tc) : TORCH.MODELFREE(tr)
  TORCH.MODELFREE(lg) : TORCH.MODELFREE(sl) : TORCH.MODELFREE(flat) : TORCH.MODELFREE(pr)
END SUB
