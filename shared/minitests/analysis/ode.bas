SUB Main()
  PRINT NUMERICAL.Integrate(FN(x) => x * x, 0, 1)
  PRINT NUMERICAL.Simpson(FN(x) => x * x, 0, 1, 256)
  DIM ys() = NUMERICAL.SolveOde({ |t, y| RETURN -y }, 1.0, 0.0, 1.0, 10)
  PRINT ys(10)
END SUB
