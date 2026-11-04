' FILE: Analysis/AnalysisParityP0.bas
' Sample ANALYSIS: Rfft / SolveIvp / Irfft / Splrep / Splev — deterministico.
' DIM esplicito sui risultati array (il C99Native tipizza gli array-returning solo da DIM).

SUB Main()
  ' Rfft su cos(2*pi*i/8): ritorna un handle (verificato non nullo).
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf = NUMERICAL.Rfft(x)
  PRINT "rfft_ok="; IIF(rf <> 0, 1, 0)

  ' SolveIvp: dy/dt = -1, y(0)=1 => numero di punti risolti.
  DIM y0(0) AS DOUBLE = {1}
  DIM npts = NUMERICAL.SolveIvp({|t, y| RETURN -1}, y0, 0.0, 1.0)
  PRINT "solveivp_n="; npts

  ' Irfft (spettro reale): re(0)=1 => irfft(0) = 1/4.
  DIM re(4) AS DOUBLE = {1, 0.5, 0, 0, 0}
  DIM im(4) AS DOUBLE = {0, 0, 0, 0, 0}
  DIM ir() = NUMERICAL.Irfft(re, im)
  PRINT "irfft0="; ROUND(ir(0), 6)

  ' Splrep/Splev su (x+1)^2: interpolazione in 2.5 => 12.25.
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = NUMERICAL.Splrep(xs, ys)
  PRINT "splrep_ok="; IIF(sp <> 0, 1, 0)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = NUMERICAL.Splev(sp, xq)
  PRINT "splev="; ROUND(sv(0), 6)
END SUB
