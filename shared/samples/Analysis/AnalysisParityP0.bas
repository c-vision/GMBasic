' FILE: Analysis/AnalysisParityP0.bas
' Sample ANALYSIS: Rfft / SolveIvp / Irfft / Splrep / Splev — deterministico.
' RFFT/IRFFT e SPLREP/SPLEV vivono nei pacchetti FFI (`signal`, `math`) — IMPORT + namespace.
' DIM esplicito sui risultati array (il C99Native tipizza gli array-returning solo da DIM).
IMPORT SIGNAL
IMPORT INTERP

SUB Main()
  ' RFFT su cos(2*pi*i/8): componenti non negative interleaved (2*(n\2+1) valori).
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf() = SIGNAL.RFFT(x)
  PRINT "rfft_ok="; IIF(LEN(rf) > 0, 1, 0)

  ' SolveIvp: dy/dt = -1, y(0)=1 => numero di punti risolti.
  DIM y0(0) AS DOUBLE = {1}
  DIM npts = NUMERICAL.SolveIvp({|t, y| RETURN -1}, y0, 0.0, 1.0)
  PRINT "solveivp_n="; npts

  ' Irfft (spettro reale): re(0)=1 => irfft(0) = 1/4.
  DIM re(4) AS DOUBLE = {1, 0.5, 0, 0, 0}
  DIM im(4) AS DOUBLE = {0, 0, 0, 0, 0}
  DIM ir() = SIGNAL.IRFFT(re, im)
  PRINT "irfft0="; ROUND(ir(0), 6)

  ' Splrep/Splev su (x+1)^2: interpolazione in 2.5 => 12.25.
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp() = INTERP.SPLREP(xs, ys)
  PRINT "splrep_ok="; IIF(LEN(sp) > 0, 1, 0)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = INTERP.SPLEV(sp, xq)
  PRINT "splev="; ROUND(sv(0), 6)
END SUB
