' Wrapper TIMESERIES sui filtri SIGNAL: design Butterworth + applicazione in un colpo.
' Oracoli: SciPy 1.18 (lfilter di butter(2, wn) su impulso).
IMPORT TIMESERIES

SUB Main()
  DIM x(9) AS DOUBLE = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  DIM lp() = TIMESERIES.TSLOWPASS(x, 0.3, 2)
  PRINT "lowpass     ="; lp(0); lp(1); lp(2); lp(3); lp(4); lp(5)
  DIM hp() = TIMESERIES.TSHIGHPASS(x, 0.3, 2)
  PRINT "highpass    ="; hp(0); hp(1); hp(2); hp(3); hp(4); hp(5)
  DIM bp() = TIMESERIES.TSBANDPASS(x, 0.2, 0.5, 2)
  PRINT "bandpass    ="; bp(0); bp(1); bp(2); bp(3); bp(4); bp(5)
  DIM bs() = TIMESERIES.TSBANDSTOP(x, 0.2, 0.5, 2)
  PRINT "bandstop    ="; bs(0); bs(1); bs(2); bs(3); bs(4); bs(5)
END SUB
