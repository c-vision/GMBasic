' ============================================================================
' TorchBasics.bas — TORCH: device, loader e tensori base (backend ML nativo)
' ============================================================================
' Il namespace TORCH espone il runtime ML (PyTorch/libtorch via gmb_torch).
' Richiede le librerie in cache: `gmb ml setup` (CLI) o `ML SETUP` (REPL).
' Device/loader: DEVICE(), LIBPATH(), LOADED().
' Tensori (handle opachi): CREATE/FROM/ADD/MUL/MATMUL/RELU/GET/SET/TOARRAY/SHAPE.
' ============================================================================
IMPORT TORCH

SUB Main()
  PRINT "== TORCH: device e loader =="
  PRINT "device  ="; TORCH.DEVICE()
  PRINT "loaded  ="; TORCH.LOADED()
  PRINT "libpath ="; TORCH.LIBPATH()

  PRINT "== TORCH: costruttori =="
  z = TORCH.ZEROS(2, 2) : o = TORCH.ONES(2, 2) : e = TORCH.EYE(2)
  PRINT "zeros sum ="; TORCH.SUM(z); "  ones sum ="; TORCH.SUM(o); "  eye sum ="; TORCH.SUM(e)

  PRINT "== TORCH: FROM e aritmetica =="
  DIM a(3) AS DOUBLE = {1, 2, 3, 4}
  DIM b(3) AS DOUBLE = {5, 6, 7, 8}
  ta = TORCH.FROM(a)
  tb = TORCH.FROM(b)
  DIM aa() = TORCH.TOARRAY(ta)
  DIM ab() = TORCH.TOARRAY(TORCH.ADD(ta, tb))
  DIM asb() = TORCH.TOARRAY(TORCH.SUB(ta, tb))
  DIM ap() = TORCH.TOARRAY(TORCH.MUL(ta, tb))
  DIM ad() = TORCH.TOARRAY(TORCH.DIV(ta, tb))
  PRINT "a        ="; aa(0); aa(1); aa(2); aa(3)
  PRINT "a+b      ="; ab(0); ab(1); ab(2); ab(3)
  PRINT "a-b      ="; asb(0); asb(1); asb(2); asb(3)
  PRINT "a*b      ="; ap(0); ap(1); ap(2); ap(3)
  PRINT "a/b      ="; ad(0); ad(1); ad(2); ad(3)
  PRINT "sum/mean ="; TORCH.SUM(ta); "/"; TORCH.MEAN(ta)
  PRINT "max/min  ="; TORCH.MAX(ta); "/"; TORCH.MIN(ta)

  PRINT "== TORCH: matmul 2x2 =="
  tr = TORCH.RESHAPE(ta, 2, 2)
  mm = TORCH.MATMUL(tr, tr)
  DIM mo() = TORCH.TOARRAY(mm)
  PRINT "tr*tr    ="; mo(0); mo(1); mo(2); mo(3)

  PRINT "== TORCH: attivazioni =="
  DIM rl() = TORCH.TOARRAY(TORCH.RELU(TORCH.MUL(ta, -1)))
  PRINT "relu(-a) ="; rl(0); rl(1); rl(2); rl(3)
  PRINT "sigmoid  ="; TORCH.SUM(TORCH.SIGMOID(ta))
  PRINT "tanh     ="; TORCH.SUM(TORCH.TANH(ta))
  PRINT "gelu     ="; TORCH.SUM(TORCH.GELU(ta))
  PRINT "sqrt     ="; TORCH.SUM(TORCH.SQRT(ta))

  PRINT "== TORCH: accesso e confronto =="
  PRINT "get(0,0) ="; TORCH.GET(ta, 0, 0)
  PRINT "allclose ="; TORCH.ALLCLOSE(ta, ta, 0.000000001)

  TORCH.FREE(ta) : TORCH.FREE(tb) : TORCH.FREE(tr) : TORCH.FREE(mm)
END SUB
