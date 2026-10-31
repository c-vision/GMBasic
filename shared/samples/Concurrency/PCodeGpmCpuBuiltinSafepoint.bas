' Verifica che un kernel numerico lungo non occupi il worker PCodeVM.
' Con GMBASIC_GOMAXPROCS=1, SIGNAL.CONVOLVE deve parcheggiare HeavyCpu e lasciare
' progredire Ticker prima di pubblicare CPU-HEAVY-DONE.
IMPORT SIGNAL
IMPORT ANALYSIS

SUB HeavyCpu(WG AS WAITGROUP)
  DIM x(4095) AS DOUBLE
  DIM k(4095) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 4095
    x(i) = 1
    k(i) = 1
  NEXT

  DIM result() = SIGNAL.CONVOLVE(x, k)
  IF result(4095) = 4096 THEN
    PRINT "CPU-RESULT-OK"
  ELSE
    PRINT "FAIL: CPU result"
  END IF

  DIM spectrum() = ANALYSIS.FFT(x)
  DIM restored() = ANALYSIS.IFFT(spectrum)
  IF restored(0, 0) > 0.999 AND restored(0, 0) < 1.001 THEN
    PRINT "CPU-IFFT-OK"
  ELSE
    PRINT "FAIL: CPU IFFT round-trip"
  END IF

  ' Anche i generatori O(n) devono lasciare libero l'unico worker P.
  DIM window() = SIGNAL.HANN(200000)
  IF window(0) = 0 AND window(199999) < 0.000001 THEN
    PRINT "CPU-WINDOW-OK"
  ELSE
    PRINT "FAIL: CPU window"
  END IF
  PRINT "CPU-HEAVY-DONE"
  DONE WG
END SUB

SUB Ticker(WG AS WAITGROUP)
  PRINT "CPU-TICK"
  DONE WG
END SUB

SUB Main()
  DIM WG AS WAITGROUP
  GO SUB HeavyCpu(WG) IN WG
  GO SUB Ticker(WG) IN WG
  WAIT WG
  PRINT "PCODE-GPM-CPU-SAFEPOINT-DONE"
END SUB
