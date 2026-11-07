' FILE: Spectral/SpectralLab.bas
' SCENARIO DIDATTICO — laboratorio di analisi di un segnale: un tono a 1 kHz
' campionato a 8 kHz, la sua scomposizione tempo-frequenza, la ricostruzione,
' la compressione in DCT di una rampa e di una piccola immagine, la wavelet di
' Haar su un gradino e la Z-trasformata di una risposta all'impulso.
'
' Regola dell'ABI: la lunghezza del segnale è quella DELL'ARRAY dichiarato.
' Per 4 campioni si dichiara `DIM x(3)`, per 8 `DIM x(7)`.
'
'   tone  : x[n] = cos(2*pi*n/8), 16 campioni  (1 periodo = 8 campioni -> bin 1)
'   fs    : 8000 Hz, N = 8 -> risoluzione fs/N = 1000 Hz per bin
'   finestra: rettangolare, hop = 4 (sovrapposizione 50%)
'   In un'analisi reale si passa una finestra di Hann (w8(i) = 0.5*(1-cos(...))):
'   qui la rettangolare rende il tono esattamente su un bin, così i numeri si
'   verificano a mano (|X| = N/2 = 4 sul bin 1, zero altrove).
'
' Lo spettro è in coppie (re, im) interleavate, frame per frame; per il frame f il
' bin k sta in out(2*(f*(N/2+1) + k)) e out(2*(f*(N/2+1) + k) + 1).
'
' Domini: SPECTRAL. Output deterministico.
IMPORT SPECTRAL

CONST PIVAL = 3.14159265358979323846

