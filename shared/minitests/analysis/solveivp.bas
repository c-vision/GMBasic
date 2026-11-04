' MiniTest ANALYSIS.SOLVEIVP — ODE adattiva RK45.
' L'oracolo usa la FORMA della matrice risultato (righe/colonne), NON il valore
' dell'handle: quello è un contatore di registry, quindi diverso fra i backend
' (PCode 1, IL 2) e non è un oracolo valido.
' dy/dt = -1, y(0)=1, t∈[0,1] → traiettoria 2×2 (t, y); y_finale ≈ 0.
' EXPECT: rows= 2|cols= 2|yend= 0
IMPORT ANALYSIS
SUB Main()
  DIM y0(0) AS DOUBLE = {1}
  DIM h = ANALYSIS.SOLVEIVP(FN(t AS DOUBLE, y() AS DOUBLE) AS ARRAY OF DOUBLE => {-1}, y0, 0.0, 1.0)
  DIM rows = MATRIX.ROWCOUNT(h)
  PRINT "rows="; rows
  PRINT "cols="; MATRIX.COLUMNCOUNT(h)
  PRINT "yend="; MATH.ROUND(MATRIX.GETVALUE(h, rows - 1, 1), 6)
END SUB
