' TORCH — aritmetica, attivazioni e funzioni puntuali.
IMPORT TORCH

SUB Main()
  a = TORCH.ZEROS(1, 3)
  TORCH.SET(a, 0, 0, -1.0) : TORCH.SET(a, 0, 1, 0.0) : TORCH.SET(a, 0, 2, 4.0)
  b = TORCH.ONES(1, 3)

  s = TORCH.SUB(a, b)
  m = TORCH.MUL(a, b)
  q = TORCH.DIV(a, TORCH.ONES(1, 3))
  PRINT "sub(0)="; TORCH.GET(s, 0, 0); " mul(2)="; TORCH.GET(m, 0, 2); " div(2)="; TORCH.GET(q, 0, 2)

  p = TORCH.POW(a, 2.0)
  PRINT "pow(0)="; ROUND(TORCH.GET(p, 0, 0), 6)

  PRINT "relu(0)="; TORCH.GET(TORCH.RELU(a), 0, 0)
  PRINT "abs(0)="; TORCH.GET(TORCH.ABS(a), 0, 0)
  PRINT "exp(1)="; ROUND(TORCH.GET(TORCH.EXP(a), 0, 1), 6)
  PRINT "sigmoid(1)="; ROUND(TORCH.GET(TORCH.SIGMOID(a), 0, 1), 6)
  PRINT "tanh(1)="; ROUND(TORCH.GET(TORCH.TANH(a), 0, 1), 6)
  PRINT "gelu(2)="; ROUND(TORCH.GET(TORCH.GELU(a), 0, 2), 6)
  PRINT "softmax sum="; ROUND(TORCH.SUM(TORCH.SOFTMAX(a)), 6)

  pos = TORCH.ZEROS(1, 1)
  TORCH.SET(pos, 0, 0, 9.0)
  PRINT "sqrt="; ROUND(TORCH.GET(TORCH.SQRT(pos), 0, 0), 6)
  PRINT "log="; ROUND(TORCH.GET(TORCH.LOG(pos), 0, 0), 6)
END SUB

