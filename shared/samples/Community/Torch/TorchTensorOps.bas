' ============================================================================
' TorchTensorOps.bas — TORCH: operazioni tensoriali complete
' ============================================================================
' Costruttori: CREATE/FROM/ZEROS/ONES/EYE/RANDN.
' Aritmetica: ADD/SUB/MUL/DIV/MATMUL.
' Attivazioni: RELU/SIGMOID/TANH/SOFTMAX/GELU.
' Puntuali: EXP/LOG/SQRT/POW/ABS.
' Shape ops: RESHAPE/PERMUTE/TRANSPOSE/SLICE/CONCAT/STACK/SPLIT.
' Riduzioni: SUM/MEAN/MAX/MIN. Confronto: ALLCLOSE. Accesso: GET/SET.
' ============================================================================
IMPORT TORCH

SUB Main()
  DIM v(3) AS DOUBLE = {1, -2, 3, -4}
  t = TORCH.FROM(v)

  PRINT "== costruttori =="
  PRINT "zeros  ="; TORCH.SUM(TORCH.ZEROS(2, 2))
  PRINT "ones   ="; TORCH.SUM(TORCH.ONES(2, 2))
  PRINT "eye    ="; TORCH.SUM(TORCH.EYE(3))
  r = TORCH.RANDN(2, 2)
  PRINT "randn  ="; TORCH.SUM(r)

  PRINT "== attivazioni =="
  PRINT "relu   ="; TORCH.SUM(TORCH.RELU(t))
  PRINT "tanh   ="; TORCH.SUM(TORCH.TANH(t))
  PRINT "softmax="; TORCH.SUM(TORCH.SOFTMAX(t))
  PRINT "gelu   ="; TORCH.SUM(TORCH.GELU(t))

  PRINT "== puntuali =="
  PRINT "exp    ="; TORCH.SUM(TORCH.EXP(t))
  PRINT "abs    ="; TORCH.SUM(TORCH.ABS(t))
  PRINT "pow2   ="; TORCH.SUM(TORCH.POW(t, 2))
  PRINT "log(exp)= "; TORCH.SUM(TORCH.LOG(TORCH.EXP(t)))

  PRINT "== shape ops =="
  m2 = TORCH.RESHAPE(t, 2, 2)
  tr = TORCH.TRANSPOSE(m2)
  pe = TORCH.PERMUTE(m2, 1, 0)
  PRINT "reshape 2x2 ="; TORCH.SUM(m2); " transpose ="; TORCH.SUM(tr); " permute ="; TORCH.SUM(pe)
  PRINT "concat  ="; TORCH.SUM(TORCH.CONCAT(t, t))
  PRINT "stack   ="; TORCH.SUM(TORCH.STACK(t, t))
  PRINT "split0  ="; TORCH.SUM(TORCH.SPLIT(t, 0, 2))

  PRINT "== riduzioni / confronto =="
  PRINT "max/min ="; TORCH.MAX(t); "/"; TORCH.MIN(t)
  PRINT "allclose self ="; TORCH.ALLCLOSE(t, t, 0.000000001)

  PRINT "== SET =="
  TORCH.SET(m2, 0, 0, 99)
  PRINT "set(0,0)=99 ->"; TORCH.GET(m2, 0, 0)
END SUB
