' TORCH — shape ops, riduzioni, normalizzazione e dati casuali.
IMPORT TORCH

SUB Main()
  ' costruttori: EYE / FROM / CREATE
  id = TORCH.EYE(3)
  PRINT "eye(0,0)="; TORCH.GET(id, 0, 0); " eye(0,1)="; TORCH.GET(id, 0, 1)
  DIM v(2) AS DOUBLE = {1, 2, 3}
  fv = TORCH.FROM(v)
  PRINT "from sum="; TORCH.SUM(fv)
  cr = TORCH.CREATE(2, 2)
  TORCH.SET(cr, 0, 0, 7.0)
  PRINT "create(0,0)="; TORCH.GET(cr, 0, 0)

  a = TORCH.ZEROS(2, 3)
  FOR i = 0 TO 1
    FOR j = 0 TO 2
      TORCH.SET(a, i, j, i * 3 + j + 1)
    NEXT j
  NEXT i

  r = TORCH.RESHAPE(a, 3, 2)
  DIM rsh() = TORCH.SHAPE(r)
  PRINT "reshape="; rsh(0); "x"; rsh(1)

  p = TORCH.PERMUTE(a, 1, 0)
  PRINT "permute(0,0)="; TORCH.GET(p, 0, 0)

  sl = TORCH.SLICE(a, 0, 0, 2)
  PRINT "slice(0,1)="; TORCH.GET(sl, 0, 1)

  b = TORCH.ONES(2, 3)
  cc = TORCH.CONCAT(a, b)
  st = TORCH.STACK(a, b)
  sp = TORCH.SPLIT(a, 0, 2)
  PRINT "concat(2,0)="; TORCH.GET(cc, 2, 0); " stack(0,0)="; TORCH.GET(st, 0, 0); " split(0,0)="; TORCH.GET(sp, 0, 0)

  DIM as() = TORCH.ARGSORT(a)
  PRINT "argsort(0)="; as(0)
  DIM tk() = TORCH.TOPK(a, 2)
  PRINT "topk(0)="; tk(0)
  PRINT "argmax="; TORCH.ARGMAX(a)
  PRINT "allclose="; TORCH.ALLCLOSE(a, a, 0.0001)

  DIM n() = TORCH.NORMALIZE(a)
  DIM sd() = TORCH.STANDARDIZE(a)
  DIM cl() = TORCH.CLIP(a, 2.0, 4.0)
  DIM oh() = TORCH.ONEHOT(1, 3)
  PRINT "normalize(0)="; ROUND(n(0), 4); " standardize(0)="; ROUND(sd(0), 4)
  PRINT "clip(0)="; ROUND(cl(0), 4); " onehot(0)="; oh(0); oh(1)

  rn = TORCH.RANDN(2, 3)
  DIM rshp() = TORCH.SHAPE(rn)
  PRINT "randn="; rshp(0); "x"; rshp(1)
END SUB

