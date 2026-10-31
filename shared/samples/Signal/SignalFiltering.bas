' ============================================================================
' SIGNAL — finestre, convoluzione, picchi e filtri IIR (Butterworth).
' Esempio per il namespace SIGNAL (scipy.signal-like).
' ============================================================================
IMPORT SIGNAL AS S

SUB Main()
  DIM x(7) AS DOUBLE = {1, 2, 3, 4, 5, 4, 3, 2}

  ' Finestra di Hann a 5 punti.
  DIM h() = S.HANN(5)
  PRINT "hann:    "; h(0); h(1); h(2); h(3); h(4)

  ' Convoluzione con kernel {1, 1}.
  DIM k(1) AS DOUBLE = {1, 1}
  DIM c() = S.CONVOLVE(x, k)
  PRINT "convolve:"; c(0); c(1); c(2)

  ' Picchi (distanza minima 1).
  DIM pk() = S.FINDPEAKS(x, 1)
  PRINT "peaks:   "; pk(0); pk(1)

  ' Filtro Butterworth low-pass (ordine 2) + applicazione.
  DIM wn(0) AS DOUBLE = {0.3}
  DIM fh = S.BUTTER(2, wn, "low")
  DIM y() = S.LFILTER(fh, x)
  PRINT "lfilter: "; ROUND(y(0), 6); ROUND(y(1), 6); ROUND(y(2), 6)

  ' Risposta in ampiezza a 4 punti.
  DIM fz() = S.FREQZ(fh, 4)
  PRINT "freqz:   "; ROUND(fz(0), 6); ROUND(fz(1), 6); ROUND(fz(2), 6); ROUND(fz(3), 6)
END SUB
