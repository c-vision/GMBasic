' MiniTest: TORCH.MODELCREATE("mlp", ...) con attivazioni per strato ("acts").
' Il config "acts" estende l'MLP di TORCH con la scelta di attivazione di gn_net
' (relu default, tanh, sigmoid, leaky, linear/none), senza una seconda API di modelli.
' La struttura è deterministica (numero parametri, shape, esito del passo); i valori
' numerici dipendono dall'inizializzazione casuale e non entrano nell'oracolo.
' EXPECT: params= 108|in/out= 4 / 2|fwd= 1 x 2|step= 0|loss>=0=-1|sig-params= 13|lin-params= 13
IMPORT TORCH

SUB Main()
  DIM m AS LONG
  m = TORCH.MODELCREATE("mlp", "{""in"":4,""hidden"":[8,6],""out"":2,""acts"":""tanh,relu""}")
  ' 4->8 (32+8) + 8->6 (48+6) + 6->2 (12+2) = 108
  PRINT "params="; TORCH.MODELPARAMETERS(m)
  PRINT "in/out="; TORCH.MODELINPUT(m); "/"; TORCH.MODELOUTPUT(m)

  DIM x AS LONG
  x = TORCH.ZEROS(1, 4)
  TORCH.SET(x, 0, 0, 0.5) : TORCH.SET(x, 0, 1, -0.25)
  TORCH.SET(x, 0, 2, 1.5) : TORCH.SET(x, 0, 3, 0.75)
  DIM f AS LONG
  f = TORCH.MODELFORWARD(m, x)
  DIM sh() = TORCH.SHAPE(f)
  PRINT "fwd="; sh(0); "x"; sh(1)

  DIM y AS LONG
  y = TORCH.ZEROS(1, 2)
  TORCH.SET(y, 0, 0, 1.0)
  PRINT "step="; TORCH.MODELTRAINSTEP(m, x, y, "sgd", "mse", 0.01)
  PRINT "loss>=0="; TORCH.MODELLOSS(m, x, y) >= 0

  ' attivazione singola per tutti gli strati e "linear" (nessuna attivazione)
  DIM m2 AS LONG
  m2 = TORCH.MODELCREATE("mlp", "{""in"":2,""hidden"":[3],""out"":1,""acts"":""sigmoid""}")
  PRINT "sig-params="; TORCH.MODELPARAMETERS(m2)
  DIM m3 AS LONG
  m3 = TORCH.MODELCREATE("mlp", "{""in"":2,""hidden"":[3],""out"":1,""acts"":""linear""}")
  PRINT "lin-params="; TORCH.MODELPARAMETERS(m3)

  TORCH.MODELFREE(m) : TORCH.MODELFREE(m2) : TORCH.MODELFREE(m3)
  TORCH.FREE(x) : TORCH.FREE(f) : TORCH.FREE(y)
END SUB
