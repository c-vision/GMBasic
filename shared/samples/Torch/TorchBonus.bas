' ============================================================================
' TorchBonus.bas — TORCH: funzioni bonus del piano ML §3.6, via libtorch
' ============================================================================
' TOPK / ARGSORT / ONEHOT / CLIP / NORMALIZE / STANDARDIZE / SIMILARITY
' (le "bonus" del piano: quelle che si possono recuperare dal backend torch;
'  KMEANS/PCA/FFT/CONVOLVE restano nel runtime C / SIGNAL già esistenti).
' ============================================================================
IMPORT TORCH

SUB Main()
  DIM v(4) AS DOUBLE = {3, 1, 4, 1, 5}
  t = TORCH.FROM(v)
  PRINT "dati     = 3  1  4  1  5"

  PRINT "== TOPK (3 valori piu' grandi) =="
  DIM tk() = TORCH.TOPK(t, 3)
  PRINT "topk     ="; tk(0); tk(1); tk(2)

  PRINT "== ARGSORT (indici ordinati) =="
  DIM aso() = TORCH.ARGSORT(t)
  PRINT "argsort  ="; aso(0); aso(1); aso(2); aso(3); aso(4)

  PRINT "== ONEHOT (classe 2 su 4) =="
  DIM oh() = TORCH.ONEHOT(2, 4)
  PRINT "onehot   ="; oh(0); oh(1); oh(2); oh(3)

  PRINT "== CLIP (in [2,4]) =="
  DIM cl() = TORCH.CLIP(t, 2, 4)
  PRINT "clip     ="; cl(0); cl(1); cl(2); cl(3); cl(4)

  PRINT "== NORMALIZE (min-max) =="
  DIM no() = TORCH.NORMALIZE(t)
  PRINT "norm     ="; no(0); no(1); no(2); no(3); no(4)

  PRINT "== STANDARDIZE (z-score) =="
  DIM st() = TORCH.STANDARDIZE(t)
  PRINT "std      ="; st(0); st(1); st(2); st(3); st(4)

  PRINT "== SIMILARITY =="
  t2 = TORCH.FROM(v)
  PRINT "cos(v,v) ="; TORCH.SIMILARITY(t, t2, "cosine")
  PRINT "euc(v,v) ="; TORCH.SIMILARITY(t, t2, "euclid")
  DIM w(4) AS DOUBLE = {5, 4, 3, 2, 1}
  tw = TORCH.FROM(w)
  PRINT "cos(v,w) ="; TORCH.SIMILARITY(t, tw, "cosine")
END SUB
