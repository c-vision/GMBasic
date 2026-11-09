' MiniTest: KDTREE.CREATE (parita' 4 backend).
' L'handle e' opaco: si verifica che sia valido (>0) e che l'albero contenga
' tutti i punti (query_ball con raggio grande).
' EXPECT: ok|5
IMPORT KDTREE
SUB Main()
  DIM flat(9) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1}
  h = KDTREE.CREATE(flat, 2)
  IF h > 0 THEN PRINT "ok"
  DIM x(1) AS DOUBLE = {4, 4}
  DIM b() = KDTREE.QUERYBALL(h, x, 100)
  PRINT UBOUND(b) + 1
END SUB