SUB Main()
  DIM x16(15) AS DOUBLE
  DIM w8(7) AS DOUBLE
  DIM st(29) AS DOUBLE          ' 3 frame * 5 bin * 2 (re,im)
  DIM rec(15) AS DOUBLE
  DIM r4(3) AS DOUBLE
  DIM d4(3) AS DOUBLE
  DIM z4(3) AS DOUBLE
  DIM img(15) AS DOUBLE         ' 4x4
  DIM coeff(15) AS DOUBLE
  DIM back(15) AS DOUBLE
  DIM grad(7) AS DOUBLE
  DIM harr(7) AS DOUBLE
  DIM h16(15) AS DOUBLE
  DIM two(1) AS DOUBLE
  DIM i AS LONG
  DIM f AS LONG, k AS LONG, idx AS LONG, peak AS LONG
  DIM fs AS DOUBLE, err AS DOUBLE, mag AS DOUBLE, best AS DOUBLE

  fs = 8000

  ' ── 1. il segnale e la finestra ────────────────────────────────────────────
  FOR i = 0 TO 15
    x16(i) = MATH.COS(2 * PIVAL * i / 8)
  NEXT i
  FOR i = 0 TO 7
    w8(i) = 1
  NEXT i
  PRINT "segnale: cos(2*pi*n/8), fs = "; fs; " Hz, N = 8 (bin = 1000 Hz)"

  ' ── 2. STFT: 3 frame, 5 bin per frame ─────────────────────────────────────
  PRINT "frame = "; SPECTRAL.STFT(x16, 8, w8, 4, st)
  FOR f = 0 TO 2
    peak = 0
    best = 0
    FOR k = 0 TO 4
      idx = 2 * (f * 5 + k)
      mag = MATH.SQR(st(idx) * st(idx) + st(idx + 1) * st(idx + 1))
      IF mag > best THEN
        best = mag
        peak = k
      END IF
    NEXT k
    PRINT " frame "; f; ": picco bin "; peak; " -> "; peak * fs / 8; " Hz, |X| = "; FORMAT(best, "#.000")
  NEXT f
  PRINT " X[frame 0, bin 1] = ("; FORMAT(st(2), "#.000000"); ", "; FORMAT(st(3), "#.000000"); ")"

  ' ── 3. iSTFT: ricostruzione del segnale ───────────────────────────────────
  PRINT "ricostruiti = "; SPECTRAL.ISTFT(st, 3, 8, w8, 4, rec)
  err = 0
  FOR i = 0 TO 15
    mag = MATH.ABS(rec(i) - x16(i))
    IF mag > err THEN err = mag
  NEXT i
  PRINT " errore massimo = "; FORMAT(err, "#.000000000000")
  PRINT " rec(0) = "; FORMAT(rec(0), "#.000000"); "  rec(4) = "; FORMAT(rec(4), "#.000000"); "  rec(8) = "; FORMAT(rec(8), "#.000000")

  ' ── 4. DCT: compressione di una rampa (tengo i primi 3 coefficienti) ──────
  r4(0) = 0 : r4(1) = 1 : r4(2) = 2 : r4(3) = 3
  PRINT "DCT rampa = "; SPECTRAL.DCT(r4, d4)
  PRINT " coefficienti = "; FORMAT(d4(0), "#.000000"); ", "; FORMAT(d4(1), "#.000000"); ", "; FORMAT(d4(2), "#.000000"); ", "; FORMAT(d4(3), "#.000000")
  d4(3) = 0                          ' butto via il dettaglio fine
  PRINT " IDCT (3 coeff) = "; SPECTRAL.IDCT(d4, z4)
  PRINT " ricostruzione = "; FORMAT(z4(0), "#.000000"); ", "; FORMAT(z4(1), "#.000000"); ", "; FORMAT(z4(2), "#.000000"); ", "; FORMAT(z4(3), "#.000000")

  ' ── 5. DCT 2D di una piccola immagine 4x4 (gradiente) ─────────────────────
  FOR i = 0 TO 15
    img(i) = i
  NEXT i
  PRINT "DCT2 4x4 = "; SPECTRAL.DCT2(img, 4, 4, coeff)
  PRINT " DC = "; FORMAT(coeff(0), "#.000000"); "  coeff(1) = "; FORMAT(coeff(1), "#.000000"); "  coeff(4) = "; FORMAT(coeff(4), "#.000000")
  PRINT " IDCT2 = "; SPECTRAL.IDCT2(coeff, 4, 4, back)
  err = 0
  FOR i = 0 TO 15
    mag = MATH.ABS(back(i) - img(i))
    IF mag > err THEN err = mag
  NEXT i
  PRINT " errore di ricostruzione = "; FORMAT(err, "#.000000000000")

  ' ── 6. Haar su un gradino [0,0,0,0,1,1,1,1] ───────────────────────────────
  FOR i = 0 TO 3
    grad(i) = 0
  NEXT i
  FOR i = 4 TO 7
    grad(i) = 1
  NEXT i
  PRINT "Haar gradino = "; SPECTRAL.HAAR(grad, harr)
  PRINT " = "; FORMAT(harr(0), "#.000000"); ", "; FORMAT(harr(1), "#.000000"); ", "; FORMAT(harr(2), "#.000000"); ", "; FORMAT(harr(3), "#.000000")
  PRINT "   "; FORMAT(harr(4), "#.000000"); ", "; FORMAT(harr(5), "#.000000"); ", "; FORMAT(harr(6), "#.000000"); ", "; FORMAT(harr(7), "#.000000")

  ' ── 7. Hilbert: per un tono puro il segnale analitico ha ampiezza costante ─
  PRINT "Hilbert = "; SPECTRAL.HILBERT(x16, h16)
  err = 0
  FOR i = 0 TO 15
    mag = MATH.SQR(x16(i) * x16(i) + h16(i) * h16(i))
    IF MATH.ABS(mag - 1) > err THEN err = MATH.ABS(mag - 1)
  NEXT i
  PRINT " |segnale analitico| - 1 (max) = "; FORMAT(err, "#.000000000000")
  PRINT " H(0) = "; FORMAT(h16(0), "#.000000"); "  H(1) = "; FORMAT(h16(1), "#.000000"); "  H(2) = "; FORMAT(h16(2), "#.000000")

  ' ── 8. Z-trasformata della risposta all'impulso [1, 0.5] ──────────────────
  ' H(1) = 1.5 (guadagno in continua), H(-1) = 0.5 (guadagno a Nyquist)
  r4(0) = 1 : r4(1) = 0.5 : r4(2) = 0 : r4(3) = 0
  PRINT "Z in z=1  = "; SPECTRAL.ZTRANSFORM(r4, 1, 0, two)
  PRINT " -> "; FORMAT(two(0), "#.000000"); " + j("; FORMAT(two(1), "#.000000"); ")"
  PRINT "Z in z=-1 = "; SPECTRAL.ZTRANSFORM(r4, -1, 0, two)
  PRINT " -> "; FORMAT(two(0), "#.000000"); " + j("; FORMAT(two(1), "#.000000"); ")"

  ' ── 9. guardie ────────────────────────────────────────────────────────────
  DIM corto(3) AS DOUBLE
  FOR i = 0 TO 3
    corto(i) = 1
  NEXT i
  PRINT "guardie: segnale piu corto della finestra = "; SPECTRAL.STFT(corto, 8, w8, 4, st)
  PRINT "         out della DCT2 troppo corto = "; SPECTRAL.DCT2(img, 4, 4, corto)
  PRINT "fine"
END SUB
